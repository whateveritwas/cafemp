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
#include "ui/nuklear.h"
#include "ui/nuklear_sdl_renderer.h"

#include "utils/media_info.hpp"
#include "utils/media_files.hpp"
#include "settings/settings.hpp"
#include "utils/app_state.hpp"
#include "utils/utils.hpp"
#include "main.hpp"
#include "player/video_player.hpp"
#include "player/audio_player.hpp"
#include "player/photo_viewer.hpp"
#include "input/input.hpp"
#include "ui/menu.hpp"

int current_page_file_browser = 0;
int selected_index = 0;

// Ui
SDL_Window* ui_window;
SDL_Renderer* ui_renderer;
SDL_Texture* ui_texture;

struct nk_context *ctx;
SDL_Rect dest_rect = (SDL_Rect){0, 0, 0, 0};
bool dest_rect_initialised = false;

bool ambiance_playing = false;
static int background_music_enabled = 1;

static int no_current_frame_info_cound = 0;
#ifndef DEBUG
int no_current_frame_info_threshold = 10;
#else
int no_current_frame_info_threshold = 20;
#endif

void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture) {
    WPADInit();
    WPADEnableURCC(true);
    
    input_check_wpad_pro_connection();   

    ui_window = _window;
    ui_renderer = _renderer;
    ui_texture = _texture;

    try {
        settings_load();
        background_music_enabled = *static_cast<int*>(settings_get_value(SETTINGS_BKG_MUSIC_ENABLED));
    } catch(...) {
        printf("[Menu] Unable to load settings.\n");
    }

    if(!background_music_enabled) {
        audio_player_init("/vol/content/empty.mp3");
        audio_player_play(true);
        audio_player_cleanup();
        audio_player_play(false);
    }

    ctx = nk_sdl_init(ui_window, ui_renderer);
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

    no_current_frame_info_cound = 0;
    dest_rect_initialised = false;
    std::string full_path = std::string(MEDIA_PATH) + get_media_files()[index];
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
        case STATE_MENU_VIDEO_FILES:
            media_folder = "Video/";
            media_type = 'V';
            break;
        default:
            printf("[Menu] Unsupported file type: %s\n", extension.c_str());
            return;
    }

    full_path = std::string(MEDIA_PATH) + media_folder + get_media_files()[index];

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

        audio_player_init(full_path.c_str());
        audio_player_play(true);
        app_state_set(STATE_PLAYING_AUDIO);
    }
    else if (media_type == 'V') {
        media_info_get()->current_audio_track_id = 1;
        media_info_get()->total_audio_track_count = 1;

        media_info_get()->current_caption_id = 1;
        media_info_get()->total_caption_count = 1;

        video_player_start(full_path.c_str(), *ui_renderer, ui_texture);
        audio_player_play(true);
        video_player_play(true);
        app_state_set(STATE_PLAYING_VIDEO);
    }
    else if (media_type == 'P') {
        media_info_get()->current_audio_track_id = 0;
        media_info_get()->total_audio_track_count = 0;

        media_info_get()->current_caption_id = index;
        media_info_get()->total_caption_count = get_media_files().size();

        photo_viewer_init(ui_renderer, ui_texture);
        app_state_set(STATE_VIEWING_PHOTO);
        photo_viewer_open_picture(full_path.c_str());
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

    switch(app_state_get()) {
        case STATE_MENU: 
            ui_handle_ambiance();
            ui_render_main_menu();
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
        case STATE_MENU_SETTINGS: 
            ui_handle_ambiance();
            ui_render_settings();
            break;
        case STATE_PLAYING_VIDEO: 
            SDL_RenderClear(ui_renderer);
            ui_render_video_player();
            break;
        case STATE_PLAYING_AUDIO:
            SDL_RenderClear(ui_renderer);
            ui_render_audio_player();
            break;
        case STATE_VIEWING_PHOTO: 
            SDL_RenderClear(ui_renderer);
            ui_render_photo_viewer();
            break;
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void ui_render_sidebar() {
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
            scan_directory(MEDIA_PATH "Video/");
        }
        /*
        if (nk_button_label(ctx, "YouTube")) {
            app_state_set(STATE_MENU_VIDEO_FILES);
        }
        */
        if (nk_button_label(ctx, "Audio")) {
            app_state_set(STATE_MENU_AUDIO_FILES);
            scan_directory(MEDIA_PATH "Audio/");
        }
        /*
        if (nk_button_label(ctx, "Internet Radio")) {
            app_state_set(STATE_MENU_AUDIO_FILES);
        }
        */
        if (nk_button_label(ctx, "Photo")) {
            app_state_set(STATE_MENU_IMAGE_FILES);
            scan_directory(MEDIA_PATH "Photo/");
        }
        
        #ifdef DEBUG
        if (nk_button_label(ctx, "Debug")) {
            app_state_set(STATE_MENU_SETTINGS);
        }
        #endif
        if (nk_button_label(ctx, "Settings")) {
            app_state_set(STATE_MENU_SETTINGS);
        }

        nk_group_end(ctx);
    }
}

