#pragma once
#include <memory>
#include "static_strings.hpp"

// cspec Property Groups
// == Home: unique to Home, custom code
// children:WidgetVec
// critical_messages:StrVec
// 
// == Title: Home, DuckTableSummaryModal, LoadingModal, Table
// title:str
// title_font:str
// title_font_size:int
// == Body font: DuckTableSummaryModal, LoadingModal, Table
// body_font:str
// body_font_size:int
// == Button font: DuckTableSummaryModal
// button_font: str
// button_font_size: int
// == Year month font: DatePicker
// year_month_font: str
// year_month_font_size: int
// == Day date font: DatePicker
// day_date_font: str
// day_date_font_size: int
// == Push font
// font:str
// font_size:int
// == Label: InputInt, Combo, CheckBox
// label: str
// == Text: Text, Button
// text: str
// == Step: InputInt
// step: int
// step_fast: int
// == Spinner: LoadingModal
// spinner_radius: int
// spinner_thickness: int
// == Flags: InputInt, DatePicker, DuckTableSummaryModal
// flags: InputInt:Int
// table_flags:[
//      DatePicker:Int, 
//      DuckTableSummaryModal:Int, 
//      LoadingModal:Int,
//      Table:Int
// ]
// combo_flags: DatePicker:Int
// window_flags:[
//      DuckTableSummaryModal:Int,
//      LoadingModal:Int,
//      Table:Int
// ]
// == Addresses: Combo, InputInt, CheckBox, DatePicker, LoadingModal
// cname: AddrInx:[
//          Combo:StrVec, 
//          InputInt:Int, 
//          CheckBox:Bool, 
//          DatePicker:IntVec,
//          LoadingModal:StrVec
// ]
// index: AddrInx:[
//          Combo:Int,
// ]
// == Queries: Table
// qname: AddrInx
// == Footer switches: Footer
// db:bool
// fps:bool
// demo : bool
// id_stack : bool
// font_scale : bool
// style : Int


// WASM cache types that capture the data and layout
// JSON, and apply constraints.

struct DataRef {
    CDT tipe{ EndDataTypes };   // cdInt, cdFloat, cdBool, cdStr
    AddrInx addr_inx;           // inx to key in data[key]
    uint32_t ref_inx{OH_FECK};  // inx to data[key]
    uint32_t size{ 1 };         // scalar or array
    // TODO: add dirty flag?
};

using DataRefMap = std::map<CacheSpecifier, DataRef>;
using DataRefVec = std::vector<DataRef>;
using MenuMap = std::map<AddrInx, DataRef>;

struct NDWidget {
    NDWidget() = default;
    NDWidget(const NDWidget&) = default;
    // NDWidget(RenderMethod meth, const std::string& wid)
    NDWidget(RenderMethod meth, EntityInx winx)
        :rname(meth), widget_inx(winx) { }

    RenderMethod    rname{ EndRenderMethod };
    EntityInx       widget_inx;
    IntValMap       cspec_int;
    BoolValMap      cspec_bool;
    FloatValMap     cspec_float;
    StrValMap       cspec_str;
    DataRefMap      data_refs;
    MenuMap         menu_map;
    std::vector<std::shared_ptr<NDWidget>>  children;
};
using WidgetPtr = std::shared_ptr<NDWidget>;
using WidgetVec = std::vector<WidgetPtr>;

using PushableMap = std::map<EntityInx, WidgetPtr>;

using WCSCSFunc = std::function<void(WidgetPtr, CacheSpecifier, CacheSpecifier)>;


struct NDAction {
    EntityInx push_ui;
    RenderMethod pop_ui{ EndRenderMethod };
    DBEventType db_action{ EndDBEventTypes }; // Query||Command||BatchRequest
    EntityInx query_id;
    AddrInx sql_cname;
};

struct NDActionInterned {
    char* push_ui{ nullptr };
    char* pop_ui{ nullptr };
    char* db_action{ nullptr };
    char* query_id{ nullptr };
    char* sql_cname{ nullptr };
};

struct NDActionErrors {
    int inx{ -1 };
    StringVec error_vec;
};

struct PendingAction {
    EntityInx   entity_inx{ OH_FECK };
    EventInx    event_inx{ OH_FECK };
};

