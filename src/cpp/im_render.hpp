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
using FileLoadFunc = std::function<void*(ImGuiIO& io, void* data, int sz)>;
using FileDeployFunc = std::function<void(const std::string& name, void* data)>;

using StringVec = std::vector<std::string>;

struct IDBFileCache {
    IDBFileCache(FileLoadFunc lf, FileDeployFunc df, const StringVec& files) 
        :load_func(lf), deploy_func(df), file_list(files) { }
    ~IDBFileCache() { std::for_each(file_mem_vec.begin(), file_mem_vec.end(), [](auto ptr) {free(ptr); }); }
    void next() {
        if (file_list.empty())
            return;
        file_name = file_list.back();
        file_list.pop_back();
        emscripten_idb_async_exists("NoDOM", file_name.c_str(), this,
                                on_async_exists, on_async_exists_error);
    }
    FileLoadFunc        load_func;
    FileDeployFunc      deploy_func;
    StringVec           file_list;
    std::string         file_name;
    std::vector<void*>  file_mem_vec;
};

// Callbacks for emscripten_idb_async_exists and 
// emscripten_idb_async_load. NB no \n in log
// lines as the impl below is only ever in play
// in the browser. 

// em_arg_callback_func: typedef void (*em_arg_callback_func)(void*);
void on_async_exists_error(void* fc) {
    const char* method = "on_async_exists_error: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);
    // NoDOM IndexedDB check fails in emscripten_idb_async_exists
    fprintf(stdout, "%sIDB_EXIST_FAIL(%s)", method, file_cache->file_name.c_str());
}

// em_arg_callback_func: typedef void (*em_arg_callback_func)(void*);
void on_load_error(void* fc) {
    const char* method = "on_load_error: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);
    // NoDOM IndexedDB doesn't exist
    fprintf(stdout, "%sIDB_LOAD_FAIL(%s)", method, file_cache->file_name.c_str());
}

// em_idb_onload_func: typedef void (*em_idb_onload_func)(void*, void*, int);
void on_load(void* fc, void* buf, int sz) {
    const char* method = "on_load: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);

    // copy the file memory as ImGui assumes it won't
    // get freed from underneath...
    if (sz < 100) {
        fprintf(stdout, "%ssz=%d", method, sz);
        return;
    }
    void* file_memory = malloc(sz);
    std::memcpy(file_memory, buf, sz);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    void* data = file_cache->load_func(io, file_memory, sz);

    // discard the .ttf suffix for NDContext registration
    auto fflen = file_cache->file_name.size();
    file_cache->file_name.erase(fflen - 4, fflen);
    file_cache->deploy_func(file_cache->file_name, data);
    file_cache->file_mem_vec.push_back(file_memory);
    printf("%sFILE_LOADED(%s)\n", method, file_cache->file_name.c_str());
    // kick off the next load, if there is one...
    file_cache->next();
}

// em_idb_exists_func: typedef void (*em_idb_exists_func)(void*, int);
void on_async_exists(void* fc, int exists) {
    const char* method = "on_async_exists: ";
    IDBFileCache* file_cache = reinterpret_cast<IDBFileCache*>(fc);
    assert(file_cache != nullptr);
    if (exists != 0) {
        emscripten_idb_async_load("NoDOM", file_cache->file_name.c_str(), fc, on_load, on_load_error);
    }
    else {
        fprintf(stdout, "%sIDB_ACCESS_FAIL(%s)\n", method, file_cache->file_name.c_str());
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
    // imgui boilerplate: moved this to happen
    // later in start_render_cycle
    // glfwSwapInterval(1); // Enable vsync

    NDConfig<JSON>& cfg{ NDConfig<JSON>::get_instance() };

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.IniFilename = ctx.get_ini_path();
    AddStyleSettingsHandler(ctx.get_style_coloring());

    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();   // IntIndices::StyleColoring init StyleColors::Dark

    // setup scaling defaults explicitly
    float scale = 1.0;
    float dpi = 1.0;        // TODO: restore font_scale_dpi config
    cfg.get_value(Static::font_scale_dpi_cs, dpi);
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale); // set _MainScale: all sizes apart from fonts
    style.FontScaleDpi = dpi;
    style.FontScaleMain = scale;
    style.FontSizeBase = 12.0;

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
    StringStringMap font_map;
    if (cfg.get_nested(Static::fonts_cs, font_map)) {
        for (auto fit = font_map.begin(); fit != font_map.end(); ++fit) {
            font = io.Fonts->AddFontFromFileTTF(fit->second.c_str());
            IM_ASSERT(font != NULL);
            ctx.register_font(fit->first.c_str(), font);
        }
    }
#else
    if (fc != nullptr) {
        auto font_cache = reinterpret_cast<IDBFileCache*>(fc);
        // Can we load fonts from IndexedDB?
        font_cache->next();
    }
#endif
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