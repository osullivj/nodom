#pragma once
#include <algorithm>
#include <array>
#include <map>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <set>
#include <cassert>
#include <ios>
#include <iomanip>
#include "fmt/base.h"
#include "fmt/chrono.h"

using StringVec = std::vector<std::string>;
using IntVec = std::vector<int>;
using FloatVec = std::vector<float>;
using StringStringMap = std::map<std::string, std::string>;
using StringIntMap = std::map<std::string, int>;
using StringSet = std::set<std::string>;

template <typename T>
bool ptr_in_vec(const std::vector<T>& vec, const T* ptr) {
    if (vec.empty()) return false;
    const T* start = vec.data();
    const T* end = start + vec.size();
    return std::greater_equal<const T*>{}(ptr, start) && std::less<const T*>{}(ptr, end);
}

// typedefs and using defns for NoDOM universal types
// universal types: not specific to JSON, DB etc

// Timestamp handling utils
using TPMilli = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;
using TPMicro = std::chrono::time_point<std::chrono::system_clock, std::chrono::microseconds>;
using TPNano = std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>;
using TPSecs = std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>;

using VSFunc = std::function<void(const std::string&)>;
using VVFunc = std::function<void()>;

// DuckDBWebCache chunk helpers
using RegWasmChunkFunc = std::function<void(const std::string&, int, int)>;
struct WasmChunk {
    WasmChunk(uint32_t sz, uint32_t address) :size(sz), addr(address) {}
    uint32_t    size;
    uint32_t    addr;
};
using WasmChunkVec = std::vector<WasmChunk>;
using WasmChunkMap = std::map<std::string, WasmChunkVec>;

// DuckDB helpers for DuckDB-WASM.
// ND these enums are not the same as duckdb.h
// These are arrow-js enums...
// https://github.com/apache/arrow-js/blob/main/src/enum.ts
// ...so do not make size explicit like C/C++ types.
enum WasmDuckType : int32_t {
    wdtNone = 0,
    wdtInt = 2,
    wdtFloat = 3,
    wdtUtf8 = 5,
    wdtTimestamp = 10,
    wdtTimestamp_s = -15,
    wdtTimestamp_ms = -16,
    wdtTimestamp_us = -17,
    wdtTimestamp_ns = -18
};

const char* WasmDuckTypeToString(WasmDuckType dt);
int WasmDuckTypeToSize(WasmDuckType dt);
void printf_comma(int i, int col_count);
void sprintf_value(char* cbuf, uint32_t* chunk_ptr, int bptr, int32_t tipe, int row_index);

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
void SetStyleColoring(int col);
void AddStyleSettingsHandler(int* style_coloring_ptr);
char* LoadIniFromMem(char* bytes, int sz);

enum RenderMethod : uint32_t {
    Noop = 0,   // Null
    Home,       // Home
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
    Window,
    ShadedPlot,
    EndRenderMethod
};

inline bool render_is_valid(RenderMethod rm) {
    return rm != EndRenderMethod;
}

// Built in hi bits for ID ranges
// 32 bits for IDs gives us 16 hi bits and 16 lo bits
// we put the index in the 16 lo bits,
// and the item and data type in 16 hi.
// This enable a max of 65535 (0xFFFF) DCIs
// of any recognised data type.
enum CacheItemType : uint32_t {
    Address = 0x1000000,    // cname, sql_cname, cindex
    Value = 0x2000000,      // Int,Float,Bool,Str,IntVec,StrVec
    EntityID = 0x3000000,   // widget_id|query_id|susbsys_id
    Event = 0x4000000,      // Click,Online,QueryResult,CommandResult
    SubSystem = 0x5000000,  // [GUI|DuckDB].Online
    Menu = 0x6000000,       // MenuBar, Menu, MenuItem
    EndItemTypes = 0xF000000 
};

enum CacheItemSubType : uint32_t {
    None = 0x000000,
    WidgetID = 0x100000,    // EntityID:widget_id
    QueryID = 0x200000,     // EntityID:query_id
    SubSysID = 0x300000,    // EntityID:[GUI|DuckDB]
    WidgetEvent = 0x400000, // Event:Click
    DBEvent = 0x500000,     // Event:[QueryResult|CommandResult]
    SubSysEvent = 0x600000,
    MenuBarID = 0x700000,
    MenuID = 0x800000,
    MenuItemID = 0x900000,
    EndSubItemTypes = 0xF00000
};