using ActionVec = std::vector<NDAction>;
using ActionInternVec = std::vector<NDActionInterned>;
using ActionErrorVec = std::vector<NDActionErrors>;
using ActionMap = std::map<ActionKey, ActionVec>;
using ActionInternMap = std::map<ActionKey, ActionInternVec>;
using ActionErrorMap = std::map<ActionKey, ActionErrorVec>;

inline RenderMethod RenderMethodFromString(const std::string& method) {
    if (method == Static::rm_noop_cs)
        return RenderMethod::Noop;
    // Home
    if (method == Static::rm_home_cs)
        return RenderMethod::Home;
    // Native
    if (method == Static::rm_input_int_cs)
        return RenderMethod::InputInt;
    if (method == Static::rm_combo_cs)
        return RenderMethod::Combo;
    if (method == Static::rm_checkbox_cs)
        return RenderMethod::Checkbox;
    if (method == Static::rm_text_cs)
        return RenderMethod::Text;
    if (method == Static::rm_button_cs)
        return RenderMethod::Button;
    if (method == Static::rm_table_cs)
        return RenderMethod::Table;
    // Compound
    if (method == Static::rm_footer_cs)
        return RenderMethod::Footer;
    if (method == Static::rm_date_picker_cs)
        return RenderMethod::DatePicker;
    if (method == Static::rm_duck_table_summary_modal_cs)
        return RenderMethod::DuckTableSummaryModal;
    if (method == Static::rm_loading_modal_cs)
        return RenderMethod::LoadingModal;
    // Layout
    if (method == Static::rm_separator_cs)
        return RenderMethod::Separator;
    if (method == Static::rm_same_line_cs)
        return RenderMethod::SameLine;
    if (method == Static::rm_new_line_cs)
        return RenderMethod::NewLine;
    if (method == Static::rm_spacing_cs)
        return RenderMethod::Spacing;
    if (method == Static::rm_align_text_to_frame_padding_cs)
        return RenderMethod::AlignTextToFramePadding;
    // Grouping
    if (method == Static::rm_begin_child_cs)
        return RenderMethod::BeginChild;
    if (method == Static::rm_end_child_cs)
        return RenderMethod::EndChild;
    if (method == Static::rm_begin_group_cs)
        return RenderMethod::BeginGroup;
    if (method == Static::rm_end_group_cs)
        return RenderMethod::EndGroup;
    // Fonts
    if (method == Static::rm_push_font_cs)
        return RenderMethod::PushFont;
    if (method == Static::rm_pop_font_cs)
        return RenderMethod::PopFont;
    if (method == Static::rm_window_cs)
        return RenderMethod::Window;
    if (method == Static::rm_shaded_plot_cs)
        return RenderMethod::ShadedPlot;
    return EndRenderMethod;
}

inline DBEventType DBEventTypeFromString(const std::string& evt) {
    if (evt == Static::command_cs)
        return dbCommand;
    if (evt == Static::command_result_cs)
        return dbCommandResult;
    if (evt == Static::query_cs)
        return dbQuery;
    if (evt == Static::query_result_cs)
        return dbQueryResult;
    if (evt == Static::batch_request_cs)
        return dbBatchRequest;
    if (evt == Static::batch_response_cs)
        return dbBatchResponse;
    return EndDBEventTypes;
}

inline const char* DBEventTypeToString(DBEventType dbet) {
    switch (dbet) {
    case dbCommand:
        return Static::command_cs;
    case dbCommandResult:
        return Static::command_result_cs;
    case dbQuery:
        return Static::query_cs;
    case dbQueryResult:
        return Static::query_result_cs;
    case dbBatchRequest:
        return Static::batch_request_cs;
    case dbBatchResponse:
        return Static::batch_response_cs;
    case EndDBEventTypes:
        return nullptr;
    }
    return nullptr;
}

inline const char* CDTToString(CacheDataType cdt) {
    switch (cdt) {
    case cdInt:
        return Static::cdt_int_cs;
    case cdFloat:
        return Static::cdt_float_cs;
    case cdBool:
        return Static::cdt_bool_cs;
    case cdStr:
        return Static::cdt_str_cs;
    case cdIntVec:
        return Static::cdt_int_vec_cs;
    case cdStrVec:
        return Static::cdt_str_vec_cs;
    case cdAny:
        return Static::cdt_any_cs;
    default:
        return nullptr;
    }
}