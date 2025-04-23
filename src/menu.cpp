#include <vector>
#include <string>
#include <dirent.h>
#include <SDL2/SDL.h>
#include <vpad/input.h>
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
#include "nuklear.h"
#include "nuklear_sdl_renderer.h"

#include "main.hpp"
#include "video_player.hpp"
#include "audio_player.hpp"
#include "menu.hpp"

std::vector<std::string> video_files;
int selected_index = 0;

// Ui
SDL_Window* ui_window;
SDL_Renderer* ui_renderer;
SDL_Texture* ui_texture;
AppState* ui_app_state;

struct nk_context *ctx;
struct nk_colorf bg;

// Input
float touch_x = 0.0f;
float touch_y = 0.0f;
bool touched = false;

bool ambiance_playing = false;

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

bool valid_file_ending(const std::string& file_ending) {
    static const std::unordered_set<std::string> valid_endings = {
        "mp4", "mov", "mkv", "avi", "webm", "flv", "asf", "mpegts"
    };
    return valid_endings.count(file_ending) > 0;
}

void scan_directory(const char* path, std::vector<std::string>& video_files) {
    video_files.clear();
    printf("Opening folder %s\n", path);
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name(ent->d_name);
        if (name.length() > 4 &&  valid_file_ending(name.substr(name.length() - 3))) {
            video_files.push_back(name);
        }
    }
    closedir(dir);
}

void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture, AppState* _app_state) {
    ui_window = _window;
    ui_renderer = _renderer;
    ui_texture = _texture;
    ui_app_state = _app_state;

    *ui_app_state = STATE_MENU;

    ctx = nk_sdl_init(ui_window, ui_renderer);

    {
        struct nk_font_atlas *atlas;
        struct nk_font_config config = nk_font_config(0);
        struct nk_font *font;

        nk_sdl_font_stash_begin(&atlas);
        font = nk_font_atlas_add_from_file(atlas, FONT_PATH, 32, &config);
        nk_sdl_font_stash_end();

        nk_style_set_font(ctx, &font->handle);
    }

    bg.r = 0.0f, bg.g = 0.0f, bg.b = 0.0f, bg.a = 1.0f;

    scan_directory(VIDEO_PATH, video_files);

    if(!ambiance_playing) {
        audio_player_init(AMBIANCE_PATH);
        ambiance_playing = true;
    }
}

void ui_handle_vpad_input() {
    VPADStatus buf;
    int key_press = VPADRead(VPAD_CHAN_0, &buf, 1, nullptr);

    touched = buf.tpNormal.touched;
    if(touched) {
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &buf.tpNormal, &buf.tpNormal);
        touch_x = (float)buf.tpNormal.x;
        touch_y = (float)buf.tpNormal.y;
    }

    if(key_press == 1) {
        if(*ui_app_state == STATE_PLAYING) {
            if(buf.trigger == DRC_BUTTON_A) {
                video_player_play(!video_player_is_playing());
            } else if(buf.trigger == VPAD_BUTTON_B) {
                video_player_play(true);
                video_player_cleanup();
                video_player_play(false);
                scan_directory(VIDEO_PATH, video_files);
                *ui_app_state = STATE_MENU;
            } else if(buf.trigger == VPAD_BUTTON_LEFT) {
                video_player_scrub(-5);
            } else if(buf.trigger == VPAD_BUTTON_RIGHT) {
                video_player_scrub(5);
            }
        } else if (*ui_app_state == STATE_MENU) {}
    }
}

void ui_render() {
    ui_handle_vpad_input();
    nk_input_begin(ctx);
    if(touched) {
        nk_input_motion(ctx, (int)touch_x, (int)touch_y);
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)touch_x, (int)touch_y, true);
    } else {
        nk_input_button(ctx, NK_BUTTON_LEFT, (int)touch_x, (int)touch_y, false);
    }
    nk_input_end(ctx);

    switch(*ui_app_state) {
        case STATE_PLAYING:
        ui_render_video();
        break;
        case STATE_MENU:
        ui_render_file_browser();
        break;
    }

}

