#include "imgui.h"
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

/*
void save_ini_to_file(const char* fpath) {
    if (fpath != nullptr) {
        ImGui::SaveIniSettingsToDisk(fpath);
    }
}

void load_ini_from_file(const char* fpath) {
    if (fpath != nullptr) {
        std::string file_path_s{ fpath };
        std::filesystem::path file_path{ file_path_s };
        if (std::filesystem::exists(file_path)) {
            ImGui::LoadIniSettingsFromDisk(fpath);
        }
    }
}


void save_ini_to_memory() {

}
*/


static void StyleSettingsHandler_ClearAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{

}

// Apply to existing windows (if any)
static void StyleSettingsHandler_ApplyAll(ImGuiContext* ctx, ImGuiSettingsHandler*)
{
    // noop: ReadLine has applied
    // TODO: call SetStyleColor Light,Dark,Classic here...
}

static void* StyleSettingsHandler_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler*, const char* name)
{
    ImGuiContext& g = *ctx;
    ImGuiStyle& style = g.Style;

    // No Style ID or count to unpack from 2nd field, and no obj to alloc as Style obj is singleton
    return &style;
}

static void StyleSettingsHandler_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line)
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