#ifndef MENU_H
#define MENU_H

#include <vector>
#include <string>

#include "utils/media_info.hpp"
#include "main.hpp"

std::string format_time(int seconds);
bool valid_file_ending(const std::string& file_ending);

void scan_directory(const char* path);
void ui_init();
void ui_render();
void ui_render_settings();
void ui_render_file_browser();
void ui_render_video_player();
void ui_shutdown();

#endif
