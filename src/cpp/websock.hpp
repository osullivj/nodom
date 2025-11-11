// websock.hpp: WebSocket comms for NoDOM. We use websockpp on win32
// and emscripten's emscripten/websocket.h on ems. Note this gist...
// https://gist.github.com/nus/564e9e57e4c107faa1a45b8332c265b9
// ...which was handy for understanding the ems API.
#include <iostream>
#ifndef __EMSCRIPTEN__  // using boost::asio event loop on win32
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
// TODO: figure out build for websockpp build with firedaemon binaries
// https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/using.html
// see the section on optional separate compilation: src.hpp is necessary
// for SSL support. 
// #include <boost/asio/ssl/impl/src.hpp>
// NB we use firedaemon SSL binaries: https://kb.firedaemon.com/support/solutions/articles/4000121705#Build-Script
#endif

#include "json_cache.hpp"
#include "context.hpp"          // NDContext template
#include "im_render.hpp"        // im_start, im_render, im_end
#include "db_cache.hpp"

#ifndef __EMSCRIPTEN__
// websockpp headers on win32
// TODO: switch to asio_client.hpp when we've figured SSL build
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client>               ws_client;
// TODO: SSL
// typedef websocketpp::client<websocketpp::config::asio_tls_client>           wss_client;
// typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>  context_ptr;
typedef websocketpp::config::asio_client::message_type::ptr                 message_ptr;

typedef websocketpp::connection_hdl     ws_handle;
typedef websocketpp::lib::error_code    ws_error_code;
typedef boost::asio::deadline_timer     asio_timer;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
#else
// ems websock abstraction
#include <emscripten/websocket.h>
#endif

struct GLFWwindow;

#ifdef __EMSCRIPTEN__
// standalone C style callbacks decls for ems websocket
// NB they all redispatch to NDWebSockClient member funcs
EM_BOOL sa_ems_on_open(int eventType, const EmscriptenWebSocketOpenEvent* websocketEvent, void* userData);
EM_BOOL sa_ems_on_close(int eventType, const EmscriptenWebSocketCloseEvent* websocketEvent, void* userData);
EM_BOOL sa_ems_on_message(int eventType, const EmscriptenWebSocketMessageEvent* websocketEvent, void* userData);
EM_BOOL sa_ems_on_error(int eventType, const EmscriptenWebSocketErrorEvent* websocketEvent, void* userData);
#endif

template<typename JSON, typename DB>
class NDWebSockClient {
private:
    std::string             uri;
    NDContext<JSON, DB>&    ctx;
    std::queue<JSON>        server_responses;
    NDProxy<DB>&            server;

    // websock data members: diff impls for 
    // websockpp on win32, and emscripten's own
    // websock abstraction
#ifndef __EMSCRIPTEN__
    ws_client           client;
    ws_handle           handle;
    ws_error_code       error_code;
#else
    EMSCRIPTEN_WEBSOCKET_T ws_handle;
    EmscriptenWebSocketCreateAttributes ws_attrs = {
                        nullptr, nullptr, EM_TRUE};
#endif

public:
    NDWebSockClient(NDProxy<DB>& svr, NDContext<JSON, DB>& c)
        :uri(svr.get_server_url()), server(svr), ctx(c) {
#ifdef __EMSCRIPTEN__
        ws_attrs.url = uri.c_str();
        ws_handle = emscripten_websocket_new(&ws_attrs);
        // ems callbacks require standalone C style funcs, so we
        // pass this ptr as userData param so our callbacks
        // can redispatch to NDWebSockClient member methods.
        emscripten_websocket_set_onopen_callback(ws_handle, this, sa_ems_on_open);
        emscripten_websocket_set_onerror_callback(ws_handle, this, sa_ems_on_error);
        emscripten_websocket_set_onclose_callback(ws_handle, this, sa_ems_on_close);
        emscripten_websocket_set_onmessage_callback(ws_handle, this, sa_ems_on_message);
#else
        client.set_access_channels(websocketpp::log::alevel::all);
        client.clear_access_channels(websocketpp::log::alevel::frame_payload);
        client.set_error_channels(websocketpp::log::alevel::frame_payload);
        client.init_asio();
        client.set_message_handler(bind(&NDWebSockClient::wspp_on_message, this, &client, ::_1, ::_2));
        client.set_open_handler(bind(&NDWebSockClient::wspp_on_open, this, &client, ::_1));
        client.set_close_handler(bind(&NDWebSockClient::wspp_on_close, this, &client, ::_1));
        client.set_fail_handler(bind(&NDWebSockClient::wspp_on_fail, this, &client, ::_1));
#endif
    }

#ifdef __EMSCRIPTEN__
    void send(const std::string& payload) {
        const static char* method = "NDWebSockClient.send: ";
        EMSCRIPTEN_RESULT result = emscripten_websocket_send_utf8_text(ws_handle, payload.c_str());
        if (result) {
            NDLogger::cerr() << method << "send failed!" << std::endl;
        }
    }

#else   // __EMSCRIPTEN__
    // NDWebSockClient::run only exists on win32 as
    // the entry point to the asio loop. On ems
    // we're using emscripten_set_main_loop_arg
    void run() {
        ctx.register_ws_callback(bind(&NDWebSockClient::send, this, ::_1));
        error_code.clear();
        ws_client::connection_ptr con = client.get_connection(uri, error_code);
        if (error_code) {
            std::cerr << "NDWebSockClient: could not create connection because: "
                << error_code.message() << std::endl;
        }
        else {
            // TODO: possible discard the websock conn
            client.connect(con);
        }
        set_timer();    // latest possible timer start
        im_start(ctx);
        client.run();   // this method just calls io_service.run()
    }

