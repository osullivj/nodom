// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
// websock hdrs
#include <iostream>
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

// websock is only used in breadboard, not EMS, so we can 
// pull in nlohmann::json directly
#include "json.hpp"

// TODO: switch to asio_client.hpp when we've figured SSL build
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include "nodom.hpp"        // NDContext template
#include "im_render.hpp"    // im_start, im_render, im_end

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

class NDProxy;
struct GLFWwindow;

class NDWebSockClient {
private:
    std::string     uri;
    ws_client       client;
    ws_handle       handle;
    ws_error_code   error_code;
    NDProxy& server;
    NDContext<nlohmann::json>& ctx;
    GLFWwindow* window;
    std::queue<nlohmann::json>  server_responses;
public:
    NDWebSockClient(NDProxy& svr, NDContext<nlohmann::json>& c)
        :uri(svr.get_server_url()), server(svr), ctx(c), window(im_start(c)) {
            client.set_access_channels(websocketpp::log::alevel::all);
            client.clear_access_channels(websocketpp::log::alevel::frame_payload);
            client.set_error_channels(websocketpp::log::alevel::frame_payload);
            client.init_asio();
            client.set_message_handler(bind(&NDWebSockClient::on_message, this, &client, ::_1, ::_2));
            client.set_open_handler(bind(&NDWebSockClient::on_open, this, &client, ::_1));
            client.set_close_handler(bind(&NDWebSockClient::on_close, this, &client, ::_1));
            client.set_fail_handler(bind(&NDWebSockClient::on_fail, this, &client, ::_1));

    }
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
        client.run();   // this method just calls io_service.run()
    }

    void send(const std::string& payload) {
        error_code.clear();
        client.send(handle, payload, websocketpp::frame::opcode::TEXT, error_code);
        if (error_code) {
            std::cerr << "NDWebSockClient::send: failed with " << error_code << std::endl;
        }
    }

protected:
    void set_timer() {
        asio_timer timer(client.get_io_service());
        timer.expires_from_now(boost::posix_time::millisec(16));
        timer.async_wait(boost::bind(&NDWebSockClient::on_timeout, this, ::_1));
    }

    void on_timeout(const boost::system::error_code& e) {
        // if im_render returns false someone has closed the app via GUI
        if (!im_render(window, ctx)) {
            im_end(window);                 // imgui finalisation
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
            server.get_duck_responses(server_responses);
            if (!server_responses.empty()) {
                // now handle results from DB
                ctx.dispatch_server_responses(server_responses);
            }
        }
    }

    void on_message(ws_client* c, ws_handle h, message_ptr msg_ptr) {
        std::string payload(msg_ptr->get_payload());
        std::cout << "NDWebSockClient::on_message: hdl( " << h.lock().get()
            << ") msg: " << payload << std::endl;
        nlohmann::json msg_json = nlohmann::json::parse(payload);
        server_responses.emplace(msg_json);

    }

    void on_open(ws_client* c, ws_handle h) {
        std::cout << "NDWebSockClient::on_open: hdl:" << h.lock().get() << std::endl;
        handle = h;
        ctx.on_ws_open();
    }

    void on_close(ws_client* c, ws_handle h) {
        std::cout << "NDWebSockClient::on_close: hdl: " << h.lock().get() << std::endl;
    }

    void on_fail(ws_client* c, ws_handle h) {
        std::cout << "NDWebSockClient::on_fail: hdl: " << h.lock().get() << std::endl;
    }
};
