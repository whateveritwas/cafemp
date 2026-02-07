#include "scene.hpp"
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

#include "ui/scenes/scene_file_browser.hpp"
#include "ui/scenes/scene_main_menu.hpp"
#include "ui/scenes/scene_media_player.hpp"
#include "ui/scenes/scene_pdf_viewer.hpp"
#include "ui/scenes/scene_photo_viewer.hpp"
#include "ui/widgets/widget_cursor.hpp"

#include "utils/sdl.hpp"
#include "utils/media_info.hpp"
#include "utils/media_files.hpp"
#include "settings/settings.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"
#include "main.hpp"
#include "player/media_player.hpp"
#ifdef DEBUG
#include "shader/easter_egg.hpp"
#endif
#include "input/input_actions.hpp"
#include "logger/logger.hpp"
#include "ui/menu.hpp"

struct nk_context *ctx;
InputState input {};

static bool ambiance_playing = false;
static bool background_music_enabled = true;

#ifndef DEBUG
#define NO_CURRENT_FRAME_INFO_THRESHOLD 10
#else
#define NO_CURRENT_FRAME_INFO_THRESHOLD 30
#endif

static void start_ambiance() {
    if (ambiance_playing || !background_music_enabled)
        return;

    media_player_init(AMBIANCE_PATH);
    media_player_play(true);
    ambiance_playing = true;
}

static void stop_ambiance() {
    if (!ambiance_playing)
        return;

    media_player_play(false);
    media_player_cleanup();
    ambiance_playing = false;
}

static void update_ambiance() {
    if (!background_music_enabled) {
        stop_ambiance();
        return;
    }

    if (!ambiance_playing) {
        start_ambiance();
        return;
    }

    double cur = media_player_get_current_time();
    double total = media_player_get_total_time();

    if (total > 0.0 && cur >= total - 0.1) {
        media_player_seek(0.0);
    }
}

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
		[](InputState& input){},
		[](nk_context* ctx){ scene_main_menu_render(ctx); },
		[](){}
		});

    ui_scene_register(STATE_MENU_VIDEO_FILES, {
	    [](){},
		[](InputState& input){},
		[](nk_context* ctx){ scene_file_browser_render(ctx); },
		[](){}
		});

    ui_scene_register(STATE_MENU_AUDIO_FILES, {
	    [](){},
		[](InputState& input){},
		[](nk_context* ctx){ scene_file_browser_render(ctx); },
		[](){}
		});

    ui_scene_register(STATE_MENU_IMAGE_FILES, {
	    [](){},
		[](InputState& input){},
		[](nk_context* ctx){ scene_file_browser_render(ctx); },
		[](){}
		});

    ui_scene_register(STATE_MENU_PDF_FILES, {
	    [](){},
		[](InputState& input){},
		[](nk_context* ctx){ scene_file_browser_render(ctx); },
		[](){}
		});

#ifdef DEBUG
    ui_scene_register(STATE_MENU_EASTER_EGG, {
	    [](){ easter_egg_init(); },
		[](InputState& input){},
		[](nk_context* ctx){ easter_egg_render(); },
		[](){ easter_egg_shutdown(); }
		});
#endif

    ui_scene_register(STATE_MENU_SETTINGS, {
	    [](){ app_state_set(STATE_MENU); },
		[](InputState& input){},
		[](nk_context* ctx){},
		[](){}
		});

    ui_scene_register(STATE_PLAYING_VIDEO, {
	    [](){ scene_media_player_init(media_info_get()->path); },
		[](InputState& input){ scene_media_player_input(input); },
		[](nk_context* ctx){ scene_media_player_render(ctx); },
		[](){ scene_media_player_shutdown(); }
		});

    ui_scene_register(STATE_PLAYING_AUDIO, {
	    [](){ scene_media_player_init(media_info_get()->path); },
		[](InputState& input){ scene_media_player_input(input); },
		[](nk_context* ctx){ scene_media_player_render(ctx); },
		[](){ scene_media_player_shutdown(); }
		});

    ui_scene_register(STATE_VIEWING_PHOTO, {
	    [](){ scene_photo_viewer_init(media_info_get()->path); },
		[](InputState& input){ scene_photo_viewer_input(input); },
		[](nk_context* ctx){ scene_photo_viewer_render(ctx); },
		[](){}
		});

    ui_scene_register(STATE_VIEWING_PDF, {
	    [](){ scene_pdf_viewer_init(media_info_get()->path); },
		[](InputState& input){ scene_pdf_viewer_input(input); },
		[](nk_context* ctx){ scene_pdf_viewer_render(ctx); },
		[](){}
		});

    update_ambiance();
    ui_scene_set(app_state_get());
}

