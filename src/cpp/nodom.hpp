#pragma once
#include <string>
#include <map>
#include <queue>
#include <filesystem>
#include <functional>
#include <duckdb.h>
#ifndef __EMSCRIPTEN__
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#endif
#include "json.hpp"

// NoDOM emulation: debugging ND impls in JS is tricky. Code compiled from C++ to clang .o
// is not available. So when we port to EM, we have to resort to printf debugging. Not good
// when we want to understand what the "correct" imgui behaviour should be. So here we have
// just enough impl to emulate ND server and client scaffolding that allows us to debug
// imgui logic. So we don't bother with HTTP get, just the websockets, with enough
// C++ code to maintain when we just want to focus on the impl that is opaque in the browser.
// JOS 2025-01-22

#define ND_MAX_COMBO_LIST 16
#define ND_WC_BUF_SZ 256

class NDWebSockClient;
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
    std::string&    get_server_url() { return server_url; }
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
    char*                               exe;    // argv[0]
    wchar_t                             wc_buf[ND_WC_BUF_SZ];
    char*                               bb_json_path;

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



class NDContext {
public:
    NDContext(NDProxy& s);
    void render();                              // invoked by main loop

    void server_request(const std::string& key);
    void notify_server(const std::string& caddr, nlohmann::json& old_val, nlohmann::json& new_val);
    void dispatch_server_responses(std::queue<nlohmann::json>& responses);

    bool duck_app();
    void set_done(bool d);

    void on_ws_open();
    void on_db_event(nlohmann::json& duck_msg);
    void on_layout();

    nlohmann::json  get_breadboard_config() { return proxy.get_breadboard_config(); }

    void register_ws_callback(ws_sender send) { ws_send = send; proxy.register_ws_callback(send); }

    void register_font(const std::string& name, ImFont* f) { font_map[name] = f; }

protected:
    void dispatch_render(nlohmann::json& w);        // w["rname"] resolve & invoke
    void action_dispatch(const std::string& action, const std::string& nd_event);
    void duck_dispatch(const std::string& nd_type, const std::string& sql, const std::string& qid);
    // Render funcs are members of NDContext, unlike in main.ts
    // Why? Separate standalone funcs like in main.ts cause too much
    // hassle with dispatch_render passing this and templates
    // defaulting to const. Wasted too much time experimenting
    // with std::bind, std::function etc. So the main.ts and
    // cpp will be different shapes, but hopefully with identical
    // decoupling profiles. JOS 2025-01-24
    void render_home(nlohmann::json& w);
    void render_input_int(nlohmann::json& w);
    void render_combo(nlohmann::json& w);
    void render_separator(nlohmann::json& w);
    void render_footer(nlohmann::json& w);
    void render_same_line(nlohmann::json& w);
    void render_date_picker(nlohmann::json& w);
    void render_text(nlohmann::json& w);
    void render_button(nlohmann::json& w);
    void render_duck_table_summary_modal(nlohmann::json& w);
    void render_duck_parquet_loading_modal(nlohmann::json& w);
    void render_table(nlohmann::json& w);

    void push_widget(nlohmann::json& w);
    void pop_widget(const std::string& rname = "");

    bool push_font(nlohmann::json& w);
    void pop_font(nlohmann::json& w);
private:
    // ref to "server process"; just an abstraction of EMV vs win32
    NDProxy&                           proxy;
    // NSServer invokes will be replaced with EM_JS
    
    nlohmann::json                      layout; // layout and data are fetched by 
    nlohmann::json                      data;   // sync c++py calls not HTTP gets
    std::deque<nlohmann::json>          stack;  // render stack

    // map layout render func names to the actual C++ impls
    std::unordered_map<std::string, std::function<void(nlohmann::json& w)>> rfmap;

    // top level layout widgets with widget_id eg modals are in pushables
    std::unordered_map<std::string, nlohmann::json> pushable;
    // main.ts:action_dispatch is called while rendering, and changes
    // the size of the render stack. JS will let us do that in the root
    // render() method. But in C++ we use an STL iterator in the root render
    // method, and that segfaults. So in C++ we have pending pushes done
    // outside the render stack walk. JOS 2025-01-31
    std::deque<nlohmann::json> pending_pushes;
    std::deque<std::string> pending_pops;
    bool    show_id_stack = false;

    // colours: https://www.w3schools.com/colors/colors_picker.asp
    ImColor red;    // ImGui.COL32(255, 51, 0);
    ImColor green;  // ImGui.COL32(102, 153, 0);
    ImColor amber;  // ImGui.COL32(255, 153, 0);
    ImColor db_status_color;

    std::map<std::string, ImFont*>  font_map;

    // default value for invoking std::find on nlohmann JSON iterators
    std::string null_value = "null_value";

    // ref to NDWebSockClient::send
    ws_sender ws_send;
};

// switch off name mangling for C funcs
extern "C" {
    GLFWwindow* im_start(NDContext& ctx);
    void        im_end(GLFWwindow* w);
    int         im_render(GLFWwindow* window, NDContext& ctx);
    void        glfw_error_callback(int error, const char* description);
};

