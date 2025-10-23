#include "widget_tooltip.hpp"

#include "main.hpp"
#include "utils/app_state.hpp"
#include "vendor/ui/nuklear.h"


void widget_tooltip_render(struct nk_context *ctx) {
    if (nk_begin(ctx, "tooltip_bar", nk_rect(0, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, SCREEN_WIDTH, TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND)) {
        switch(app_state_get()) {
        case STATE_MENU: 
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(Left Stick) Select | (A) Open", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_FILES:
        case STATE_MENU_NETWORK_FILES:
        case STATE_MENU_VIDEO_FILES:
        case STATE_MENU_AUDIO_FILES:
        case STATE_MENU_IMAGE_FILES: 
        case STATE_MENU_PDF_FILES:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(Left Stick) Select | (A) Open | (-) Scan", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_SETTINGS: 
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 1);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_EASTER_EGG: break;
        case STATE_PLAYING_VIDEO: break;
        case STATE_PLAYING_AUDIO: break;
        case STATE_VIEWING_PHOTO:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 1);
            nk_label(ctx, "(Left Stick) Change Photo | [ZL] Zoom + | [ZR] Zoom - | (Touch) Pan", NK_TEXT_LEFT);
            break;
        case STATE_VIEWING_PDF:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 1);
            nk_label(ctx, "(Left Stick) Change Page | [ZL] Zoom + | [ZR] Zoom - | (Touch) Pan", NK_TEXT_LEFT);
            break;
        }

        nk_end(ctx);
    }
}
