#include "scene_main_menu.hpp"

#include "main.hpp"
#include "ui/widgets/widget_sidebar.hpp"
#include "ui/widgets/widget_tooltip.hpp"
#include "utils/display.hpp"

#include <imgui/imgui.h>

void scene_main_menu_render() {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_get().width, display_get().height - TOOLTIP_BAR_HEIGHT * display_get().scale), ImGuiCond_Always);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin(VERSION_STRING, nullptr, window_flags)) {
        ImGui::Columns(2, nullptr, false);

        ImGui::SetColumnWidth(0, 200.0f * display_get().scale);
        widget_sidebar_render();

        ImGui::NextColumn();

        if (ImGui::BeginChild("Content", ImVec2(0, 0), true)) {
            ImGui::Text("Welcome to %s!", VERSION_STRING);

            ImGui::Spacing();
            ImGui::Text("What's new:");
            ImGui::Spacing();

            ImGui::Text("- Hardware video decoding for h264 baseline 720p@30 (NO 1080p!)");
            ImGui::Text("- General stability improvements");
            ImGui::Text("- Library for reading pdf and epub files");
            ImGui::Text("- Wiimote support");
            ImGui::Text("- Changing between multiple audio tracks in a video");
            ImGui::Text("- New Ui");
            ImGui::Text("- Cover ard displayed when playing audio files (if present)");
        }
        ImGui::EndChild();

        ImGui::Columns(1);
    }

    ImGui::End();

    widget_tooltip_render();
}
