#include <string>

#include "utils/media_info.hpp"
#include "player/audio_player.hpp"
#include "ui/widgets/widget_player_hud.hpp"

#include "ui/scenes/scene_audio_player.hpp"

void scene_audio_player_init(std::string full_path) {
	audio_player_init(full_path.c_str());
    audio_player_play(true);
}

void scene_audio_player_render(struct nk_context *ctx) {
    if (media_info_get()->total_audio_playback_time == 0) media_info_get()->total_audio_playback_time = audio_player_get_total_play_time();
    media_info_get()->current_audio_playback_time = audio_player_get_current_play_time();

    widget_player_hud_render(ctx, media_info_get());
}

void scene_audio_player_shutdown() {
	audio_player_cleanup();
}
