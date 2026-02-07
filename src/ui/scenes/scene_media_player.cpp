#include <string>
#include <vector>

#include "input/input_actions.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"
#include "utils/media_info.hpp"
#include "player/media_player.hpp"
#include "ui/widgets/widget_player_hud.hpp"
#include "ui/scenes/scene_media_player.hpp"
#include "main.hpp"

static bool show_hud = false;

void scene_media_player_init(std::string full_path) {
    media_player_init(full_path.c_str());
    media_player_play(true);
}

void scene_media_player_render(struct nk_context *ctx) {
    media_player_update();
    
    if (!media_info_get()->playback_status || show_hud) {
        widget_player_hud_render(ctx, media_info_get());
    }
}

void scene_media_player_input(InputState& input) {
    if (input_pressed(input, BTN_A)) {
        bool is_playing = media_info_get()->playback_status;
        media_player_play(!is_playing);

    } else if (input_pressed(input, BTN_B)) {
	media_player_cleanup();

	switch (app_state_get()) {
	case STATE_PLAYING_AUDIO:
	    app_state_set(STATE_MENU_AUDIO_FILES);
            scan_directory(MEDIA_PATH_AUDIO);
            break;
	case STATE_PLAYING_VIDEO:
	    app_state_set(STATE_MENU_VIDEO_FILES);
            scan_directory(MEDIA_PATH_VIDEO);
            break;
	default: break;
	}        
        
    } else if (input_pressed(input, BTN_LEFT)) {
#ifdef DEBUG
        double current_time = media_player_get_current_time();
        media_player_seek(current_time - 5.0);
#endif
        
    } else if (input_pressed(input, BTN_RIGHT)) {
#ifdef DEBUG
        double current_time = media_player_get_current_time();
        media_player_seek(current_time + 5.0);
#endif
        
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
        
    } else if (input_touched(input)) {
        show_hud = true;
    } else {
        show_hud = false;
    }
}

void scene_media_player_shutdown() {
    media_player_cleanup();
}
