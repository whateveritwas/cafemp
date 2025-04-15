#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <coreinit/debug.h>

extern "C" {
#include <SDL2/SDL.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#define OS_PRINTF_BUF_SIZE 2048

#define OSPrintf(fmt, ...) do { \
    char __os_printf_buf[OS_PRINTF_BUF_SIZE]; \
    int __os_printf_len = snprintf(__os_printf_buf, OS_PRINTF_BUF_SIZE, fmt, ##__VA_ARGS__); \
    if (__os_printf_len > 0) { \
        OSConsoleWrite(__os_printf_buf, __os_printf_len); \
    } \
} while (0)

uint8_t audio_buffer[192000];
int audio_buffer_len = 0;
int audio_buffer_pos = 0;

void audio_callback(void *userdata, Uint8 *stream, int len) {
    if (audio_buffer_len == 0) {
        SDL_memset(stream, 0, len);
        return;
    }

    int to_copy = (audio_buffer_len - audio_buffer_pos > len) ? len : audio_buffer_len - audio_buffer_pos;
    SDL_memcpy(stream, audio_buffer + audio_buffer_pos, to_copy);
    audio_buffer_pos += to_copy;

    if (audio_buffer_pos >= audio_buffer_len) {
        audio_buffer_len = 0;
        audio_buffer_pos = 0;
    }
}

void play_audio_frame(AVFrame* frame, SwrContext* swr_ctx, int out_channels) {
    uint8_t* out_buf = audio_buffer;
    int max_out_samples = av_rescale_rnd(frame->nb_samples, 48000, frame->sample_rate, AV_ROUND_UP);

    int out_samples = swr_convert(swr_ctx, &out_buf, max_out_samples,
                                  (const uint8_t**)frame->data, frame->nb_samples);

    audio_buffer_len = out_samples * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    audio_buffer_pos = 0;
}

int main(int argc, char ** argv) {
    const int screen_width = 854;
    const int screen_height = 480;
    const char* filename = "/vol/external01/wiiu/apps/cafemp/test.mp4";

    WHBProcInit();
    OSPrintf("Starting...\n");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0) {
        OSPrintf("Failed to init SDL: %s\n", SDL_GetError());
        return -1;
    }

    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = 48000;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 1024;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = NULL;

    if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
        OSPrintf("SDL_OpenAudio error: %s\n", SDL_GetError());
        return -1;
    }

    avformat_network_init();
    AVFormatContext* fmt_ctx = NULL;
    if (avformat_open_input(&fmt_ctx, filename, NULL, NULL) != 0) {
        OSPrintf("Could not open file.\n");
        return -1;
    }

    avformat_find_stream_info(fmt_ctx, NULL);
    int audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_stream_index < 0) {
        OSPrintf("No audio stream found.\n");
        return -1;
    }

    int video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_stream_index < 0) {
        OSPrintf("No video stream found.\n");
        return -1;
    }

    // Audio codec context
    AVCodecParameters* audio_codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    AVCodec* audio_codec = avcodec_find_decoder(audio_codecpar->codec_id);
    AVCodecContext* audio_codec_ctx = avcodec_alloc_context3(audio_codec);
    avcodec_parameters_to_context(audio_codec_ctx, audio_codecpar);
    avcodec_open2(audio_codec_ctx, audio_codec, NULL);

    // Video codec context
    AVCodecParameters* video_codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    AVCodec* video_codec = avcodec_find_decoder(video_codecpar->codec_id);
    AVCodecContext* video_codec_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_codec_ctx, video_codecpar);
    avcodec_open2(video_codec_ctx, video_codec, NULL);

    
    SDL_Window* window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_width, screen_height, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, screen_width, screen_height);
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, video_codec_ctx->width, video_codec_ctx->height);

    if (!texture) {
        OSPrintf("Error creating texture: %s\n", SDL_GetError());
        return -1;
    }

    // Setup swr context for audio resampling
    SwrContext* swr_ctx = swr_alloc_set_opts(NULL,
        AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, 48000,
        audio_codec_ctx->channel_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
        0, NULL);
    swr_init(swr_ctx);

    // Frame and packet
    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* frameRGB = av_frame_alloc();

    // Setup sws context for converting to YUV420
    struct SwsContext* sws_ctx = sws_getContext(screen_width, screen_height, AV_PIX_FMT_YUV420P, screen_width, screen_height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, video_codec_ctx->width, video_codec_ctx->height, 1);
    uint8_t* buffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer,
                         AV_PIX_FMT_YUV420P, video_codec_ctx->width, video_codec_ctx->height, 1);

    SDL_PauseAudio(0); // Start audio playback

    while (WHBProcIsRunning() && av_read_frame(fmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == audio_stream_index) {
            if (avcodec_send_packet(audio_codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                    play_audio_frame(frame, swr_ctx, 2);
                    while (audio_buffer_len > 0) {
                        OSSleepTicks(OSMillisecondsToTicks(5));
                    }
                }
            }
        }

        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(video_codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(video_codec_ctx, frame) == 0) { // only runs once?
                    // Convert frame to YUV420
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, video_codec_ctx->height,
                              frameRGB->data, frameRGB->linesize);

                    if (frame->format != AV_PIX_FMT_YUV420P) {
                        OSPrintf("Unexpected pixel format: %d\n", frame->format);
                        return -1;
                    }

                    if (frame->format == AV_PIX_FMT_YUV420P) {
                        SDL_UpdateYUVTexture(texture, NULL,
                            frame->data[0], frame->linesize[0],
                            frame->data[1], frame->linesize[1],
                            frame->data[2], frame->linesize[2]);
                    
                        SDL_RenderClear(renderer);
                        SDL_RenderCopy(renderer, texture, NULL, NULL);
                        SDL_RenderPresent(renderer);
                    } else {
                        OSPrintf("Unexpected format: %d\n", frame->format);
                    }      
                }
            }
        }

        av_packet_unref(pkt);
    }

    SDL_CloseAudio();

    // Clean up
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    av_packet_free(&pkt);
    swr_free(&swr_ctx);
    avcodec_free_context(&audio_codec_ctx);
    avcodec_free_context(&video_codec_ctx);
    avformat_close_input(&fmt_ctx);
    SDL_Quit();

    OSPrintf("Exiting...\n");
    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}