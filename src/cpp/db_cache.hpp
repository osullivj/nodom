#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include "static_strings.hpp"
#include "json_ops.hpp"

#ifndef __EMSCRIPTEN__
#include "nlohmann.hpp"     
#include <boost/thread.hpp>
#include <boost/atomic.hpp>

#ifdef NODOM_DUCK
#include <duckdb.h>
#else   // sqlite
#endif  // NODOM_DUCK

#else   // __EMSCRIPTEN__
#include <emscripten/val.h>
#endif  // __EMSCRIPTEN__


typedef std::function<void(const std::string&)> ws_sender;

#define STR_BUF_LEN 256
#define FMT_BUF_LEN 16

template <typename JSON>
class EmptyDBCache {
public:
    char* buffer{ 0 };  // zero copy accessor

    // Data access methods called by tables in NDContext. All DBCache
    // impls must provide these, even EmptyDBCache...
    std::uint64_t get_handle(JSON& ctx_data, const std::string& cname) { return 0; }
    std::uint64_t get_row_count(std::uint64_t handle) { return 0; }
    bool get_meta_data(std::uint64_t handle, std::uint64_t column_count) {return false;}
    char* get_datum(std::uint64_t handle, std::uint64_t colm_index, std::uint64_t row_index) { return "NULL"; }

    // Query submission and response handling methods
    void get_db_responses(std::queue<JSON>& responses) {
        static const char* method = "EmptyDBCache::get_db_responses: ";
        NDLogger::cout() << method << "NULL impl!" << std::endl;
    }

    void db_dispatch(JSON& db_request) {
        const static char* method = "EmptyDBCache::db_dispatch: ";
        NDLogger::cout() << method << "NULL impl!" << std::endl;
    }

    virtual ~EmptyDBCache() {}
};

#ifndef __EMSCRIPTEN__

// No DuckDB specifics in BreadBoardDBCache,
// but we are using stuff we don't have on EMS.
// For example, boost threads. Also note that since
// this is BreadBoard we know we're on win32, so
// can explicitly specialise EmptyDBCache with
// nlohmann::json
class BreadBoardDBCache : public EmptyDBCache<nlohmann::json> {
protected:
    char string_buffer[STR_BUF_LEN];
    char format_buffer[FMT_BUF_LEN];
    std::queue<nlohmann::json>          db_queries;
    std::queue<nlohmann::json>          db_results;
    boost::mutex                        query_mutex;
    boost::mutex                        result_mutex;
    boost::condition_variable           query_cond;
    boost::thread                       db_thread;
    // ref to NDWebSockClient::send
    ws_sender                           ws_send;
    bool                                done{ false };
public:
    BreadBoardDBCache() {
        memset(string_buffer, 0, STR_BUF_LEN);
        memset(format_buffer, 0, FMT_BUF_LEN);
    }
    virtual ~BreadBoardDBCache() {}

    void get_db_responses(std::queue<nlohmann::json>& responses) {
        static const char* method = "DBCache::get_db_responses: ";
        boost::unique_lock<boost::mutex> from_lock(result_mutex);
        db_results.swap(responses);
        if (!responses.empty()) {
            std::cout << method << responses.size() << " responses" << std::endl;
        }
    }

    void db_dispatch(nlohmann::json& db_request) {
        const static char* method = "DBCache::db_dispatch: ";
        std::cout << method << db_request << std::endl;
        try {
            // grab lock for this Q: should be free as ::duck_loop should be in to_cond.wait()
            boost::unique_lock<boost::mutex> request_lock(query_mutex);
            db_queries.push(db_request);
        }
        catch (...) {
            std::cerr << "db_dispatch EXCEPTION!" << std::endl;
        }
        // lock is out of scope so released: signal duck thread to wake up
        query_cond.notify_one();
    }
};

static constexpr int MAX_COLUMNS = 256;

