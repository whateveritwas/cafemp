#include "ui/widgets/widget_cursor.hpp"

#include "input/input_actions.hpp"
#include "input/input_device.hpp"

#include <imgui/imgui.h>

void widget_cursor_render() {
    input_device *input = input_get();
    if (!input->pointer.valid) return;

    ImDrawList *draw_list = ImGui::GetForegroundDrawList();

    ImU32 color = IM_COL32(255, 0, 0, 255);

    draw_list->AddText(nullptr, 64.0f, ImVec2(input->pointer.x, input->pointer.y), color, "\uF245");
}
