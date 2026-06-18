#include "input/input_actions.hpp"

#include "ui/widgets/widget_cursor.hpp"
#include "vendor/ui/imgui.h"

void widget_cursor_render(InputState &state) {    
    if (!state.valid_cursor) return;    

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();

    ImU32 color = IM_COL32(255, 0, 0, 255);
    
    draw_list->AddText(nullptr, 64.0f, ImVec2(state.cursor_position.x, state.cursor_position.y), color, "\uF245");
}
