#include "ui/scenes/scene_file_browser.hpp"

#include "input/input_actions.hpp"
#include "logger/logger.hpp"
#include "main.hpp"
#include "ui/widgets/widget_button_icon.hpp"
#include "ui/widgets/widget_sidebar.hpp"
#include "ui/widgets/widget_tooltip.hpp"
#include "utils/app_state.hpp"
#include "utils/display.hpp"
#include "utils/font.hpp"
#include "utils/media_info.hpp"

#include <dirent.h>
#include <imgui/imgui.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum file_types { FILE_FOLDER, FILE_AUDIO, FILE_VIDEO, FILE_IMAGE, FILE_BOOK };

static const std::unordered_map<file_types, const char *> file_icons = {{FILE_FOLDER, ICON_FOLDER}, {FILE_AUDIO, ICON_AUDIO}, {FILE_VIDEO, ICON_VIDEO}, {FILE_IMAGE, ICON_PHOTO}, {FILE_BOOK, ICON_LIBRARY}};

static const std::unordered_set<std::string> valid_video_endings = {"mp4", "mov", "avi", "mkv"};
static const std::unordered_set<std::string> valid_audio_endings = {"mp3", "wav", "ogg", "flac", "aac"};
static const std::unordered_set<std::string> valid_image_endings = {"png", "jpg", "gif", "tga", "bmp"};
static const std::unordered_set<std::string> valid_pdf_endings = {"pdf", "epub", "cbz"};

struct file {
    std::string path;
    file_types file_type;
};

static std::vector<file> files;

static std::string media_root;
static std::string relative_dir;

struct MediaMapping {
    const char *folder;
    char type;
};

static const std::unordered_map<int, MediaMapping> state_map = {
    {STATE_MENU_AUDIO_FILES, {"Audio/", 'A'}},
    {STATE_MENU_VIDEO_FILES, {"Video/", 'V'}},
    {STATE_MENU_IMAGE_FILES, {"Photo/", 'P'}},
    {STATE_MENU_PDF_FILES, {"Library/", 'L'}},
};

static std::string get_extension(const std::string &filename) {
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos || dot == filename.size() - 1) return "";
    return filename.substr(dot + 1);
}

static bool scene_file_browser_valid_file_ending(const std::string &file_ending) {
    switch (app_state_get()) {
        case STATE_MENU_VIDEO_FILES:
            return valid_video_endings.count(file_ending) > 0;
        case STATE_MENU_AUDIO_FILES:
            return valid_audio_endings.count(file_ending) > 0;
        case STATE_MENU_IMAGE_FILES:
            return valid_image_endings.count(file_ending) > 0;
        case STATE_MENU_PDF_FILES:
            return valid_pdf_endings.count(file_ending) > 0;
        default:
            return false;
    }
}

static file_types file_type_for_extension(const std::string &ext) {
    if (valid_video_endings.count(ext)) return FILE_VIDEO;
    if (valid_audio_endings.count(ext)) return FILE_AUDIO;
    if (valid_image_endings.count(ext)) return FILE_IMAGE;
    if (valid_pdf_endings.count(ext)) return FILE_BOOK;
    return FILE_FOLDER;
}

static std::string join_relative(const std::string &base, const std::string &name) {
    if (base.empty()) return name;
    return base + "/" + name;
}

static std::string parent_relative(const std::string &base) {
    size_t slash = base.find_last_of('/');
    if (slash == std::string::npos) return "";
    return base.substr(0, slash);
}

