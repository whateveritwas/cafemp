#include <vector>
#include <string>
#include "show.hpp"
#include <dirent.h>
#include <SDL2/SDL.h>
#include <vpad/input.h>
#include <SDL2/SDL_ttf.h>

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

#include "config.hpp"
#include "video_player.hpp"
#include "menu.hpp"

std::vector<std::string> video_files;
int selected_index = 0;

// Ui
SDL_Window* ui_window;
SDL_Renderer* ui_renderer;
SDL_Texture* ui_texture;
AppState* ui_app_state;
SDL_AudioSpec ui_wanted_spec;
static SDL_mutex* audio_mutex = NULL;

struct nk_context *ctx;
struct nk_colorf bg;

// Input
float touch_x = 0.0f;
float touch_y = 0.0f;
bool touched = false;

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

void scan_directory(const char* path, std::vector<std::string>& video_files) {
    video_files.clear();
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name(ent->d_name);
        if (name.length() > 4 && name.substr(name.length() - 4) == ".mp4") {
            video_files.push_back(name);
        }
    }
    closedir(dir);
}

void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture, AppState* _app_state, SDL_AudioSpec _wanted_spec) {
    ui_window = _window;
    ui_renderer = _renderer;
    ui_texture = _texture;
    ui_app_state = _app_state;
    ui_wanted_spec = _wanted_spec;

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
}

void ui_handle_vpad_input() {
    VPADStatus buf;
    int key_press = VPADRead(VPAD_CHAN_0, &buf, 1, nullptr);

    touched = buf.tpNormal.touched;
    if(touched) {
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &buf.tpNormal, &buf.tpNormal);
        touch_x = (float)buf.tpNormal.x;
        touch_y = (float)buf.tpNormal.y;
    } else {
        touch_x = 0.0f;
        touch_y = 0.0f;
    }

    if (key_press == 1) {
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
        nk_input_motion(ctx, 0, 0);
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
    if (nk_begin(ctx, "File Browser", nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {
        nk_layout_row_dynamic(ctx, 48, 1);
        for (int i = 0; i < static_cast<int>(video_files.size()); ++i) {
            std::string display_str = video_files[i];

            if (nk_button_label(ctx, display_str.c_str())) {
                std::string full_path = std::string(VIDEO_PATH) + video_files[i];
                audio_mutex = SDL_CreateMutex();
                video_player_start(full_path.c_str(), ui_app_state, *ui_renderer, ui_texture, *audio_mutex, ui_wanted_spec);
                video_player_play(true);
                *ui_app_state = STATE_PLAYING;
                SDL_RenderClear(ui_renderer);
            }
        }
    }
    nk_end(ctx);

    SDL_SetRenderDrawColor(ui_renderer, bg.r * 255, bg.g * 255, bg.b * 255, bg.a * 255);
    SDL_RenderClear(ui_renderer);

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void ui_render_video() {
    video_player_update(ui_app_state, ui_renderer, ui_texture);
}

void ui_render_video_hud() {
/*
    SDL_RenderCopy(renderer, texture, NULL, NULL);

    std::string time_str = format_time(current_pts_seconds) + " / " + format_time(duration_seconds);
    SDL_Color white = {255, 255, 255};

    SDL_Surface* text_surface = TTF_RenderText_Blended(font, time_str.c_str(), white);
    SDL_Texture* text_texture = SDL_CreateTextureFromSurface(renderer, text_surface);

    int text_w, text_h;
    SDL_QueryTexture(text_texture, NULL, NULL, &text_w, &text_h);
    SDL_Rect dst_rect = {10, SCREEN_WIDTH - text_h - 10, text_w, text_h};

    SDL_RenderCopy(renderer, text_texture, NULL, &dst_rect);

    SDL_FreeSurface(text_surface);
    SDL_DestroyTexture(text_texture);
*/
}

void ui_shutodwn() {
    if (ui_texture) {
        SDL_DestroyTexture(ui_texture);
        ui_texture = nullptr;
    }
    if (audio_mutex) {
        SDL_DestroyMutex(audio_mutex);
        audio_mutex = NULL;
    }
    nk_sdl_shutdown();
}