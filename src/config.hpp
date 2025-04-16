#ifndef CONFIG_H
#define CONFIG_H

#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#define DRC_BUTTON_A 0x00008000
#define RING_BUFFER_SIZE (192000 * 4)

extern uint8_t ring_buffer[RING_BUFFER_SIZE];
extern int ring_buffer_write_pos;
extern int ring_buffer_read_pos;
extern int ring_buffer_fill;

extern SDL_mutex* audio_mutex;
extern SDL_AudioSpec wanted_spec;
extern AVFormatContext* fmt_ctx;
extern AVCodecContext* audio_codec_ctx;
extern AVCodecContext* video_codec_ctx;
extern SDL_Window* window;
extern SDL_Renderer* renderer;
extern SDL_Texture* texture;
extern SwrContext* swr_ctx;
extern AVPacket* pkt;
extern AVFrame* frame;
extern int audio_stream_index;
extern int video_stream_index;
extern AVRational framerate;

const int screen_width = 1280;
const int screen_height = 720;
const double audio_sample_rate = 48000;
extern const char* filename;
extern bool playing_video;

#endif