#ifndef MAIN_H
#define MAIN_H

#include <SDL2/SDL.h>

// #define SCREEN_WIDTH 1920
// #define SCREEN_HEIGHT 1080
// #define UI_SCALE 1.5

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define UI_SCALE 1.2

#define FONT_PATH "/vol/content/Roboto-Regular.ttf"
#define AMBIANCE_PATH "/vol/content/769925__lightmister__game-main-menu-fluids.mp3"
#define MEDIA_PATH "/vol/external01/wiiu/apps/cafemp/"
#define SETTINGS_PATH "/vol/external01/wiiu/apps/cafemp/settings.json"

#define VERSION_STRING_NUMBER "v0.5.1"

#ifdef DEBUG
#define VERSION_STRING "café media player " VERSION_STRING_NUMBER " " __DATE__ " " __TIME__
#else
#define VERSION_STRING "café media player " VERSION_STRING_NUMBER
#endif

#define TOOLTIP_BAR_HEIGHT (48)
#define GRID_COLS 4
#define GRID_ROWS 3
#define ITEMS_PER_PAGE (GRID_COLS * GRID_ROWS)
#define CELL_HEIGHT ((SCREEN_HEIGHT - (TOOLTIP_BAR_HEIGHT / GRID_ROWS)) / GRID_ROWS)

struct frame_info {
    SDL_Texture* texture;
    int frame_width;
    int frame_height;
};

#endif