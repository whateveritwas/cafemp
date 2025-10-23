
#ifdef DEBUG
#include "shader/easter_egg.hpp"
#endif
#include "main.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"
#include "vendor/ui/nuklear.h"

#include "widget_sidebar.hpp"

void widget_sidebar_render(struct nk_context *ctx) {
    nk_layout_row_push(ctx, 200 * UI_SCALE);
    if (nk_group_begin(ctx, "Sidebar", NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 64 * UI_SCALE, 1);

        if (nk_button_label(ctx, "Home")) {
            app_state_set(STATE_MENU);
        }
        /*
        if (nk_button_label(ctx, "Local Media")) {
            app_state_set(STATE_MENU_VIDEO_FILES);
        }
        */
        if (nk_button_label(ctx, "Video")) {
            app_state_set(STATE_MENU_VIDEO_FILES);
            scan_directory(MEDIA_PATH_VIDEO);
        }
        /*
        if (nk_button_label(ctx, "YouTube")) {
            app_state_set(STATE_MENU_VIDEO_FILES);
        }
        */
        if (nk_button_label(ctx, "Audio")) {
            app_state_set(STATE_MENU_AUDIO_FILES);
            scan_directory(MEDIA_PATH_AUDIO);
        }
        /*
        if (nk_button_label(ctx, "Internet Radio")) {
            app_state_set(STATE_MENU_AUDIO_FILES);
        }
        */
        if (nk_button_label(ctx, "Photo")) {
            app_state_set(STATE_MENU_IMAGE_FILES);
            scan_directory(MEDIA_PATH_PHOTO);
        }
        
        if (nk_button_label(ctx, "Library")) {
            app_state_set(STATE_MENU_PDF_FILES);
            scan_directory(MEDIA_PATH_PDF);
        }

#ifdef DEBUG
        if (nk_button_label(ctx, "Easter Egg")) {
            app_state_set(STATE_MENU_EASTER_EGG);
            easter_egg_init();

        }
#endif
        if (nk_button_label(ctx, "Settings")) {
            app_state_set(STATE_MENU_SETTINGS);
        }

        nk_group_end(ctx);
    }
}
