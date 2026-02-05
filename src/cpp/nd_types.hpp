#pragma once
#include <map>
#include <functional>
#include <string>

using StringVec = std::vector<std::string>;
using IntVec = std::vector<int>;


// typedefs and using defns for NoDOM universal types
// universal types: not specific to JSON, DB etc

class Facade;
using FacadeMap = std::map<std::string, Facade*>;

using WebSockSenderFunc = std::function<void(const std::string&)>;
using MessagePumpFunc = std::function<void()>;

// DuckDBWebCache chunk helpers
using RegChunkFunc = std::function<void(const std::string&, int, int)>;
struct Chunk {
    Chunk(int sz, int address) :size(sz), addr(address) {}
    int size;
    int addr;
};
using ChunkVec = std::vector<Chunk>;
using ChunkMap = std::map<std::string, ChunkVec>;

// DuckDB helpers for DuckDB-WASM.
// ND these enums are not the same as duckdb.h
// These are arrow-js enums...
// https://github.com/apache/arrow-js/blob/main/src/enum.ts
// ...so do not make size explicit like C/C++ types.
enum DuckType : int32_t {
    Int = 2,
    Float = 3,
    Utf8 = 5,
    Timestamp = 10,
    Timestamp_s = -15,
    Timestamp_ms = -16,
    Timestamp_us = -17,
    Timestamp_ns = -18
};

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

// decls as these are in a separate unit of compilation
void pix_init();
void pix_fini();
void pix_begin_render(int render_count);
void pix_begin_dbase();
void pix_end_event();