class DuckDBCache : public BreadBoardDBCache {
private:
    duckdb_database                     duck_db;
    duckdb_connection                   duck_conn;
    std::unordered_map<std::uint64_t, std::vector<duckdb_vector>> column_map;
    std::unordered_map<std::uint64_t, std::vector<uint64_t*>> validity_map;
    std::unordered_map<std::uint64_t, std::vector<duckdb_type>> type_map;
    std::unordered_map<std::uint64_t, std::vector<duckdb_logical_type>> logical_type_map;
    int16_t* sidata = nullptr;
    int32_t* idata = nullptr;
    int64_t* bidata = nullptr;
    duckdb_hugeint* hidata = nullptr;
    double* dbldata = nullptr;
    duckdb_string_t* vcdata = nullptr;
    duckdb_type decimal_type;
    uint8_t decimal_width = 0;
    uint8_t decimal_scale = 0;
    double  decimal_divisor = 1.0;
    double  decimal_value = 0.0;
public:
    // db_init, db_fnls, db_loop: these three methods exec 
    // on the DB thread
    bool db_init() {
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

    void db_fnls() {
        duckdb_disconnect(&duck_conn);
        duckdb_close(&duck_db);
    }

    void db_loop() {
        static const char* method = "DuckDBCache::db_loop: ";

        std::cout << method << "starting..." << std::endl;
        if (!db_init()) {
            std::cout << method << "DB: init FAILED" << std::endl;
            exit(1);
        }
        else {
            // lock the result queue and post back to the GUI thread
            nlohmann::json db_instance = { {nd_type_cs, duck_instance_cs} };
            boost::unique_lock<boost::mutex> results_lock(result_mutex);
            // send nd_type:DuckInstance
            db_results.push(db_instance);
            std::cout << method << "DB: " << db_instance << std::endl;
        }

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
            std::cout << method << "db_queries depth : " << db_queries.size() << std::endl;
            while (!db_queries.empty()) {
                nlohmann::json db_request(db_queries.front());
                db_queries.pop();
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
                    std::cout << method << "QID: " << qid << ", SQL: " << sql << std::endl;
                    db_response[nd_type_cs] = query_result_cs;
                    if (dbstate == DuckDBError) {
                        std::cerr << method << "Query failed: " << db_request << std::endl;
                        db_response[error_cs] = 1;
                    }
                    else {
                        db_response[error_cs] = 0;
                        duckdb_data_chunk chunk = duckdb_fetch_chunk(dbresult);
                        std::uint64_t chunk_ptr = reinterpret_cast<std::uint64_t>(chunk);
                        nlohmann::json handle(chunk_ptr);
                        std::cout << method << qid << " chunk_ptr: " << std::hex << chunk_ptr << std::endl;
                        // std::uint32_t will discard the hi 32 bits
                        db_response[db_handle_cs] = handle;
                    }
                }
                // lock the result queue and post back to the GUI thread
                boost::unique_lock<boost::mutex> results_lock(result_mutex);
                db_results.push(db_response);
            }
        }
        db_fnls();
    }

    void start_db_thread() {
        db_thread = boost::thread(&DuckDBCache::db_loop, this);
    }

    // GUI thread methods for accessing the data
    std::uint64_t get_handle(nlohmann::json& ctx_data, const std::string& cname) {
        static const char* method = "DuckDBCache::get_handle: ";

        nlohmann::json handle(ctx_data[cname]);
        return handle.get<std::uint64_t>();
        /* TODO: rm as this is now redundant given uint64
        if (handle_array.type() != nlohmann::json::value_t::array) {
            std::cerr << method << "handle_array: " << handle_array 
                << "isn't an array!" << std::endl;
            return 0;
        }
        if (handle_array.size() != 2) {
            std::cerr << method << "handle_array: " << handle_array
                << "isn't two elements!" << std::endl;
            return 0;
        }
        if (handle_array[0].type() != nlohmann::json::value_t::number_unsigned
            || handle_array[1].type() != nlohmann::json::value_t::number_unsigned) {
            std::cerr << method << "handle_array: " << handle_array
                << "isn't two number_unsigned!" << std::endl;
            return 0;
        }

        std::uint64_t chunk_lo = handle_array[0];
        std::uint64_t chunk_hi = static_cast<uint64_t>(handle_array[1]) << 32;
        std::uint64_t chunk_ptr = chunk_hi + chunk_lo;
        std::cout << method << "chunk_ptr: " << chunk_hi << "," << chunk_lo << std::endl;
        std::cout << method << "chunk_ptr: " << std::hex << chunk_ptr << std::endl;
        duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(chunk_ptr);
        return reinterpret_cast<std::uint64_t>(chunk); */
        return 0;
    }

    std::uint64_t get_row_count(std::uint64_t handle) {
        duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(handle);
        return duckdb_data_chunk_get_size(chunk);
    }

    bool get_meta_data(std::uint64_t handle, std::uint64_t column_count) {
        duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(handle);
        // yes, these fire default ctor on 1st visit
        std::vector<duckdb_vector>& columns(column_map[handle]);
        std::vector<uint64_t*>& validities(validity_map[handle]);
        std::vector<duckdb_type>& types(type_map[handle]);
        std::vector<duckdb_logical_type>& logical_types(logical_type_map[handle]);
        columns.clear();
        validities.clear();
        types.clear();
        logical_types.clear();
        duckdb_vector colm{};
        duckdb_logical_type type_l{};
        for (std::uint64_t index = 0; index < column_count; ++index) {
            colm = duckdb_data_chunk_get_vector(chunk, index);
            columns.push_back(colm);
            validities.push_back(duckdb_vector_get_validity(colm));
            type_l = duckdb_vector_get_column_type(colm);
            logical_types.push_back(type_l);
            types.push_back(duckdb_get_type_id(type_l));
        }
        return true;
    }

    char* get_datum(std::uint64_t handle, std::uint64_t colm_index, std::uint64_t row_index) {
        std::vector<duckdb_vector>& columns(column_map[handle]);
        std::vector<uint64_t*> validities(validity_map[handle]);
        std::vector<duckdb_type> types(type_map[handle]);
        std::vector<duckdb_logical_type> logical_types(logical_type_map[handle]);
        duckdb_type colm_type(types[colm_index]);
        duckdb_logical_type colm_type_l(logical_types[colm_index]);
        duckdb_vector colm(columns[colm_index]);
        // In most cases we'll copy into string_buffer, or
        // use sprintf to format into buffer. But for long
        // strings we may want to hand back Duck's start
        // and end char*
        buffer = string_buffer;
        if (duckdb_validity_row_is_valid(validities[colm_index], row_index)) {
            switch (colm_type) {
            case DUCKDB_TYPE_VARCHAR:
                vcdata = (duckdb_string_t*)duckdb_vector_get_data(colm);
                if (duckdb_string_is_inlined(vcdata[row_index])) {
                    // if inlined is 12 chars, there will be no zero terminator
                    memcpy(string_buffer, vcdata[row_index].value.inlined.inlined, vcdata[row_index].value.inlined.length);
                    string_buffer[vcdata[row_index].value.inlined.length] = 0;
                }
                else {
                    // NB ptr arithmetic using length to calc the end
                    // ptr as these strings may be packed without 0 terminators
                    buffer = vcdata[row_index].value.pointer.ptr;
                    return vcdata[row_index].value.pointer.ptr + vcdata[row_index].value.pointer.length;
                }
                break;
            case DUCKDB_TYPE_BIGINT:
                bidata = (int64_t*)duckdb_vector_get_data(colm);
                sprintf(string_buffer, "%I64d", bidata[row_index]);
                break;
            case DUCKDB_TYPE_DOUBLE:
                dbldata = (double_t*)duckdb_vector_get_data(colm);
                sprintf(string_buffer, "%f", dbldata[row_index]);
                break;
            case DUCKDB_TYPE_DECIMAL:
                decimal_width = duckdb_decimal_width(colm_type_l);
                decimal_scale = duckdb_decimal_scale(colm_type_l);
                decimal_type = duckdb_decimal_internal_type(colm_type_l);
                decimal_divisor = pow(10, decimal_scale);

                switch (decimal_type) {
                case DUCKDB_TYPE_SMALLINT:  // int16_t
                    sidata = (int16_t*)duckdb_vector_get_data(colm);
                    decimal_value = (double)sidata[row_index] / decimal_divisor;
                    break;
                case DUCKDB_TYPE_INTEGER:   // int32_t
                    idata = (int32_t*)duckdb_vector_get_data(colm);
                    decimal_value = (double)idata[row_index] / decimal_divisor;
                    break;
                case DUCKDB_TYPE_BIGINT:    // int64_t
                    bidata = (int64_t*)duckdb_vector_get_data(colm);
                    decimal_value = (double)bidata[row_index] / decimal_divisor;
                    break;
                case DUCKDB_TYPE_HUGEINT:   // duckdb_hugeint: 128
                    hidata = (duckdb_hugeint*)duckdb_vector_get_data(colm);
                    decimal_value = duckdb_hugeint_to_double(hidata[row_index]) / decimal_divisor;
                    break;
                }
                // decimal scale 2 means we want "%.2f"
                // NB %% is the "escape sequence" for %
                // in a printf format, not \%.
                sprintf(format_buffer, "%%.%df", decimal_scale);
                sprintf(string_buffer, format_buffer, decimal_value);
                break;
            }
        }
        else {
            buffer = (char*)null_cs;
        }
        return 0;
    }
};

