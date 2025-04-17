#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#include "config.hpp"

void audio_callback(void *userdata, Uint8 *stream, int len);
void play_audio_frame(AVFrame* frame, SwrContext* swr_ctx, int out_channels);
SDL_AudioSpec create_audio_spec();
AVCodecContext* create_codec_context(AVFormatContext* fmt_ctx, int stream_index);

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture* &texture);
void video_player_start(const char* path, AppState* app_state, SDL_Renderer& renderer, SDL_Texture*& texture, SDL_mutex& _audio_mutex, SDL_AudioSpec wanted_spec);
void video_player_scrub(int dt);
int64_t video_player_get_current_time();
void video_player_update(AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture);
int video_player_cleanup();

#endif