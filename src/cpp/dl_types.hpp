#pragma once
#include "json_ops.hpp"

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


// JSON aware types that build on JSON ops and ND types
template <typename JSON>
struct NDWidget {
    NDWidget() = delete;
    NDWidget(StringVec& entity_ids, StringIntMap& entity_map, const JSON& w)
        :widget_id(extract_entity_id<JSON>(entity_ids, entity_map, w, Static::widget_id_cs)),
        rname(extract_render_name<JSON>(w)),
        cspec(extract_cspec<JSON>(w))
    {
        // TODO: parse cspec

    }
    RenderMethod    rname{ EndRenderMethod };
    uint32_t        widget_id{ 0 }; // invalid ID
    const JSON& cspec;
};


