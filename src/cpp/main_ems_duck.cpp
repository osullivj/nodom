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
#include "emscripten.h"
#include "emscripten_mainloop_stub.h"

// NoDOM: this main.cpp is intended to stay as close as possible
// to the imgui/examples/example_glfw_opengl3/main.cpp as that's 
// the appropriate renderer for the Chrome codebase. JOS 2025-08-25

using json_t = emscripten::val;
using DuckDB_t = WebDuckDBCache;
using NDContext_t = NDContext<json_t, DuckDB_t>;

void im_loop_body(void* c) {
    auto ctx = reinterpret_cast<NDContext_t*>(c);
    if (ctx != nullptr) {
        if (!im_render(*ctx)) im_end(ctx->get_glfw_window());
        ctx->pump_messages();
    }
}


int main(int argc, char* argv[]) {
    const static char* method = "main: ";

    for (int i = 0; i < argc; i++) {
        std::cout << method << "argv[" << i << "]=" << argv[i] << std::endl;
    }

    std::string app_key(argc > 1 ? argv[1] : Static::empty_cs);
    std::string init_data(argc > 2 ? argv[2] : Static::init_data_cs);
    std::string init_layout(argc > 3 ? argv[3] : Static::init_layout_cs);
    std::string init_config(argc > 4 ? argv[4] : Static::empty_obj_cs);

    NDConfig<json_t>& cfg{ NDConfig<json_t>::get_instance() };
    cfg.initialize(init_config.c_str());

    DuckDB_t server;
    // Static::empty_cs as 3rd parm causes NDContext::get_ini_path()
    // to return a null ptr in im_start, so io.IniFilename is NULL
    // preventing attempt to write to localFS, which is nulled out by
    // IMGUI_DISABLE_FILE_FUNCTIONS
    NDContext_t ctx(server, app_key, Static::empty_cs, init_data.c_str(), init_layout.c_str());
    NDWebSockClient<json_t, DuckDB_t> ws_client(server, ctx);

    ctx.register_msg_pump([&ws_client]() {ws_client.pump_messages();});
    DBResultDispatcher& dbrd(DBResultDispatcher::get_instance());
    dbrd.set_dispatcher([&server](emscripten::EM_VAL v)
                                    {server.add_db_response(v); });
    dbrd.set_reg_chunk([&server](const std::string& qid, int sz, int addr)
                                    {server.register_chunk(qid.c_str(), sz, addr); });
    IDBFileCache font_cache(
        [](ImGuiIO& io, void* f, int sz)->ImFont* {return io.Fonts->AddFontFromMemoryTTF(f, sz); },
        [&ctx](const std::string& n, void* f){ctx.register_font(n, (ImFont*)f); },
        { "Arial.ttf", "CourierNew.ttf" }); // TODO: JSON config

    GLFWwindow* window = im_start(ctx, &font_cache);
    if (window == nullptr) {
        std::cout << method << "im_start failed" << std::endl;
        exit(1);
    }
    emscripten_set_main_loop_arg(im_loop_body, &ctx, 0, 1);
}