enum CacheDataType : uint32_t {
    cdInt = 0x10000, 
    cdFloat = 0x20000, 
    cdBool = 0x30000, 
    cdStr = 0x40000, 
    cdIntVec = 0x50000,
    cdStrVec = 0x60000,
    cdAny = 0x70000,
    cdResultSet = 0x80000,
    EndDataTypes = 0xF0000
};

enum DBEventType : uint32_t {
    dbCommand = 0,
    dbCommandResult,
    dbQuery,
    dbQueryResult,
    dbBatchRequest,
    dbBatchResponse,
    EndDBEventTypes
};

inline bool db_event_is_valid(DBEventType dbev) {
    return dbev != EndDBEventTypes;
}

using CIT = CacheItemType;
using CST = CacheItemSubType;
using CDT = CacheDataType;

// cache index range is 16 bits 0x0000->0xFFFF
// eg 0->65535
static constexpr int MAX_DCI = 0xFFFF;  // 65536
static constexpr int OH_FECK = 0x0FEC0000;

template <CIT itype, CDT dtype>
struct DataCacheIndex {

    static constexpr CIT item_type{ itype };
    static constexpr CDT data_type{ dtype };

    // 0 is a bad value as hi 16 bits not set
    // to valid IDType
    uint32_t    magic_index{ OH_FECK };

    // no default construction so we require
    // real inx at instantiation. Take the 
    // defaults for assign and copy. Move
    // doesn't make sense as this is a "by val" 
    // type...
    DataCacheIndex() = default;
    DataCacheIndex(const DataCacheIndex&) = default;
    DataCacheIndex& operator=(const DataCacheIndex&) = default;

    DataCacheIndex(uint32_t inx, CST stype=CST::None) {
        // check template param val OK
        static_assert(itype != EndItemTypes);
        static_assert(dtype != EndDataTypes);
        // check i is in lo 16 bit range
        if (inx > MAX_DCI)
            throw std::runtime_error("NoDOM BAD_DCI");
        // check stype has sane value
        switch (item_type) {
        case EntityID:
            switch (stype) {
            case None:      // OK during DLC::on_data/actions
            case WidgetID:  // Widget, Query and
            case QueryID:   // SubSystem IDs all fine
            case SubSysID:  // for EntityID
            case MenuBarID:
            case MenuID:
            case MenuItemID:
                break;
            default:
                throw std::runtime_error("NoDOM BAD_ENTITY_ID");
            }
            break;
        case Event:
            switch (stype) {
            case None:
            case WidgetEvent: // Event:Click
            case DBEvent:     // Event:[QueryResult|CommandResult]
            case SubSysEvent:
                break;
            default:
                throw std::runtime_error("NoDOM BAD_ENTITY_TYPE");
            }
            break;
        default:
            break;
        }
        // itipe,dtype in hi 16, inx in lo 16
        magic_index = item_type | data_type;
        magic_index |= stype;
        magic_index += inx;
    }

    uint32_t operator()() {
        if (magic_index == OH_FECK)
            throw std::runtime_error("NoDOM BAD_INX");
        // return only bottom 16bits
        return magic_index & MAX_DCI;
    }

    bool operator==(const DataCacheIndex& rhs) {
        if ((magic_index & MAX_DCI) != (rhs.magic_index & MAX_DCI))
            return false;
        if (data_type != rhs.data_type)
            return false;
        if (item_type != rhs.item_type)
            return false;
        // one and only one side as subtype None
        // is the only mismatch allowed
        if (subtype() == CST::None || rhs.subtype() == CST::None)
            return true;
        if (subtype() != rhs.subtype())
            return false;
        return true;
    }

    DataCacheIndex operator++(int) {
        magic_index++;
        return *this;
    }

    bool is_valid() const {
        return magic_index != OH_FECK;
    }

    CST subtype() const {
        return CST{ magic_index & EndSubItemTypes };
    }

    friend std::ostream& operator<<(std::ostream& os, const DataCacheIndex<itype, dtype>& dci) {
        os << "0x" << std::setfill('0') << std::setw(8) << std::hex << dci.magic_index << std::dec;
        return os;
    }

    friend bool operator<(const DataCacheIndex<itype, dtype>& lhs, const DataCacheIndex<itype, dtype>& rhs) {
        return lhs.magic_index < rhs.magic_index;
    }
};
// DCI examples
// Value string at inx:3    00240003
// Value float at inx:10    0022000A

