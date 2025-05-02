#include <vector>
#include <string>
#include <dirent.h>
#include <jansson.h>
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
SDL_Rect dest_rect = (SDL_Rect){0, 0, 0, 0};
bool dest_rect_initialised = false;

// Input
float touch_x = 0.0f;
float touch_y = 0.0f;
bool touched = false;

bool ambiance_playing = false;
static int background_music_enabled = 1;

int current_page_file_browser = 0;
int current_page_settings = 0;

#define TOOLTIP_BAR_HEIGHT (48)
#define GRID_COLS 4
#define GRID_ROWS 3
#define ITEMS_PER_PAGE (GRID_COLS * GRID_ROWS)
#define CELL_HEIGHT (((SCREEN_HEIGHT / 3) - TOOLTIP_BAR_HEIGHT) * UI_SCALE)

static const std::unordered_set<std::string> valid_video_endings = {
    "mp4", "mov", "mkv", "avi"
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

std::string truncate_filename(const std::string& name, size_t max_length) {
    if (name.length() <= max_length) {
        return name;
    }
    return name.substr(0, max_length - 3) + "...";
}

void save_settings() {

    json_t *root = json_object();

    json_object_set_new(root, "background_music_enabled", json_boolean(background_music_enabled));

    FILE *file = fopen(SETTINGS_PATH, "w");
    if (file) {
        json_dumpf(root, file, JSON_INDENT(4));
        fclose(file);
    } else {
        printf("[Settings] Failed to save settings to file.\n");
    }

    json_decref(root);
}

void load_settings() {
    FILE *file = fopen(SETTINGS_PATH, "r");
    if (file) {
        json_error_t error;
        json_t *root = json_loadf(file, 0, &error);
        fclose(file);

        if (root) {
            json_t *bg_music = json_object_get(root, "background_music_enabled");
            if (json_is_boolean(bg_music)) {
                background_music_enabled = json_is_true(bg_music);
            }

            json_decref(root);  // Free the JSON object
        } else {
            printf("[Settings] Failed to load settings: %s\n", error.text);
        }
    } else {
        printf("[Settings] No settings file found, using default settings.\n");
    }
}

bool valid_file_ending(const std::string& file_ending) {
    return valid_video_endings.count(file_ending) > 0 || valid_audio_endings.count(file_ending) > 0;
}

void scan_directory(const char* path, std::vector<std::string>& video_files) {
    video_files.clear();
    printf("[Menu] Opening folder %s\n", path);
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
    std::string full_path = std::string(MEDIA_PATH) + video_files[selected_index];
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
    std::string full_path = std::string(MEDIA_PATH) + video_files[selected_index];
    audio_player_init(full_path.c_str());
    audio_player_audio_play(true);
    *ui_app_state = STATE_PLAYING_AUDIO;
    SDL_RenderClear(ui_renderer);
}

void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture, AppState* _app_state) {
    audio_player_init("/vol/content/empty.mp3");
    audio_player_audio_play(true);
    audio_player_cleanup();
    audio_player_audio_play(false);

    ui_window = _window;
    ui_renderer = _renderer;
    ui_texture = _texture;
    ui_app_state = _app_state;

    *ui_app_state = STATE_MENU;

    try {
        load_settings();
    } catch(...) {
        printf("[Menu] Unable to load settings.\n");
    }

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

    // Scan local directories
    scan_directory(MEDIA_PATH, video_files);
}

void start_file(int i) {
    SDL_SetRenderDrawColor(ui_renderer, 0, 0, 0, 255);

    dest_rect_initialised = false;
    std::string full_path = std::string(MEDIA_PATH) + video_files[i];
    std::string extension = full_path.substr(full_path.find_last_of('.') + 1);
    
    for (auto& c : extension) c = std::tolower(c);

    if (valid_video_endings.count(extension)) {
        start_selected_video();
    } else if (valid_audio_endings.count(extension)) {
        start_selected_audio();
    } else {
        printf("[Menu] Unsupported file type: %s\n", extension.c_str());
    }
}

