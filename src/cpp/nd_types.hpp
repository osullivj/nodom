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
// types = 2, 10, 3, 2, 3, 3, 2, 2, 10, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 10, 10
// typ_o = Int32, Timestamp<MICROSECOND>, Float64, Int32, Float64, Float64, Int32, Int32, Timestamp<MICROSECOND>, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Int32, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Float64, Int32, Timestamp<MICROSECOND>, Timestamp<MICROSECOND>
// batch_materializer JS code stores type as uint32
// ND these enums are not the same as duckdb.h!
enum DuckType : uint32_t {
    Int32 = 2,
    Timestamp_micro = 10,
    Float64 = 3,
    Utf8 = 5
};

const char* DuckTypeToString(DuckType dt) {
    switch (dt) {
    case DuckType::Int32:
        return "Int32";
    case DuckType::Float64:
        return "Float64";
    case DuckType::Timestamp_micro:
        return "Timestamp_micro";
    case DuckType::Utf8:
        return "Utf8";
    }
    return "Unknown";
}

int DuckTypeToSize(DuckType dt) {
    switch (dt) {
    case DuckType::Int32:
        return 4;
    case DuckType::Float64:
        return 8;
    case DuckType::Timestamp_micro:
        return 4;
    case DuckType::Utf8:
        return 0;
    }
    return -1;
}


