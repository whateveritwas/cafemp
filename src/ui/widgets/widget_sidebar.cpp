#include <imgui.h>

#include "main.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"

#include "widget_sidebar.hpp"

void widget_sidebar_render() {
    ImGui::BeginChild("Sidebar", ImVec2(200.0f * UI_SCALE, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    const ImVec2 button_size(-1.0f, 64.0f * UI_SCALE);

    if (ImGui::Button("Home", button_size)) {
        app_state_set(STATE_MENU);
    }

    if (ImGui::Button("Video", button_size)) {
        app_state_set(STATE_MENU_VIDEO_FILES);
        scan_directory(MEDIA_PATH_VIDEO);
    }

    if (ImGui::Button("Audio", button_size)) {
        app_state_set(STATE_MENU_AUDIO_FILES);
        scan_directory(MEDIA_PATH_AUDIO);
    }

    if (ImGui::Button("Photo", button_size)) {
        app_state_set(STATE_MENU_IMAGE_FILES);
        scan_directory(MEDIA_PATH_PHOTO);
    }

    if (ImGui::Button("Library", button_size)) {
        app_state_set(STATE_MENU_PDF_FILES);
        scan_directory(MEDIA_PATH_PDF);
    }

    if (ImGui::Button("Settings", button_size)) {
        app_state_set(STATE_MENU_SETTINGS);
    }

    ImGui::EndChild();
}
