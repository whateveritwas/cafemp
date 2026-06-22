#ifndef UI_VIDEO_PLAYER_HPP
#define UI_VIDEO_PLAYER_HPP

#include "input/input_actions.hpp"

#include <string>

void scene_media_player_init(std::string full_path);
void scene_media_player_render();
void scene_media_player_input(InputState &input);
void scene_media_player_shutdown();

#endif