static void start_file(const std::string &filename) {
    int state = app_state_get();
    auto it = state_map.find(state);
    if (it == state_map.end()) {
        log_message(LOG_ERROR, "Menu", "Unsupported file type");
        return;
    }
    const MediaMapping &mapping = it->second;

    auto new_info = std::make_unique<media_info>();
    media_info_set(std::move(new_info));
    media_info *info = media_info_get();

    std::string rel_file = join_relative(relative_dir, filename);

    info->type = mapping.type;
    info->path = media_root + rel_file;
    info->filename = filename;
    info->current_playback_time = 0;

    if (mapping.type == 'P' || mapping.type == 'L') {
        info->current_audio_track_id = 0;
        info->total_audio_track_count = 0;
        info->current_caption_id = 0;
        info->total_caption_count = 0;
    } else {
        info->current_audio_track_id = 1;
        info->total_audio_track_count = 1;
        info->current_caption_id = 1;
        info->total_caption_count = 1;
    }

    switch (mapping.type) {
        case 'A':
            app_state_set(STATE_PLAYING_AUDIO);
            break;
        case 'V':
            app_state_set(STATE_PLAYING_VIDEO);
            break;
        case 'P':
            app_state_set(STATE_VIEWING_PHOTO);
            break;
        case 'L':
            app_state_set(STATE_VIEWING_PDF);
            break;
    }
}

static void scan_relative_directory(const std::string &new_relative_dir) {
    std::string abs_path = media_root + new_relative_dir;

    log_message(LOG_OK, "File Browser", "Opening folder %s\n", abs_path.c_str());

    DIR *dir = opendir(abs_path.c_str());
    if (!dir) {
        log_message(LOG_ERROR, "File Browser", "Failed to open folder %s\n", abs_path.c_str());
        return;
    }

    files.clear();
    relative_dir = new_relative_dir;

    if (!relative_dir.empty()) {
        files.push_back({"..", FILE_FOLDER});
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name = ent->d_name;

        if (name == "." || name == "..") continue;

        if (ent->d_type == DT_DIR) {
            files.push_back({name, FILE_FOLDER});
            continue;
        }

        std::string ext = get_extension(name);
        if (!scene_file_browser_valid_file_ending(ext)) continue;

        files.push_back({name, file_type_for_extension(ext)});
    }

    closedir(dir);
}

void scene_file_browser_open(int state) {
    auto it = state_map.find(state);
    if (it == state_map.end()) {
        log_message(LOG_ERROR, "File Browser", "Unsupported file type");
        return;
    }
    media_root = std::string(BASE_PATH) + it->second.folder;
    scan_relative_directory("");
}

void scene_file_browser_scan_directory(const char *path) {
    media_root = path;
    scan_relative_directory("");
}

void scene_file_browser_go_up() {
    if (relative_dir.empty()) return;
    scan_relative_directory(parent_relative(relative_dir));
}

void scene_file_browser_input(InputState &input) {
    if (input_pressed(input, BTN_B)) {
        scene_file_browser_go_up();
    }

//   if (input_pressed(input, BTN_X)) {
//	DIR *dir = opendir("usb:/");
//	struct dirent *ent;
//	
//	while ((ent = readdir(dir)) != nullptr) {
//	    printf("%s\n", ent->d_name);
//	}
//	
//	closedir(dir);
//    }
}

void scene_file_browser_render() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin(VERSION_STRING, nullptr, window_flags)) {
        ImGui::SetWindowPos(ImVec2(0, 0));
        ImGui::SetWindowSize(ImVec2(display_get().width, display_get().height - TOOLTIP_BAR_HEIGHT * display_get().scale));

        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 200.0f * display_get().scale);
        widget_sidebar_render();
        ImGui::NextColumn();

        ImGui::BeginChild("FileList", ImVec2(0, 0), true, ImGuiWindowFlags_None);

        for (const file &f : files) {
            if (widget_button_icon(f.path.c_str(), file_icons.at(f.file_type), false, ImVec2(-1, 64 * display_get().scale))) {
                if (f.file_type == FILE_FOLDER) {
                    if (f.path == "..") {
                        scene_file_browser_go_up();
                    } else {
                        scan_relative_directory(join_relative(relative_dir, f.path));
                    }
                } else {
                    start_file(f.path);
                }
            }
        }

        ImGui::EndChild();
        ImGui::Columns(1);
        ImGui::End();
    }

    widget_tooltip_render();
}
