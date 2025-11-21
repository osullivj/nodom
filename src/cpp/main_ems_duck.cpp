// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
// 
// websock hdrs: only used win32 for breadboard builds 


// imgui hdrs
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "static_strings.hpp"
#include "context.hpp"
#include "im_render.hpp"
#include "db_cache.hpp"
#include "websock.hpp"
#ifdef __EMSCRIPTEN__
#include "emscripten_mainloop_stub.h"
#endif

// NoDOM: this main.cpp is intended to stay as close as possible
// to the imgui/examples/example_glfw_opengl3/main.cpp as that's 
// the appropriate rendeder for the Chrome codebase. JOS 2025-08-25

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

#ifdef __EMSCRIPTEN__
using ems_val_t = emscripten::val;
using DuckDB_t = DuckDBWebCache;
using NDContext_t = NDContext<ems_val_t, DuckDB_t>;

void im_loop_body(void* c) {
    auto ctx = reinterpret_cast<NDContext_t*>(c);
    if (ctx != nullptr) {
        if (!im_render(*ctx)) im_end(ctx->get_glfw_window());
        ctx->pump_messages();
    }
}
#endif

int main(int argc, char* argv[]) {
    const static char* method = "main: ";
#ifndef __EMSCRIPTEN__
    NDProxy<DuckDBCache> server(argc, argv);
    NDContext<nlohmann::json, DuckDBCache> ctx(server);
    try {
        // launch DB thread: see db_loop impls
        server.start_db_thread();
        // now launch websock client with a boost::asio
        // event loop on the main thread to dispatch
        // the timeout and on_message callbacks
        NDWebSockClient<nlohmann::json, DuckDBCache> ws_client(server, ctx);
        ws_client.run();
    }
    catch (websocketpp::exception const& ex) {
        std::cout << method << ex.what() << std::endl;
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << method << ex.what() << std::endl;
    }
#else

    NDProxy<DuckDB_t> server;
    NDContext_t ctx(server);
    NDWebSockClient<ems_val_t, DuckDB_t> ws_client(server, ctx);
    ctx.register_msg_pump([&ws_client]() {ws_client.pump_messages();});
    DBResultDispatcher::get_instance().set_dispatcher([&server](emscripten::EM_VAL v) {server.add_db_response(v); });
    IDBFontCache font_cache([&ctx](const std::string& n, ImFont* f)
                                {ctx.register_font(n, f); },
                                { "Arial.ttf", "CourierNew.ttf" });
    GLFWwindow* window = im_start(ctx, &font_cache);
    emscripten_set_main_loop_arg(im_loop_body, &ctx, 0, 1);
#endif
}

