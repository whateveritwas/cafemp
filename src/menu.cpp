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
SDL_Rect dest_rect = (SDL_Rect){0, 0, 0, 0};
bool dest_rect_initialised = false;

// Input
float touch_x = 0.0f;
float touch_y = 0.0f;
bool touched = false;

bool ambiance_playing = false;

static const std::unordered_set<std::string> valid_video_endings = {
    "mp4", "mov", "mkv", "avi", "webm", "flv", "asf", "mpegts"
};

static const std::unordered_set<std::string> valid_audio_endings = {
    "mp3", "wav", "ogg", "flac", "aac"
};

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

bool valid_file_ending(const std::string& file_ending) {
    return valid_video_endings.count(file_ending) > 0 || valid_audio_endings.count(file_ending) > 0;
}

void scan_directory(const char* path, std::vector<std::string>& video_files) {
    video_files.clear();
    printf("Opening folder %s\n", path);
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name(ent->d_name);
        if (name.length() > 4) {
            std::string ext = name.substr(name.find_last_of(".") + 1);
            for (auto& c : ext) c = std::tolower(c);
            if (valid_file_ending(ext)) {
                video_files.push_back(name);
            }
        }
    }    
    closedir(dir);
}

void start_selected_video() {
    if (ambiance_playing) {
        audio_player_cleanup();
        ambiance_playing = false;
    }
    std::string full_path = std::string(VIDEO_PATH) + video_files[selected_index];
    video_player_start(full_path.c_str(), ui_app_state, *ui_renderer, ui_texture);
    audio_player_audio_play(true);
    video_player_play(true);
    *ui_app_state = STATE_PLAYING_VIDEO;
    SDL_RenderClear(ui_renderer);
}

void start_selected_audio() {
    if (ambiance_playing) {
        audio_player_cleanup();
        ambiance_playing = false;
    }
    std::string full_path = std::string(VIDEO_PATH) + video_files[selected_index];
    audio_player_init(full_path.c_str());
    audio_player_audio_play(true);
    *ui_app_state = STATE_PLAYING_AUDIO;
    SDL_RenderClear(ui_renderer);
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
        font = nk_font_atlas_add_from_file(atlas, FONT_PATH, 32 * UI_SCALE, &config);
        nk_sdl_font_stash_end();

        nk_style_set_font(ctx, &font->handle);
    }

    bg.r = 0.0f, bg.g = 0.0f, bg.b = 0.0f, bg.a = 1.0f;

    // Scan local directories
    scan_directory(VIDEO_PATH, video_files);
}

void start_file(int i) {
    dest_rect_initialised = false;
    std::string full_path = std::string(VIDEO_PATH) + video_files[i];
    std::string extension = full_path.substr(full_path.find_last_of('.') + 1);
    
    for (auto& c : extension) c = std::tolower(c);

    if (valid_video_endings.count(extension)) {
        start_selected_video();
    } else if (valid_audio_endings.count(extension)) {
        start_selected_audio();
    } else {
        printf("Unsupported file type: %s\n", extension.c_str());
    }
}

void ui_menu_input(VPADStatus* buf) {
    if (buf->trigger == VPAD_BUTTON_UP || buf->trigger == VPAD_STICK_L_EMULATION_UP) {
        nk_input_key(ctx, NK_KEY_UP, 1);
        nk_input_key(ctx, NK_KEY_UP, 0);
        selected_index--;
        if (selected_index < 0) selected_index = video_files.size() - 1;
    }
    if (buf->trigger == VPAD_BUTTON_DOWN || buf->trigger == VPAD_STICK_L_EMULATION_DOWN) {
        nk_input_key(ctx, NK_KEY_DOWN, 1);
        nk_input_key(ctx, NK_KEY_DOWN, 0);
        selected_index++;
        if (selected_index >= (int)video_files.size()) selected_index = 0;
    }
    if (buf->trigger == VPAD_BUTTON_A) {
        nk_input_key(ctx, NK_KEY_ENTER, 1);
        nk_input_key(ctx, NK_KEY_ENTER, 0);
        start_file(selected_index);
        //start_selected_video();
    }
}

void ui_settings_input(VPADStatus* buf) {}

