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

#define VERSION_STRING "café media player v0.4.6 " __DATE__ " " __TIME__

enum AppState {
    STATE_MENU,
    STATE_PLAYING_VIDEO,
    STATE_PLAYING_AUDIO,
    STATE_SETTINGS
};

struct frame_info {
    SDL_Texture* texture;
    int frame_width;
    int frame_height;
    int64_t total_time;
};

#endif