#ifndef MAIN_H
#define MAIN_H

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

#define FONT_PATH "/vol/content/Roboto-Regular.ttf"
#define AMBIANCE_PATH "/vol/content/769925__lightmister__game-main-menu-fluids.mp3"
#define VIDEO_PATH "/vol/external01/wiiu/apps/cafemp/"

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