void ui_menu_input(VPADStatus* buf) {
    int total_items = static_cast<int>(video_files.size());
    int total_pages = (total_items + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;

    int start = current_page_file_browser * ITEMS_PER_PAGE;
    int end = std::min(start + ITEMS_PER_PAGE, total_items);

    int local_index = selected_index - start;
    int row = local_index / GRID_COLS;
    int col = local_index % GRID_COLS;

    if (buf->trigger == VPAD_BUTTON_UP || buf->trigger == VPAD_STICK_L_EMULATION_UP) {
        if (row > 0) {
            selected_index -= GRID_COLS;
        }
    } else if (buf->trigger == VPAD_BUTTON_DOWN || buf->trigger == VPAD_STICK_L_EMULATION_DOWN) {
        if ((row + 1) * GRID_COLS < (end - start)) {
            selected_index += GRID_COLS;
        }
    } else if (buf->trigger == VPAD_BUTTON_LEFT || buf->trigger == VPAD_STICK_L_EMULATION_LEFT) {
        if (col > 0) {
            selected_index--;
        } else if (current_page_file_browser > 0) {
            current_page_file_browser--;
            start = current_page_file_browser * ITEMS_PER_PAGE;
            end = std::min(start + ITEMS_PER_PAGE, total_items);
            selected_index = start + std::min(row * GRID_COLS + (GRID_COLS - 1), end - start - 1);
        }
    } else if (buf->trigger == VPAD_BUTTON_RIGHT || buf->trigger == VPAD_STICK_L_EMULATION_RIGHT) {
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
    } else if (buf->trigger == VPAD_BUTTON_A) {
        start_file(selected_index);
    } else if (buf->trigger == VPAD_BUTTON_L) {
        if (current_page_file_browser > 0) {
            current_page_file_browser--;
            selected_index = current_page_file_browser * ITEMS_PER_PAGE;
        }
    } else if (buf->trigger == VPAD_BUTTON_R) {
        if (current_page_file_browser < total_pages - 1) {
            current_page_file_browser++;
            selected_index = current_page_file_browser * ITEMS_PER_PAGE;
        }
    } else if (buf->trigger == VPAD_BUTTON_MINUS) {
        scan_directory(MEDIA_PATH, video_files);
        selected_index = 0;
        current_page_file_browser = 0;
    } else if(buf->trigger == VPAD_BUTTON_PLUS) {
        *ui_app_state = STATE_SETTINGS;
    }
}

void ui_settings_input(VPADStatus* buf) {
    if(buf->trigger == VPAD_BUTTON_PLUS || buf->trigger == VPAD_BUTTON_B) {
        *ui_app_state = STATE_MENU;
    }
}

void ui_video_player_input(VPADStatus* buf) {
    if (buf->trigger == VPAD_BUTTON_A) {
        audio_player_audio_play(!video_player_is_playing());
        video_player_play(!video_player_is_playing());
        SDL_RenderClear(ui_renderer);
    } else if (buf->trigger == VPAD_BUTTON_B) {
        audio_player_audio_play(true);
        video_player_play(true);
        video_player_cleanup();
        audio_player_audio_play(false);
        video_player_play(false);
        scan_directory(MEDIA_PATH, video_files);
        *ui_app_state = STATE_MENU;
    } else if (buf->trigger == VPAD_BUTTON_LEFT) {
        // video_player_seek(-5.0f);
    } else if (buf->trigger == VPAD_BUTTON_RIGHT) {
        // video_player_seek(5.0f);
    }
}

void ui_audio_player_input(VPADStatus* buf) {
    if (buf->trigger == VPAD_BUTTON_A) {
        audio_player_audio_play(!audio_player_get_audio_play_state());
    } else if (buf->trigger == VPAD_BUTTON_B) {
        audio_player_cleanup();
        scan_directory(MEDIA_PATH, video_files);
        *ui_app_state = STATE_MENU;
    } else if (buf->trigger == VPAD_BUTTON_LEFT) {
        // audio_player_seek(-5.0f);
    } else if (buf->trigger == VPAD_BUTTON_RIGHT) {
        // audio_player_seek(5.0f);
    }
}

void ui_handle_vpad_input() {
    VPADStatus buf;
    VPADTouchData touchpoint_calibrated;
    int key_press = VPADRead(VPAD_CHAN_0, &buf, 1, nullptr);

    if(!key_press) return;
    
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touchpoint_calibrated, &buf.tpNormal);
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
    
    switch(*ui_app_state) {
        case STATE_PLAYING_VIDEO: ui_video_player_input(&buf); break;
        case STATE_PLAYING_AUDIO: ui_audio_player_input(&buf); break;
        case STATE_MENU: ui_menu_input(&buf); break;
        case STATE_SETTINGS: ui_settings_input(&buf); break;
    }
}

