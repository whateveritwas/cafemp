#ifndef UTILS_H
#define UTILS_H

#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <unordered_set>

static const std::unordered_set<std::string> valid_video_endings = {
    "mp4", "mov", "avi", "mkv"
};

static const std::unordered_set<std::string> valid_audio_endings = {
    "mp3", "wav", "ogg", "flac", "aac"
};

static const std::unordered_set<std::string> valid_image_endings = {
    "png", "jpg", "gif", "tga", "bmp"
};

std::string format_time(int seconds);
std::string truncate_filename(const std::string& name, size_t max_length);
bool valid_file_ending(const std::string& file_ending);
void scan_directory(const char* path);
void start_file(int index);
void start_selected_video(int selected_index);
void start_selected_audio(int selected_index);
void start_selected_photo(int selected_index);
SDL_Rect calculate_aspect_fit_rect(int media_w, int media_h);

#endif
