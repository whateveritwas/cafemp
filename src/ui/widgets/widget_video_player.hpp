#ifndef UI_VIDEO_PLAYER_H
#define UI_VIDEO_PLAYER_H

#include <string>

void widget_video_player_init(std::string full_path);
void widget_video_player_render(struct nk_context *ctx);
void widget_video_player_shutdown();

#endif
