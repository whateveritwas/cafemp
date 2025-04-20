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


#define AUDIO_BUFFER_SIZE 8192
#define RING_BUFFER_SIZE (AUDIO_BUFFER_SIZE * 4)

struct AudioPlayer {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* audio_codec_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;
    AVPacket* audio_packet = nullptr;
    AVFrame* audio_frame = nullptr;
    uint8_t* audio_buf = nullptr;
    int audio_buf_size = 0;
    int audio_buf_index = 0;
    int audio_stream_index = -1;
    bool playing = false;
    SDL_AudioDeviceID device_id;
    uint64_t current_audio_pts = 0;
};

int audio_player_init(const char* filepath);
void audio_player_update();
void audio_player_cleanup();

#endif