void ui_render_file_browser() {
    if (nk_begin(ctx, "cafemp", nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), NK_WINDOW_BORDER|NK_WINDOW_TITLE)) {
        nk_layout_row_dynamic(ctx, 64, 1);
        for (int i = 0; i < static_cast<int>(video_files.size()); ++i) {
            std::string display_str = video_files[i];

            if (nk_button_label(ctx, display_str.c_str())) {
                if(ambiance_playing) {
                    audio_player_cleanup();
                    ambiance_playing = false;
                }

                std::string full_path = std::string(VIDEO_PATH) + video_files[i];
                video_player_start(full_path.c_str(), ui_app_state, *ui_renderer, ui_texture);
                video_player_play(true);
                *ui_app_state = STATE_PLAYING;
                SDL_RenderClear(ui_renderer);
            }
        }
        nk_end(ctx);

        SDL_SetRenderDrawColor(ui_renderer, bg.r * 255, bg.g * 255, bg.b * 255, bg.a * 255);
        SDL_RenderClear(ui_renderer);
    
        nk_sdl_render(NK_ANTI_ALIASING_ON);
    }
}

void ui_render_video() {
    uint64_t test_ticks = OSGetSystemTime();
    video_player_update(ui_app_state, ui_renderer);
    uint64_t decoding_time = OSTicksToMicroseconds(OSGetSystemTime() - test_ticks);

    frame_info* current_frame_info = video_player_get_current_frame_info();

    if (current_frame_info && current_frame_info->texture) {
        SDL_SetRenderDrawColor(ui_renderer, 0, 0, 0, 255);
        SDL_RenderClear(ui_renderer);

        int video_width = current_frame_info->frame_width;
        int video_height = current_frame_info->frame_height;

        int screen_width = SCREEN_WIDTH;
        int screen_height = SCREEN_HEIGHT;

        int new_width = screen_width;
        int new_height = (video_height * new_width) / video_width;

        if (new_height > screen_height) {
            new_height = screen_height;
            new_width = (video_width * new_height) / video_height;
        }

        SDL_Rect dest_rect = { 0, 0, new_width, new_height };

        dest_rect.x = (screen_width - new_width) / 2;
        dest_rect.y = (screen_height - new_height) / 2;
        SDL_RenderCopy(ui_renderer, current_frame_info->texture, NULL, &dest_rect);
    }

    static uint64_t last_decoding_time = 0;

    struct nk_rect hud_rect = nk_rect(0, 0, 128, 64);
    if (nk_begin(ctx, "Decoding", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 30, 2);
    
        // Use the last decoding time if the current one is 0ms
        if (decoding_time > 100) {
            last_decoding_time = decoding_time;  // Update with the new decoding time
        }
    
        // Display the last valid decoding time, even if current decoding time is 0
        std::string decoding_time_str = std::to_string(last_decoding_time);
        nk_label(ctx, decoding_time_str.c_str(), NK_TEXT_LEFT);
    
        nk_end(ctx);
        nk_sdl_render(NK_ANTI_ALIASING_ON);
    }
    

    if (!video_player_is_playing()) {
        const int hud_height = 80;
        struct nk_rect hud_rect = nk_rect(0, SCREEN_HEIGHT - hud_height, SCREEN_WIDTH, hud_height);

        if (nk_begin(ctx, "HUD", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 30, 2);
            if (current_frame_info) {
                std::string hud_str = "Paused | " + format_time(video_player_get_current_time()) + " / " + format_time(0);
                nk_label(ctx, hud_str.c_str(), NK_TEXT_LEFT);
            }
            nk_end(ctx);
            nk_sdl_render(NK_ANTI_ALIASING_ON);
        }
    }
}

void ui_shutodwn() {
    if(ambiance_playing) { audio_player_cleanup(); ambiance_playing = false; }
    if(video_player_is_playing()) video_player_cleanup();

    if (ui_texture) {
        SDL_DestroyTexture(ui_texture);
        ui_texture = nullptr;
    }
    nk_sdl_shutdown();
}