#else   // __EMSCRIPTEN__

EM_JS(void, ems_db_dispatch, (emscripten::EM_VAL db_request_handle), {
    var db_request = Emval.toValue(db_request_handle);
    window.__nodom__.duck_module.postMessage(db_request);
});

class DuckDBWebCache : public EmptyDBCache<emscripten::val> {
public:
    char* buffer{ 0 };
private:
    std::unordered_map<std::uint64_t, std::vector<emscripten::val>> column_map;
    std::unordered_map<std::uint64_t, std::vector<int>> type_map;

protected:
    std::queue<emscripten::val>          db_queries;
    std::queue<emscripten::val>          db_results;
public:
    // Duck specific methods
    // check_duck_module: has nodom.html finished
    // loading duck_module.js yet? Call from im_loop_body
    bool check_duck_module() {
        // NB this is based on imgui-jswt main.ts:check_duck_module
        // Q: has duck_module.js created window.__nodom__ ?
        emscripten::val window_global = emscripten::val::global("window");
        return JContains(window_global, __nodom__cs);
    }

    // register this function with DBResultDispatcher
    // at startup time
    void add_db_response(emscripten::EM_VAL result_handle) {
        emscripten::val result = emscripten::val::take_ownership(result_handle);
        db_results.push(result);
    }

