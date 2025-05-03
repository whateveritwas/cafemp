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

AVCodecContext* video_player_create_codec_context(AVFormatContext* fmt_ctx, int stream_index);
void video_player_seek(float delta_time);
bool video_player_is_playing();
void video_player_play(bool new_state);
int64_t video_player_get_current_time();
frame_info* video_player_get_current_frame_info();
int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture);
void video_player_start(const char* path, SDL_Renderer& renderer, SDL_Texture*& texture);
void start_video_decoding_thread();
void process_video_frame_thread();
int64_t video_player_get_total_play_time();
void render_video_frame(SDL_Renderer* renderer);
void video_player_update(SDL_Renderer* renderer);
void stop_video_decoding_thread();
int video_player_cleanup();

#endif