void ui_handle_ambiance() {
    if (!ambiance_playing && background_music_enabled) {
        audio_player_init(AMBIANCE_PATH);
        audio_player_audio_play(true);
        ambiance_playing = true;
    } else if((int)audio_player_get_current_play_time() == (int)audio_player_get_total_play_time() && ambiance_playing && background_music_enabled) {
        audio_player_audio_play(true);
        audio_player_cleanup();
        audio_player_audio_play(false);
        audio_player_init(AMBIANCE_PATH);
        audio_player_audio_play(true);
    } else if(ambiance_playing && !background_music_enabled) {
        audio_player_audio_play(true);
        audio_player_cleanup();
        audio_player_audio_play(false);
    }
}

void ui_render() {
    nk_input_begin(ctx);

    ui_handle_vpad_input();

    nk_input_end(ctx);

    switch(*ui_app_state) {
        case STATE_PLAYING_VIDEO:
        ui_render_video_player();
        break;
        case STATE_PLAYING_AUDIO:
        ui_render_audio_player();
        break;
        case STATE_MENU:
        ui_handle_ambiance();
        ui_render_file_browser();
        break;
        case STATE_SETTINGS:
        ui_handle_ambiance();
        ui_render_settings();
        break;
    }

    nk_sdl_render(NK_ANTI_ALIASING_ON);
}

void ui_render_settings() {
    if (nk_begin(ctx, VERSION_STRING, 
                 nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), 
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_TITLE)) {

        nk_layout_row_dynamic(ctx, 64 * UI_SCALE, 1);
        nk_label(ctx, "Settings", NK_TEXT_LEFT);

        nk_layout_row_dynamic(ctx, 64 * UI_SCALE, 3);
        if (nk_button_label(ctx, background_music_enabled ? "Background Music: On" : "Background Music: Off")) {
            background_music_enabled = !background_music_enabled;
        }
        nk_end(ctx);
    }

    ui_render_tooltip(current_page_file_browser, ui_app_state);
}

