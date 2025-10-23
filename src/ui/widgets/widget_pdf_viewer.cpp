#include <string>

#include "input/input.hpp"
#include "player/pdf_viewer.hpp"
#include "utils/app_state.hpp"
#include "ui/widgets/widget_tooltip.hpp"

#include "ui/widgets/widget_pdf_viewer.hpp"

void widget_pdf_viewer_init(std::string full_path) {
    pdf_viewer_init();
}

void widget_pdf_viewer_render(struct nk_context *ctx) {
    pdf_viewer_render();
    if(input_is_vpad_touched()) widget_tooltip_render(ctx);
}
