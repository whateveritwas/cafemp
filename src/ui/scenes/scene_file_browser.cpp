#include <string>
#include <vector>

#include "main.hpp"
#include "ui/widgets/widget_tooltip.hpp"
#include "ui/widgets/widget_sidebar.hpp"
#include "utils/utils.hpp"
#include "utils/media_files.hpp"
#include "vendor/ui/nuklear.h"

#include "ui/scenes/scene_file_browser.hpp"

int selected_index = 0;

void scene_file_browser_render(struct nk_context *ctx) {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
        widget_sidebar_render(ctx);

        nk_layout_row_push(ctx, SCREEN_WIDTH - (200 * UI_SCALE));

        if (nk_group_begin(ctx, "FileList", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 64 * UI_SCALE, 1);

            int total_file_count = static_cast<int>(get_media_files().size());

            for (int i = 0; i < total_file_count; ++i) {
                std::string display_str = truncate_filename(get_media_files()[i], 100);
                struct nk_style_button button_style = ctx->style.button;

                if (i == selected_index) {
                    ctx->style.button.border_color = nk_rgb(255, 172, 28);
                    ctx->style.button.border = 4.0f;
                }

                if (nk_button_label(ctx, display_str.c_str())) {
                    selected_index = i;
                    start_file(selected_index);
                }

                ctx->style.button = button_style;
            }

            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    widget_tooltip_render(ctx);
}
