#ifndef PHOTO_VIEWER_H
#define PHOTO_VIEWER_H

#include <SDL2/SDL.h>

void photo_viewer_init();
void photo_viewer_open_picture(const char* filepath);
void photo_texture_zoom(float delta_zoom);
void photo_viewer_pan(int delta_x, int delta_y);
void photo_viewer_render();
void photo_viewer_cleanup();

#endif
