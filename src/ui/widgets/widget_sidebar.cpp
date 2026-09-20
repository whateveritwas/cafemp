#include "ui/widgets/widget_sidebar.hpp"

#include "main.hpp"
#include "ui/scenes/scene_file_browser.hpp"
#include "ui/widgets/widget_button_icon.hpp"
#include "utils/app_state.hpp"
#include "utils/font.hpp"
#include "utils/usb.hpp"

#include <imgui/imgui.h>

void widget_sidebar_render() {
    ImVec2 size(WIDGET_SIDEBAR_WIDTH - 16.0f, 64.0f);

    ImGui::BeginChild("Sidebar", ImVec2(WIDGET_SIDEBAR_WIDTH, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar);

    if (widget_button_icon("Home", ICON_HOME, app_state_get() == STATE_MENU, size)) {
        app_state_set(STATE_MENU);
    }

    if (widget_button_icon("Video", ICON_VIDEO, app_state_get() == STATE_MENU_FILES, size)) {
        app_state_set(STATE_MENU_FILES);
        scene_file_browser_cd(MEDIA_PATH_VIDEO);
    }

    if (widget_button_icon("Audio", ICON_AUDIO, app_state_get() == STATE_MENU_FILES, size)) {
        app_state_set(STATE_MENU_FILES);
        scene_file_browser_cd(MEDIA_PATH_AUDIO);
    }

    if (widget_button_icon("Photo", ICON_PHOTO, app_state_get() == STATE_MENU_FILES, size)) {
        app_state_set(STATE_MENU_FILES);
        scene_file_browser_cd(MEDIA_PATH_PHOTO);
    }

    if (widget_button_icon("Library", ICON_LIBRARY, app_state_get() == STATE_MENU_FILES, size)) {
        app_state_set(STATE_MENU_FILES);
        scene_file_browser_cd(MEDIA_PATH_PDF);
    }

#ifndef PLATFORM_WIIU_LEGACY
    if (usb_active_drive()) {
        if (widget_button_icon("USB Drive", ICON_USB, app_state_get() == STATE_MENU_FILES, size)) {
            app_state_set(STATE_MENU_FILES);
            scene_file_browser_cd(MEDIA_PATH_USB);
        }
    }
#endif

#ifdef DEBUG    
    if (widget_button_icon("YouTube", ICON_YOUTUBE, app_state_get() == STATE_MENU_SETTINGS, size)) {
    }

    if (widget_button_icon("Jellyfin", ICON_JELLYFIN, app_state_get() == STATE_MENU_SETTINGS, size)) {
    }

    if (widget_button_icon("Settings", ICON_SETTINGS, app_state_get() == STATE_MENU_SETTINGS, size)) {
    }
#endif
    ImGui::EndChild();
}
