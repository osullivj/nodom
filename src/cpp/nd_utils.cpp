#include "nd_types.hpp"
#include "static_strings.hpp"
#include "imgui.h"

// Two WASM utils for DuckDB-WASM client code in C++
// Not the same as DuckDB-WASM/Arrow JS client code,
// or DuckDB C API client code as seen in breadboard.
const char* DuckTypeToString(WasmDuckType dt) {
    switch (dt) {
    case WasmDuckType::wdtInt:
        return "Int";
    case WasmDuckType::wdtFloat:
        return "Float";
    case WasmDuckType::wdtTimestamp:
        return "TS";
    case WasmDuckType::wdtTimestamp_s:
        return "TS_s";
    case WasmDuckType::wdtTimestamp_ms:
        return "TS_ms";
    case WasmDuckType::wdtTimestamp_us:
        return "TS_us";
    case WasmDuckType::wdtTimestamp_ns:
        return "TS_ns";
    case WasmDuckType::wdtUtf8:
        return "Utf8";
    }
    return "Unknown";
}


int DuckTypeToSize(WasmDuckType dt) {
    switch (dt) {
    case WasmDuckType::wdtInt:
        return 4;
    case WasmDuckType::wdtFloat:
    case WasmDuckType::wdtTimestamp:
    case WasmDuckType::wdtTimestamp_s:
    case WasmDuckType::wdtTimestamp_ms:
    case WasmDuckType::wdtTimestamp_us:
    case WasmDuckType::wdtTimestamp_ns:
    case WasmDuckType::wdtUtf8:
        return 8;
    }
    return 0;
}





const char* CriticalToString(Critical crit) {
    switch (crit) {
    case Clear:
        return Static::clear_cs;
    case WebSockConnectionFailed:
        return Static::websock_connection_failed_cs;
    case EndCritical:
        return nullptr;
    }
    return 0;
}


