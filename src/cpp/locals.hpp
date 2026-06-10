// Local working state structs for renderables
#pragma once
#include "nd_types.hpp"

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