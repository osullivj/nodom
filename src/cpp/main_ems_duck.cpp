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
#include "emscripten_mainloop_stub.h"

// NoDOM: this main.cpp is intended to stay as close as possible
// to the imgui/examples/example_glfw_opengl3/main.cpp as that's 
// the appropriate rendeder for the Chrome codebase. JOS 2025-08-25

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

int main(int argc, char* argv[]) {
    const static char* method = "main: ";

    NDProxy<DuckDB_t> server;
    NDContext_t ctx(server);
    NDWebSockClient<ems_val_t, DuckDB_t> ws_client(server, ctx);
    ctx.register_msg_pump([&ws_client]() {ws_client.pump_messages();});
    DBResultDispatcher& dbrd(DBResultDispatcher::get_instance());
    dbrd.set_dispatcher([&server](emscripten::EM_VAL v)
                                    {server.add_db_response(v); });
    dbrd.set_reg_chunk([&server](const std::string& qid, int sz, int addr)
                                    {server.register_chunk(qid.c_str(), sz, addr); });
    IDBFontCache font_cache([&ctx](const std::string& n, ImFont* f)
                                {ctx.register_font(n, f); },
                                { "Arial.ttf", "CourierNew.ttf" });
    GLFWwindow* window = im_start(ctx, &font_cache);
    if (window == nullptr) {
        std::cout << method << "im_start failed" << std::endl;
        exit(1);
    }
    emscripten_set_main_loop_arg(im_loop_body, &ctx, 0, 1);
}