    void send(const std::string& payload) {
        error_code.clear();
        client.send(handle, payload, websocketpp::frame::opcode::TEXT, error_code);
        if (error_code) {
            std::cerr << "NDWebSockClient::send: failed with " << error_code << std::endl;
        }
    }
#endif

protected:
// win32 only: websocketpp timouts are used to trigger rendering
// on ems emscripten_set_main_loop_arg does that job
#ifndef __EMSCRIPTEN__
    void set_timer() {
        asio_timer timer(client.get_io_service());
        timer.expires_from_now(boost::posix_time::millisec(16));
        timer.async_wait(boost::bind(&NDWebSockClient::on_timeout, this, ::_1));
    }

    void on_timeout(const boost::system::error_code& e) {
        // if im_render returns false someone has closed the app via GUI
        if (!im_render(ctx)) {
            im_end(ctx.get_glfw_window());                 // imgui finalisation
            ctx.set_done(true);             // py thread loop exit
            client.get_io_service().stop(); // asio finalisation
        }
        else {
            set_timer();
            // Are there any responses from the server side?
            // NB this.server_responses will be empty at init.
            // proxy.duck_responses may have an nd_type:DuckInstance,
            // but no results until a scan or query happens. 
            // get_duck_responses will swap them, then
            // ctx.dispatch_server_responses will drain the
            // swapped Q. Also note that NDWebSockClient::on_message
            // is on the GUI thread and can populate server_responses,
            // hence one check and dispatch before get_duck_responses
            if (!server_responses.empty()) {
                // handle incoming websock from server
                ctx.dispatch_server_responses(server_responses);
            }
            // Potential lock contention in get_duck_responses()
            // which attempts to acquire server.result_mutex, when
            // duck_loop() may be holding result_mutex to enqueue
            // DB responses.
            server.get_db_responses(server_responses);
            if (!server_responses.empty()) {
                // now handle results from DB
                ctx.dispatch_server_responses(server_responses);
            }
        }
    }

    void wspp_on_message(ws_client* c, ws_handle h, message_ptr msg_ptr) {
        std::string payload(msg_ptr->get_payload());
        std::cout << "NDWebSockClient::on_message: hdl( " << h.lock().get()
            << ") msg: " << payload << std::endl;
        nlohmann::json msg_json = nlohmann::json::parse(payload);
        server_responses.emplace(msg_json);
    }

    void wspp_on_open(ws_client* c, ws_handle h) {
        std::cout << "NDWebSockClient::on_open: hdl:" << h.lock().get() << std::endl;
        handle = h;
        ctx.on_ws_open();
    }

    void wspp_on_close(ws_client* c, ws_handle h) {
        std::cout << "NDWebSockClient::on_close: hdl: " << h.lock().get() << std::endl;
    }

    void wspp_on_fail(ws_client* c, ws_handle h) {
        std::cout << "NDWebSockClient::on_fail: hdl: " << h.lock().get() << std::endl;
    }
#else   // ems
public:
    // public so the ems standalone callback funcs can invoke;
    // after all, we don't wanna use the friend keyword...
    void ems_on_message(const std::string& payload) {
        emscripten::val msg_json = JParse<emscripten::val>(payload);
        server_responses.emplace(msg_json);
    }

    void ems_on_open() {
        ctx.on_ws_open();
    }

    void ems_on_close() { }

    void ems_on_fail() { }
#endif // ems
};

#ifdef __EMSCRIPTEN__
// standalone C style callbacks decls for ems websocket
// NB they all redispatch to NDWebSockClient member funcs
template <typename DB>
EM_BOOL sa_ems_on_open_(int eventType, const EmscriptenWebSocketOpenEvent* websocketEvent, void* userData) {
    auto ws_client = reinterpret_cast<NDWebSockClient<emscripten::val, DB>*>(userData);
    ws_client->ems_on_open();
    return EM_TRUE;
}

template <typename DB>
EM_BOOL sa_ems_on_close_(int eventType, const EmscriptenWebSocketCloseEvent* websocketEvent, void* userData) {
    auto ws_client = reinterpret_cast<NDWebSockClient<emscripten::val, DB>*>(userData);
    return EM_TRUE;
}

template <typename DB>
EM_BOOL sa_ems_on_message_(int eventType, const EmscriptenWebSocketMessageEvent* websocketEvent, void* userData) {
    const static char* method = "ems_on_message: ";
    auto ws_client = reinterpret_cast<NDWebSockClient<emscripten::val, DB>*>(userData);
    if (websocketEvent->isText) {
        // For only ascii chars.
        ws_client->ems_on_message((const char*)websocketEvent->data);
    }
    else {
        NDLogger::cerr() << method << "non text message!" << std::endl;
    }
    return EM_TRUE;
}

template <typename DB>
EM_BOOL sa_ems_on_error(int eventType, const EmscriptenWebSocketErrorEvent* websocketEvent, void* userData) {
    auto ws_client = reinterpret_cast<NDWebSockClient<emscripten::val, DB>*>(userData);
    return EM_TRUE;
}
#endif
