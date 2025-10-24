#ifndef UI_PHOTO_VIEWER_H
#define UI_PHOTO_VIEWER_H

#include <string>
#include "input/input_actions.hpp"

void scene_photo_viewer_init(std::string full_path);
void scene_photo_viewer_render(struct nk_context* ctx);
void scene_photo_viewer_input(InputState& input);

#endif