void ui_video_player_input(VPADStatus* buf) {
    if (buf->trigger == VPAD_BUTTON_A) {
        audio_player_audio_play(!video_player_is_playing());
        video_player_play(!video_player_is_playing());
    } else if (buf->trigger == VPAD_BUTTON_B) {
        audio_player_audio_play(true);
        video_player_play(true);
        video_player_cleanup();
        audio_player_audio_play(false);
        video_player_play(false);
        scan_directory(VIDEO_PATH, video_files);
        *ui_app_state = STATE_MENU;
    } else if (buf->trigger == VPAD_BUTTON_LEFT) {
        video_player_seek(-5.0f);
    } else if (buf->trigger == VPAD_BUTTON_RIGHT) {
        video_player_seek(5.0f);
    }
}

void ui_audio_player_input(VPADStatus* buf) {
    if (buf->trigger == VPAD_BUTTON_A) {
        audio_player_audio_play(!audio_player_get_audio_play_state());
    } else if (buf->trigger == VPAD_BUTTON_B) {
        audio_player_cleanup();
        scan_directory(VIDEO_PATH, video_files);
        *ui_app_state = STATE_MENU;
    } else if (buf->trigger == VPAD_BUTTON_LEFT) {
        audio_player_seek(-5.0f);
    } else if (buf->trigger == VPAD_BUTTON_RIGHT) {
        audio_player_seek(5.0f);
    }
}

void ui_handle_vpad_input() {
    VPADStatus buf;
    int key_press = VPADRead(VPAD_CHAN_0, &buf, 1, nullptr);

    /*
    touched = buf.tpNormal.touched;
    if (touched) {
        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &buf.tpNormal, &buf.tpNormal);
        touch_x = (float)buf.tpNormal.x;
        touch_y = (float)buf.tpNormal.y;
    }
    */
    if(!key_press) return;

    switch(*ui_app_state) {
        case STATE_PLAYING_VIDEO: ui_video_player_input(&buf); break;
        case STATE_PLAYING_AUDIO: ui_audio_player_input(&buf); break;
        case STATE_MENU: ui_menu_input(&buf); break;
        case STATE_SETTINGS: ui_settings_input(&buf); break;
    }
}

void ui_render() {
    nk_input_begin(ctx);

    ui_handle_vpad_input();

    nk_input_motion(ctx, (int)touch_x, (int)touch_y);
    nk_input_button(ctx, NK_BUTTON_LEFT, (int)touch_x, (int)touch_y, touched);

    nk_input_end(ctx);

    switch(*ui_app_state) {
        case STATE_PLAYING_VIDEO:
        ui_render_video_player();
        break;
        case STATE_PLAYING_AUDIO:
        ui_render_audio_player();
        break;
        case STATE_MENU:
        if (!ambiance_playing) {
            audio_player_init(AMBIANCE_PATH);
            audio_player_audio_play(true);
            ambiance_playing = true;
        }
        ui_render_file_browser();
        break;
        case STATE_SETTINGS:
        if (!ambiance_playing) {
            audio_player_init(AMBIANCE_PATH);
            audio_player_audio_play(true);
            ambiance_playing = true;
        }
        ui_render_settings();
        break;
    }
}

void ui_render_settings() {
    if (nk_begin(ctx, "café media player v0.4.0 |  Files  [Settings]", nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), NK_WINDOW_BORDER | NK_WINDOW_TITLE)) {

        nk_layout_row_dynamic(ctx, SCREEN_HEIGHT - 20, 1);
        if (nk_group_begin(ctx, "settings_group", NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE)) {

            // --- Video Playback ---
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Settings", NK_TEXT_LEFT);

            // --- Storage ---
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Network Storage Setup", NK_TEXT_LEFT);
            static char net_ip[64] = "192.168.1.100";
            static char net_path[256] = "/share/media/";
            static char net_pass[64] = "password";

            nk_label(ctx, "IP Address", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, net_ip, sizeof(net_ip), nk_filter_default);

            nk_label(ctx, "Password", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, net_pass, sizeof(net_pass), nk_filter_default);

            nk_label(ctx, "Path", NK_TEXT_LEFT);
            nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, net_path, sizeof(net_path), nk_filter_default);

            // --- Debug ---
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Debug", NK_TEXT_LEFT);

            static int debug_ui = 0;
            nk_checkbox_label(ctx, "Toggle debug UI", &debug_ui);

            nk_group_end(ctx);
        }

        nk_end(ctx);

        // Background clear
        SDL_SetRenderDrawColor(ui_renderer, bg.r * 255, bg.g * 255, bg.b * 255, bg.a * 255);
        SDL_RenderClear(ui_renderer);

        // Render GUI
        nk_sdl_render(NK_ANTI_ALIASING_ON);
    }
}