    // standard DB methods implemented by every cache
    void get_db_responses(std::queue<emscripten::val>& responses) {
        static const char* method = "DBCache::get_db_responses: ";
        db_results.swap(responses);
        if (!responses.empty()) {
            std::cout << method << responses.size() << " responses" << std::endl;
        }
    }

    void db_dispatch(emscripten::val& db_request) {
        const static char* method = "DBCache::db_dispatch: ";
        std::cout << method << db_request << std::endl;
        ems_db_dispatch(db_request.as_handle());
    }

    // GUI thread methods for accessing the data
    std::uint64_t get_handle(emscripten::val& ctx_data, const std::string& cname) {
        emscripten::val results(ctx_data[cname]);
        // return the EM_VAL handle...
        return reinterpret_cast<std::uint64_t>(results.as_handle());
    }

    std::uint64_t get_row_count(std::uint64_t handle) {
        emscripten::val results = emscripten::val::take_ownership(reinterpret_cast<emscripten::EM_VAL>(handle));
        emscripten::val arrow_table(results["arrow_table"]);
        std::uint64_t row_count = arrow_table["numRows"].template as<std::uint64_t>();
        return row_count;
    }

    bool get_meta_data(std::uint64_t handle, std::uint64_t column_count) {
        emscripten::val results = emscripten::val::take_ownership(reinterpret_cast<emscripten::EM_VAL>(handle));
        emscripten::val types(results["types"]);
        emscripten::val names(results["names"]);
        std::vector<std::string> colm_names = emscripten::vecFromJSArray<std::string>(names);
        std::vector<int> colm_types = emscripten::convertJSArrayToNumberVector< int>(types);
        type_map[handle] = colm_types;
        std::vector<emscripten::val>& columns(column_map[handle]);
        emscripten::val arrow_table(results["arrow_table"]);
        for (std::uint64_t index = 0; index < column_count; ++index) {
            const std::string& colm_name(colm_names[column_count]);
            emscripten::val colm = 
                arrow_table.call<emscripten::val>("getColumn", colm_names[index]);
            columns.push_back(colm);
        }
        return true;
    }

    char* get_datum(std::uint64_t handle, std::uint64_t colm_index, std::uint64_t row_index) {
        buffer = (char*)null_cs;
        return 0;
    }
};

using Dispatcher = std::function<void(emscripten::EM_VAL result_handle)>;
class DBResultDispatcher {
private:
    DBResultDispatcher() {};
    Dispatcher dispatcher_func = nullptr;
public:
    static DBResultDispatcher& get_instance() {
        static DBResultDispatcher instance;
        return instance;
    }
    void set_dispatcher(Dispatcher df) { dispatcher_func = df; }
    void dispatch(emscripten::EM_VAL result_handle) {
        if (dispatcher_func == nullptr) {
            fprintf(stderr, "NULL DBResultDispatcher func\n");
            return;
        }
        dispatcher_func(result_handle);
    }
};

extern "C" {
    void on_db_result(emscripten::EM_VAL result_handle) {
        auto d = DBResultDispatcher::get_instance();
        d.dispatch(result_handle);
    }
};

#endif  // __EMSCRIPTEN__