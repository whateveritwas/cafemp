#ifndef CONFIG_H
#define CONFIG_H

#include <SDL2/SDL.h>

#define DRC_BUTTON_A 0x00008000
#define DRC_BUTTON_START 0x00000008
#define DRC_BUTTON_SELECT 0x00000004
#define DRC_BUTTON_DPAD_LEFT 0x00000800
#define DRC_BUTTON_DPAD_RIGHT 0x00000400
#define DRC_BUTTON_DPAD_UP 0x00000200
#define DRC_BUTTON_DPAD_DOWN 0x00000100

#define RING_BUFFER_SIZE (192000*4)

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define AUDIO_SAMPLE_RATE 48000

#define FONT_PATH "/vol/external01/wiiu/apps/cafemp/Roboto-Regular.ttf"
#define VIDEO_PATH "/vol/external01/wiiu/apps/cafemp/"

enum AppState {
    STATE_MENU,
    STATE_PLAYING
};

struct frame_info {
    SDL_Texture* texture;
    int frame_width;
    int frame_height;
    int total_time;
    int current_time;
};

#endif