void ui_render_tooltip(int _current_page_file_browser, AppState* _app_state) {
    if (nk_begin(ctx, "tooltip_bar", nk_rect(0, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE, SCREEN_WIDTH, TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BORDER | NK_WINDOW_BACKGROUND)) {
        switch(*_app_state) {
            case STATE_PLAYING_VIDEO:
            break;
            case STATE_PLAYING_AUDIO:
            break;
            case STATE_MENU:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(A) Start (-) Refresh (+) Settings", NK_TEXT_LEFT);
            nk_label(ctx, ("[L]/[R] Page " + std::to_string(_current_page_file_browser + 1)).c_str(), NK_TEXT_RIGHT);
            break;
            case STATE_SETTINGS:
            nk_layout_row_dynamic(ctx, TOOLTIP_BAR_HEIGHT * UI_SCALE, 2);
            nk_label(ctx, "(A) Select (+) / (B) File browser & Save", NK_TEXT_LEFT);
            nk_label(ctx, ("[L]/[R] Page " + std::to_string(_current_page_file_browser + 1)).c_str(), NK_TEXT_RIGHT);
            break;
        }

        nk_end(ctx);
    }
}

void ui_render_file_browser() {
    if (nk_begin(ctx, VERSION_STRING, nk_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - TOOLTIP_BAR_HEIGHT * UI_SCALE), NK_WINDOW_NO_SCROLLBAR)) {

        nk_layout_row_dynamic(ctx, CELL_HEIGHT, GRID_COLS);

        int total_files = static_cast<int>(video_files.size());
        int start = current_page_file_browser * ITEMS_PER_PAGE;
        int end = std::min(start + ITEMS_PER_PAGE, total_files);

        size_t max_name_length = 15;

        for (int i = start; i < end; ++i) {
            std::string display_str = truncate_filename(video_files[i], max_name_length);

            struct nk_style_button button_style = ctx->style.button;
            if (i == selected_index) {
                ctx->style.button.border_color = nk_rgb(13, 146, 244);
                ctx->style.button.border = 4.0f;
            }

            if (nk_button_label(ctx, display_str.c_str())) {
                selected_index = i;
                start_file(selected_index);
            }

            ctx->style.button = button_style;
            if ((i - start + 1) % GRID_COLS == 0 && i + 1 < end) {
                nk_layout_row_dynamic(ctx, CELL_HEIGHT, GRID_COLS);
            }
        }

        nk_end(ctx);
    }

    // Render tooltip or other UI elements
    ui_render_tooltip(current_page_file_browser, ui_app_state);
}

void ui_render_player_hud(bool state, double current_time, double total_time) {
    const int hud_height = 80 * UI_SCALE;
    struct nk_rect hud_rect = nk_rect(0, SCREEN_HEIGHT - hud_height, SCREEN_WIDTH, hud_height);

    if (nk_begin(ctx, "HUD", hud_rect, NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_BORDER)) {
        nk_layout_row_dynamic(ctx, (hud_height / 2) - 5, 1);
        nk_size progress = static_cast<nk_size>(current_time);
        nk_progress(ctx, &progress, total_time, NK_FIXED);

        nk_layout_row_dynamic(ctx, hud_height / 2, 2);
        size_t max_filename_length = 10;

        std::string filename = truncate_filename(video_files[selected_index].c_str(), max_filename_length);
        std::string hud_str = state ? "> " : "|| ";
        hud_str += format_time(current_time);
        hud_str += " / ";
        hud_str += format_time(total_time);
        hud_str += " Playing: ";
        hud_str += filename;

        nk_label(ctx, hud_str.c_str(), NK_TEXT_LEFT);
        nk_end(ctx);
    }
}

void ui_render_video_player() {
    SDL_RenderClear(ui_renderer);
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

    if(video_player_get_current_time() == video_player_get_total_play_time()) {
        audio_player_audio_play(true);
        video_player_play(true);
        video_player_cleanup();
        audio_player_audio_play(false);
        video_player_play(false);
        scan_directory(MEDIA_PATH, video_files);
        *ui_app_state = STATE_MENU;
    }

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

    if (!video_player_is_playing() || touched) ui_render_player_hud(video_player_is_playing(), video_player_get_current_time(), video_player_get_total_play_time());
}

void ui_render_audio_player() {
    SDL_RenderClear(ui_renderer);

    if((int)audio_player_get_current_play_time() == (int)audio_player_get_total_play_time()) {
        audio_player_audio_play(true);
        audio_player_cleanup();
        audio_player_audio_play(false);
        scan_directory(MEDIA_PATH, video_files);
        *ui_app_state = STATE_MENU;
    }
    ui_render_player_hud(audio_player_get_audio_play_state(), audio_player_get_current_play_time(), audio_player_get_total_play_time());
}

void ui_shutdown() {
    if (ambiance_playing) { audio_player_cleanup(); ambiance_playing = false; }
    if(!video_player_is_playing()) video_player_play(true);
    if (video_player_is_playing()) video_player_cleanup();

    if (ui_texture) {
        SDL_DestroyTexture(ui_texture);
        ui_texture = nullptr;
    }
    nk_sdl_shutdown();
}