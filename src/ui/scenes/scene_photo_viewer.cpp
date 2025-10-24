#include <string>

#include "player/photo_viewer.hpp"
#include "utils/app_state.hpp"
#include "ui/widgets/widget_tooltip.hpp"

#include "ui/scenes/scene_photo_viewer.hpp"
#include "input/input_manager.hpp"

void scene_photo_viewer_init(std::string full_path) {
    photo_viewer_init();
    photo_viewer_open_picture(full_path.c_str());
}

void scene_photo_viewer_render(struct nk_context *ctx) {
    photo_viewer_render();
    if(input_is_vpad_touched()) widget_tooltip_render(ctx);
}
