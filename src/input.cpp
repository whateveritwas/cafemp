
#include <vpad/input.h>
#include <padscore/wpad.h>

#include "app_state.hpp"
#include "main.hpp"
#include "nuklear.h"
#include "utils.hpp"
#include "settings.hpp"
#include "media_files.hpp"
#include "audio_player.hpp"
#include "video_player.hpp"
#include "input.hpp"

bool use_wpad_pro = false;
float touch_x = 0.0f;
float touch_y = 0.0f;
bool touched = false;

void input_menu(VPADStatus* vpad_status, WPADStatusProController* wpad_status, int& current_page_file_browser, int& selected_index) {
    int total_items = static_cast<int>(get_media_files().size());
    int total_pages = (total_items + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

    int start = current_page_file_browser * ITEMS_PER_PAGE;
    int end = std::min(start + ITEMS_PER_PAGE, total_items);

    int local_index = selected_index - start;
    int row = local_index / GRID_COLS;
    int col = local_index % GRID_COLS;

    if (vpad_status->trigger == VPAD_BUTTON_UP
        || vpad_status->trigger == VPAD_STICK_L_EMULATION_UP
        || wpad_status->leftStick.y > 500) {
        if (row > 0) {
            selected_index -= GRID_COLS;
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_DOWN
                || vpad_status->trigger == VPAD_STICK_L_EMULATION_DOWN
                || wpad_status->leftStick.y < -500) {
        if ((row + 1) * GRID_COLS < (end - start)) {
            selected_index += GRID_COLS;
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_LEFT
                || vpad_status->trigger == VPAD_STICK_L_EMULATION_LEFT
                || wpad_status->buttons & WPAD_PRO_STICK_L_EMULATION_LEFT
                || wpad_status->leftStick.x < -500) {
        if (col > 0) {
            selected_index--;
        } else if (current_page_file_browser > 0) {
            current_page_file_browser--;
            start = current_page_file_browser * ITEMS_PER_PAGE;
            end = std::min(start + ITEMS_PER_PAGE, total_items);
            selected_index = start + std::min(row * GRID_COLS + (GRID_COLS - 1), end - start - 1);
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_RIGHT
                || vpad_status->trigger == VPAD_STICK_L_EMULATION_RIGHT
                || wpad_status->buttons & WPAD_PRO_STICK_L_EMULATION_RIGHT
                || wpad_status->leftStick.x > 500) {
        if (col < GRID_COLS - 1 && selected_index + 1 < end) {
            selected_index++;
        } else if (current_page_file_browser < total_pages - 1) {
            current_page_file_browser++;
            start = current_page_file_browser * ITEMS_PER_PAGE;
            end = std::min(start + ITEMS_PER_PAGE, total_items);
            selected_index = start + row * GRID_COLS;
            if (selected_index >= end) {
                selected_index = end - 1;
            }
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_A
                || wpad_status->buttons == WPAD_PRO_BUTTON_A) {
        start_file(selected_index);
    } else if (vpad_status->trigger == VPAD_BUTTON_L
                || wpad_status->buttons == WPAD_PRO_TRIGGER_L) {
        if (current_page_file_browser > 0) {
            current_page_file_browser--;
            selected_index = current_page_file_browser * ITEMS_PER_PAGE;
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_R
                || wpad_status->buttons == WPAD_PRO_TRIGGER_R) {
        if (current_page_file_browser < total_pages - 1) {
            current_page_file_browser++;
            selected_index = current_page_file_browser * ITEMS_PER_PAGE;
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_MINUS
                || wpad_status->buttons == WPAD_PRO_BUTTON_MINUS) {
        scan_directory(MEDIA_PATH);
        selected_index = 0;
        current_page_file_browser = 0;
    } else if(vpad_status->trigger == VPAD_BUTTON_PLUS
                || wpad_status->buttons == WPAD_PRO_BUTTON_PLUS) {
            app_state_set(STATE_SETTINGS);
    }
}

void input_use_wpad(bool state) {
    use_wpad_pro = state;
}

bool input_use_wpad() {
    return use_wpad_pro;
}

void input_check_wpad_pro_connection() {
    WPADExtensionType extType;
    if (WPADProbe(WPAD_CHAN_0, &extType) == 0 && extType == WPAD_EXT_PRO_CONTROLLER) {
        if (!input_use_wpad()) {
            WPADSetDataFormat(WPAD_CHAN_0, WPAD_FMT_PRO_CONTROLLER);
            input_use_wpad(true);
            printf("[Input] Pro Controller connected\n");
        }
    } else {
        if (input_use_wpad()) {
            input_use_wpad(false);
            printf("[Input] Pro Controller disconnected.\n");
        }
    }
}

bool input_is_vpad_touched() {
    return touched;
}

void input_settings(VPADStatus* vpad_status, WPADStatusProController* wpad_status) {
    if(vpad_status->trigger == VPAD_BUTTON_PLUS
        || vpad_status->trigger == VPAD_BUTTON_B
        || wpad_status->buttons == WPAD_PRO_BUTTON_B) {
        settings_save();
        app_state_set(STATE_MENU);
    }
}

void input_video_player(VPADStatus* vpad_status, WPADStatusProController* wpad_status) {
    if (vpad_status->trigger == VPAD_BUTTON_A
        || wpad_status->buttons == WPAD_PRO_BUTTON_A) {
        audio_player_audio_play(!video_player_is_playing());
        video_player_play(!video_player_is_playing());
    } else if (vpad_status->trigger == VPAD_BUTTON_B
        || wpad_status->buttons == WPAD_PRO_BUTTON_B) {
        audio_player_audio_play(true);
        video_player_play(true);
        video_player_cleanup();
        audio_player_audio_play(false);
        video_player_play(false);
        scan_directory(MEDIA_PATH);
        app_state_set(STATE_MENU);
    } else if (vpad_status->trigger == VPAD_BUTTON_LEFT) {
        // video_player_seek(-5.0f);
    } else if (vpad_status->trigger == VPAD_BUTTON_RIGHT) {
        // video_player_seek(5.0f);
    }
}

void input_audio_player(VPADStatus* vpad_status, WPADStatusProController* wpad_status) {
    if (vpad_status->trigger == VPAD_BUTTON_A
        || wpad_status->buttons == WPAD_PRO_BUTTON_A) {
        audio_player_audio_play(!audio_player_get_audio_play_state());
    } else if (vpad_status->trigger == VPAD_BUTTON_B
        || wpad_status->buttons == WPAD_PRO_BUTTON_B) {
        audio_player_cleanup();
        scan_directory(MEDIA_PATH);
        app_state_set(STATE_MENU);
    } else if (vpad_status->trigger == VPAD_BUTTON_LEFT) {
        // audio_player_seek(-5.0f);
    } else if (vpad_status->trigger == VPAD_BUTTON_RIGHT) {
        // audio_player_seek(5.0f);
    }
}

void input_update(int& current_page_file_browser, int& selected_index, nk_context *ctx) {
    int key_press = 0;
    WPADStatusProController wpad_status = { 0 };
    VPADStatus vpad_status = { 0 };
    VPADTouchData touchpoint_calibrated = { 0 };
    key_press = VPADRead(VPAD_CHAN_0, &vpad_status, 1, nullptr);

    if(use_wpad_pro) WPADRead(WPAD_CHAN_0, &wpad_status);
    
    if(key_press) {    
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touchpoint_calibrated, &vpad_status.tpNormal);
        touched = touchpoint_calibrated.touched;
        if (touched) {
            touch_x = (float)touchpoint_calibrated.x;
            touch_y = (float)touchpoint_calibrated.y;
        } else {
            touch_x = 0;
            touch_y = 0;
        }
        nk_input_motion(ctx, (int)touch_x, (int)touch_y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)touch_x, (int)touch_y, touched);
    }
    
    switch(app_state_get()) {
        case STATE_PLAYING_VIDEO: input_video_player(&vpad_status, &wpad_status); break;
        case STATE_PLAYING_AUDIO: input_audio_player(&vpad_status, &wpad_status); break;
        case STATE_MENU: input_menu(&vpad_status, &wpad_status, current_page_file_browser, selected_index); break;
        case STATE_SETTINGS: input_settings(&vpad_status, &wpad_status); break;
    }
}