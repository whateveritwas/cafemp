#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>

#include "main.hpp"

std::string format_time(int seconds);
bool valid_file_ending(const std::string& file_ending);

void scan_directory(const char* path);
void ui_init(SDL_Window* _window, SDL_Renderer* _renderer, SDL_Texture* &_texture);
void ui_render();
void ui_render_settings();
void ui_render_file_browser();
void ui_render_video_player();
void ui_render_audio_player();
void ui_render_player_hud(bool state, double current_time, double total_time);
void ui_render_tooltip(int _current_page);
void ui_render_console();
void ui_shutdown();

#endif