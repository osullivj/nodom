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

