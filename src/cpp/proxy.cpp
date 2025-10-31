#include <filesystem>
#include <iostream>
#include <fstream>
#include "proxy.hpp"
#include "static_strings.hpp"

NDProxy::NDProxy(int argc, char** argv)
    :is_duck_app(false), done(false)
{
    std::string usage("breadboard <breadboard_config_json_path> <test_dir> [<server_url>]");
    if (argc < 2) {
        printf("breadboard <breadboard_config_json_path>");
        exit(1);
    }
    exe = argv[0];
    bb_json_path = argv[1];

    if (!std::filesystem::exists(bb_json_path)) {
        std::cerr << usage << std::endl << "Cannot load breadboard config json from " << bb_json_path << std::endl;
        exit(1);
    }
    try {
        // breadboard.json specifies the base paths for the embedded py
        std::stringstream json_buffer;
        std::ifstream in_file_stream(bb_json_path);
        json_buffer << in_file_stream.rdbuf();
        bb_config = nlohmann::json::parse(json_buffer);
        server_url = bb_config["server_url"];
        is_duck_app = bb_config["duck_app"];
    }
    catch (...) {
        printf("cannot load breadboard.json");
        exit(1);
    }

    std::stringstream log_buffer;
    log_buffer << "NoDOM starting with..." << std::endl;
    log_buffer << "exe: " << exe << std::endl;
    log_buffer << "Breadboard config json: " << bb_json_path << std::endl;
    log_buffer << "Server URL: " << server_url << std::endl;
    std::cout << log_buffer.str() << std::endl;

    if (is_duck_app) {  // kick off DB thread
        duck_thread = boost::thread(&NDProxy::duck_loop, this);
    }
}

NDProxy::~NDProxy() {

}

void NDProxy::notify_server(const std::string& caddr, nlohmann::json& old_val, nlohmann::json& new_val)
{
    std::cout << "cpp: notify_server: " << caddr << ", old: " << old_val << ", new: " << new_val << std::endl;

    // build a JSON msg for the to_python Q
    nlohmann::json msg = { {nd_type_cs, data_change_cs}, {cache_key_cs, caddr}, {new_value_cs, new_val}, {old_value_cs, old_val} };
    try {
        // TODO: websockpp send in breadboard, EM_JS fetch for EMS
#ifndef __EMSCRIPTEN__
        // TODO: websockpp send in breadboard
        ws_send(msg.dump());
#else   // 
        // TODO: EM_JS fetch
#endif  // __EMSCRIPTEN__
    }
    catch (...) {
        std::cerr << "notify_server EXCEPTION!" << std::endl;
    }
}

void NDProxy::duck_dispatch(nlohmann::json& db_request)
{
    const static char* method = "NDProxy::duck_dispatch: ";

    std::cout << method << db_request << std::endl;
    try {
        // grab lock for this Q: should be free as ::duck_loop should be in to_cond.wait()
        boost::unique_lock<boost::mutex> request_lock(query_mutex);
        duck_queries.push(db_request);
    }
    catch (...) {
        std::cerr << "duck_dispatch EXCEPTION!" << std::endl;
    }
    // lock is out of scope so released: signal duck thread to wake up
    query_cond.notify_one();
}


#ifndef __EMSCRIPTEN__
bool NDProxy::duck_init()
{
    if (duckdb_open(NULL, &duck_db) == DuckDBError) {
        std::cerr << "DuckDB instantiation failed" << std::endl;
        return false;
    }
    if (duckdb_connect(duck_db, &duck_conn) == DuckDBError) {
        std::cerr << "DuckDB instantiation failed" << std::endl;
        return false;
    }
    return true;
}

void NDProxy::duck_fnls()
{
    duckdb_disconnect(&duck_conn);
    duckdb_close(&duck_db);
}

