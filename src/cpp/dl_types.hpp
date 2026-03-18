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
        // parse cspec: atomics first
        const CacheSpecVec& atomics(atomic_cspecs[rname]);
        for (auto cit = atomics.cbegin(); cit != atomics.cend(); ++cit) {
            CacheSpecifier spec{*cit};
            CacheDataType atomic_type = atomic_cspec_types[spec];
            const char* atomic_name = atomic_cspec_names[spec];
        }


    }
    RenderMethod    rname{ EndRenderMethod };
    uint32_t        widget_id{ 0 }; // invalid ID
    const JSON& cspec;

    inline static std::array<CacheDataType, cs_end_cache_specs> atomic_cspec_types{
        cdStr,      // cs_title
        cdStr,      // cs_title_font
        cdInt,      // cs_title_font_size
        cdStr,      // cs_body_font
        cdInt,      // cs_body_font_size
        cdStr,      // cs_button_font
        cdInt,      // cs_button_font_size
        cdStr,      // cs_year_month_font
        cdInt,      // cs_year_month_font_size
        cdStr,      // cs_day_date_font
        cdInt,      // cs_day_date_font_size
        cdStr,      // cs_label
        cdStr,      // cs_text
        cdInt,      // cs_step,
        cdInt,      // cs_step_fast
        cdInt,      // cs_spinner_radius
        cdInt,      // cs_spinner_thickness
        cdInt,      // cs_flags
        cdInt,      // cs_table_flags
        cdInt,      // cs_combo_flags
        cdInt,      // cs_window_flags
        cdBool,     // cs_db,
        cdBool,     // cs_fps,
        cdBool,     // cs_demo,
        cdBool,     // cs_id_stack,
        cdFloat,    // cs_font_scale,
        cdInt,      // cs_style
        cdAny,      //.cs_cname
        cdInt,      // cs_index
        cdResultSet
    };
    
    inline static std::map<RenderMethod, CacheDataType> cname_cspec_types{
        {Combo, cdStrVec},
        {InputInt, cdInt},
        {Checkbox, cdBool},
        {DatePicker, cdIntVec},
        {LoadingModal, cdStrVec}
    };
    inline static std::map<RenderMethod, CacheDataType> index_cspec_types{
        {Combo, cdInt}
    };
    inline static  std::map<RenderMethod, CacheSpecVec> atomic_cspecs{
        {Home, {cs_title, cs_title_font, cs_title_font_size}},
        {InputInt, {cs_label, cs_step, cs_step_fast, cs_flags}},
        {Combo, {cs_label, cs_step}},
        {Checkbox, {cs_label}},
        {Text, {cs_text}},
        {Button, {cs_text}},
        {Table, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_table_flags, cs_window_flags}},
        {Footer, {cs_db, cs_fps, cs_demo, cs_id_stack, cs_font_scale, cs_style}},
        {DatePicker, {cs_year_month_font, cs_year_month_font_size,
                        cs_day_date_font, cs_day_date_font_size,
                        cs_table_flags, cs_combo_flags}},
        {DuckTableSummaryModal, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_table_flags, cs_window_flags,
                    cs_button_font, cs_button_font_size}},
        {LoadingModal, {cs_title, cs_title_font, cs_title_font_size,
                    cs_body_font, cs_body_font_size,
                    cs_spinner_thickness, cs_spinner_radius,
                    cs_window_flags}},
        {PushFont, {cs_font, cs_font_size}}
    };
};


