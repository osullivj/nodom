#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include "nd_types.hpp"
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



#define STR_BUF_LEN 256
#define FMT_BUF_LEN 16

template <typename JSON>
class EmptyDBCache {
public:
    char* buffer{ 0 };  // zero copy accessor

    // Data access methods called by tables in NDContext. All DBCache
    // impls must provide these, even EmptyDBCache...
    std::uint64_t get_handle(const std::string& qid) { return 0; }
    std::uint64_t get_row_count(std::uint64_t handle) { return 0; }
    bool get_meta_data(std::uint64_t handle, std::uint64_t& column_count, std::uint64_t& row_count) {return false;}
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
    WebSockSenderFunc                   ws_send;
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

using ChunkDeque = std::deque<duckdb_data_chunk>;

class DuckDBCache : public BreadBoardDBCache {
private:
    duckdb_database                     duck_db;
    duckdb_connection                   duck_conn;

    std::unordered_map<std::string, duckdb_result>  result_map;
    std::unordered_map<std::string, ChunkDeque>     chunk_map;

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
            nlohmann::json db_instance = {
                {nd_type_cs, duck_instance_cs},
                {query_id_cs, "TEST_QUERY_0"}
            };
            boost::unique_lock<boost::mutex> results_lock(result_mutex);
            // send nd_type:DuckInstance
            db_results.push(db_instance);
            std::cout << method << "DB: " << db_instance << std::endl;
        }