void NDProxy::duck_loop()
{
    static const char* method = "NDProxy::duck_loop: ";

    std::cout << method << "starting..." << std::endl;
    if (!duck_init()) {
        exit(1);
    }
    else {
        // lock the result queue and post back to the GUI thread
        nlohmann::json db_instance = { {nd_type_cs, duck_instance_cs} };
        boost::unique_lock<boost::mutex> results_lock(result_mutex);
        // send nd_type:DuckInstance
        duck_results.push(db_instance);
    }
    std::cout << method << "init done" << std::endl;

    nlohmann::json response_list_j = nlohmann::json::array();
    duckdb_state dbstate;
    std::unordered_map<std::string, duckdb_result> result_map;

    // https://www.boost.org/doc/libs/1_34_0/doc/html/boost/condition.html
    // A condition object is always used in conjunction with a mutex object (an object
    // whose type is a model of a Mutex or one of its refinements). The mutex object
    // must be locked prior to waiting on the condition, which is verified by passing a lock
    // object (an object whose type is a model of Lock or one of its refinements) to the
    // condition object's wait functions. Upon blocking on the condition object, the thread
    // unlocks the mutex object. When the thread returns from a call to one of the condition object's
    // wait functions the mutex object is again locked. The tricky unlock/lock sequence is performed
    // automatically by the condition object's wait functions.

    while (!done) {

        boost::unique_lock<boost::mutex> to_lock(query_mutex);
        query_cond.wait(to_lock);
        // thead quiesces in the wait above, with query_mutex
        // unlocked so the C++ thread can add work items
        std::cout << method << "duck_queries depth : " << duck_queries.size() << std::endl;
        while (!duck_queries.empty()) {
            nlohmann::json db_request(duck_queries.front());
            duck_queries.pop();
            if (!db_request.contains(nd_type_cs)) {
                std::cerr << method << "nd_type missing: " << db_request << std::endl;
                continue;
            }
            if (!db_request.contains(sql_cs)) {
                std::cerr << method << "sql missing: " << db_request << std::endl;
                continue;
            }
            const std::string& nd_type(db_request[nd_type_cs]);
            const std::string& sql(db_request[sql_cs]);
            const std::string& qid(db_request[query_id_cs]);
            nlohmann::json db_response = { {query_id_cs, qid}, { error_cs, 0 } };
            // ParquetScan request do not produce a result set, unlike queries
            if (nd_type == parquet_scan_cs) {
                dbstate = duckdb_query(duck_conn, sql.c_str(), nullptr);
                db_response[nd_type_cs] = parquet_scan_result_cs;
                if (dbstate == DuckDBError) {
                    std::cerr << method << "ParquetScan failed: " << db_request << std::endl;
                    db_response[error_cs] = 1;
                }
                else {
                    db_response[error_cs] = 0;
                }
            }
            else {
                // yes, this fires the duckdb_result default ctor the 1st
                // time a query_id is presented
                duckdb_result& dbresult(result_map[qid]);
                dbstate = duckdb_query(duck_conn, sql.c_str(), &dbresult);
                db_response[nd_type_cs] = query_result_cs;
                if (dbstate == DuckDBError) {
                    std::cerr << method << "Query failed: " << db_request << std::endl;
                    db_response[error_cs] = 1;
                }
                else {
                    db_response[error_cs] = 0;
                    duckdb_data_chunk chunk = duckdb_fetch_chunk(dbresult);
                    std::uint64_t chunk_ptr = reinterpret_cast<std::uint64_t>(chunk);
                    std::cout << method << "chunk_ptr: " << std::hex << chunk_ptr << std::endl;
                    // std::uint32_t will discard the hi 32 bits
                    db_response[chunk_cs] = nlohmann::json::array({ std::uint32_t(chunk_ptr), std::uint32_t(chunk_ptr >> 32) });
                }
            }
            // lock the result queue and post back to the GUI thread
            boost::unique_lock<boost::mutex> results_lock(result_mutex);
            duck_results.push(db_response);
        }
    }
    duck_fnls();
}

void NDProxy::get_duck_responses(std::queue<nlohmann::json>& responses)
{
    static const char* method = "NDProxy::get_duck_responses: ";
    boost::unique_lock<boost::mutex> from_lock(result_mutex);
    duck_results.swap(responses);
    if (!responses.empty()) {
        std::cout << method << responses.size() << " responses" << std::endl;
    }
}

#endif
