#include <string>

#include "player/pdf_viewer.hpp"
#include "utils/app_state.hpp"
#include "ui/widgets/widget_tooltip.hpp"

#include "ui/scenes/scene_pdf_viewer.hpp"

bool pdf_viewer_show_tooltip = false;

void scene_pdf_viewer_init(std::string full_path) {
    pdf_viewer_init();
	pdf_viewer_open_file(full_path.c_str());
}

void scene_pdf_viewer_render(struct nk_context *ctx) {
    pdf_viewer_render();
	if(pdf_viewer_show_tooltip) widget_tooltip_render(ctx);
}

void scene_pdf_viewer_input(InputState& input) {
    if (input_pressed(input, BTN_B)) {
        pdf_viewer_cleanup();
        app_state_set(STATE_MENU_PDF_FILES);
    } else if (input_touched(input)) {
//        pdf_viewer_pan(input.touch.x - input.touch.old_x, input.touch.y - input.touch.old_y);
		pdf_viewer_show_tooltip = true;
    } else if (input_held(input, BTN_ZL)) {
//        pdf_texture_zoom(0.05f);
    } else if (input_held(input, BTN_ZR)) {
//        pdf_texture_zoom(-0.05f);
    } else if(fabs(input.left_stick.x) || fabs(input.left_stick.y)) {
//		pdf_viewer_pan(input.left_stick.x * 10.0f, input.left_stick.y * -10.0f);
	} else if (input_pressed(input, BTN_UP)) {
        pdf_viewer_prev_page();
    } else if (input_pressed(input, BTN_DOWN)) {
        pdf_viewer_next_page();
    } else {
		pdf_viewer_show_tooltip = false;
	}
}
