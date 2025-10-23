#include <vector>
#include <string>
#include <SDL2/SDL.h>
#include <unordered_set>
#include <SDL2/SDL_ttf.h>

#include <coreinit/time.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_SDL_RENDERER_IMPLEMENTATION
#define NK_SDL_RENDERER_SDL_H <SDL2/SDL.h>
#include "vendor/ui/nuklear.h"
#include "vendor/ui/nuklear_sdl_renderer.h"

#include "widgets/widget_audio_player.hpp"
#include "widgets/widget_main_menu.hpp"
#include "widgets/widget_pdf_viewer.hpp"
#include "widgets/widget_photo_viewer.hpp"
#include "widgets/widget_player_hud.hpp"
#include "widgets/widget_tooltip.hpp"
#include "widgets/widget_sidebar.hpp"

#include "utils/sdl.hpp"
#include "utils/media_info.hpp"
#include "utils/media_files.hpp"
#include "settings/settings.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"
#include "main.hpp"
#include "player/video_player.hpp"
#include "player/audio_player.hpp"
#ifdef DEBUG
#include "shader/easter_egg.hpp"
#endif
#include "input/input.hpp"
#include "player/subtitle.hpp"
#include "logger/logger.hpp"
#include "ui/menu.hpp"

int current_page_file_browser = 0;
int selected_index = 0;

struct nk_context *ctx;

bool ambiance_playing = false;
static bool background_music_enabled = true;
static char jellyfin_url[MAX_URL_LENGTH] = { 0 };
static char api_key[MAX_API_KEY_LENGTH] = { 0 };

#ifndef DEBUG
#define NO_CURRENT_FRAME_INFO_THRESHOLD 10
#else
#define NO_CURRENT_FRAME_INFO_THRESHOLD 30
#endif

void ui_init() {
    WPADInit();
    WPADEnableURCC(true);

    input_check_wpad_pro_connection();   

    try {
        settings_load();
        settings_get(SETTINGS_BKG_MUSIC_ENABLED, &background_music_enabled);
    } catch(...) {
    	log_message(LOG_ERROR, "Menu", "Unable to load settings");
    }

    if(!background_music_enabled) {
        audio_player_init("/vol/content/empty.mp3");
        audio_player_play(true);
        audio_player_cleanup();
        audio_player_play(false);
    }

    ctx = nk_sdl_init(sdl_get()->sdl_window, sdl_get()->sdl_renderer);
    ctx->style.window.scrollbar_size.x = 32 * UI_SCALE;

    {
        struct nk_font_atlas *atlas;
        struct nk_font_config config = nk_font_config(0);
        struct nk_font *font;

        nk_sdl_font_stash_begin(&atlas);
        font = nk_font_atlas_add_from_file(atlas, FONT_PATH, 32 * UI_SCALE, &config);
        nk_sdl_font_stash_end();

        nk_style_set_font(ctx, &font->handle);
    }
}

void start_file(int index) {
    if (ambiance_playing) {
        audio_player_cleanup();
        ambiance_playing = false;
    }

    std::string full_path = std::string(BASE_PATH) + get_media_files()[index];
    std::string extension = full_path.substr(full_path.find_last_of('.') + 1);

    for (auto& c : extension) c = std::tolower(c);

    auto new_info = std::make_unique<media_info>();
    media_info_set(std::move(new_info));

    media_info_get()->type = '\0';
    media_info_get()->path = "";
    media_info_get()->filename = "";
    media_info_get()->current_video_playback_time = 0;
    media_info_get()->current_audio_playback_time = 0;

    media_info_get()->current_audio_track_id = 1;
    media_info_get()->total_audio_track_count = 1;

    media_info_get()->current_caption_id = 1;
    media_info_get()->total_caption_count = 1;

    // Determine the media type from the app state
    int state = app_state_get();

    std::string media_folder;
    char media_type = '\0';

    switch (state) {
        case STATE_MENU_AUDIO_FILES:
            media_folder = "Audio/";
            media_type = 'A';
            break;
        case STATE_MENU_IMAGE_FILES:
            media_folder = "Photo/";
            media_type = 'P';
            break;
        case STATE_MENU_PDF_FILES:
            media_folder = "Library/";
            media_type = 'L';
            break;
        case STATE_MENU_VIDEO_FILES:
            media_folder = "Video/";
            media_type = 'V';
            // subtitle_start("/vol/external01/wiiu/apps/cafemp/Test/test.srt");
            break;
        default:
        	log_message(LOG_ERROR, "Menu", "Unsupported file type: %s", extension.c_str());
            return;
    }

    full_path = std::string(BASE_PATH) + media_folder + get_media_files()[index];

    media_info_get()->type = media_type;
    media_info_get()->path = full_path;
    media_info_get()->filename = get_media_files()[index];
    media_info_get()->current_video_playback_time = 0;
    media_info_get()->current_audio_playback_time = 0;

    if (media_type == 'A') {
        media_info_get()->current_audio_track_id = 1;
        media_info_get()->total_audio_track_count = 1;

        media_info_get()->current_caption_id = 1;
        media_info_get()->total_caption_count = 1;

        // full_path = "http://192.168.178.113:8096/Items/c8808f6fb3a1133926b35887ce286cfe/Download?api_key=e847650b901e4c1ea7b72b8404b40fdb&TranscodingMaxVideoBitrate=1500000&TranscodingMaxHeight=720";

        widget_audio_player_init(full_path);
        app_state_set(STATE_PLAYING_AUDIO);
    } else if (media_type == 'V') {
        media_info_get()->current_audio_track_id = 1;
        media_info_get()->total_audio_track_count = 1;

        media_info_get()->current_caption_id = 1;
        media_info_get()->total_caption_count = 1;

        // full_path = "http://192.168.178.113:8096/Items/81b3564968b66075f27f5c83f4a98367/Download?api_key=e847650b901e4c1ea7b72b8404b40fdb&TranscodingMaxVideoBitrate=1500000&TranscodingMaxHeight=720";

        video_player_init(full_path.c_str());
        audio_player_play(true);
        video_player_play(true);
        app_state_set(STATE_PLAYING_VIDEO);
    } else if (media_type == 'P') {
        media_info_get()->current_audio_track_id = 0;
        media_info_get()->total_audio_track_count = 0;

        media_info_get()->current_caption_id = index;
        media_info_get()->total_caption_count = get_media_files().size();

		app_state_set(STATE_VIEWING_PHOTO);
		widget_photo_viewer_init(full_path);
    } else if (media_type == 'L') {
        media_info_get()->current_audio_track_id = 0;
        media_info_get()->total_audio_track_count = 0;

        media_info_get()->current_caption_id = index;
        media_info_get()->total_caption_count = get_media_files().size();

        app_state_set(STATE_VIEWING_PDF);
        widget_pdf_viewer_init(full_path);
    }
}