void ui_render_settings() {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
        ui_render_sidebar();

        nk_layout_row_push(ctx, SCREEN_WIDTH - (200 * UI_SCALE));
        if (nk_group_begin(ctx, "Content", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 64, 1);

            if (nk_button_label(ctx, background_music_enabled ? "Background Music: On" : "Background Music: Off")) {
                background_music_enabled = !background_music_enabled;

                settings_set(SETTINGS_BKG_MUSIC_ENABLED, &background_music_enabled);
                settings_save();
            }

            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    ui_render_tooltip();
}

void ui_render_tooltip() {
    if (nk_begin(ctx, "tooltip_bar", nk_rect(0, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, SCREEN_WIDTH, TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND)) {
        switch(app_state_get()) {
        case STATE_MENU: 
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(Left Stick) Select | (A) Open", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_FILES:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(A) Start | (-) Refresh | (-) Scan", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_NETWORK_FILES: break;
        case STATE_MENU_VIDEO_FILES:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(Left Stick) Select | (A) Open | (-) Scan", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_AUDIO_FILES:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(Left Stick) Select | (A) Open | (-) Scan", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_IMAGE_FILES: 
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(Left Stick) Select | (A) Open | (-) Scan", NK_TEXT_LEFT);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_MENU_SETTINGS: 
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 1);
            nk_label(ctx, "[Touch only!]", NK_TEXT_LEFT);
            break;
        case STATE_PLAYING_VIDEO: break;
        case STATE_PLAYING_AUDIO: break;
        case STATE_VIEWING_PHOTO:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 1);
            nk_label(ctx, "(Left Stick) Change Photo | [ZL] Zoom + | [ZR] Zoom - | (Touch) Pan", NK_TEXT_LEFT);
            break;
        }

        nk_end(ctx);
    }
}

void ui_render_main_menu() {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
        ui_render_sidebar();

        nk_layout_row_push(ctx, SCREEN_WIDTH - (200 * UI_SCALE));
        if (nk_group_begin(ctx, "Content", NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Welcome to " VERSION_STRING "!", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "What's new:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Support for Tiramisu cfw", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- General stability improvements", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Animated gifs", NK_TEXT_LEFT);
            /*
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Video / Audio player hud updates", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "- Changing between multiple audio tracks in a video", NK_TEXT_LEFT);
            */
            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    ui_render_tooltip();
}

void ui_render_file_browser() {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER)) {
        nk_layout_row_begin(ctx, NK_STATIC, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);

        ui_render_sidebar();
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
                    SDL_SetRenderDrawColor(ui_renderer, 0, 0, 0, 255);
                    SDL_RenderClear(ui_renderer);
                    start_file(selected_index);
                }

                ctx->style.button = button_style;
            }

            nk_group_end(ctx);
        }

        nk_layout_row_end(ctx);
        nk_end(ctx);
    }

    ui_render_tooltip();
}

