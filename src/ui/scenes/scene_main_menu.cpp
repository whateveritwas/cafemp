#include "ui/scenes/scene_main_menu.hpp"

#include "main.hpp"
#include "vendor/ui/nuklear.h"
#include "ui/widgets/widget_sidebar.hpp"
#include "ui/widgets/widget_tooltip.hpp"

void scene_main_menu_render(struct nk_context *ctx) {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
        widget_sidebar_render(ctx);

        nk_layout_row_push(ctx, SCREEN_WIDTH - (200 * UI_SCALE));
        if (nk_group_begin(ctx, "Content", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Welcome to " VERSION_STRING "!", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "What's new:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Hardware video decoding for h264 baseline 720p@30 (NO 1080p!)", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- General stability improvements", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Library for reading pdf and epub files", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Wiimote support", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Changing between multiple audio tracks in a video", NK_TEXT_LEFT);
            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    widget_tooltip_render(ctx);
}
