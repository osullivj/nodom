#include "imgui.h"
#include "static_strings.hpp"
#include "nd_types.hpp"
#include <filesystem>

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

void save_ini_to_file(const char* fpath) {
    if (fpath != nullptr) {
        ImGui::SaveIniSettingsToDisk(fpath);
    }
}

void load_ini_from_file(const char* fpath) {
    if (fpath != nullptr) {
        std::string file_path_s{ fpath };
        std::filesystem::path file_path{ file_path_s };
        if (std::filesystem::exists(file_path)) {
            ImGui::LoadIniSettingsFromDisk(fpath);
        }
    }
}


void save_ini_to_memory() {

}