// Identifiers are always strings...
// Event IDs (Click, Online, QueryResult)
// Query IDs (the_depth_query, the_depth_summary)
// Widget IDs (i_am_the_footer_button)
// System IDs (GUI, DuckDB)
// LHS of ActionKey
using EntityInx = DataCacheIndex<CIT::EntityID, CDT::cdStr>;
// RHS of ActionKey
using EventInx = DataCacheIndex<CIT::Event, CDT::cdStr>;
using AddrInx = DataCacheIndex<CIT::Address,CDT::cdStr>;
// Three types of atomic value; all mutable
using IntInx = DataCacheIndex<CIT::Value,CDT::cdInt>;
using BoolInx = DataCacheIndex<CIT::Value, CDT::cdBool>;
using FloatInx = DataCacheIndex<CIT::Value, CDT::cdFloat>;
using StrInx = DataCacheIndex<CIT::Value, CDT::cdStr>;
// Array values
using IntVecInx = DataCacheIndex<CIT::Value, CDT::cdIntVec>;  // mutable
using StrVecInx = DataCacheIndex<CIT::Value, CDT::cdStrVec>;  // !mutable

// Cache entry defn
// AInx:VInx    eg  "op1":<int>
// 
// Action key defn
// EntityInx:EventInx   eg  "the_depth_query":"QueryResult"
//                      eg  "i_am_footer_db_button":"Click"
//                      eg  "DuckDB":"Online"
//                           "GUI":"Online"

struct ActionKey {
    uint32_t key{OH_FECK};

    ActionKey() = default;
    ~ActionKey() = default;

    ActionKey(EntityInx ninx, EventInx einx) {
        // ninx in the hi 16 bits, einx in the lo 16
        key = ninx() << 16 | einx();
    }

    // Reconstruct the EntityInx passed to the ctor,
    // but with CST::None as we cannot know if it
    // was WidgetID, QueryID or SubSysID
    EntityInx entity_inx() const {
        return EntityInx(key >> 16);
    }

    // Reconstruct the EntityInx passed to the ctor,
    // but with CST::None as we cannot know if it
    // was Widget, Query or SubSys event type.
    EventInx event_inx() const {
        return EventInx(key & MAX_DCI);
    }

    friend std::ostream& operator<<(std::ostream& os, const ActionKey& ak) {
        os << "0x" << std::setfill('0') << std::setw(8) << std::hex << ak.key;
        return os;
    }
    friend bool operator<(const ActionKey& lhs, const ActionKey& rhs) {
        return lhs.key < rhs.key;
    }

};

enum CacheSpecifier : uint32_t {
    cs_title = 0,           // start atomic_cspec_types
    cs_title_font,
    cs_title_font_size,
    cs_body_font,
    cs_body_font_size,
    cs_button_font,
    cs_button_font_size,
    cs_year_month_font,
    cs_year_month_font_size,
    cs_day_date_font,
    cs_day_date_font_size,
    cs_font,
    cs_font_size,
    cs_label,
    cs_text,
    cs_step,
    cs_step_fast,
    cs_spinner_radius,
    cs_spinner_thickness,
    cs_flags,
    cs_table_flags,
    cs_combo_flags,
    cs_window_flags,
    cs_column_flags,
    cs_show_lines,
    cs_show_fills,
    cs_shaded_plot_flags,
    cs_menu_bar,
    cs_menu,
    cs_menu_item,
    cs_show_footer_db,
    cs_show_footer_fps,
    cs_show_footer_demo,
    cs_show_footer_id_stack,
    cs_show_footer_font_scale,
    cs_show_footer_style,       // end atomic_cspec_types
    cs_cname,                   // cname_cpsec_types
    cs_cindex,                  // cindex_cpsec_types
    cs_query_id,
    cs_xname,
    cs_yname,
    cs_end_cache_specs
};

using CacheSpecVec = std::vector<CacheSpecifier>;
using CacheSpecTypeMap = std::map<CacheSpecifier, CDT>;
using IntValMap = std::map<CacheSpecifier, IntInx>;
using BoolValMap = std::map<CacheSpecifier, BoolInx>;
using FloatValMap = std::map<CacheSpecifier, FloatInx>;
using StrValMap = std::map<CacheSpecifier, StrInx>;

// YMD, WEEK and their index enums for DatePicker
using YMD = std::array<int, 3>;
enum eYMD { Year = 0, Month, Day};

using WEEK = std::array<int, 7>;
enum WeekDay {
    Mon = 0,
    Tues,
    Weds,
    Thurs,
    Fri,
    Sat,
    Sun,
    EndWeekDay
};
