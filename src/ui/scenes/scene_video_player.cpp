#include <string>

#include "input/input_actions.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"
#include "utils/media_info.hpp"
#include "player/audio_player.hpp"
#include "player/video_player.hpp"
#include "ui/widgets/widget_player_hud.hpp"

#include "ui/scenes/scene_video_player.hpp"
#include "input/input_manager.hpp"

bool show_hud = false;

void scene_video_player_init(std::string full_path) {
	video_player_init(full_path.c_str());
    video_player_play(true);
}

void scene_video_player_render(struct nk_context *ctx) {
    video_player_update();

    if (!media_info_get()->playback_status || show_hud) {
        widget_player_hud_render(ctx, media_info_get());
    }
}

void scene_video_player_input(InputState& input) {
    if (input_pressed(input, BTN_A)) {
        video_player_play(!media_info_get()->playback_status);
    } else if (input_pressed(input, BTN_B)) {
        video_player_cleanup();
        app_state_set(STATE_MENU_VIDEO_FILES);
        scan_directory(MEDIA_PATH_VIDEO);
    } else if (input_pressed(input, BTN_LEFT)) {
#ifdef DEBUG
//        video_player_seek(-5.0f);
#endif
    } else if (input_pressed(input, BTN_RIGHT)) {
#ifdef DEBUG
//        video_player_seek(5.0f);
#endif
    } else if (input_pressed(input, BTN_X)) {
        if (media_info_get()->total_audio_track_count == 1) return;

        video_player_play(false);
        audio_player_play(false);

        media_info_get()->current_audio_track_id++;
        if (media_info_get()->current_audio_track_id > media_info_get()->total_audio_track_count)
            media_info_get()->current_audio_track_id = 1;

        audio_player_switch_audio_stream(media_info_get()->current_audio_track_id);

        video_player_play(true);
        audio_player_play(true);
    } else if (input_touched(input)) {
		show_hud = true;
	} else {
		show_hud = false;
	}
}

void scene_video_player_shutdown() {
	video_player_cleanup();
}
