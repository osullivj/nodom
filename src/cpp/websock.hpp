// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
// websock hdrs
#include <iostream>
#include <boost/asio/io_service.hpp>
#include <boost/asio/deadline_timer.hpp>
// https://www.boost.org/doc/libs/1_79_0/doc/html/boost_asio/using.html
// see the section on optional separate compilation: src.hpp is necessary
// for SSL support. 
// #include <boost/asio/ssl/impl/src.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "json.hpp"

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

typedef websocketpp::client<websocketpp::config::asio_client>               ws_client;
typedef websocketpp::client<websocketpp::config::asio_tls_client>           wss_client;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context>  context_ptr;
typedef websocketpp::config::asio_client::message_type::ptr                 message_ptr;

typedef websocketpp::connection_hdl     ws_handle;
typedef websocketpp::lib::error_code    ws_error_code;
typedef boost::asio::deadline_timer     asio_timer;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

class NDServer;
class NDContext;
struct GLFWwindow;

class NDWebSockClient {
public:
    NDWebSockClient(NDServer& svr, NDContext& c);
    void run();
    void send(const std::string& payload); 

protected:
    void        set_timer();
    void        on_timeout(const boost::system::error_code& e);
    void        on_message(wss_client* c, ws_handle h, message_ptr msg_ptr);
    void        on_open(wss_client* c, ws_handle h);
    void        on_close(wss_client* c, ws_handle h);
    void        on_fail(wss_client* c, ws_handle h);
    context_ptr on_tls_init(const char* hostname, ws_handle);

private:
    std::string     uri;
    wss_client      client;
    ws_handle       handle;
    ws_error_code   error_code;
    NDServer&       server;
    NDContext&      ctx;
    GLFWwindow*     window;
    std::queue<nlohmann::json>  server_responses;
};
