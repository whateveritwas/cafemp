#include "ui/widgets/widget_sidebar.hpp"

#include "main.hpp"
#include "ui/scenes/scene_file_browser.hpp"
#include "ui/widgets/widget_button_icon.hpp"
#include "utils/app_state.hpp"
#include "utils/font.hpp"

#include <imgui.h>

void widget_sidebar_render() {
    ImVec2 size(200.0f * display_get().scale, 64.0f * display_get().scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::BeginChild("Sidebar", ImVec2(200.0f * display_get().scale, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    if (widget_button_icon("Home", ICON_HOME, app_state_get() == STATE_MENU, size)) {
        app_state_set(STATE_MENU);
    }

    if (widget_button_icon("Video", ICON_VIDEO, app_state_get() == STATE_MENU_VIDEO_FILES, size)) {
        app_state_set(STATE_MENU_VIDEO_FILES);
        scene_file_browser_scan_directory(MEDIA_PATH_VIDEO);
    }

    if (widget_button_icon("Audio", ICON_AUDIO, app_state_get() == STATE_MENU_AUDIO_FILES, size)) {
        app_state_set(STATE_MENU_AUDIO_FILES);
        scene_file_browser_scan_directory(MEDIA_PATH_AUDIO);
    }

    if (widget_button_icon("Photo", ICON_PHOTO, app_state_get() == STATE_MENU_IMAGE_FILES, size)) {
        app_state_set(STATE_MENU_IMAGE_FILES);
        scene_file_browser_scan_directory(MEDIA_PATH_PHOTO);
    }

    if (widget_button_icon("Library", ICON_LIBRARY, app_state_get() == STATE_MENU_PDF_FILES, size)) {
        app_state_set(STATE_MENU_PDF_FILES);
        scene_file_browser_scan_directory(MEDIA_PATH_PDF);
    }

    if (widget_button_icon("Settings", ICON_SETTINGS, app_state_get() == STATE_MENU_SETTINGS, size)) {
        app_state_set(STATE_MENU_SETTINGS);
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
}