void ui_render_file_browser() {
    if (nk_begin(ctx, "café media player v0.5.0.this.is.pain", nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT), NK_WINDOW_BORDER|NK_WINDOW_TITLE)) {
        nk_layout_row_dynamic(ctx, 64 * UI_SCALE, 1);
        nk_window_set_scroll(ctx, 0, 64 * UI_SCALE * selected_index);
        for (int i = 0; i < static_cast<int>(video_files.size()); ++i) {
            std::string display_str = video_files[i];

            // Highlight selected item
            struct nk_style_button button_style = ctx->style.button;
            if (i == selected_index) {
                ctx->style.button.border_color = nk_rgb(13, 146, 244);
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
        nk_end(ctx);

        SDL_SetRenderDrawColor(ui_renderer, bg.r * 255, bg.g * 255, bg.b * 255, bg.a * 255);
        SDL_RenderClear(ui_renderer);
    
        nk_sdl_render(NK_ANTI_ALIASING_ON);
    }
}

void ui_render_player_hud(bool state, double current_time, double total_time) {
    const int hud_height = 80 * UI_SCALE;
    struct nk_rect hud_rect = nk_rect(0, SCREEN_HEIGHT - hud_height, SCREEN_WIDTH, hud_height);

    if (nk_begin(ctx, "HUD", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, (hud_height / 2) - 5, 1);
        nk_size progress = static_cast<nk_size>(current_time);
        nk_progress(ctx, &progress, total_time, NK_FIXED);

        nk_layout_row_dynamic(ctx, hud_height / 2, 2);
        std::string hud_str = state ? "> " : "|| ";
        hud_str += format_time(current_time);
        hud_str += " / ";
        hud_str += format_time(total_time);
        nk_label(ctx, hud_str.c_str(), NK_TEXT_LEFT);

        nk_end(ctx);
    }
}

void ui_render_video_player() {
    uint64_t test_ticks = OSGetSystemTime();
    video_player_update(ui_app_state, ui_renderer);
    uint64_t decoding_time = OSTicksToMicroseconds(OSGetSystemTime() - test_ticks);

    frame_info* current_frame_info = video_player_get_current_frame_info();
    if (!current_frame_info || !current_frame_info->texture) return;   

    static uint64_t last_decoding_time = 0;
    static uint64_t last_rendering_time = 0;

    uint64_t rendering_ticks = 0;

    rendering_ticks = OSGetSystemTime();  // Start timing

    if(!dest_rect_initialised) {
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

        dest_rect = { 0, 0, new_width, new_height };

        dest_rect.x = (screen_width - new_width) / 2;
        dest_rect.y = (screen_height - new_height) / 2;

        dest_rect_initialised = true;
    }
    SDL_RenderCopy(ui_renderer, current_frame_info->texture, NULL, &dest_rect);

    uint64_t current_rendering_time = OSTicksToMicroseconds(OSGetSystemTime() - rendering_ticks);
    if (current_rendering_time > 10) {
        last_rendering_time = current_rendering_time;
    }

    struct nk_rect hud_rect = nk_rect(0, 0, 512 * UI_SCALE, 30 * UI_SCALE);
    if (nk_begin(ctx, "Decoding", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, 30 * UI_SCALE, 2);
    
        if (decoding_time > 10) {
            last_decoding_time = decoding_time;
        }

        std::string decoding_time_str = "Decode: " + std::to_string(last_decoding_time) + "us";
        nk_label(ctx, decoding_time_str.c_str(), NK_TEXT_LEFT);

        std::string rendering_time_str = "Render: " + std::to_string(last_rendering_time) + "us";
        nk_label(ctx, rendering_time_str.c_str(), NK_TEXT_LEFT);

        nk_end(ctx);
    }

    if (!video_player_is_playing()) ui_render_player_hud(video_player_is_playing(), video_player_get_current_time(), video_player_get_total_play_time());

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void ui_render_audio_player() {
    ui_render_player_hud(audio_player_get_audio_play_state(), audio_player_get_current_play_time(), audio_player_get_total_play_time());
}

void ui_shutodwn() {
    if (ambiance_playing) { audio_player_cleanup(); ambiance_playing = false; }
    if(!video_player_is_playing()) video_player_play(true);
    if (video_player_is_playing()) video_player_cleanup();

    if (ui_texture) {
        SDL_DestroyTexture(ui_texture);
        ui_texture = nullptr;
    }
    nk_sdl_shutdown();
}