#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <wut.h>

typedef struct {
    float x, y, w, h;
} rect;

typedef struct {
    uint16_t width, height;
    float scale;
} scan_mode;

rect display_calculate_aspect_fit(int width, int height);
scan_mode display_get();
void display_setup();

#endif