void ui_render_player_hud(media_info* info) {
    const int hud_height = 80 * UI_SCALE;
    struct nk_rect hud_rect = nk_rect(0, SCREEN_HEIGHT - hud_height, SCREEN_WIDTH, hud_height);

    if (nk_begin(ctx, "HUD", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        // Progress bar layout
        nk_layout_row_dynamic(ctx, (hud_height / 2) - 5, 1);

        // Calculate progress and total in seconds
        double progress_seconds = 0.0;
        double total_seconds = 0.0;

        switch(info->type) {
            case 'V': // Video
                progress_seconds = std::min(info->current_video_playback_time, info->total_video_playback_time);
                total_seconds = info->total_video_playback_time;
                break;
            case 'A': // Audio
                progress_seconds = std::min(info->current_audio_playback_time, info->total_audio_playback_time);
                total_seconds = info->total_audio_playback_time;
                break;
            default:
                progress_seconds = 0.0;
                total_seconds = 1.0; // Avoid division by zero in nk_progress
                break;
        }

        nk_size progress = static_cast<nk_size>(progress_seconds);
        nk_size total = static_cast<nk_size>(total_seconds > 0 ? total_seconds : 1);

        nk_progress(ctx, &progress, total, NK_FIXED);

        // HUD text and buttons layout
        nk_layout_row_begin(ctx, NK_DYNAMIC, hud_height / 2, 2);
        nk_layout_row_push(ctx, 0.8f); // 80% left for playback info text
        {
            std::string hud_str = (info->playback_status ? "> " : "|| ");
            hud_str += format_time(progress_seconds);
            hud_str += " / ";
            hud_str += format_time(total_seconds);
            hud_str += " [";
            hud_str += info->filename;
            hud_str += "]";

            nk_label(ctx, hud_str.c_str(), NK_TEXT_LEFT);
        }

        if(app_state_get() == STATE_PLAYING_VIDEO) {
            nk_layout_row_push(ctx, 0.2f); // 20% right for audio/caption track info
            {
                std::string hud_str = "A:";
                hud_str += std::to_string(info->current_audio_track_id);
                hud_str += "/";
                hud_str += std::to_string(info->total_audio_track_count);
                hud_str += " S:";
                hud_str += std::to_string(info->current_caption_id);
                hud_str += "/";
                hud_str += std::to_string(info->total_caption_count);
                nk_label(ctx, hud_str.c_str(), NK_TEXT_RIGHT);
            }
        }
        nk_end(ctx);
    }
}

void ui_render_captions() {}

void ui_render_photo_viewer() {
    photo_viewer_render();
    if(input_is_vpad_touched()) ui_render_tooltip();
}

void ui_render_video_player() {
    SDL_RenderClear(ui_renderer);

    #ifdef DEBUG_VIDEO
    uint64_t test_ticks = OSGetSystemTime();
    #endif

    video_player_update(ui_renderer);

    #ifdef DEBUG_VIDEO
    uint64_t decoding_time = OSTicksToMicroseconds(OSGetSystemTime() - test_ticks);
    #endif

    frame_info* current_frame_info = video_player_get_current_frame_info();
    if (!current_frame_info || !current_frame_info->texture) {
        no_current_frame_info_cound++;
        if (no_current_frame_info_cound < no_current_frame_info_threshold) return;

        printf("[Video Player] Shuting down due to no video frames being present\n");
        video_player_cleanup();
        scan_directory(MEDIA_PATH "Video/");
        app_state_set(STATE_MENU_VIDEO_FILES);
        return;
    }
    no_current_frame_info_cound = 0;

    #ifdef DEBUG_VIDEO
    static uint64_t last_decoding_time = 0;
    static uint64_t last_rendering_time = 0;

    uint64_t rendering_ticks = OSGetSystemTime();  // Start timing
    #endif

    if (!dest_rect_initialised) {
        dest_rect = calculate_aspect_fit_rect(current_frame_info->frame_width, current_frame_info->frame_height);
        dest_rect_initialised = true;
    }

    SDL_RenderCopy(ui_renderer, current_frame_info->texture, NULL, &dest_rect);
    /*
    if ((int)media_info_get()->current_video_playback_time == (int)video_player_get_total_play_time()) {
        video_player_cleanup();
        scan_directory(MEDIA_PATH "Video/");
        app_state_set(STATE_MENU_VIDEO_FILES);
    }
    */
    #ifdef DEBUG_VIDEO
    uint64_t current_rendering_time = OSTicksToMicroseconds(OSGetSystemTime() - rendering_ticks);
    if (current_rendering_time > 5) {
        last_rendering_time = current_rendering_time;
    }

    #ifdef DEBUG
    printf("Dec: %lli Rndr: %lli\n", decoding_time, current_rendering_time);
    #endif

    struct nk_rect hud_rect = nk_rect(0, 0, 512 * UI_SCALE, 30 * UI_SCALE);
    if (nk_begin(ctx, "Decoding", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 30 * UI_SCALE, 2);

        if (decoding_time > 5) {
            last_decoding_time = decoding_time;
        }

        std::string decoding_time_str = "Decode: " + std::to_string(last_decoding_time) + "us";
        nk_label(ctx, decoding_time_str.c_str(), NK_TEXT_LEFT);

        std::string rendering_time_str = "Render: " + std::to_string(last_rendering_time) + "us";
        nk_label(ctx, rendering_time_str.c_str(), NK_TEXT_LEFT);

        nk_end(ctx);
    }
    #endif

    if (!media_info_get()->playback_status || input_is_vpad_touched()) {
        ui_render_player_hud(media_info_get());
    }
}

void ui_render_audio_player() {
    SDL_RenderClear(ui_renderer);
    /*
    if((int)audio_player_get_current_play_time() == (int)audio_player_get_total_play_time()) {
        audio_player_play(true);
        audio_player_cleanup();
        audio_player_play(false);
        scan_directory(MEDIA_PATH "Audio/");
        app_state_set(STATE_MENU_AUDIO_FILES);
    }
    */
    if (media_info_get()->total_audio_playback_time == 0) media_info_get()->total_audio_playback_time = audio_player_get_total_play_time();
    media_info_get()->current_audio_playback_time = audio_player_get_current_play_time();

    ui_render_player_hud(media_info_get());
}

void ui_shutdown() {
    WPADShutdown();
    if (ambiance_playing) { audio_player_cleanup(); ambiance_playing = false; }
    if (!media_info_get()->playback_status) video_player_play(true);
    if (media_info_get()->playback_status) video_player_cleanup();

    if (ui_texture) {
        SDL_DestroyTexture(ui_texture);
        ui_texture = nullptr;
    }
    nk_sdl_shutdown();
}
