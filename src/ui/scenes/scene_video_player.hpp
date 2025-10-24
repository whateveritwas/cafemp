#ifndef UI_VIDEO_PLAYER_H
#define UI_VIDEO_PLAYER_H

#include <string>

void scene_video_player_init(std::string full_path);
void scene_video_player_render(struct nk_context *ctx);
void scene_video_player_shutdown();

#endif
