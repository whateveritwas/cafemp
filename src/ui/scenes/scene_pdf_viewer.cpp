#include "ui/scenes/scene_pdf_viewer.hpp"

#include "input/input_actions.hpp"
#include "input/input_device.hpp"
#include "player/pdf_viewer.hpp"
#include "ui/widgets/widget_tooltip.hpp"
#include "utils/app_state.hpp"

#include <math.h>
#include <string>

bool pdf_viewer_show_tooltip = false;

void scene_pdf_viewer_init(std::string full_path) {
    pdf_viewer_init();
    pdf_viewer_open_file(full_path.c_str());
}

void scene_pdf_viewer_render() {
    pdf_viewer_render();
    if (pdf_viewer_show_tooltip)
        widget_tooltip_render();
}

void scene_pdf_viewer_input() {
    input_device *input = input_get();

    if (input_pressed(BUTTON_B)) {
        pdf_viewer_cleanup();
        app_state_set(STATE_MENU_FILES);
    } else if (input_held(BUTTON_TOUCH)) {
        pdf_viewer_pan(input->pointer.delta_x, input->pointer.delta_y);
        pdf_viewer_show_tooltip = true;
    } else if (input_held(BUTTON_ZL)) {
        pdf_texture_zoom(0.05f);
    } else if (input_held(BUTTON_ZR)) {
        pdf_texture_zoom(-0.05f);
    } else if (input_pressed(BUTTON_UP)) {
        pdf_viewer_prev_page();
    } else if (input_pressed(BUTTON_DOWN)) {
        pdf_viewer_next_page();
    } else {
        pdf_viewer_show_tooltip = false;
    }
}
