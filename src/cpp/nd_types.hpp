#pragma once
#include <map>
#include <functional>
#include <string>

using StringVec = std::vector<std::string>;
using IntVec = std::vector<int>;
using FloatVec = std::vector<float>;
using StringStringMap = std::map<std::string, std::string>;
using StringIntMap = std::map<std::string, int>;


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

const char* DuckTypeToString(DuckType dt);
int DuckTypeToSize(DuckType dt);

enum PixReportType : int32_t {
    RenderFPS = 2,
    RenderPushPC,
    RenderPopPC,
    RenderBadHandlePC,
    DBScan,
    DBQuery,
    DBBatch
};

// decls as these are in a separate unit of compilation
void pix_init();
void pix_fini();
void pix_begin_render(int render_count);
void pix_begin_dbase();
void pix_end_event();
void pix_report(PixReportType t, float val);

enum StyleColor : uint32_t {
    Dark = 0,   // default
    Light,
    Classic,
    EndColors
};
const char* StyleColorToString(StyleColor col);

// FastPath [Int|Float]Indices are used as array indices
// so inherit from uint32_t
enum IntIndices : uint32_t {
    StyleColoring = 0,
    EndInts
};

// FloatIndices does not include FontSizeBase as that is
// set accessed via render_push_font invocation of PushFont
// which sets style.FontSizeBase. We may add _MainScale if
// we want to make it a GUI setting.
enum FloatIndices : uint32_t {
    FontScaleDpi = 0,
    FontScaleMain,
    EndFloats
};

enum StringIndices : uint32_t {
    ServerUrl = 0,
    EndStrings
};

enum Critical : uint32_t {
    Clear = 0,
    WebSockConnectionFailed,
    EndCritical
};

const char* CriticalToString(Critical ec);

static constexpr int STR_BUF_LEN{ 256 };
static constexpr int FMT_BUF_LEN{ 16 };

// NDContext helpers
const char* NextEvent(const char* nd_event);
void SetStyleColoring(int col);

enum RenderMethod : uint32_t {
    Home = 0,   // Home
    InputInt,   // Native widgets
    Combo,
    Checkbox,
    Text,
    Button,
    Table,      // Compound widgets
    Footer,
    DatePicker,
    DuckTableSummaryModal,
    LoadingModal,
    Separator,  // Layout widgets
    SameLine,
    NewLine,
    Spacing,
    AlignTextToFramePadding,
    BeginChild, // Grouping widgets
    EndChild,
    BeginGroup,
    EndGroup,
    PushFont,   // Fonts
    PopFont,
    EndRenderMethod
};

RenderMethod RenderMethodFromString(const char* m);