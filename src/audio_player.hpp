#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/opt.h>
    #include <libavutil/channel_layout.h>
}

typedef struct AudioPlayer {
    AVFormatContext* fmt_ctx;
    AVCodecContext* audio_codec_ctx;
    SwrContext* swr_ctx;
    AVFrame* audio_frame;
    AVPacket* audio_packet;
    uint8_t* audio_buf;
    int audio_buf_size;
    int audio_buf_index;
    int audio_stream_index;
    SDL_AudioDeviceID device_id;
    bool playing;
} AudioPlayer;


int audio_player_init(AudioPlayer* player, const char* filepath);
void audio_player_play(AudioPlayer* player, bool play);
void audio_player_decode_audio_frame(AudioPlayer* player);
void audio_player_cleanup(AudioPlayer* player);

#endif
