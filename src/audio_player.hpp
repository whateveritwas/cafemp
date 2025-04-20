#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/frame.h>
}

#define RING_BUFFER_SIZE 65536 // 64KB

int audio_player_init(const char* filepath);
void audio_player_cleanup();

extern bool audio_enabled;

#endif
