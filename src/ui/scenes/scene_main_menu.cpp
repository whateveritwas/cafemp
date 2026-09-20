#include "scene_main_menu.hpp"

#include "main.hpp"
#include "ui/widgets/widget_sidebar.hpp"
#include "ui/widgets/widget_tooltip.hpp"
#include "utils/display.hpp"

#include <imgui/imgui.h>

void scene_main_menu_render() {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_get().width, display_get().height - TOOLTIP_BAR_HEIGHT), ImGuiCond_Always);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin(VERSION_STRING, nullptr, window_flags)) {
        ImGui::Columns(2, nullptr, false);

        ImGui::SetColumnWidth(0, WIDGET_SIDEBAR_WIDTH);
        widget_sidebar_render();

        ImGui::NextColumn();

        if (ImGui::BeginChild("Content", ImVec2(0, 0), true)) {
            ImGui::Text("Welcome to %s!", VERSION_STRING);
            ImGui::Separator();

            ImGui::BeginChild("changelog", ImVec2(0, 512), true);

            ImGui::TextUnformatted("What's New");
            ImGui::Spacing();

            ImGui::BulletText("Hardware video decoding for H.264 Baseline (720p @ 30 FPS)");
            ImGui::BulletText("General stability improvements");
            ImGui::BulletText("PDF and EPUB reader");
            ImGui::BulletText("Wiimote support");
            ImGui::BulletText("Pro controller support");
            ImGui::BulletText("Switch between multiple audio tracks");
            ImGui::BulletText("User interface improvements");
            ImGui::BulletText("Display embedded cover art for audio files (folder.jpg)");
            ImGui::BulletText("USB drive support (MBR/FAT32)");
            ImGui::EndChild();
        }

        ImGui::EndChild();
        ImGui::Columns(1);
    }

    ImGui::End();

    widget_tooltip_render();
}
