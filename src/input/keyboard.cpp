#include "input/keyboard.hpp"

#include "vendor/ui/nuklear.h"
#include <string>
#include <cstring>

static bool g_keyboard_open = false;
static bool g_keyboard_done = false;
static std::string g_result;

static char g_buffer[256];
static int g_len = 0;

void keyboard_open(const char* initial) {
    g_keyboard_open = true;
    g_keyboard_done = false;
    g_result.clear();
    g_len = 0;

    if (initial) {
        strncpy(g_buffer, initial, sizeof(g_buffer) - 1);
        g_buffer[sizeof(g_buffer) - 1] = '\0';
        g_len = strlen(g_buffer);
    } else {
        g_buffer[0] = '\0';
    }
}

bool keyboard_is_open() {
    return g_keyboard_open;
}

std::string keyboard_get_result() {
    if (g_keyboard_done) {
        g_keyboard_done = false;
        return g_result;
    }
    return {};
}

void keyboard_update(struct nk_context* ctx) {
    if (!g_keyboard_open) return;

    if (nk_begin(ctx, "Keyboard", nk_rect(0, 0, 1280, 720),
        NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {

        // darken background
        struct nk_color bg = nk_rgba(0, 0, 0, 180);
        nk_fill_rect(nk_window_get_canvas(ctx), nk_rect(0,0,1280,720), 0, bg);

        // text preview
        nk_layout_row_dynamic(ctx, 40, 1);
        nk_label(ctx, g_buffer, NK_TEXT_LEFT);

        static const char* rows[] = {
            "1234567890",
            "QWERTYUIOP",
            "ASDFGHJKL",
            "ZXCVBNM"
        };

        // draw rows of keys
        for (int r = 0; r < (int)(sizeof(rows)/sizeof(rows[0])); r++) {
            nk_layout_row_dynamic(ctx, 40, (int)strlen(rows[r]));
            for (int i = 0; i < (int)strlen(rows[r]); i++) {
                char c[2] = { rows[r][i], 0 };
                if (nk_button_label(ctx, c)) {
                    if (g_len < (int)sizeof(g_buffer) - 1) {
                        g_buffer[g_len++] = c[0];
                        g_buffer[g_len] = 0;
                    }
                }
            }
        }

        // space / backspace / ok / cancel
        nk_layout_row_dynamic(ctx, 40, 4);
        if (nk_button_label(ctx, "Space")) {
            if (g_len < (int)sizeof(g_buffer) - 1) {
                g_buffer[g_len++] = ' ';
                g_buffer[g_len] = 0;
            }
        }
        if (nk_button_label(ctx, "Back")) {
            if (g_len > 0) g_buffer[--g_len] = 0;
        }
        if (nk_button_label(ctx, "OK")) {
            g_keyboard_open = false;
            g_keyboard_done = true;
            g_result = g_buffer;
        }
        if (nk_button_label(ctx, "Cancel")) {
            g_keyboard_open = false;
            g_keyboard_done = true;
            g_result.clear();
        }
    }
    nk_end(ctx);
}