        nlohmann::json response_list_j = nlohmann::json::array();
        duckdb_state dbstate;

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
                const std::string& nd_type(db_request[nd_type_cs]);
                if ((nd_type == parquet_scan_cs || nd_type == query_cs)
                    && !db_request.contains(sql_cs)) {
                    std::cerr << method << "sql missing: " << db_request << std::endl;
                    continue;
                }
                const std::string& qid(db_request[query_id_cs]);
                nlohmann::json db_response = { {query_id_cs, qid}, { error_cs, 0 } };
                // ParquetScan request do not produce a result set, unlike queries
                if (nd_type == parquet_scan_cs) {
                    const std::string& sql(db_request[sql_cs]);
                    dbstate = duckdb_query(duck_conn, sql.c_str(), nullptr);
                    db_response[nd_type_cs] = parquet_scan_result_cs;
                    if (dbstate == DuckDBError) {
                        std::cerr << method << "ParquetScan failed: " << db_request << std::endl;
                        db_response[error_cs] = 1;
                    }
                }
                else if (nd_type == query_cs) {
                    const std::string& sql(db_request[sql_cs]);
                    duckdb_result dbresult;
                    dbstate = duckdb_query(duck_conn, sql.c_str(), &dbresult);
                    std::cout << method << "QID: " << qid << ", SQL: " << sql << std::endl;
                    db_response[nd_type_cs] = query_result_cs;
                    if (dbstate == DuckDBError) {
                        std::cerr << method << "Query failed: " << db_request << std::endl;
                        db_response[error_cs] = 1;
                    }
                    else {
                        result_map[qid] = dbresult;
                    }
                }
                else if (nd_type == batch_request_cs) {
                    db_response[nd_type_cs] = batch_response_cs;
                    auto result_iter = result_map.find(qid);
                    if (result_iter == result_map.end()) {
                        db_response[error_cs] = 1;
                        std::cerr << method << "BatchRequest failed: " << db_request << std::endl;
                    }
                    else {
                        db_response[error_cs] = 0;
                        duckdb_data_chunk chunk = duckdb_fetch_chunk(result_iter->second);
                        ChunkDeque& chunk_deck(chunk_map[qid]);
                        chunk_deck.push_back(chunk);
                        /*
                        std::uint64_t chunk_ptr = reinterpret_cast<std::uint64_t>(chunk);
                        // NB [0] is the low word, and has the top 32bits sliced off
                        //    [1] is the high word, so we right shift by 32bits to discard the bottom 32 
                        nlohmann::json chunk_array = nlohmann::json::array({ std::uint32_t(chunk_ptr), std::uint32_t(chunk_ptr >> 32) });
                        std::cout << method << qid << " chunk_ptr: " << std::hex << chunk_ptr
                            << ", chunk_array: " << chunk_array << std::endl;
                        db_response[chunk_cs] = chunk_array;
                        */
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
    std::uint64_t get_handle(const std::string& qid) {
        static const char* method = "DuckDBCache::get_handle: ";
        ChunkDeque& chunk_deck(chunk_map[qid]);
        if (chunk_deck.empty())
            return 0;
        return reinterpret_cast<std::uint64_t>(chunk_deck.back());
    }

    std::uint64_t get_row_count(std::uint64_t handle) {
        duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(handle);
        return duckdb_data_chunk_get_size(chunk);
    }

    bool get_meta_data(std::uint64_t handle, std::uint64_t& column_count, std::uint64_t& row_count) {
        duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(handle);
        row_count = duckdb_data_chunk_get_size(chunk);
        column_count = duckdb_data_chunk_get_column_count(chunk);
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
    // duck_module.js is a deferred load, so may not have
    // completed by the time WASM starts running. So
    // by the time we get here window.__nodom__ will be
    // defined, but the duck_module will not have been set.
    // Suspect wasm doesn't see assignments that happen
    // after WASM start. However, if we send to window,
    // it looks like the event is shared with duck_module
    // as it gets through to the onmessage handler...
    // window.__nodom__.duck_module.postMessage(db_request);
    window.postMessage(db_request);
});


class DuckDBWebCache : public EmptyDBCache<emscripten::val> {
public:
    char* buffer{ 0 };
private:
    std::unordered_map<std::uint64_t, std::vector<std::string>> column_map;
    std::unordered_map<std::uint64_t, std::vector<int>> type_map;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> caddr_map;
    ChunkMap    chunk_map;
protected:
    char string_buffer[STR_BUF_LEN];
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

    // register with DBResultDispatcher at startup time
    void add_db_response(emscripten::EM_VAL result_handle) {
        emscripten::val result = emscripten::val::take_ownership(result_handle);
        db_results.push(result);
    }

    void register_chunk(const char* qid, int size, int addr) {
        // Will ctor ChunkVec on first batch...
        ChunkVec& chunk_vector = chunk_map[qid];
        chunk_vector.emplace_back(Chunk(size, addr));
    }

    // standard DB methods implemented by every cache
    void get_db_responses(std::queue<emscripten::val>& responses) {
        static const char* method = "DuckDBWebCache::get_db_responses: ";
        db_results.swap(responses);
        if (!responses.empty()) {
            std::cout << method << responses.size() << " responses" << std::endl;
        }
    }

    void db_dispatch(emscripten::val& db_request) {
        const static char* method = "DuckDBWebCache::db_dispatch: ";
        std::cout << method << db_request << std::endl;
        ems_db_dispatch(db_request.as_handle());
    }

    // GUI thread methods for accessing the data
    std::uint64_t get_handle(const std::string& qname) {
        auto cv_iter = chunk_map.find(qname);
        if (cv_iter == chunk_map.end()) return 0;
        ChunkVec& chunk_vector = chunk_map[qname];
        if (chunk_vector.empty()) return 0;
        Chunk& chunk(chunk_vector.back());
        return chunk.addr;
    }

    std::uint64_t get_row_count(std::uint64_t handle) {
        // First 3 32 bit words are done, ncols, nrows
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        std::uint64_t row_count = *(chunk_ptr + 2);
        return row_count;
    }

    bool get_meta_data(std::uint64_t handle, std::uint64_t& column_count, std::uint64_t& row_count) {
        // First 3 32 bit words are done, ncols, nrows
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        bool done = static_cast<bool>(*chunk_ptr++);
        column_count = reinterpret_cast<uint32_t>(*chunk_ptr++);
        row_count = reinterpret_cast<uint32_t>(*chunk_ptr++);
        // Next we have types, one 32bit int per col
        std::vector<int>& tipes = type_map[handle];
        tipes.clear();
        for (int i = 0; i < column_count; i++) {
            DuckType tipe{ *chunk_ptr++ };
            tipes.push_back(tipe);
        }
        // Column addresses: one 32 bit word per col
        std::vector<uint32_t>& caddrs = caddr_map[handle];
        for (int i = 0; i < column_count; i++) {
            uint32_t* col_addr = reinterpret_cast<uint32_t*>(*chunk_ptr++);
        }
        // After types we have column names
        std::vector<std::string>& colm_names = column_map[handle];
        colm_names.clear();
        for (int i = 0; i < column_count; i++) {
            std::string name;
            // Build name str one char at a time
            while (*chunk_ptr != 0)
                name.push_back(*chunk_ptr++);
            // Skip over null term
            chunk_ptr++;
            colm_names.push_back(name);
        }
        return done;
    }

    char* get_datum(std::uint64_t handle, std::uint64_t colm_index, std::uint64_t row_index) {
        const static char* method = "DuckDBWebCache::get_datum: ";
        // First block in a chunk is metadata, so calc the skip fwd...
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        std::vector<int>& tipes = type_map[handle];
        std::vector<std::string>& colm_names = column_map[handle];
        // ffwd past done,ncols,nrows,types to the col addresses
        // and get the column ptr
        chunk_ptr += 3 + tipes.size();
        uint32_t* col_addr = chunk_ptr + colm_index;
        // sanity check column type
        uint32_t col_type = *chunk_ptr++;
        uint32_t col_size = *chunk_ptr++;
        if (col_type != tipes[colm_index]) {
            fprintf(stderr, "%s: col type mismatch, col:%d, mtype:%d, stype:%d\n", 
                colm_index, col_type, tipes[colm_index]);
        }
        // stride 1 for 32bit data and 2 for 64bit inc str
        uint32_t stride = col_size / 4;
        DuckType dt{ col_type };
        int32_t* i32data = nullptr;
        double* dbldata = nullptr;
        time_t* timdata = nullptr;
        tm* tmdata = nullptr;
        // In most cases we'll copy into string_buffer, or
        // use sprintf to format into buffer. 
        buffer = string_buffer;
        switch (dt) {
        case DuckType::Int32:
            i32data = reinterpret_cast<int32_t*>(chunk_ptr);
            sprintf(string_buffer, "%f", i32data[row_index]);
            return 0;
        case DuckType::Float64:
            dbldata = reinterpret_cast<double*>(chunk_ptr);
            sprintf(string_buffer, "%f", dbldata[row_index]);
            return 0;
        case DuckType::Timestamp_micro:
            timdata = reinterpret_cast<time_t*>(chunk_ptr);
            tmdata = gmtime(timdata);
            strftime(string_buffer, sizeof(string_buffer), "%Y%m%dT%I:%M:%S.%p", tmdata);
            return 0;
        case DuckType::Utf8:    // null term trunc to 8 bytes
            dbldata = reinterpret_cast<double*>(chunk_ptr);
            sprintf(string_buffer, "%s", dbldata[row_index]);
            return 0;
        default:
            sprintf(string_buffer, "%s", "UNK");
            return 0;
        }
        return 0;
    }
};

using Dispatcher = std::function<void(emscripten::EM_VAL result_handle)>;
class DBResultDispatcher {
private:
    DBResultDispatcher() {};
    Dispatcher dispatcher_func = nullptr;
    RegChunkFunc reg_chunk_func = nullptr;
public:
    static DBResultDispatcher& get_instance() {
        static DBResultDispatcher instance;
        return instance;
    }
    void set_dispatcher(Dispatcher df) { dispatcher_func = df; }
    void set_reg_chunk(RegChunkFunc cf) { reg_chunk_func = cf; }
    void dispatch(emscripten::EM_VAL result_handle) {
        if (dispatcher_func != nullptr) dispatcher_func(result_handle);
        else fprintf(stdout, "NULL DBResultDispatcher func\n");
    }
    void register_chunk(const std::string& qid, int sz, int addr) {
        if (reg_chunk_func != nullptr) reg_chunk_func(qid, sz, addr);
        else fprintf(stdout, "NULL RegChunkFunc func\n");       
    }
};

extern "C" {
    void on_db_result_cpp(emscripten::EM_VAL result_handle) {
        auto d = DBResultDispatcher::get_instance();
        d.dispatch(result_handle);
    }

    int get_chunk_cpp(const char* qid, int size) {
        // size in 64bit words
        const static char* method = "get_chunk_cpp";
        // batch_materializer calls us to get a handle on WASM memory
        // https://stackoverflow.com/questions/56010390/emscripten-how-to-get-uint8-t-array-from-c-to-javascript
        // Real impl will use some kind of mem pooling
        uint64_t* buffer = new uint64_t[size];
        memset(buffer, 0, size * 8);
        int buffer_address = reinterpret_cast<int>(buffer);
        printf("%s: size: %d, buffer_address: %d\n", method, size, buffer_address);
        auto d = DBResultDispatcher::get_instance();
        d.register_chunk(qid, size, buffer_address);
        return buffer_address;
    }

    void on_chunk_cpp(int* chunk) {
        const static char* method = "on_chunk_cpp";
        printf("%s: chunk=%d\n", method, chunk);
    }
};

#endif  // __EMSCRIPTEN__