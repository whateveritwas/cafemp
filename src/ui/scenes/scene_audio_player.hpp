#ifndef UI_AUDIO_PLAYER_H
#define UI_AUDIO_PLAYER_H

#include <string>

void scene_audio_player_init(std::string full_path);
void scene_audio_player_render(struct nk_context *ctx);
void scene_audio_player_shutdown();

#endif
