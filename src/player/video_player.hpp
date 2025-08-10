#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavutil/time.h>
}
#include "main.hpp"

int video_player_init(const char* filepath);
void video_player_play(bool play);
void video_player_seek(double seconds);
void video_player_update();
void video_player_cleanup();

#endif
