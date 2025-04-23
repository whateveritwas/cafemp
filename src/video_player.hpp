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
void video_player_scrub(int dt);
bool video_player_is_playing();
void video_player_play(bool new_state);
int64_t video_player_get_current_time();
frame_info* video_player_get_current_frame_info();
int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture);
void video_player_start(const char* path, AppState* app_state, SDL_Renderer& renderer, SDL_Texture*& texture);
void start_video_decoding_thread();
void process_video_frame_thread();
void render_video_frame(AppState* app_state, SDL_Renderer* renderer);
void video_player_update(AppState* app_state, SDL_Renderer* renderer);
void stop_video_decoding_thread();
int video_player_cleanup();

#endif