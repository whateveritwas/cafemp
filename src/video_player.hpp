#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavutil/time.h>
}
#include "config.hpp"

void audio_callback(void *userdata, Uint8 *stream, int len);
void play_audio_frame(AVFrame* frame, SwrContext* swr_ctx, int out_channels);
SDL_AudioSpec create_audio_spec();
AVCodecContext* create_codec_context(AVFormatContext* fmt_ctx, int stream_index, bool video);
inline AVFrame* convert_nv12_to_yuv420p(AVFrame* src_frame);

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture* &texture);
void video_player_start(const char* path, AppState* app_state, SDL_Renderer& renderer, SDL_Texture*& texture, SDL_mutex& _audio_mutex, SDL_AudioSpec wanted_spec);
void video_player_scrub(int dt);
int64_t video_player_get_current_time();
bool video_player_is_playing();
void video_player_play(bool new_state);
frame_info* video_player_get_current_frame_info();
void process_audio_frame(AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture, AVPacket* pkt);
void process_video_frame(AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture, AVPacket* pkt, uint64_t* last_frame_ticks);
void video_player_update(AppState* app_state, SDL_Renderer* renderer);
int video_player_cleanup();

void process_video_frame(AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture, AVPacket* pkt, uint64_t* last_frame_ticks);
void render_video_frame(AppState* app_state, SDL_Renderer* renderer);
void process_video_frame_thread();
void stop_video_decoding_thread();
void start_video_decoding_thread();

#endif