#include <dirent.h>
#include <unordered_map>

#include "utils/app_state.hpp"
#include "vendor/ui/imgui.h"
#include "main.hpp"
#include "ui/widgets/widget_button_icon.hpp"
#include "ui/widgets/widget_tooltip.hpp"
#include "ui/widgets/widget_sidebar.hpp"
#include "utils/utils.hpp"
#include "utils/media_files.hpp"
#include "logger/logger.hpp"
#include "utils/media_info.hpp"
#include "utils/media_files.hpp"

#include "ui/scenes/scene_file_browser.hpp"

static void start_file(int index) {
    const std::string filename = get_media_files()[index];
    std::string full_path;

    auto new_info = std::make_unique<media_info>();
    media_info_set(std::move(new_info));

    media_info* info = media_info_get();
    info->type = '\0';
    info->path = "";
    info->filename = filename;
    info->current_video_playback_time = 0;
    info->current_audio_playback_time = 0;
    info->current_audio_track_id = 1;
    info->total_audio_track_count = 1;
    info->current_caption_id = 1;
    info->total_caption_count = 1;

    struct MediaMapping {
        const char* folder;
        char type;
    };

    static const std::unordered_map<int, MediaMapping> state_map = {
        {STATE_MENU_AUDIO_FILES, {"Audio/", 'A'}},
        {STATE_MENU_VIDEO_FILES, {"Video/", 'V'}},
        {STATE_MENU_IMAGE_FILES, {"Photo/", 'P'}},
        {STATE_MENU_PDF_FILES, {"Library/", 'L'}}
    };

    int state = app_state_get();
    auto it = state_map.find(state);
    if (it == state_map.end()) {
        log_message(LOG_ERROR, "Menu", "Unsupported file type");
        return;
    }

    const MediaMapping& mapping = it->second;
    full_path = std::string(BASE_PATH) + mapping.folder + filename;

    info->type = mapping.type;
    info->path = full_path;

    if (mapping.type == 'P' || mapping.type == 'L') {
        info->current_audio_track_id = 0;
        info->total_audio_track_count = 0;
        info->current_caption_id = index;
        info->total_caption_count = get_media_files().size();
    }

    switch (mapping.type) {
    case 'A': app_state_set(STATE_PLAYING_AUDIO); break;
    case 'V': app_state_set(STATE_PLAYING_VIDEO); break;
    case 'P': app_state_set(STATE_VIEWING_PHOTO); break;
    case 'L': app_state_set(STATE_VIEWING_PDF); break;
    }
}

static bool scene_file_browser_valid_file_ending(const std::string& file_ending) {
    AppState state = app_state_get();
    bool result = false;

    switch (state) {
    case STATE_MENU_VIDEO_FILES:
	result = valid_video_endings.count(file_ending) > 0;
	break;
    case STATE_MENU_AUDIO_FILES:
	result = valid_audio_endings.count(file_ending) > 0;
	break;
    case STATE_MENU_IMAGE_FILES:
	result = valid_image_endings.count(file_ending) > 0;
	break;
    case STATE_MENU_PDF_FILES:
	result = valid_pdf_ending.count(file_ending) > 0;
	break;
    default:
	break;
    }
    return result;
}

void scene_file_browser_scan_directory(const char* path) {
    clear_media_files();
    log_message(LOG_OK, "File Browser", "Opening folder %s\n", path);
    
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name(ent->d_name);
	add_media_file(name);

    }    
    closedir(dir);
}

void scene_file_browser_render() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;
    
    if (ImGui::Begin(VERSION_STRING, nullptr, window_flags)) {
	ImGui::SetWindowPos(ImVec2(0, 0));
	ImGui::SetWindowSize(ImVec2(SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE));

	ImGui::Columns(2, nullptr, false);

	ImGui::SetColumnWidth(0, 200.0f * UI_SCALE);
	widget_sidebar_render();

	ImGui::NextColumn();

	ImGui::BeginChild("FileList", ImVec2(0, 0), true, ImGuiWindowFlags_None);

	int total_file_count = static_cast<int>(get_media_files().size());

	for (int i = 0; i < total_file_count; ++i) {
	    std::string display_str = truncate_filename(get_media_files()[i], 100);

            if (widget_button_icon(display_str.c_str(), "\uf15b", false, ImVec2(-1, 64 * UI_SCALE))) {
		start_file(i);
            }
	}

	ImGui::EndChild();

	ImGui::Columns(1);

	ImGui::End();
    }

    widget_tooltip_render();
}
