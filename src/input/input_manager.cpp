#include <vector>

#include "utils/utils.hpp"
#include "input/input_manager.hpp"
#include "input/input_actions.hpp"
#include "utils/app_state.hpp"
#include "utils/media_info.hpp"
#include "media_files.hpp"
#include "player/audio_player.hpp"
#include "player/video_player.hpp"
#include "player/photo_viewer.hpp"
#include "player/pdf_viewer.hpp"
#include "vendor/ui/nuklear.h"
#ifdef DEBUG
#include "shader/easter_egg.hpp"
#endif

void input_menu(InputState& input, int& current_page_file_browser, int& selected_index) {
    int total_items = static_cast<int>(get_media_files().size());

    if (input_pressed(input, BTN_UP) || input.left_stick.y > 0.5f) {
        if (selected_index > 0) selected_index--;
    } else if (input_pressed(input, BTN_DOWN) || input.left_stick.y < -0.5f) {
        if (selected_index < total_items - 1) selected_index++;
    } else if (input_pressed(input, BTN_A)) {
        start_file(selected_index);
    } else if (input_pressed(input, BTN_MINUS)) {
        switch (app_state_get()) {
            case STATE_MENU_AUDIO_FILES: scan_directory(MEDIA_PATH_AUDIO); break;
            case STATE_MENU_IMAGE_FILES: scan_directory(MEDIA_PATH_PHOTO); break;
            case STATE_MENU_VIDEO_FILES: scan_directory(MEDIA_PATH_VIDEO); break;
            default: return;
        }
        selected_index = 0;
        current_page_file_browser = 0;
    } else if (input_pressed(input, BTN_PLUS)) {
        app_state_set(STATE_MENU_SETTINGS);
    }
}

void input_settings(InputState& input) {
}

void input_easter_egg(InputState& input) {
    if (input_pressed(input, BTN_B)) {
#ifdef DEBUG
        easter_egg_shutdown();
#endif
        app_state_set(STATE_MENU);
    }
}

void input_video_player(InputState& input) {
    if (input_pressed(input, BTN_A)) {
        video_player_play(!media_info_get()->playback_status);
    } else if (input_pressed(input, BTN_B)) {
        video_player_cleanup();
        app_state_set(STATE_MENU_VIDEO_FILES);
        scan_directory(MEDIA_PATH_VIDEO);
    } else if (input_pressed(input, BTN_LEFT)) {
#ifdef DEBUG
        video_player_seek(-5.0f);
#endif
    } else if (input_pressed(input, BTN_RIGHT)) {
#ifdef DEBUG
        video_player_seek(5.0f);
#endif
    } else if (input_pressed(input, BTN_X)) {
        if (media_info_get()->total_audio_track_count == 1) return;

        video_player_play(false);
        audio_player_play(false);

        media_info_get()->current_audio_track_id++;
        if (media_info_get()->current_audio_track_id > media_info_get()->total_audio_track_count)
            media_info_get()->current_audio_track_id = 1;

        audio_player_switch_audio_stream(media_info_get()->current_audio_track_id);

        video_player_play(true);
        audio_player_play(true);
    }
}

void input_audio_player(InputState& input) {
    if (input_pressed(input, BTN_A)) {
        audio_player_play(!audio_player_get_audio_play_state());
    } else if (input_pressed(input, BTN_B)) {
        audio_player_cleanup();
        app_state_set(STATE_MENU_AUDIO_FILES);
        scan_directory(MEDIA_PATH_AUDIO);
    } else if (input_pressed(input, BTN_LEFT)) {
        audio_player_seek(-5.0f);
    } else if (input_pressed(input, BTN_RIGHT)) {
        audio_player_seek(5.0f);
    }
}

void input_photo_viewer(InputState& input) {

}

void input_pdf_viewer(InputState& input) {
    if (input_pressed(input, BTN_B)) {
        pdf_viewer_cleanup();
        app_state_set(STATE_MENU_PDF_FILES);
    } else if (input_touched(input)) {
        pdf_viewer_pan(input.touch.x - input.touch.old_x, input.touch.y - input.touch.old_y);
    } else if (input_held(input, BTN_ZL)) {
        pdf_texture_zoom(0.05f);
    } else if (input_held(input, BTN_ZR)) {
        pdf_texture_zoom(-0.05f);
    } else if (input_held(input, BTN_RSTICK_UP)) {
        pdf_viewer_pan(0, -10.0f);
    } else if (input_held(input, BTN_RSTICK_DOWN)) {
        pdf_viewer_pan(0, 10.0f);
    } else if (input_held(input, BTN_RSTICK_LEFT)) {
        pdf_viewer_pan(-10.0f, 0);
    } else if (input_held(input, BTN_RSTICK_RIGHT)) {
        pdf_viewer_pan(10.0f, 0);
    } else if (input_pressed(input, BTN_LSTICK_UP)) {
        pdf_viewer_prev_page();
    } else if (input_pressed(input, BTN_LSTICK_DOWN)) {
        pdf_viewer_next_page();
    }
}

void input_update(int& current_page_file_browser, int& selected_index, struct nk_context* ctx) {
//    switch (app_state_get()) {
//        case STATE_MENU:
//        case STATE_MENU_FILES: break;
//        case STATE_MENU_NETWORK_FILES:
//        case STATE_MENU_VIDEO_FILES:
//        case STATE_MENU_AUDIO_FILES:
//        case STATE_MENU_IMAGE_FILES:
//        case STATE_MENU_PDF_FILES:
//            input_menu(input, current_page_file_browser, selected_index);
//            break;
//        case STATE_MENU_SETTINGS:
//            input_settings(input);
//            break;
//        case STATE_MENU_EASTER_EGG:
//            input_easter_egg(input);
//            break;
//        case STATE_PLAYING_VIDEO:
//            input_video_player(input);
//            break;
//        case STATE_PLAYING_AUDIO:
//            input_audio_player(input);
//            break;
//        case STATE_VIEWING_PHOTO:
//            input_photo_viewer(input);
//            break;
//        case STATE_VIEWING_PDF:
//            input_pdf_viewer(input);
//            break;
//    }
}
