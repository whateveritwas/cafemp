#ifndef UTILS_H
#define UTILS_H

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <unordered_set>

std::string format_time(int seconds);
std::string truncate_filename(const std::string& name, size_t max_length);
bool valid_file_ending(const std::string& file_ending);
void start_selected_video(int selected_index);
void start_selected_audio(int selected_index);
void start_selected_photo(int selected_index);
SDL_Rect calculate_aspect_fit_rect(int media_w, int media_h);
void draw_checkerboard_pattern(SDL_Renderer* renderer, int width, int height, int cell_size);

#endif
