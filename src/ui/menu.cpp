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

#include "ui/scene.hpp"

//#include "ui/scenes/scene_audio_player.hpp"
#include "ui/scenes/scene_file_browser.hpp"
#include "ui/scenes/scene_main_menu.hpp"
//#include "ui/scenes/scene_pdf_viewer.hpp"
//#include "ui/scenes/scene_photo_viewer.hpp"
//#include "ui/scenes/scene_video_player.hpp"

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
#include "input/input_manager.hpp"
#include "player/subtitle.hpp"
#include "logger/logger.hpp"
#include "ui/menu.hpp"

struct nk_context *ctx;

#ifndef DEBUG
#define NO_CURRENT_FRAME_INFO_THRESHOLD 10
#else
#define NO_CURRENT_FRAME_INFO_THRESHOLD 30
#endif

void ui_init() {
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

    ui_scene_register(STATE_MENU, {
        [](){},
        [](nk_context* ctx){},
        [](nk_context* ctx){ scene_main_menu_render(ctx); },
        [](){}
    });

    ui_scene_register(STATE_MENU_VIDEO_FILES, {
        [](){},
        [](nk_context* ctx){},
        [](nk_context* ctx){ scene_file_browser_render(ctx); },
        [](){}
    });

    ui_scene_register(STATE_MENU_AUDIO_FILES, {
        [](){},
        [](nk_context* ctx){},
        [](nk_context* ctx){ scene_file_browser_render(ctx); },
        [](){}
    });

    ui_scene_register(STATE_MENU_IMAGE_FILES, {
        [](){},
        [](nk_context* ctx){},
        [](nk_context* ctx){ scene_file_browser_render(ctx); },
        [](){}
    });

    ui_scene_register(STATE_MENU_PDF_FILES, {
        [](){},
        [](nk_context* ctx){},
        [](nk_context* ctx){ scene_file_browser_render(ctx); },
        [](){}
    });

    ui_scene_register(STATE_MENU_EASTER_EGG, {
        [](){ easter_egg_init(); },
        [](nk_context* ctx){},
        [](nk_context* ctx){ easter_egg_render(); },
        [](){ easter_egg_shutdown(); }
    });

	ui_scene_set(app_state_get());
}

void start_file(int index) {
//    std::string full_path = std::string(BASE_PATH) + get_media_files()[index];
//    std::string extension = full_path.substr(full_path.find_last_of('.') + 1);
//
//    for (auto& c : extension) c = std::tolower(c);
//
//    auto new_info = std::make_unique<media_info>();
//    media_info_set(std::move(new_info));
//
//    media_info_get()->type = '\0';
//    media_info_get()->path = "";
//    media_info_get()->filename = "";
//    media_info_get()->current_video_playback_time = 0;
//    media_info_get()->current_audio_playback_time = 0;
//
//    media_info_get()->current_audio_track_id = 1;
//    media_info_get()->total_audio_track_count = 1;
//
//    media_info_get()->current_caption_id = 1;
//    media_info_get()->total_caption_count = 1;
//
//    int state = app_state_get();
//
//    std::string media_folder;
//    char media_type = '\0';
//
//    switch (state) {
//        case STATE_MENU_AUDIO_FILES:
//            media_folder = "Audio/";
//            media_type = 'A';
//            break;
//        case STATE_MENU_IMAGE_FILES:
//            media_folder = "Photo/";
//            media_type = 'P';
//            break;
//        case STATE_MENU_PDF_FILES:
//            media_folder = "Library/";
//            media_type = 'L';
//            break;
//        case STATE_MENU_VIDEO_FILES:
//            media_folder = "Video/";
//            media_type = 'V';
//            // subtitle_start("/vol/external01/wiiu/apps/cafemp/Test/test.srt");
//            break;
//        default:
//        	log_message(LOG_ERROR, "Menu", "Unsupported file type: %s", extension.c_str());
//            return;
//    }
//
//    full_path = std::string(BASE_PATH) + media_folder + get_media_files()[index];
//
//    media_info_get()->type = media_type;
//    media_info_get()->path = full_path;
//    media_info_get()->filename = get_media_files()[index];
//    media_info_get()->current_video_playback_time = 0;
//    media_info_get()->current_audio_playback_time = 0;
//
//    if (media_type == 'A') {
//        media_info_get()->current_audio_track_id = 1;
//        media_info_get()->total_audio_track_count = 1;
//
//        media_info_get()->current_caption_id = 1;
//        media_info_get()->total_caption_count = 1;
//
//        // full_path = "http://192.168.178.113:8096/Items/c8808f6fb3a1133926b35887ce286cfe/Download?api_key=e847650b901e4c1ea7b72b8404b40fdb&TranscodingMaxVideoBitrate=1500000&TranscodingMaxHeight=720";
//
//        widget_audio_player_init(full_path);
//        app_state_set(STATE_PLAYING_AUDIO);
//    } else if (media_type == 'V') {
//        media_info_get()->current_audio_track_id = 1;
//        media_info_get()->total_audio_track_count = 1;
//
//        media_info_get()->current_caption_id = 1;
//        media_info_get()->total_caption_count = 1;
//
//        // full_path = "http://192.168.178.113:8096/Items/81b3564968b66075f27f5c83f4a98367/Download?api_key=e847650b901e4c1ea7b72b8404b40fdb&TranscodingMaxVideoBitrate=1500000&TranscodingMaxHeight=720";
//
//        video_player_init(full_path.c_str());
//        audio_player_play(true);
//        video_player_play(true);
//        app_state_set(STATE_PLAYING_VIDEO);
//    } else if (media_type == 'P') {
//        media_info_get()->current_audio_track_id = 0;
//        media_info_get()->total_audio_track_count = 0;
//
//        media_info_get()->current_caption_id = index;
//        media_info_get()->total_caption_count = get_media_files().size();
//
//		app_state_set(STATE_VIEWING_PHOTO);
//		widget_photo_viewer_init(full_path);
//    } else if (media_type == 'L') {
//        media_info_get()->current_audio_track_id = 0;
//        media_info_get()->total_audio_track_count = 0;
//
//        media_info_get()->current_caption_id = index;
//        media_info_get()->total_caption_count = get_media_files().size();
//
//        app_state_set(STATE_VIEWING_PDF);
//        widget_pdf_viewer_init(full_path);
//    }
}

void ui_render() {
    nk_input_begin(ctx);

	int temp = 0;
	input_update(temp, temp, ctx);

    nk_input_end(ctx);

    if (!sdl_get()->use_native_renderer) {
        SDL_SetRenderDrawColor(sdl_get()->sdl_renderer, 0, 0, 0, 255);
        SDL_RenderClear(sdl_get()->sdl_renderer);
    }

    ui_scene_input(ctx);
    ui_scene_render(ctx);

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void ui_shutdown() {
    ui_scene_shutdown();
    nk_sdl_shutdown();
}
