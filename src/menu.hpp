#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>

#include "main.hpp"

std::string format_time(int seconds);
bool valid_file_ending(const std::string& file_ending);

void scan_directory(const char* path, std::vector<std::string>& video_files);
void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture, AppState* _app_state);
void ui_render();
void ui_render_file_browser();
void ui_render_video();
void ui_render_console();
void ui_shutodwn();

#endif