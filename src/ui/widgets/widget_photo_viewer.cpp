#include <string>

#include "input/input.hpp"
#include "player/photo_viewer.hpp"
#include "utils/app_state.hpp"
#include "ui/widgets/widget_tooltip.hpp"

#include "ui/widgets/widget_photo_viewer.hpp"

void widget_photo_viewer_init(std::string full_path) {
    photo_viewer_init();
    photo_viewer_open_picture(full_path.c_str());
}

void widget_photo_viewer_render(struct nk_context *ctx) {
    photo_viewer_render();
    if(input_is_vpad_touched()) widget_tooltip_render(ctx);
}
