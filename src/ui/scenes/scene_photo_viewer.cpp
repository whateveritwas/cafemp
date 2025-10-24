#include <string>
#include <vector>

#include "main.hpp"
#include "player/photo_viewer.hpp"
#include "utils/app_state.hpp"
#include "utils/media_files.hpp"
#include "utils/media_info.hpp"
#include "ui/widgets/widget_tooltip.hpp"

#include "ui/scenes/scene_photo_viewer.hpp"
#include "input/input_actions.hpp"

bool show_tooltip = false;

void scene_photo_viewer_init(std::string full_path) {
    photo_viewer_init();
    photo_viewer_open_picture(full_path.c_str());
}

void scene_photo_viewer_render(struct nk_context* ctx) {
    photo_viewer_render();
    if(show_tooltip) widget_tooltip_render(ctx);
}

void scene_photo_viewer_input(InputState& input) {
    if (input_pressed(input, BTN_B)) {
        photo_viewer_cleanup();
        app_state_set(STATE_MENU_IMAGE_FILES);
    } else if (input_touched(input)) {
        photo_viewer_pan(input.touch.move_x, input.touch.move_y);
		show_tooltip = true;
    } else if (input_pressed(input, BTN_ZL)) {
        photo_texture_zoom(0.05f);
    } else if (input_pressed(input, BTN_ZR)) {
        photo_texture_zoom(-0.05f);
    } else if(fabs(input.left_stick.x) || fabs(input.left_stick.y)) {
		photo_viewer_pan(input.left_stick.x * 10.0f, input.left_stick.y * -10.0f);
	} else if (input_pressed(input, BTN_LEFT)) {
        auto info = media_info_get();
        if (--info->current_caption_id < 0)
            info->current_caption_id = info->total_caption_count - 1;

        std::string full_path = std::string(MEDIA_PATH_PHOTO) + get_media_files()[info->current_caption_id];
        photo_viewer_open_picture(full_path.c_str());
    } else if (input_pressed(input, BTN_RIGHT)) {
        auto info = media_info_get();
        if (++info->current_caption_id >= info->total_caption_count)
            info->current_caption_id = 0;

        std::string full_path = std::string(MEDIA_PATH_PHOTO) + get_media_files()[info->current_caption_id];
        photo_viewer_open_picture(full_path.c_str());
    } else {
		show_tooltip = false;
	}
}
