
#include <vpad/input.h>
#include <padscore/wpad.h>

#include "utils/app_state.hpp"
#include "utils/media_info.hpp"
#include "main.hpp"
#include "ui/nuklear.h"
#include "utils.hpp"
#include "settings/settings.hpp"
#include "media_files.hpp"
#include "player/audio_player.hpp"
#include "player/video_player.hpp"
#include "player/photo_viewer.hpp"
#include "input/input.hpp"

bool use_wpad_pro = false;
float touch_x = 0.0f;
float touch_y = 0.0f;
float old_touch_x = 0.0f;
float old_touch_y = 0.0f;
bool touched = false;
static bool was_touched = false;
int current_audio_track_id = 1;

bool is_pressed(VPADStatus* vpad, WPADStatusProController* wpad, int vpad_btn, int wpad_btn) {
    return (int)vpad->trigger == vpad_btn || ((int)wpad->buttons & wpad_btn);
}

bool is_hold(VPADStatus* vpad, int vpad_btn) {
    return (int)vpad->hold == vpad_btn;
}

void input_menu(VPADStatus* vpad_status, WPADStatusProController* wpad_status, int& current_page_file_browser, int& selected_index) {
    int total_items = static_cast<int>(get_media_files().size());

    if (vpad_status->trigger == VPAD_BUTTON_UP
        || vpad_status->trigger == VPAD_STICK_L_EMULATION_UP
        || wpad_status->leftStick.y > 500) {
        if (selected_index > 0) {
            selected_index -= 1;
        }
    } else if (vpad_status->trigger == VPAD_BUTTON_DOWN
                || vpad_status->trigger == VPAD_STICK_L_EMULATION_DOWN
                || wpad_status->leftStick.y < -500) {
        if (selected_index < total_items - 1) {
            selected_index += 1;
        }
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_A, WPAD_PRO_BUTTON_A)) {
        start_file(selected_index);
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_MINUS, WPAD_PRO_BUTTON_MINUS)) {
        scan_directory(MEDIA_PATH);
        selected_index = 0;
        current_page_file_browser = 0;
    } else if(is_pressed(vpad_status, wpad_status, VPAD_BUTTON_PLUS, WPAD_PRO_BUTTON_PLUS)) {
        app_state_set(STATE_MENU_SETTINGS);
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
    if (vpad_status->trigger == VPAD_BUTTON_PLUS
        || vpad_status->trigger == VPAD_BUTTON_B
        || wpad_status->buttons == WPAD_PRO_BUTTON_B) {
        settings_save();
        app_state_set(STATE_MENU_FILES);
    }
}

void input_video_player(VPADStatus* vpad_status, WPADStatusProController* wpad_status) {
    if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_A, WPAD_PRO_BUTTON_A)) {
        audio_player_play(!media_info_get()->playback_status);
        video_player_play(!media_info_get()->playback_status);
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_B, WPAD_PRO_BUTTON_B)) {
        video_player_cleanup();

        app_state_set(STATE_MENU_VIDEO_FILES);
        scan_directory(MEDIA_PATH "Video/");
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_LEFT, WPAD_PRO_BUTTON_LEFT))  {
		#ifdef DEBUG
        video_player_seek(-5.0f);
		#endif
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_RIGHT, WPAD_PRO_BUTTON_RIGHT)) {
		#ifdef DEBUG
        video_player_seek(5.0f);
		#endif
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_X, WPAD_PRO_BUTTON_X)) {
        if(media_info_get()->total_audio_track_count == 1) return;

        video_player_play(false);
        audio_player_play(false);

        media_info_get()->current_audio_track_id++;
        if(media_info_get()->current_audio_track_id > media_info_get()->total_audio_track_count)
            media_info_get()->current_audio_track_id = 1;
        
        audio_player_switch_audio_stream(media_info_get()->current_audio_track_id);

        video_player_play(true);
        audio_player_play(true);
        // Change Audio track
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_Y, WPAD_PRO_BUTTON_Y)) {
        // Change Subtitles
    }
}

void input_audio_player(VPADStatus* vpad_status, WPADStatusProController* wpad_status) {
    if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_A, WPAD_PRO_BUTTON_A)) {
        audio_player_play(!audio_player_get_audio_play_state());
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_B, WPAD_PRO_BUTTON_B)) {
        audio_player_cleanup();

        app_state_set(STATE_MENU_AUDIO_FILES);
        scan_directory(MEDIA_PATH "Audio/");
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_LEFT, WPAD_PRO_BUTTON_LEFT)) {
        audio_player_seek(-5.0f);
    } else if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_RIGHT, WPAD_PRO_BUTTON_RIGHT)) {
        audio_player_seek(5.0f);
    }
}

