#include <string>

#include "utils/media_info.hpp"
#include "player/video_player.hpp"
#include "ui/widgets/widget_player_hud.hpp"

#include "ui/scenes/scene_video_player.hpp"
#include "input/input_manager.hpp"

void scene_video_player_init(std::string full_path) {
	video_player_init(full_path.c_str());
    video_player_play(true);
}

void scene_video_player_render(struct nk_context *ctx) {
    video_player_update();

    if (!media_info_get()->playback_status || input_is_vpad_touched()) {
        widget_player_hud_render(ctx, media_info_get());
    }
}

void scene_video_player_shutdown() {
	video_player_cleanup();
}
