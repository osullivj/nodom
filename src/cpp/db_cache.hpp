#pragma once
#include <iostream>
#include <queue>
#include <vector>
#include <stdexcept>
#include <chrono>
#include "nd_types.hpp"
#include "static_strings.hpp"
#include "json_ops.hpp"
#include "fmt/base.h"
#include "fmt/chrono.h"

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
    const char* get_datum(std::uint64_t handle, std::uint64_t colm_index, std::uint64_t row_index) { return "NULL"; }

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

using ResultHandle = std::uint64_t; // == &duckdb_result
using Bobbin = std::deque<duckdb_data_chunk>;



class DuckDBCache : public BreadBoardDBCache {
private:
    duckdb_database                     duck_db;
    duckdb_connection                   duck_conn;
    idx_t                               duck_chunk_size;

    std::unordered_map<std::string, duckdb_result>  result_map;
    std::unordered_map<ResultHandle, Bobbin>        bobbin_map;

    std::unordered_map<std::uint64_t, StringVec> col_names_map;
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
    duckdb_timestamp_s* ts_secs = nullptr;
    duckdb_timestamp_ms* ts_milli = nullptr;
    duckdb_timestamp* ts_micro = nullptr;
    duckdb_timestamp_ns* ts_nano = nullptr;
    int scan_count{ 0 };
    int query_count{ 0 };
    int batch_count{ 0 };
    fmt::format_to_n_result<char*> fmt_result;
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
            std::cout << method << "DB_INIT_FAIL" << std::endl;
            exit(1);
        }
        else {
            // lock the result queue and post back to the GUI thread
            nlohmann::json db_instance = {
                {nd_type_cs, duck_instance_cs},
                {query_id_cs, db_online_cs}
            };
            boost::unique_lock<boost::mutex> results_lock(result_mutex);
            // send nd_type:DuckInstance
            db_results.push(db_instance);
            std::cout << method << "DB: " << db_instance << std::endl;
        }

        nlohmann::json response_list_j = nlohmann::json::array();
        duck_chunk_size = duckdb_vector_size();

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
            pix_begin_dbase();
            // thead quiesces in the wait above, with query_mutex
            // unlocked so the C++ thread can add work items
            std::cout << method << "db_queries depth : " << db_queries.size() << std::endl;
            while (!db_queries.empty()) {
                nlohmann::json db_request(db_queries.front());
                std::cout << method << "processing " << db_request.dump() << std::endl;
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
                    duckdb_state dbstate;
                    const std::string& sql(db_request[sql_cs]);
                    // Duck C API scans may throw C++ duckdb.HTTPException
                    std::exception_ptr active_exception;
                    try {
                        dbstate = duckdb_query(duck_conn, sql.c_str(), nullptr);
                        db_response[nd_type_cs] = parquet_scan_result_cs;
                        pix_report(DBScan, static_cast<float>(scan_count++));
                    }
                    catch (...) {
                        // DuckDB C API can throw exceptions from the parquet
                        // extension. 
                        std::cerr << method << "PARQUET_SCAN_FAIL: " << db_request.dump() << std::endl;
                        db_response[error_cs] = 1;
                    }
                    // dbstate should be defaulted to DuckDBSuccess if an 
                    // exception was thrown above, so this is the non except
                    // error path
                    if (dbstate == DuckDBError) {
                        std::cerr << method << "PARQUET_SCAN_FAIL: " << db_request.dump() << std::endl;
                        db_response[error_cs] = 1;
                    }
                }
                else if (nd_type == query_cs) {
                    const std::string& sql(db_request[sql_cs]);
                    duckdb_result dbresult;
                    duckdb_state dbstate = duckdb_query(duck_conn, sql.c_str(), &dbresult);
                    std::cout << method << "QID: " << qid << ", SQL: " << sql << std::endl;
                    db_response[nd_type_cs] = query_result_cs;
                    if (dbstate == DuckDBError) {
                        std::cerr << method << "Query failed: " << db_request << std::endl;
                        db_response[error_cs] = 1;
                    }
                    else {
                        result_map[qid] = dbresult;
                        pix_report(DBQuery, static_cast<float>(query_count++));
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
                        ResultHandle handle = reinterpret_cast<std::uint64_t>(&(result_iter->second));
                        Bobbin& chunk_deck(bobbin_map[handle]);
                        chunk_deck.push_back(chunk);
                        pix_report(DBBatch, static_cast<float>(batch_count++));
                    }
                }
                else {
                    // unrecognised nd_type error!
                    std::cerr << method << "BAD_ND_TYPE: " << nd_type << std::endl;
                    continue;
                }
                // lock the result queue and post back to the GUI thread
                boost::unique_lock<boost::mutex> results_lock(result_mutex);
                db_results.push(db_response);
                pix_end_event();
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
        auto res_iter = result_map.find(qid);
        if (res_iter != result_map.end()) {
            ResultHandle handle = reinterpret_cast<std::uint64_t>(&(res_iter->second));
            return handle;
        }
        return 0;
    }

    std::uint64_t get_total_row_count(std::uint64_t h) {
        duckdb_result* result_ptr = reinterpret_cast<duckdb_result*>(h);
        return duckdb_row_count(result_ptr);
    }

    std::uint64_t get_bobbin_row_count(std::uint64_t h) {
        ResultHandle handle = static_cast<ResultHandle>(h);
        auto bob_iter = bobbin_map.find(handle);
        std::uint64_t row_count = 0;
        if (bob_iter == bobbin_map.end())
            return 0;
        Bobbin& bob = bobbin_map.at(handle);
        auto chunk_iter = bob.begin();
        while (chunk_iter != bob.end()) {
            row_count += duckdb_data_chunk_get_size(*chunk_iter);
            chunk_iter++;
        }
        return row_count;
    }

    StringVec& get_col_names(std::uint64_t handle) {
        // First 3 32 bit words are done, ncols, nrows
        uint64_t* chunk_ptr = reinterpret_cast<uint64_t*>(handle);
        StringVec& colm_names = col_names_map[handle];
        return colm_names;
    }

    bool get_meta_data(std::uint64_t h, std::uint64_t& column_count, std::uint64_t& row_count) {
        duckdb_result* result_ptr = reinterpret_cast<duckdb_result*>(h);
        ResultHandle handle = static_cast<ResultHandle>(h);

        column_count = duckdb_column_count(result_ptr);
        row_count = get_bobbin_row_count(h);

        // duckdb_data_chunk chunk = reinterpret_cast<duckdb_data_chunk>(handle);
        // yes, these are slow and fire default ctor on 1st visit, which is why
        // we don't use .at()
        std::vector<duckdb_type>& types(type_map[handle]);
        std::vector<duckdb_logical_type>& logical_types(logical_type_map[handle]);
        StringVec& col_names = col_names_map[handle];
        if (types.empty()) {
            types.clear();
            logical_types.clear();
            col_names.clear();
            duckdb_logical_type type_l{};
            for (std::uint64_t index = 0; index < column_count; ++index) {
                type_l = duckdb_column_logical_type(result_ptr, index);
                logical_types.push_back(type_l);
                types.push_back(duckdb_get_type_id(type_l));
                col_names.push_back(duckdb_column_name(result_ptr, index));
            }
        }
        return true;
    }

    using TPMilli = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
    using TPMicro = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;
    using TPNano = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
    using TPSecs = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;


    const char* get_datum(std::uint64_t h, std::uint64_t colm_index, std::uint64_t row_index) {
        duckdb_result* result_ptr = reinterpret_cast<duckdb_result*>(h);
        ResultHandle handle = static_cast<ResultHandle>(h);

        const Bobbin& bob{ bobbin_map.at(handle)};
        duckdb_data_chunk chunk;

        int rel_index = row_index % duck_chunk_size;
        int chunk_index = row_index / duck_chunk_size;
        auto bob_iter = bob.cbegin();
        while (chunk_index > 0) {
            --chunk_index;
            ++bob_iter;
        }
        chunk = *bob_iter;
        duckdb_vector colm = duckdb_data_chunk_get_vector(chunk, colm_index);
        uint64_t* validities = duckdb_vector_get_validity(colm);
        // In most cases we'll copy into string_buffer, or
        // use sprintf to format into buffer. But for long
        // strings we may want to hand back Duck's start
        // and end char*
        buffer = string_buffer;
        if (duckdb_validity_row_is_valid(validities, rel_index)) {
            // get type metadata for the colm vector and validities
            // NB we only do this work if the field is valid
            const std::vector<duckdb_type>& types{ type_map.at(handle) };
            duckdb_type colm_type(types[colm_index]);
            const std::vector<duckdb_logical_type>& logical_types{ logical_type_map.at(handle) };
            duckdb_logical_type colm_type_l(logical_types[colm_index]);
            char* fraction_cs = nullptr;
            char* fraction_fmt = nullptr;
            double fraction{ 0.0 };

            switch (colm_type) {
            case DUCKDB_TYPE_VARCHAR:
                vcdata = (duckdb_string_t*)duckdb_vector_get_data(colm);
                if (duckdb_string_is_inlined(vcdata[rel_index])) {
                    // if inlined is 12 chars, there will be no zero terminator
                    memcpy(string_buffer, vcdata[rel_index].value.inlined.inlined, vcdata[rel_index].value.inlined.length);
                    string_buffer[vcdata[rel_index].value.inlined.length] = 0;
                }
                else {
                    // NB ptr arithmetic using length to calc the end
                    // ptr as these strings may be packed without 0 terminators
                    buffer = vcdata[rel_index].value.pointer.ptr;
                    return vcdata[rel_index].value.pointer.ptr + vcdata[rel_index].value.pointer.length;
                }
                break;
            case DUCKDB_TYPE_BIGINT:
                bidata = (int64_t*)duckdb_vector_get_data(colm);
                sprintf(string_buffer, "%I64d", bidata[rel_index]);
                break;
            case DUCKDB_TYPE_INTEGER:
                idata = (int32_t*)duckdb_vector_get_data(colm);
                fmt_result = fmt::format_to_n(string_buffer, STR_BUF_LEN, "{}", idata[rel_index]);
                return buffer + fmt_result.size;
            case DUCKDB_TYPE_DOUBLE:
                dbldata = (double_t*)duckdb_vector_get_data(colm);
                fmt_result = fmt::format_to_n(string_buffer, STR_BUF_LEN, "{}", dbldata[rel_index]);
                return buffer + fmt_result.size;
            // 4 duckdb_timestamp[_??] types, all holding
            // a single int64_t named differently
            case DUCKDB_TYPE_TIMESTAMP_S:
                ts_secs = (duckdb_timestamp_s*)duckdb_vector_get_data(colm);
                fmt_result = fmt::format_to_n(string_buffer, STR_BUF_LEN, "{:%F %T}", TPSecs{ std::chrono::seconds{ ts_secs->seconds } });
                return buffer + fmt_result.size;
            case DUCKDB_TYPE_TIMESTAMP_MS:  
                ts_milli = (duckdb_timestamp_ms*)duckdb_vector_get_data(colm);
                fmt_result = fmt::format_to_n(string_buffer, STR_BUF_LEN, "{:%F %T}", TPMilli{ std::chrono::milliseconds{ts_milli->millis} });
                return buffer + fmt_result.size;
            case DUCKDB_TYPE_TIMESTAMP:     
                ts_micro = (duckdb_timestamp*)duckdb_vector_get_data(colm);
                fmt_result = fmt::format_to_n(string_buffer, STR_BUF_LEN, "{:%F %T}", TPMicro{ std::chrono::microseconds{ ts_micro->micros } });
                return buffer + fmt_result.size;
            case DUCKDB_TYPE_TIMESTAMP_NS:  
                ts_nano = (duckdb_timestamp_ns*)duckdb_vector_get_data(colm);
                fmt_result = fmt::format_to_n(string_buffer, STR_BUF_LEN, "{:%F %T}", TPNano{ std::chrono::microseconds{ ts_nano->nanos } });
                return buffer + fmt_result.size;
            case DUCKDB_TYPE_DECIMAL:
                decimal_width = duckdb_decimal_width(colm_type_l);
                decimal_scale = duckdb_decimal_scale(colm_type_l);
                decimal_type = duckdb_decimal_internal_type(colm_type_l);
                decimal_divisor = pow(10, decimal_scale);

                switch (decimal_type) {
                case DUCKDB_TYPE_SMALLINT:  // int16_t
                    sidata = (int16_t*)duckdb_vector_get_data(colm);
                    decimal_value = (double)sidata[rel_index] / decimal_divisor;
                    break;
                case DUCKDB_TYPE_INTEGER:   // int32_t
                    idata = (int32_t*)duckdb_vector_get_data(colm);
                    decimal_value = (double)idata[rel_index] / decimal_divisor;
                    break;
                case DUCKDB_TYPE_BIGINT:    // int64_t
                    bidata = (int64_t*)duckdb_vector_get_data(colm);
                    decimal_value = (double)bidata[rel_index] / decimal_divisor;
                    break;
                case DUCKDB_TYPE_HUGEINT:   // duckdb_hugeint: 128
                    hidata = (duckdb_hugeint*)duckdb_vector_get_data(colm);
                    decimal_value = duckdb_hugeint_to_double(hidata[rel_index]) / decimal_divisor;
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
    std::unordered_map<std::uint64_t, StringVec> column_map;
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
        static const char* method = "DuckDBWebCache::register_chunk: ";
        // Will ctor ChunkVec on first batch...
        std::cout << method << "QID: " << qid << ", sz:" << size << ", addr: " << addr << std::endl;
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
        const static char* method = "DuckDBWebCache::get_handle: ";
        auto cv_iter = chunk_map.find(qname);
        if (cv_iter == chunk_map.end()) {
            fprintf(stderr, "%s: no chunk_map element for %s\n",
                method, qname.c_str());
            return 0;
        }
        ChunkVec& chunk_vector = chunk_map[qname];
        if (chunk_vector.empty()) {
            fprintf(stderr, "%s: %s chunk_map empty!\n",
                method, qname.c_str());
            return 0;
        }
        Chunk& chunk(chunk_vector.back());
        return chunk.addr;
    }

    std::uint64_t get_row_count(std::uint64_t handle) {
        // First 3 32 bit words are done, ncols, nrows
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        std::uint64_t row_count = *(chunk_ptr + 2);
        return row_count;
    }

    StringVec& get_col_names(std::uint64_t handle) {
        // First 3 32 bit words are done, ncols, nrows
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        StringVec& colm_names = column_map[handle];
        return colm_names;
    }

    IntVec& get_col_types(std::uint64_t handle) {
        // First 3 32 bit words are done, ncols, nrows
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        IntVec& colm_types = type_map[handle];
        return colm_types;
    }

    bool get_meta_data(std::uint64_t handle, std::uint64_t& colm_count, std::uint64_t& row_count) {
        // First 3 32 bit words are done, ncols, nrows
        uint32_t* chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        bool done = static_cast<bool>(*chunk_ptr++);
        column_count = reinterpret_cast<uint32_t>(*chunk_ptr++);
        row_count = reinterpret_cast<uint32_t>(*chunk_ptr++);
        // Next we have types, one 32bit int per col
        // Have we already populated types and names?
        std::vector<int>& tipes = type_map[handle];
        if (tipes.empty()) {
            tipes.clear();
            for (int i = 0; i < column_count; i++) {
                DuckType tipe{ static_cast<int32_t>(*chunk_ptr++) };
                tipes.push_back(tipe);
            }
            // Column addresses: one 32 bit word per col
            auto& caddrs = caddr_map[handle];
            caddrs.clear();
            for (int i = 0; i < column_count; i++) {
                uint32_t col_offset = *chunk_ptr++;
                caddrs.push_back(col_offset);
            }
            // After types we have column names
            StringVec& colm_names = column_map[handle];
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
        }
        return &column_names;
    }

    const char* get_datum(std::uint64_t handle, std::uint64_t colm_index, std::uint64_t row_index) {
        const static char* method = "DuckDBWebCache::get_datum: ";
        static double   dubble{ 0.0 };
        static tm       tm_data{ 0 };
        static time_t   tm_time_t{ 0 };
        static double   ts_secs_f{ 0.0 };
        static uint32_t ts_secs_i{ 0 };
        static double   ts_fraction{ 0.0 };
        static uint32_t scale{ 1 };
        static const char* decimal_fmt{ nullptr };
        static std::map<DuckType, int32_t>  timestamp_scale_map{
            {Timestamp_s, 1},
            {Timestamp_ms, 1e3},
            {Timestamp_us, 1e6},
            {Timestamp_ns, 1e9}
        };
        static std::map<DuckType, const char*> timestamp_dec_fmt_map{
            {Timestamp_s, nullptr},
            {Timestamp_ms, "%03.3f"},
            {Timestamp_us, "%06.6f"},
            {Timestamp_ns, "%09.9f"}
        };
        static int error_count{ 0};
        // type_map and column_map have been populated by an
        // earlier invocation of get_meta_data()
        auto& tipes = type_map[handle];
        auto& colm_addrs = caddr_map[handle];
        // First block in a chunk is metadata, so calc how far we
        // skip fwd to get to the col block addrs
        uint32_t* base_chunk_ptr = reinterpret_cast<uint32_t*>(handle);
        // ffwd past done,ncols,nrows,types to the col addresses
        // and point to the colm_index col addr
        uint32_t colm_addr_offset = colm_addrs[colm_index];
        // set chunk_ptr to col addr. NB addr is written after
        // 64bit bump, so we don't need to recorrect.
        uint32_t* chunk_ptr = base_chunk_ptr + colm_addr_offset;
        // sanity check column type
        int32_t col_type = *chunk_ptr++;
        uint32_t col_size = *chunk_ptr++;

        if (col_type != tipes[colm_index]) {
            fprintf(stderr, "%s: col type mismatch: base_chunk:%d, col:%d, col_addr_off:%d, mtype:%d, stype:%d\n", 
                method, (int)base_chunk_ptr, (int)colm_index, (int)colm_addr_offset, col_type, tipes[colm_index]);
            error_count++;
            if (error_count == 5)
                exit(1);
        }
        // stride 1 for 32bit data and 2 for 64bit inc str
        uint32_t stride = col_size / 4;
        DuckType dt{ col_type };
        int32_t* i32data = nullptr;
        double* dbldata = nullptr;
        // In most cases we'll copy into string_buffer, or
        // use sprintf to format into buffer. 
        buffer = string_buffer;
        switch (dt) {
        case DuckType::Int:
            i32data = reinterpret_cast<int32_t*>(chunk_ptr);
            sprintf(string_buffer, "%d", i32data[row_index]);
            return 0;
        case DuckType::Float:
            dbldata = reinterpret_cast<double*>(chunk_ptr);
            sprintf(string_buffer, "%f", dbldata[row_index]);
            return 0;
        case DuckType::Timestamp_ns:
        case DuckType::Timestamp_us:
        case DuckType::Timestamp_ms:
        case DuckType::Timestamp_s:
            scale = timestamp_scale_map[dt];
            decimal_fmt = timestamp_dec_fmt_map[dt];
            dbldata = reinterpret_cast<double*>(chunk_ptr);
            dubble = dbldata[row_index];
            // cast dubble to 64 bit signed int before dividing
            // by 1e[1,3,6,9]
            ts_secs_i = static_cast<uint64_t>(dubble) / scale;
            ts_secs_f = dubble / static_cast<double>(scale);
            ts_fraction = ts_secs_f - ts_secs_i;
            tm_time_t = static_cast<time_t>(ts_secs_i);
            gmtime_r(&tm_time_t, &tm_data);
            strftime(string_buffer, STR_BUF_LEN, "%Y-%m-%d %I:%M:%S", &tm_data);
            // date_length:2026-01-21 10:57:33 is 19 chars
            if (decimal_fmt) {
                sprintf(string_buffer + strlen(string_buffer), decimal_fmt, ts_fraction);
            }            return 0;
        case DuckType::Utf8:    // null term trunc to 8 bytes
            dbldata = reinterpret_cast<double*>(chunk_ptr);
            sprintf(string_buffer, "%s", (char*)&(dbldata[row_index]));
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
        else fprintf(stderr, "NULL DBResultDispatcher func\n");
    }
    void register_chunk(const std::string& qid, int sz, int addr) {
        if (reg_chunk_func != nullptr) reg_chunk_func(qid, sz, addr);
        else fprintf(stderr, "NULL RegChunkFunc func\n");       
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
        printf("%s: chunk=%d\n", method, (int)chunk);
    }
};

#endif  // __EMSCRIPTEN__