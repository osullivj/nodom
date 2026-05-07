#include "imgui.h"
#include "static_strings.hpp"
#include "nd_types.hpp"

const char* StyleColorToString(StyleColor col) {
    switch (col) {
    case Dark:
        return Static::dark_cs;
    case Light:
        return Static::light_cs;
    case Classic:
        return Static::classic_cs;
    default:
        return 0;
    }
}

void SetStyleColoring(int col) {
    switch (col) {
    case Dark:
        ImGui::StyleColorsDark();
        break;
    case Light:
        ImGui::StyleColorsLight();
        break;
    case Classic:
        ImGui::StyleColorsClassic();
        break;
    }
}
