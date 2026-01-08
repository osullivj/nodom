#pragma once
#include <map>
#include <functional>

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
enum DuckType : uint32_t {
    Int = 2,
    Float = 3,
    Utf8 = 5,
    Timestamp_micro = 10
};

const char* DuckTypeToString(DuckType dt) {
    switch (dt) {
    case DuckType::Int:
        return "Int";
    case DuckType::Float:
        return "Float";
    case DuckType::Timestamp_micro:
        return "Timestamp_micro";
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
        return 8;
    case DuckType::Timestamp_micro:
        return 8;
    case DuckType::Utf8:
        return 8;
    }
    return 0;
}