void start_file(int index) {
    stop_ambiance();

    const std::string filename = get_media_files()[index];
    std::string full_path;

    auto new_info = std::make_unique<media_info>();
    media_info_set(std::move(new_info));

    media_info* info = media_info_get();
    info->type = '\0';
    info->path = "";
    info->filename = filename;
    info->current_video_playback_time = 0;
    info->current_audio_playback_time = 0;
    info->current_audio_track_id = 1;
    info->total_audio_track_count = 1;
    info->current_caption_id = 1;
    info->total_caption_count = 1;

    struct MediaMapping {
        const char* folder;
        char type;
    };

    static const std::unordered_map<int, MediaMapping> state_map = {
        {STATE_MENU_AUDIO_FILES, {"Audio/", 'A'}},
        {STATE_MENU_VIDEO_FILES, {"Video/", 'V'}},
        {STATE_MENU_IMAGE_FILES, {"Photo/", 'P'}},
        {STATE_MENU_PDF_FILES, {"Library/", 'L'}}
    };

    int state = app_state_get();
    auto it = state_map.find(state);
    if (it == state_map.end()) {
        log_message(LOG_ERROR, "Menu", "Unsupported file type");
        return;
    }

    const MediaMapping& mapping = it->second;
    full_path = std::string(BASE_PATH) + mapping.folder + filename;

    info->type = mapping.type;
    info->path = full_path;

    if (mapping.type == 'P' || mapping.type == 'L') {
        info->current_audio_track_id = 0;
        info->total_audio_track_count = 0;
        info->current_caption_id = index;
        info->total_caption_count = get_media_files().size();
    }

    switch (mapping.type) {
    case 'A': app_state_set(STATE_PLAYING_AUDIO); break;
    case 'V': app_state_set(STATE_PLAYING_VIDEO); break;
    case 'P': app_state_set(STATE_VIEWING_PHOTO); break;
    case 'L': app_state_set(STATE_VIEWING_PDF); break;
    }
}

void ui_render() {
    nk_input_begin(ctx);

    input_poll(input);

    nk_input_motion(ctx, (int)input.touch.x, (int)input.touch.y);
    nk_input_button(ctx, NK_BUTTON_LEFT,
                    (int)input.touch.x, (int)input.touch.y,
                    input.touch.touched);

    if (input.valid_cursor) {
        nk_input_motion(ctx,
                        (int)input.cursor_position.x,
                        (int)input.cursor_position.y);
        nk_input_button(ctx, NK_BUTTON_LEFT,
                        (int)input.cursor_position.x,
                        (int)input.cursor_position.y,
                        input_pressed(input, BTN_A));
    }

    nk_input_end(ctx);

    if (!sdl_get()->use_native_renderer) {
        SDL_SetRenderDrawColor(sdl_get()->sdl_renderer, 0, 0, 0, 255);
        SDL_RenderClear(sdl_get()->sdl_renderer);
    }

    ui_scene_input(input);
    ui_scene_render(ctx);

    nk_sdl_render(NK_ANTI_ALIASING_ON);

    if (input.valid_cursor)
        widget_cursor_render(input.cursor_position.x,
			     input.cursor_position.y, 10);

    update_ambiance();
}

void ui_shutdown() {
    stop_ambiance();
    ui_scene_shutdown();
    nk_sdl_shutdown();
}
