#include <time.h>
#include "nd_types.hpp"
#include "static_strings.hpp"
#include "imgui.h"

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

#ifdef __EMSCRIPTEN__

// Two WASM utils for DuckDB-WASM client code in C++
// Not the same as DuckDB-WASM/Arrow JS client code,
// or DuckDB C API client code as seen in breadboard.
const char* WasmDuckTypeToString(WasmDuckType dt) {
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


int WasmDuckTypeToSize(WasmDuckType dt) {
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


void printf_comma(int i, int col_count) {
    if (i == col_count - 1) printf("\n");
    else printf(",");
}

void sprintf_value(char* cbuf, uint32_t* chunk_ptr, int bptr, int32_t tipe, int row_index) {
    WasmDuckType dt{ tipe };
    uint32_t* ui32data = &(chunk_ptr[bptr]);
    int32_t* i32data = nullptr;
    int64_t* i64data = nullptr;
    double* dbldata = nullptr;

    static tm       tm_data{ 0 };
    static time_t   tm_time_t{ 0 };
    static double   dubble{ 0.0 };
    static double   ts_secs_f{ 0.0 };
    static uint32_t ts_secs_i{ 0 };
    static double   ts_fraction{ 0.0 };
    static uint32_t scale{ 1 };
    static uint32_t preamble_length{ 0 };
    static uint32_t date_length{ 19 };
    static const char* decimal_fmt{ nullptr };
    static std::map<WasmDuckType, int32_t>  timestamp_scale_map{
        {wdtTimestamp_s, 1},
        {wdtTimestamp_ms, 1e3},
        {wdtTimestamp_us, 1e6},
        {wdtTimestamp_ns, 1e9}
    };
    static std::map<WasmDuckType, const char*> timestamp_dec_fmt_map{
        {wdtTimestamp_s, nullptr},
        {wdtTimestamp_ms, "%03.3f"},
        {wdtTimestamp_us, "%06.6f"},
        {wdtTimestamp_ns, "%09.9f"}
    };

    switch (dt) {
    case WasmDuckType::wdtInt:     // stride 4
        i32data = reinterpret_cast<int32_t*>(ui32data);
        sprintf(cbuf, "[%d]=%d", row_index, *i32data);
        break;
    case WasmDuckType::wdtFloat:   // stride 8
        dbldata = reinterpret_cast<double*>(ui32data);
        sprintf(cbuf, "[%d]=%f", row_index, *dbldata);
        break;
    case WasmDuckType::wdtUtf8:    // null term trunc to 8 bytes
        sprintf(cbuf, "[%d]=%s", row_index, (char*)ui32data);
        break;
        // but value is Unix seconds/millis/micros/nanos since epoch
    case WasmDuckType::wdtTimestamp_ns:
    case WasmDuckType::wdtTimestamp_us:
    case WasmDuckType::wdtTimestamp_ms:
    case WasmDuckType::wdtTimestamp_s:
        scale = timestamp_scale_map[dt];
        decimal_fmt = timestamp_dec_fmt_map[dt];
        dbldata = reinterpret_cast<double*>(ui32data);
        dubble = *dbldata;
        // cast dubble to 64 bit signed int before dividing
        // by 1e[1,3,6,9]
        ts_secs_i = static_cast<uint64_t>(dubble) / scale;
        ts_secs_f = dubble / static_cast<double>(scale);
        ts_fraction = ts_secs_f - ts_secs_i;
        tm_time_t = static_cast<time_t>(ts_secs_i);
        gmtime_r(&tm_time_t, &tm_data);
        // fprintf(stdout, "%s: ts_secs_f(%f) ts_secs_i(%d) ts_frac(%f)", DuckTypeToString(dt), ts_secs_f, ts_secs_i, ts_fraction);
        sprintf(cbuf, "[%d]=", row_index);
        preamble_length = strlen(cbuf);
        strftime(cbuf + preamble_length, sizeof(cbuf) - preamble_length, "%Y-%m-%d %I:%M:%S", &tm_data);
        // date_length:2026-01-21 10:57:33 is 19 chars
        if (decimal_fmt) {
            sprintf(cbuf + strlen(cbuf), decimal_fmt, ts_fraction);
        }
        break;
    case WasmDuckType::wdtTimestamp:
        sprintf(cbuf, "[%d]=%s", row_index, "TSu");
        break;
    default:
        sprintf(cbuf, "[%d]=%s", row_index, "unk");
        break;
    }
    printf(cbuf);
    printf("\n");
}

#endif
