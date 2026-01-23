#pragma once
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "logger.hpp"
#include <functional>

#ifdef __EMSCRIPTEN__
// fwd decl funcs used by IDBFontCache::next()
void on_async_exists_error(void* m);
void on_async_exists(void* c, int exists);
// Only used in ems as a font data memo passed to
// ems IDBStore callbacks, and as a mem cache too.
using FontRegistrar = std::function<void(const std::string& name, ImFont* font)>;
using StringVec = std::vector<std::string>;
struct IDBFontCache {
    IDBFontCache(FontRegistrar fr, const StringVec& fonts) :registrar(fr), font_list(fonts) { }
    ~IDBFontCache() { std::for_each(font_mem_vec.begin(), font_mem_vec.end(), [](auto ptr) {free(ptr); }); }
    void next() {
        if (font_list.empty()) return;
        font_file_name = font_list.back();
        font_list.pop_back();
        emscripten_idb_async_exists("NoDOM", font_file_name.c_str(),
            this, on_async_exists, on_async_exists_error);

    }
    FontRegistrar registrar;
    StringVec font_list;
    std::string font_file_name;
    std::vector<void*> font_mem_vec;
};
// Callbacks for emscripten_idb_async_exists and 
// emscripten_idb_async_load. 

// em_arg_callback_func: typedef void (*em_arg_callback_func)(void*);
void on_async_exists_error(void* m) {
    const char* method = "on_async_exists_error: ";
    auto fm = reinterpret_cast<IDBFontCache*>(m);
    // NoDOM IndexedDB check fails in emscripten_idb_async_exists
    fprintf(stderr, "%sIDB exists failed: %s\n", method, fm->font_file_name.c_str());
}

// em_arg_callback_func: typedef void (*em_arg_callback_func)(void*);
void on_load_error(void* m) {
    const char* method = "on_load_error: ";
    auto fm = reinterpret_cast<IDBFontCache*>(m);
    // NoDOM IndexedDB doesn't exist
    fprintf(stderr, "%sIDB load failed: %s\n", method, fm->font_file_name.c_str());
}

// em_idb_onload_func: typedef void (*em_idb_onload_func)(void*, void*, int);
void on_load(void* m, void* buf, int sz) {
    const char* method = "on_load: ";
    auto fm = reinterpret_cast<IDBFontCache*>(m);
    // copy the font memory as ImGui assumes it won't
    // get freed from underneath...
    if (sz < 100) {
        fprintf(stderr, "%ssz=%d\n", method, sz);
        return;
    }

    void* font_memory = malloc(sz);
    std::memcpy(font_memory, buf, sz);
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImFont* font = io.Fonts->AddFontFromMemoryTTF(font_memory, sz);
    if (font == nullptr) {
        fprintf(stderr, "%sAddFontFromMemoryTTF failed\n", method);
        free(font_memory);
        return;
    }
    // discard the .ttf suffix for NDContext registration
    auto fflen = fm->font_file_name.size();
    fm->font_file_name.erase(fflen - 4, fflen);
    fm->registrar(fm->font_file_name, font);
    fm->font_mem_vec.push_back(font_memory);
    printf("%s%s loaded\n", method, fm->font_file_name.c_str());
    // kick off the next load, if there is one...
    fm->next();
}

// em_idb_exists_func: typedef void (*em_idb_exists_func)(void*, int);
void on_async_exists(void* c, int exists) {
    const char* method = "on_async_exists: ";
    auto fm = reinterpret_cast<IDBFontCache*>(c);
    if (exists != 0) {
        emscripten_idb_async_load("NoDOM", fm->font_file_name.c_str(), c, on_load, on_load_error);
    }
    else {
        fprintf(stderr, "on_async_exists: cannot access NoDOM.fonts\n");
    }
}

#endif

inline void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

template <typename JSON, typename DB>
#ifdef __EMSCRIPTEN__
GLFWwindow* im_start(NDContext<JSON, DB>& ctx, void* fc = nullptr)
#else
GLFWwindow* im_start(NDContext<JSON, DB>& ctx)
#endif
{
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return nullptr;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    // NB EMS example has this line...
    // float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow(1280, 720, "NoDOM", NULL, NULL);
    if (window == nullptr)
        return window;
    ctx.set_glfw_window(window);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;
#endif
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // setup scaling
    float scale = 4.0;
#ifndef __EMSCRIPTEN__
    JSON bbcfg(ctx.get_breadboard_config());
    if (JContains(bbcfg, "font_scale_dpi"))
        scale = JAsFloat(bbcfg, "font_scale_dpi");
#endif
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale);
    style.FontScaleDpi = scale;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // NB default font is ProggyClean; scalable but slow
    ImFont* font = io.Fonts->AddFontDefault();
    ctx.register_font("Default", font);
#ifndef __EMSCRIPTEN__
    JSON jfonts = bbcfg["fonts"];
    for (auto fit = jfonts.begin(); fit != jfonts.end(); ++fit) {
        // fonts is an untyped list of strings. so we get<std::str>()
        // to coerce and avoid extra quotes
        font = io.Fonts->AddFontFromFileTTF(fit.value().template get<std::string>().c_str());
        IM_ASSERT(font != NULL);
        ctx.register_font(fit.key(), font);
    }
#else
    if (fc != nullptr) {
        auto font_cache = reinterpret_cast<IDBFontCache*>(fc);
        // Can we load fonts from IndexedDB?
        font_cache->next();
    }
#endif

    // Our state
    return window;
}


template <typename JSON, typename DB>
int im_render(NDContext<JSON, DB>& ctx)
{
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    GLFWwindow* window = ctx.get_glfw_window();
    if (window == nullptr)
        return 0;

    if (!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ctx.render();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#ifndef __EMSCRIPTEN__
        glfwSwapBuffers(window);
#endif

        return 1;
    }
    return 0;
}

inline void im_end(GLFWwindow* window)
{
    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}