#include "imgui.h"
#include "ems_idb.hpp"
#include "imgui_internal.h"
#include "static_strings.hpp"
#include "nd_types.hpp"
#include <filesystem>

const char* StyleColorToString(StyleColor col) {
    switch (col) {
    case Dark:
        return Static::dark_cs;
    case Light:
        return Static::light_cs;
    case Classic:
        return Static::classic_cs;
    default:
        return 0;
    }
}

void SetStyleColoring(int col) {
    switch (col) {
    case Dark:
        ImGui::StyleColorsDark();
        break;
    case Light:
        ImGui::StyleColorsLight();
        break;
    case Classic:
        ImGui::StyleColorsClassic();
        break;
    }
}


static void StyleSettingsHandler_ClearAll(ImGuiContext* , ImGuiSettingsHandler*)
{
}

static void StyleSettingsHandler_ApplyAll(ImGuiContext* , ImGuiSettingsHandler*)
{
    // noop: ReadLine has applied
}

static void* StyleSettingsHandler_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler*, const char* )
{
    ImGuiContext& g = *ctx;
    ImGuiStyle& style = g.Style;

    // No Style ID or count to unpack from 2nd field,
    // and no obj to alloc as Style obj is singleton
    return &style;
}

static void StyleSettingsHandler_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* , const char* line)
{
    ImGuiContext& g = *ctx;
    ImGuiStyle& style = g.Style;
    float font_scale_main = 1.0f;
    int style_coloring = 0;

    if (sscanf(line, "FontScaleMain=%f", &font_scale_main) == 1) {
        style.FontScaleMain = font_scale_main;
    }
    if (sscanf(line, "StyleColoring=%d", &style_coloring) == 1) {
        int* sc = (int*)handler->UserData;
        *sc = style_coloring;
        SetStyleColoring(style_coloring);
    }
}

static void StyleSettingsHandler_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    ImGuiContext& g = *ctx;
    ImGuiStyle& style = g.Style;
    int* style_coloring = (int*)handler->UserData;

    buf->reserve(buf->size() + 30); // ballpark reserve
    buf->appendf("[%s][singleton]\n", handler->TypeName);
    buf->appendf("FontScaleMain=%g\n", style.FontScaleMain);
    buf->appendf("StyleColoring=%d\n", *style_coloring);
    buf->append("\n");
}

void AddStyleSettingsHandler(int* style_coloring_ptr)
{
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "Style";
    ini_handler.TypeHash = ImHashStr("Style");
    ini_handler.ClearAllFn = StyleSettingsHandler_ClearAll;
    ini_handler.ReadOpenFn = StyleSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = StyleSettingsHandler_ReadLine;
    ini_handler.ApplyAllFn = StyleSettingsHandler_ApplyAll;
    ini_handler.WriteAllFn = StyleSettingsHandler_WriteAll;
    ini_handler.UserData = style_coloring_ptr;
    ImGui::AddSettingsHandler(&ini_handler);
}

/*
#ifdef __EMSCRIPTEN__
// TODO: rm when we've figured out how to Load and Save Ini from memory

// provide our own implementation of imgui's ImFile[Open|Close|GetSize|Read] API
// declared in imgui_internal.h, and used by ImGui::UpdateSettings(), as called
// by ImGui::NewFrame(). When running ems wasm, as opposed to win32 Breadbaord,
// we want the file writes redirected to the ems IndexedDB API.

ImFileHandle ImFileOpen(const char* filename, const char* mode) {
    // posix equiv: fopen(filename, mode);
    return (ImFileHandle)(new IDBFileWriter(filename));
}

// We should in theory be using fseeko()/ftello() with off_t and _fseeki64()/_ftelli64() with __int64, waiting for the PR that does that in a very portable pre-C++11 zero-warnings way.
bool ImFileClose(ImFileHandle f) {
    // posix equiv: fclose(f);
    IDBFileWriter* fw = reinterpret_cast<IDBFileWriter*>(f);
    delete fw;
    return true;
}

ImU64 ImFileGetSize(ImFileHandle f) {
    return 0;
}

ImU64 ImFileRead(void* data, ImU64 sz, ImU64 count, ImFileHandle f) {
    // fread(data, (size_t)sz, (size_t)count, f); 
    return 0;
}

ImU64 ImFileWrite(const void* data, ImU64 sz, ImU64 count, ImFileHandle f) {
    // posix equiv: fwrite(data, (size_t)sz, (size_t)count, f); 
    IDBFileWriter* fw = reinterpret_cast<IDBFileWriter*>(f);
    int data_len = sz * count;
    fw->write((void*)data, data_len);
    return data_len;
}

#endif
*/