#include <time.h>
#include "nd_types.hpp"
#include "static_strings.hpp"
#include "fmt/base.h"
#include "fmt/chrono.h"

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
    static fmt::format_to_n_result<char*> fmt_result;
    static constexpr int dbuf_size{ 32 };
    static char dbuf[dbuf_size];

    WasmDuckType dt{ tipe };
    uint32_t* ui32data = &(chunk_ptr[bptr]);
    int32_t* i32data = reinterpret_cast<int32_t*>(ui32data);
    int64_t* i64data = reinterpret_cast<int64_t*>(ui32data);
    double* dbldata = reinterpret_cast<double*>(ui32data);

    static std::map<WasmDuckType, int64_t>  timestamp_scale_map{
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
    case WasmDuckType::wdtTimestamp_ns:
        fmt_result = fmt::format_to_n(dbuf, dbuf_size, "{:%F %T}", TPNano{ std::chrono::nanoseconds{ *i64data } });
        dbuf[fmt_result.size] = 0;
        sprintf(cbuf, "[%d]=%s", row_index, dbuf);
        break;
    case WasmDuckType::wdtTimestamp_us:
        fmt_result = fmt::format_to_n(dbuf, dbuf_size, "{:%F %T}", TPMicro{ std::chrono::microseconds{ *i64data } });
        dbuf[fmt_result.size] = 0;
        sprintf(cbuf, "[%d]=%s", row_index, dbuf);
        break;
    case WasmDuckType::wdtTimestamp_ms:
        fmt_result = fmt::format_to_n(dbuf, dbuf_size, "{:%F %T}", TPMilli{ std::chrono::milliseconds{ *i64data } });
        dbuf[fmt_result.size] = 0;
        sprintf(cbuf, "[%d]=%s", row_index, dbuf);
        break;
    case WasmDuckType::wdtTimestamp_s:
        fmt_result = fmt::format_to_n(dbuf, dbuf_size, "{:%F %T}", TPSecs{ std::chrono::seconds{ *i64data } });
        dbuf[fmt_result.size] = 0;
        sprintf(cbuf, "[%d]=%s", row_index, dbuf);
        break;
    case WasmDuckType::wdtTimestamp:
        sprintf(cbuf, "[%d]=%s", row_index, "TSu");
        break;
    default:
        sprintf(cbuf, "[%d]=%s", row_index, "unk");
        break;
    }
    printf("%s\n", cbuf);
}

#endif
