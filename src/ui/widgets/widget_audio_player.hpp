#ifndef UI_AUDIO_PLAYER_H
#define UI_AUDIO_PLAYER_H

#include <string>

void widget_audio_player_init(std::string full_path);
void widget_audio_player_render(struct nk_context *ctx);
void widget_audio_player_shutdown();

#endif
