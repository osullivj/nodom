// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
// websock hdrs
#include <iostream>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

// imgui hdrs
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "websock.hpp"
#include "nodom.hpp"

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif


NDWebSockClient::NDWebSockClient(NDProxy& svr, NDContext& c)
:uri(svr.get_server_url()), server(svr), ctx(c), window(im_start(c)) {
    client.set_access_channels(websocketpp::log::alevel::all);
    client.clear_access_channels(websocketpp::log::alevel::frame_payload);
    client.set_error_channels(websocketpp::log::alevel::frame_payload);
    client.init_asio();
    client.set_message_handler(bind(&NDWebSockClient::on_message, this, &client, ::_1, ::_2));
    client.set_open_handler(bind(&NDWebSockClient::on_open, this, &client, ::_1));
    client.set_close_handler(bind(&NDWebSockClient::on_close, this, &client, ::_1));
    client.set_fail_handler(bind(&NDWebSockClient::on_fail, this, &client, ::_1));
    // TODO: SSL
    // client.set_tls_init_handler(bind(&NDWebSockClient::on_tls_init, this, "localhost", ::_1));
}

void NDWebSockClient::run() {
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

void NDWebSockClient::send(const std::string& payload) {
    error_code.clear();
    client.send(handle, payload, websocketpp::frame::opcode::TEXT, error_code);
    if (error_code) {
        std::cerr << "NDWebSockClient::send: failed with " << error_code << std::endl;
    }
}

void NDWebSockClient::set_timer() {
    asio_timer timer(client.get_io_service());
    timer.expires_from_now(boost::posix_time::millisec(16));
    timer.async_wait(boost::bind(&NDWebSockClient::on_timeout, this, ::_1));
}

void NDWebSockClient::on_timeout(const boost::system::error_code& e) {
    // if im_render returns false someone has closed the app via GUI
    if (!im_render(window, ctx)) {
        im_end(window);                 // imgui finalisation
        ctx.set_done(true);             // py thread loop exit
        client.get_io_service().stop(); // asio finalisation
    }
    else {
        set_timer();
        // Are there any responses from the server side?
        if (!server_responses.empty()) {
            ctx.dispatch_server_responses(server_responses);
        }

    }
}

void NDWebSockClient::on_message(ws_client* c, ws_handle h, message_ptr msg_ptr) {
    std::string payload(msg_ptr->get_payload());
    std::cout << "NDWebSockClient::on_message: hdl( " << h.lock().get()
        << ") msg: " << payload << std::endl;

    nlohmann::json msg_json = nlohmann::json::parse(payload);
    server_responses.emplace(msg_json);
}


void NDWebSockClient::on_open(ws_client* c, ws_handle h) {
    std::cout << "NDWebSockClient::on_open: hdl:" << h.lock().get() << std::endl;
    handle = h;
    ctx.on_ws_open();
}

void NDWebSockClient::on_close(ws_client* c, ws_handle h) {
    std::cout << "NDWebSockClient::on_close: hdl: " << h.lock().get() << std::endl;
}

void NDWebSockClient::on_fail(ws_client* c, ws_handle h) {
    std::cout << "NDWebSockClient::on_fail: hdl: " << h.lock().get() << std::endl;
}

/* TODO: SSL
bool verify_certificate(const char* hostname, bool preverified, boost::asio::ssl::verify_context& ctx)
{
    return true;
} */

/* TODO: reintroduce when we've figured SSL build
context_ptr NDWebSockClient::on_tls_init(const char* hostname, ws_handle)
{
    context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds);
        ctx->set_verify_mode(boost::asio::ssl::verify_none);
    }
    catch (std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    return ctx;
} */

