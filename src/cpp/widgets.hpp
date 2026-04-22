// Extension GUI widgets: widgets not implemented
// by imgui. They may or may not be exposed directly
// as NDContext renderables. For example, the Spinner
// widget.
#pragma once
#include <math.h>
#include "locals.hpp"

bool Spinner(const char* label, SpinnerLocals& sp_vars) {
    sp_vars.col = ImGui::GetColorU32(sp_vars.color);

    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) {    // not visible
        return false;
    }
    ImGuiContext& g = *GImGui;
    ImGuiStyle& style = ImGui::GetStyle();

    sp_vars.id = window->GetID(label);

    sp_vars.position = window->DC.CursorPos;
    sp_vars.size.x = sp_vars.radius * 2.0;
    sp_vars.size.y = (sp_vars.radius + style.FramePadding.y) * 2.0;
    sp_vars.bounding_box.Min = sp_vars.position;
    sp_vars.bounding_box.Max.x = sp_vars.position.x + sp_vars.size.x;
    sp_vars.bounding_box.Max.y = sp_vars.position.y + sp_vars.size.y;
    ImGui::ItemSize(sp_vars.bounding_box, style.FramePadding.y);
    if (!ImGui::ItemAdd(sp_vars.bounding_box, sp_vars.id)) {
        printf("spinner: ItemAdd failed\n");
        return false;
    }

    window->DrawList->PathClear();

    sp_vars.start = (int)abs(sinf((float)g.Time * 1.8f) * (sp_vars.segments - 5));
    sp_vars.a_min = IM_PI * 2.0f * ((float)sp_vars.start) / (float)sp_vars.segments;
    sp_vars.a_max = IM_PI * 2.0f * ((float)sp_vars.segments - 3) / (float)sp_vars.segments;
    sp_vars.centre.x = sp_vars.position.x + sp_vars.radius;
    sp_vars.centre.y = sp_vars.position.y + sp_vars.radius + style.FramePadding.y;

    for (int i = 0; i < sp_vars.segments; i++) {
        sp_vars.a = sp_vars.a_min + ((float)i / (float)sp_vars.segments) * (sp_vars.a_max - sp_vars.a_min);
        // reuse sp_vars.position as it's job as an intermediate result is done
        sp_vars.position.x = sp_vars.centre.x + cosf(sp_vars.a + (float)g.Time * 8) * sp_vars.radius;
        sp_vars.position.y = sp_vars.centre.y + sinf(sp_vars.a + (float)g.Time * 8) * sp_vars.radius;
        window->DrawList->PathLineTo(sp_vars.position);
    }
    window->DrawList->PathStroke(sp_vars.col, 0, (float)sp_vars.thickness);
    return true;
}