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
double audio_player_get_current_play_time();
double audio_player_get_total_play_time();
void audio_player_audio_play(bool state);
bool audio_player_get_audio_play_state();
void audio_player_cleanup();

extern bool audio_enabled;

#endif
