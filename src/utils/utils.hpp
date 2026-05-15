#ifndef UTILS_H
#define UTILS_H

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <unordered_set>

#include "main.hpp"

struct Rect {
    float x;
    float y;
    float w;
    float h;
};

static Rect calculate_aspect_fit_rect( int media_w, int media_h) {
    float scale_x = (float)SCREEN_WIDTH / media_w;
    float scale_y = (float)SCREEN_HEIGHT / media_h;

    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    Rect r;

    r.w = media_w * scale;
    r.h = media_h * scale;

    r.x = (SCREEN_WIDTH - r.w) * 0.5f;
    r.y = (SCREEN_HEIGHT - r.h) * 0.5f;

    return r;
}

std::string format_time(int seconds);
std::string truncate_filename(const std::string& name, size_t max_length);
bool valid_file_ending(const std::string& file_ending);
void start_selected_video(int selected_index);
void start_selected_audio(int selected_index);
void start_selected_photo(int selected_index);
void draw_checkerboard_pattern(SDL_Renderer* renderer, int width, int height, int cell_size);

#endif
