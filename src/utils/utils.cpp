#include <string>
#include <vector>
#include <dirent.h>
#include <unordered_set>
#include <SDL2/SDL_image.h>

#include "utils/utils.hpp"
#include "main.hpp"
#include "utils/app_state.hpp"
#include "media_files.hpp"

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
