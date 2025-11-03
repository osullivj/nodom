// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)
// If you are new to Dear ImGui, read documentation from the docs/ folder + read the top of imgui.cpp.
// Read online: https://github.com/ocornut/imgui/tree/master/docs
// 
// websock hdrs: only used win32 for breadboard builds 
#ifndef __EMSCRIPTEN__
#include "websock.hpp"
#else
#include "emscripten_mainloop_stub.h"
#endif

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


// NoDOM: this main.cpp is intended to stay as close as possible
// to the imgui/examples/example_glfw_opengl3/main.cpp as that's 
// the appropriate rendeder for the Chrome codebase. JOS 2025-08-25

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

int main(int argc, char* argv[]) {
#ifndef __EMSCRIPTEN__
    NDProxy<DuckDBCache> server(argc, argv);
    NDContext<nlohmann::json> ctx(server);
    try {
        NDWebSockClient ws_client(server, ctx);
        ws_client.run();
    }
    catch (websocketpp::exception const& e) {
        std::cout << e.what() << std::endl;
    }
#else
    NDContext<emscripten::val> ctx(server);
    GLFWwindow* window = im_start(ctx);
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
    while (im_render(window, ctx)) {

    }
    EMSCRIPTEN_MAINLOOP_END;
    im_end(window);
#endif
}

