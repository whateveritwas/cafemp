#include <string>
#include <vector>
#include <dirent.h>
#include <unordered_set>
#include <SDL2/SDL_image.h>

#include "utils.hpp"
#include "utils/app_state.hpp"
#include "media_files.hpp"
#include "player/audio_player.hpp"
#include "player/video_player.hpp"

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

bool valid_file_ending(const std::string& file_ending) {
    AppState state = app_state_get();
    bool result = false;

    switch (state) {
        case STATE_MENU_VIDEO_FILES:
            result = valid_video_endings.count(file_ending) > 0;
            break;
        case STATE_MENU_AUDIO_FILES:
            result = valid_audio_endings.count(file_ending) > 0;
            break;
        case STATE_MENU_IMAGE_FILES:
            result = valid_image_endings.count(file_ending) > 0;
            break;
        case STATE_MENU_PDF_FILES:
            result = valid_pdf_ending.count(file_ending) > 0;
            break;
        default:
            break;
    }
    return result;
}

void scan_directory(const char* path) {
    clear_media_files();
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
                add_media_file(name);
            }
        }
    }    
    closedir(dir);
}

SDL_Rect calculate_aspect_fit_rect(int media_w, int media_h) {
    int new_w = SCREEN_WIDTH;
    int new_h = (media_h * new_w) / media_w;

    if (new_h > SCREEN_HEIGHT) {
        new_h = SCREEN_HEIGHT;
        new_w = (media_w * new_h) / media_h;
    }

    SDL_Rect rect;
    rect.w = new_w;
    rect.h = new_h;
    rect.x = (SCREEN_WIDTH - new_w) / 2;
    rect.y = (SCREEN_HEIGHT - new_h) / 2;

    return rect;
}

void draw_checkerboard_pattern(SDL_Renderer* renderer, int width, int height, int cell_size) {
    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
    bool toggle = false;
    for (int y = 0; y < height; y += cell_size) {
        toggle = !toggle;
        for (int x = 0; x < width; x += cell_size) {
            if (toggle) {
                SDL_Rect cell = { x, y, cell_size, cell_size };
                SDL_RenderFillRect(renderer, &cell);
            }
            toggle = !toggle;
        }
    }
}
