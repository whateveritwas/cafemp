#ifndef UI_FILEBROWSER_H
#define UI_FILEBROWSER_H

#include "input/input_actions.hpp"

void scene_file_browser_scan_directory(const char *path);
void scene_file_browser_input(InputState& input);
void scene_file_browser_render();

#endif