void ui_handle_ambiance() {
    if (!ambiance_playing && background_music_enabled) {
        audio_player_init(AMBIANCE_PATH);
        audio_player_play(true);
        ambiance_playing = true;
    } else if((int)audio_player_get_current_play_time() == (int)audio_player_get_total_play_time() && ambiance_playing && background_music_enabled) {
        audio_player_seek(-1000);
    } else if(ambiance_playing && !background_music_enabled) {
        audio_player_play(true);
        audio_player_cleanup();
        audio_player_play(false);
    }
}

void ui_render() {
    nk_input_begin(ctx);

    input_check_wpad_pro_connection();
    input_update(current_page_file_browser, selected_index, ctx);

    nk_input_end(ctx);

    if (!sdl_get()->use_native_renderer) {
		SDL_SetRenderDrawColor(sdl_get()->sdl_renderer, 0, 0, 0, 255);
		SDL_RenderClear(sdl_get()->sdl_renderer);
    }

    switch(app_state_get()) {
        case STATE_MENU:
            ui_handle_ambiance();
            widget_main_menu_render(ctx);
            break;
        case STATE_MENU_FILES: break;
        case STATE_MENU_NETWORK_FILES: break;
        case STATE_MENU_VIDEO_FILES:
            ui_handle_ambiance();
            ui_render_file_browser();
            break;
        case STATE_MENU_AUDIO_FILES:
            ui_handle_ambiance();
            ui_render_file_browser();
            break;
        case STATE_MENU_IMAGE_FILES:
            ui_handle_ambiance();
            ui_render_file_browser();
            break;
        case STATE_MENU_PDF_FILES:
            ui_handle_ambiance();
            ui_render_file_browser();
            break;
        case STATE_MENU_SETTINGS:
            ui_handle_ambiance();
            ui_render_settings();
            break;
        case STATE_MENU_EASTER_EGG:
#ifdef DEBUG
            ui_handle_ambiance();
            easter_egg_render();
#endif
            break;
        case STATE_PLAYING_VIDEO:
            ui_render_video_player();
            break;
        case STATE_PLAYING_AUDIO:
            widget_audio_player_render(ctx);
            break;
        case STATE_VIEWING_PHOTO:
            widget_photo_viewer_render(ctx);
            break;
        case STATE_VIEWING_PDF:
        	widget_pdf_viewer_render(ctx);
            break;
    }

	nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void ui_render_settings() {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
        widget_sidebar_render(ctx);

        nk_layout_row_push(ctx, SCREEN_WIDTH - (200 * UI_SCALE));
        if (nk_group_begin(ctx, "Content", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 64, 1);

            nk_label(ctx, "General", NK_TEXT_CENTERED);

            if (nk_button_label(ctx, background_music_enabled ? "Background Music: On" : "Background Music: Off")) {
                background_music_enabled = !background_music_enabled;

                settings_set(SETTINGS_BKG_MUSIC_ENABLED, &background_music_enabled);
            }

            nk_label(ctx, "Jellyfin URL", NK_TEXT_CENTERED);
            if (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, jellyfin_url, MAX_URL_LENGTH, nk_filter_default)) {
            }

            nk_label(ctx, "Jellyfin API Key", NK_TEXT_CENTERED);
            if (nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, api_key, MAX_API_KEY_LENGTH, nk_filter_default)) {
            }

            if (nk_button_label(ctx, "Save")) {
            	settings_save();
            }

            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    widget_tooltip_render(ctx);
}

void ui_render_file_browser() {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
        widget_sidebar_render(ctx);

        nk_layout_row_push(ctx, SCREEN_WIDTH - (200 * UI_SCALE)); // Content area

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

void ui_shutdown() {
    WPADShutdown();
    if (ambiance_playing) { audio_player_cleanup(); ambiance_playing = false; }
    if (!media_info_get()->playback_status) video_player_play(true);
    if (media_info_get()->playback_status) video_player_cleanup();

    nk_sdl_shutdown();
}
