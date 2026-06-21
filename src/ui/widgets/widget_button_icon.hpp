#ifndef UI_WIDGET_BUTTON_ICON_H
#define UI_WIDGET_BUTTON_ICON_H

#include "vendor/ui/imgui.h"

static bool widget_button_icon(const char *label, const char *icon, bool selected, ImVec2 size) {
    ImGui::PushID(label);

    bool clicked = ImGui::Button("##sidebar_btn", size);

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImDrawList *dl = ImGui::GetWindowDrawList();

    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();

    float icon_size = size.y;

    ImU32 bg = ImGui::GetColorU32(ImGuiCol_Button);

    if (selected)
        bg = ImGui::GetColorU32(ImGuiCol_Header);
    else if (hovered)
        bg = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
    else if (active)
        bg = ImGui::GetColorU32(ImGuiCol_ButtonActive);

    dl->AddRectFilled(p0, p1, bg);

    ImVec2 icon_min = p0;
    ImVec2 icon_max = ImVec2(p0.x + icon_size, p0.y + icon_size);

    dl->AddRectFilled(icon_min, icon_max, ImGui::GetColorU32(ImGuiCol_FrameBg));

    dl->AddLine(ImVec2(p0.x + icon_size, p0.y), ImVec2(p0.x + icon_size, p1.y), ImGui::GetColorU32(ImGuiCol_Border));

    if (icon) {
        ImGui::SetWindowFontScale(2.0f);

        ImVec2 text_size = ImGui::CalcTextSize(icon);

        ImVec2 icon_pos(icon_min.x + (icon_size - text_size.x) * 0.5f, icon_min.y + (icon_size - text_size.y) * 0.5f);

        dl->AddText(icon_pos, ImGui::GetColorU32(ImGuiCol_Text), icon);

        ImGui::SetWindowFontScale(1.0f);
    } else {
        dl->AddText(ImVec2(icon_min.x + 20, icon_min.y + 20), IM_COL32(255, 0, 0, 255), "?");
    }

    float text_x = p0.x + icon_size + 12.0f;
    float text_y = p0.y + (size.y - ImGui::GetTextLineHeight()) * 0.5f;

    dl->AddText(ImVec2(text_x, text_y), ImGui::GetColorU32(ImGuiCol_Text), label);

    ImGui::PopID();

    return clicked;
}

#endif
