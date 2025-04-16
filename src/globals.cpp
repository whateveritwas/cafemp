#include "config.hpp"

uint8_t ring_buffer[RING_BUFFER_SIZE];
int ring_buffer_write_pos = 0;
int ring_buffer_read_pos = 0;
int ring_buffer_fill = 0;

SDL_mutex* audio_mutex = NULL;
SDL_AudioSpec wanted_spec;
AVFormatContext* fmt_ctx = NULL;
AVCodecContext* audio_codec_ctx = NULL;
AVCodecContext* video_codec_ctx = NULL;
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;
SwrContext* swr_ctx = NULL;
AVPacket* pkt = NULL;
AVFrame* frame = NULL;
int audio_stream_index = -1;
int video_stream_index = -1;
AVRational framerate;

const char* filename = "/vol/external01/wiiu/apps/cafemp/test.mp4";
bool playing_video = true;