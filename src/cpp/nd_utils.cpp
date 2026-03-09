#include "nd_types.hpp"
#include "static_strings.hpp"

// Two WASM utils for DuckDB-WASM client code in C++
// Not the same as DuckDB-WASM/Arrow JS client code,
// or DuckDB C API client code as seen in breadboard.
const char* DuckTypeToString(DuckType dt) {
    switch (dt) {
    case DuckType::Int:
        return "Int";
    case DuckType::Float:
        return "Float";
    case DuckType::Timestamp:
        return "TS";
    case DuckType::Timestamp_s:
        return "TS_s";
    case DuckType::Timestamp_ms:
        return "TS_ms";
    case DuckType::Timestamp_us:
        return "TS_us";
    case DuckType::Timestamp_ns:
        return "TS_ns";
    case DuckType::Utf8:
        return "Utf8";
    }
    return "Unknown";
}


int DuckTypeToSize(DuckType dt) {
    switch (dt) {
    case DuckType::Int:
        return 4;
    case DuckType::Float:
    case DuckType::Timestamp:
    case DuckType::Timestamp_s:
    case DuckType::Timestamp_ms:
    case DuckType::Timestamp_us:
    case DuckType::Timestamp_ns:
    case DuckType::Utf8:
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