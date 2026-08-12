// Local working state structs for renderables
#pragma once
#include "nd_types.hpp"
#include "dl_types.hpp"
#include "imgui.h"
#include "imgui_internal.h"

// Constraint: do not store imgui working state in these
// structs; ie GetCurrentTable(), which is inlined and
// designed to be invoked when the data is needed.
// The value is the boundary, and GetCurrentTable()
// will let you cross the boundary only when appropriate.

struct DatePickerLocals {
    YMD     old_date{ 1970, 1, 1 };   // render_date_picker
    YMD     new_date{ 1970, 1, 1 };
    int     month_index{ 0 };
    int     combo_flags{ 0 };
    float   content_width{ 0.0 };
    float   arrow_size{ 0.0 };
    float   bullet_size{ 0.0 };
    float   arrow_button_width{ 0.0 };
    float   bullet_button_width{ 0.0 };
    float   combined_width{ 0.0 };
    float   offset{ 0.0 };
    WEEK    day_array{ 0,0,0,0,0,0,0 };
};

struct SpinnerLocals {
    int     radius{ 0 };          // parameters
    int     thickness{ 0 };
    int     segments{ 30 };
    int     color{ 0 };
    ImU32   col;
    ImGuiID id;
    ImVec2  position;
    ImVec2  size;
    ImRect  bounding_box;
    int     start;
    float   a_min;
    float   a_max;
    float   a;
    ImVec2  centre;
};

struct ShadedPlotLocals {
    ImPlotSpec  spec;
    bool        show_fills{ true };
    bool        show_lines{ true };
    double      xmin_dbl{ 0.0 };
    double      xmax_dbl{ 0.0 };
    double      ymin_dbl{ 0.0 };
    double      ymax_dbl{ 0.0 };
    uint32_t    row_count{ 0 };
    uint32_t    offset{ 0 };
};

struct EndRenderLocals {
    int     old_int{ 0 };
    int*    new_int{ nullptr };
    double  old_double{ 0.0 };
    double* new_double{ nullptr };
    bool    old_bool{ false };
    bool*   new_bool{ nullptr };
    // no old_string/new_string: see render_input_string comment 
    // for strings we must invoke w->clear_buffer();
};

struct SummaryTableContext {
    DataRef*    menupop_data_ref{ nullptr };
    RSHandle    smry_handle{ 0 };   // uint64_t on win32, uint32_t on ems
    uint32_t    row_inx{ 0 };
};

struct TableContext {
    DataRef*    menupop_data_ref{ nullptr };
    RSHandle    handle{ 0 };   // uint64_t on win32, uint32_t on ems
    uint32_t    col_inx{ 0 };
    uint32_t    row_inx{ 0 };
};

struct TableMemEditContext {
    // primed by render_table if,
    // and only if, the table has a menupop,
    // and a  menuitem is selected
    // render_memory_editor working storage
    uint32_t    col_inx{ 0 };
    uint32_t    row_count{ 0 };
    uint32_t    col_count{ 0 };
    uint32_t    offset{ 0 };
    char*       data{ nullptr };
    std::size_t size{ 0 };
    std::size_t base_addr{ 0 };
};