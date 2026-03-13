#include "nd_types.hpp"
#include "static_strings.hpp"
#include "imgui.h"

// Two WASM utils for DuckDB-WASM client code in C++
// Not the same as DuckDB-WASM/Arrow JS client code,
// or DuckDB C API client code as seen in breadboard.
const char* DuckTypeToString(DuckType dt) {
    switch (dt) {
    case DuckType::dtInt:
        return "Int";
    case DuckType::dtFloat:
        return "Float";
    case DuckType::dtTimestamp:
        return "TS";
    case DuckType::dtTimestamp_s:
        return "TS_s";
    case DuckType::dtTimestamp_ms:
        return "TS_ms";
    case DuckType::dtTimestamp_us:
        return "TS_us";
    case DuckType::dtTimestamp_ns:
        return "TS_ns";
    case DuckType::dtUtf8:
        return "Utf8";
    }
    return "Unknown";
}


int DuckTypeToSize(DuckType dt) {
    switch (dt) {
    case DuckType::dtInt:
        return 4;
    case DuckType::dtFloat:
    case DuckType::dtTimestamp:
    case DuckType::dtTimestamp_s:
    case DuckType::dtTimestamp_ms:
    case DuckType::dtTimestamp_us:
    case DuckType::dtTimestamp_ns:
    case DuckType::dtUtf8:
        return 8;
    }
    return 0;
}


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
    return 0;
}


const char* CriticalToString(Critical crit) {
    switch (crit) {
    case Clear:
        return Static::clear_cs;
    case WebSockConnectionFailed:
        return Static::websock_connection_failed_cs;
    default:
        return 0;
    }
    return 0;
}

// NDContext helper NextEvent: what comes next in a 
// DB action_dispatch sequence?
const char* NextEvent(const char* nd_event) {
    if (strcmp(nd_event, Static::command_cs) == 0) {
        return Static::command_result_cs;
    }
    if (strcmp(nd_event, Static::query_cs) == 0) {
        return Static::query_result_cs;
    }
    if (strcmp(nd_event, Static::batch_request_cs) == 0) {
        return Static::batch_response_cs;
    }
    return nullptr;
}

// NDContext helper: SetStyleColoring
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

RenderMethod RenderMethodFromString(const std::string& method) {
    // Home
    if (method == Static::rm_home_cs)
        return RenderMethod::Home;
    // Native
    if (method == Static::rm_input_int_cs)
        return RenderMethod::InputInt;
    if (method == Static::rm_combo_cs)
        return RenderMethod::Combo;
    if (method == Static::rm_checkbox_cs)
        return RenderMethod::Checkbox;
    if (method == Static::rm_text_cs)
        return RenderMethod::Text;
    if (method == Static::rm_button_cs)
        return RenderMethod::Button;
    if (method == Static::rm_table_cs)
        return RenderMethod::Table;
    // Compound
    if (method == Static::rm_footer_cs)
        return RenderMethod::Footer;
    if (method == Static::rm_date_picker_cs)
        return RenderMethod::DatePicker;
    if (method == Static::rm_duck_table_summary_modal_cs)
        return RenderMethod::DuckTableSummaryModal;
    if (method == Static::rm_loading_modal_cs)
        return RenderMethod::LoadingModal;
    // Layout
    if (method == Static::rm_separator_cs)
        return RenderMethod::Separator;
    if (method == Static::rm_same_line_cs)
        return RenderMethod::SameLine;
    if (method == Static::rm_new_line_cs)
        return RenderMethod::NewLine;
    if (method == Static::rm_spacing_cs)
        return RenderMethod::Spacing;
    if (method == Static::rm_align_text_to_frame_padding_cs)
        return RenderMethod::AlignTextToFramePadding;
    // Grouping
    if (method == Static::rm_begin_child_cs)
        return RenderMethod::BeginChild;
    if (method == Static::rm_end_child_cs)
        return RenderMethod::EndChild;
    if (method == Static::rm_begin_group_cs)
        return RenderMethod::BeginGroup;
    if (method == Static::rm_end_group_cs)
        return RenderMethod::EndGroup;
    // Fonts
    if (method == Static::rm_push_font_cs)
        return RenderMethod::PushFont;
    if (method == Static::rm_pop_font_cs)
        return RenderMethod::PopFont;
    return EndRenderMethod;
}