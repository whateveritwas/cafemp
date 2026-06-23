#include "ui/scenes/scene_media_player.hpp"

#include "input/input_actions.hpp"
#include "main.hpp"
#include "player/media_player.hpp"
#include "player/photo_viewer.hpp"
#include "ui/scenes/scene_file_browser.hpp"
#include "ui/widgets/widget_player_hud.hpp"
#include "utils/app_state.hpp"
#include "utils/media_info.hpp"

#include <imgui/imgui.h>
#include <string>
#include <vector>

static bool show_hud = false;

ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground;

void scene_media_player_init(std::string full_path) {
    if (media_info_get()->type == 'A') {
        std::string cover_path = full_path.substr(0, full_path.find_last_of('/') + 1) + "folder.jpg";

        photo_viewer_init();
        photo_viewer_open_picture(cover_path.c_str());
    }

    media_player_init(full_path.c_str());
    media_player_play(true);
}

void scene_media_player_render() {
    media_player_update();

    if (media_info_get()->type == 'A') {
        photo_texture_zoom(0.0f);
        photo_viewer_pan(0, 0);
        photo_viewer_render();
    }

    if (!media_info_get()->playback_status || show_hud) {
        widget_player_hud_render(media_info_get());
    }
}

void scene_media_player_input(InputState &input) {
    if (input_pressed(input, BTN_A)) {
        bool is_playing = media_info_get()->playback_status;
        media_player_play(!is_playing);
    } else if (input_pressed(input, BTN_B)) {
        media_player_cleanup();
	app_state_set(STATE_MENU_FILES);
    } else if (input_pressed(input, BTN_LEFT)) {
        double current_time = media_player_get_current_time();
        media_player_seek(current_time - 5.0);
    } else if (input_pressed(input, BTN_RIGHT)) {
        double current_time = media_player_get_current_time();
        media_player_seek(current_time + 5.0);
    } else if (input_pressed(input, BTN_X)) {
        if (media_info_get()->total_audio_track_count <= 1) return;

        std::vector<AudioTrackInfo> tracks = media_player_get_audio_tracks();
        if (tracks.empty()) return;

        int current_track = media_player_get_current_audio_track();
        int current_index = -1;

        for (size_t i = 0; i < tracks.size(); ++i) {
            if (tracks[i].stream_index == current_track) {
                current_index = i;
                break;
            }
        }

        int next_index = (current_index + 1) % tracks.size();
        int next_stream_index = tracks[next_index].stream_index;

        if (media_player_switch_audio_track(next_stream_index)) {
            media_info_get()->current_audio_track_id = next_index + 1;
        }
    } else if (input_touched(input) || input.valid_cursor) {
        show_hud = true;
    } else {
        show_hud = false;
    }
}

void scene_media_player_shutdown() {
    photo_viewer_cleanup();
    media_player_cleanup();
}