void input_photo_viewer(VPADStatus* vpad_status, WPADStatusProController* wpad_status) {
    if (is_pressed(vpad_status, wpad_status, VPAD_BUTTON_B, WPAD_PRO_BUTTON_B)) {
        photo_viewer_cleanup();
        app_state_set(STATE_MENU_IMAGE_FILES);
    } else if (input_is_vpad_touched()) {
        photo_viewer_pan(touch_x - old_touch_x, touch_y - old_touch_y);
    } else if (is_hold(vpad_status, VPAD_BUTTON_ZL)) {
        photo_texture_zoom(0.05f);
    } else if (is_hold(vpad_status, VPAD_BUTTON_ZR)) {
        photo_texture_zoom(-0.05f);
    } else if (is_hold(vpad_status, VPAD_STICK_R_EMULATION_UP)) {
        photo_viewer_pan(0, -10.0f);
    } else if (is_hold(vpad_status, VPAD_STICK_R_EMULATION_DOWN)) {
        photo_viewer_pan(0, 10.0f);
    } else if (is_hold(vpad_status, VPAD_STICK_R_EMULATION_LEFT)) {
        photo_viewer_pan(-10.0f, 0);
    } else if (is_hold(vpad_status, VPAD_STICK_R_EMULATION_RIGHT)) {
        photo_viewer_pan(10.0f, 0);
    } else if (is_pressed(vpad_status, wpad_status, VPAD_STICK_L_EMULATION_LEFT, 0)) {
        media_info* info = media_info_get();
        if (--info->current_caption_id < 0)
            info->current_caption_id = info->total_caption_count - 1;

        std::string full_path = std::string(MEDIA_PATH "Photo/") + get_media_files()[info->current_caption_id];
        photo_viewer_open_picture(full_path.c_str());

    } else if (is_pressed(vpad_status, wpad_status, VPAD_STICK_L_EMULATION_RIGHT, 0)) {
        media_info* info = media_info_get();
        if (++info->current_caption_id >= info->total_caption_count)
            info->current_caption_id = 0;

        std::string full_path = std::string(MEDIA_PATH "Photo/") + get_media_files()[info->current_caption_id];
        photo_viewer_open_picture(full_path.c_str());
    }
}

void input_update(int& current_page_file_browser, int& selected_index, nk_context *ctx) {
    WPADStatus wpad_status = { 0 };
    WPADStatusProController wpad_status_pro = { 0 };
    VPADStatus vpad_status = { 0 };
    VPADTouchData touchpoint_calibrated = { 0 };

    if(use_wpad_pro) WPADRead(WPAD_CHAN_0, &wpad_status);
    
    if(VPADRead(VPAD_CHAN_0, &vpad_status, 1, nullptr)) {    
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touchpoint_calibrated, &vpad_status.tpNormal);
        if (touchpoint_calibrated.touched) {
            if (!was_touched) {
                // First frame of touch — initialize both to avoid jump
                touch_x = old_touch_x = (float)touchpoint_calibrated.x;
                touch_y = old_touch_y = (float)touchpoint_calibrated.y;
            } else {
                old_touch_x = touch_x;
                old_touch_y = touch_y;
                touch_x = (float)touchpoint_calibrated.x;
                touch_y = (float)touchpoint_calibrated.y;
            }
            touched = true;
            was_touched = true;
        } else {
            touched = false;
            was_touched = false;
            old_touch_x = touch_x = 0;
            old_touch_y = touch_y = 0;
        }

        nk_input_motion(ctx, (int)touch_x, (int)touch_y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)touch_x, (int)touch_y, touched);
    }
    
    switch(app_state_get()) {
        case STATE_MENU: break;
        case STATE_MENU_FILES: break;
        case STATE_MENU_NETWORK_FILES: break;
        case STATE_MENU_VIDEO_FILES: input_menu(&vpad_status, &wpad_status_pro, current_page_file_browser, selected_index); break;
        case STATE_MENU_AUDIO_FILES: input_menu(&vpad_status, &wpad_status_pro, current_page_file_browser, selected_index); break;
        case STATE_MENU_IMAGE_FILES: input_menu(&vpad_status, &wpad_status_pro, current_page_file_browser, selected_index); break;
        case STATE_MENU_SETTINGS: break;
        case STATE_PLAYING_VIDEO: input_video_player(&vpad_status, &wpad_status_pro); break;
        case STATE_PLAYING_AUDIO: input_audio_player(&vpad_status, &wpad_status_pro); break;
        case STATE_VIEWING_PHOTO: input_photo_viewer(&vpad_status, &wpad_status_pro); break;
    }
}
