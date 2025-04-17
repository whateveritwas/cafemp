#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>

void scan_directory(const char* path, std::vector<std::string>& video_files);
std::string format_time(int seconds);
void render_file_browser(SDL_Renderer* renderer, TTF_Font* font, int selected_index, const std::vector<std::string>& video_files);
void render_video_hud(SDL_Renderer* renderer, TTF_Font* font, SDL_Texture* texture, int current_pts_seconds, int duration_seconds);

#endif