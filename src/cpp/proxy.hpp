#pragma once
#include <string>
#include <map>
#include <queue>
#ifndef __EMSCRIPTEN__
#include <duckdb.h>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#endif
#include "json.hpp"

#define ND_WC_BUF_SZ 256

typedef std::function<void(const std::string&)> ws_sender;

// NDProxy encapsulates the server side. In Breadboard it also hosts
// the DuckDB instance.

class NDProxy {
public:
    NDProxy(int argc, char** argv);
    virtual         ~NDProxy();

    bool            duck_app() { return is_duck_app; }

    // GUI thread
    void            notify_server(const std::string& caddr, nlohmann::json& old_val, nlohmann::json& new_val);
    void            duck_dispatch(nlohmann::json& db_request);
    void            set_done(bool d) { done = d; }
    nlohmann::json  get_breadboard_config() { return bb_config; }
    std::string& get_server_url() { return server_url; }
    void            register_ws_callback(ws_sender send) { ws_send = send; }
#ifndef __EMSCRIPTEN__
    void            get_duck_responses(std::queue<nlohmann::json>& responses);
#endif
protected:
    // DB thread
#ifndef __EMSCRIPTEN__
    bool            duck_init();
    void            duck_fnls();
    void            duck_loop();
#endif
private:
    nlohmann::json                      bb_config;
    bool                                is_duck_app;
    char* exe;    // argv[0]
    wchar_t                             wc_buf[ND_WC_BUF_SZ];
    char* bb_json_path;

    // these three test config strings are written once at startup
    // time in the cpp thread, and read from the py thread
    std::string                         test_module_name;
    bool                                done;
    std::queue<nlohmann::json>          server_responses;
    std::string                         server_url;
    // ref to NDWebSockClient::send
    ws_sender                           ws_send;

#ifndef __EMSCRIPTEN__
    duckdb_database                     duck_db;
    duckdb_connection                   duck_conn;
    boost::thread                       duck_thread;
    boost::atomic<bool>                 duck_done;
    std::queue<nlohmann::json>          duck_queries;
    std::queue<nlohmann::json>          duck_results;
    boost::mutex                        query_mutex;
    boost::mutex                        result_mutex;
    boost::condition_variable           query_cond;
#endif
};
