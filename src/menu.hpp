#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>

#include "config.hpp"

std::string format_time(int seconds);

void scan_directory(const char* path, std::vector<std::string>& video_files);
void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture, AppState* _app_state, SDL_AudioSpec _wanted_spec);
void ui_render();
void ui_render_file_browser();
void ui_render_video();
void ui_render_video_hud(SDL_Renderer* renderer, TTF_Font* font, SDL_Texture* texture, int current_pts_seconds, int duration_seconds);
void ui_shutodwn();

#endif