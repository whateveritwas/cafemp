#include <cstdint>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include "video_player.hpp"
#include "config.hpp"
//#include "menu.hpp"

int audio_stream_index = -1;
int video_stream_index = -1;
AVFormatContext* fmt_ctx = NULL;
AVCodecContext* audio_codec_ctx = NULL;
AVCodecContext* video_codec_ctx = NULL;
SwrContext* swr_ctx = NULL;
AVPacket* pkt = NULL;
AVFrame* frame = NULL;
AVRational framerate;
SDL_mutex* audio_mutex;

uint8_t ring_buffer[RING_BUFFER_SIZE];
int ring_buffer_write_pos = 0;
int ring_buffer_read_pos = 0;
int ring_buffer_fill = 0;

void audio_callback(void *userdata, Uint8 *stream, int len) {
    SDL_LockMutex(audio_mutex);

    int bytes_to_copy = (len > ring_buffer_fill) ? ring_buffer_fill : len;

    int first_chunk = RING_BUFFER_SIZE - ring_buffer_read_pos;
    if (first_chunk > bytes_to_copy) first_chunk = bytes_to_copy;

    SDL_memcpy(stream, ring_buffer + ring_buffer_read_pos, first_chunk);
    SDL_memcpy(stream + first_chunk, ring_buffer, bytes_to_copy - first_chunk);

    ring_buffer_read_pos = (ring_buffer_read_pos + bytes_to_copy) % RING_BUFFER_SIZE;
    ring_buffer_fill -= bytes_to_copy;

    if (bytes_to_copy < len) {
        SDL_memset(stream + bytes_to_copy, 0, len - bytes_to_copy);
    }

    SDL_UnlockMutex(audio_mutex);
}

void play_audio_frame(AVFrame* frame, SwrContext* swr_ctx, int out_channels) {
    uint8_t temp_buffer[8192];
    uint8_t* out_buffers[1] = { temp_buffer };

    int out_samples = swr_convert(
        swr_ctx,
        out_buffers,
        sizeof(temp_buffer) / (out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)),
        (const uint8_t**)frame->data,
        frame->nb_samples
    );

    if (out_samples < 0) {
        fprintf(stderr, "Error while converting audio.\n");
        return;
    }

    int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    int data_size = out_samples * out_channels * bytes_per_sample;

    SDL_LockMutex(audio_mutex);

    if (data_size <= RING_BUFFER_SIZE - ring_buffer_fill) {
        int first_chunk = RING_BUFFER_SIZE - ring_buffer_write_pos;
        if (first_chunk > data_size) first_chunk = data_size;

        memcpy(ring_buffer + ring_buffer_write_pos, temp_buffer, first_chunk);
        memcpy(ring_buffer, temp_buffer + first_chunk, data_size - first_chunk);

        ring_buffer_write_pos = (ring_buffer_write_pos + data_size) % RING_BUFFER_SIZE;
        ring_buffer_fill += data_size;
    }

    SDL_UnlockMutex(audio_mutex);
}

SDL_AudioSpec create_audio_spec() {
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = AUDIO_SAMPLE_RATE;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 1024;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = NULL;
    return wanted_spec;
}

AVCodecContext* create_codec_context(AVFormatContext* fmt_ctx, int stream_index) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        printf("Failed to open codec.\n");
        return NULL;
    }
    return codec_ctx;
}

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture* &texture) {
    printf("Starting Video Player...\n");

    if (avformat_open_input(&fmt_ctx, filepath, NULL, NULL) != 0) {
        printf("Could not open file: %s\n", filepath);
        return -1;
    }

    avformat_find_stream_info(fmt_ctx, NULL);

    audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (audio_stream_index < 0 || video_stream_index < 0) {
        printf("Could not find both audio and video streams.\n");
        return -1;
    }

    audio_codec_ctx = create_codec_context(fmt_ctx, audio_stream_index);
    video_codec_ctx = create_codec_context(fmt_ctx, video_stream_index);

    if (!audio_codec_ctx || !video_codec_ctx) return -1;

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, video_codec_ctx->width, video_codec_ctx->height);

    framerate = fmt_ctx->streams[video_stream_index]->r_frame_rate;
    double frameRate = av_q2d(framerate);
    printf("FPS: %f\n", frameRate);

    swr_ctx = swr_alloc_set_opts(NULL,
        AV_CH_LAYOUT_STEREO, AV_SAMPLE_FMT_S16, AUDIO_SAMPLE_RATE,
        audio_codec_ctx->channel_layout, audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate,
        0, NULL);
    swr_init(swr_ctx);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    SDL_PauseAudio(0);

    return 0;
}

void video_player_start(const char* path, AppState* app_state, SDL_Renderer& renderer, SDL_Texture*& texture, SDL_mutex& _audio_mutex, SDL_AudioSpec wanted_spec) {
    audio_mutex = &_audio_mutex;
    video_player_init(path, &renderer, texture);
    *app_state = STATE_PLAYING;
}

void video_player_update(uint64_t current_pts_seconds, AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture) {
    uint64_t ticks_per_frame = OSMillisecondsToTicks(1000) / av_q2d(framerate);
    uint64_t last_frame_ticks = OSGetSystemTime();
    // int64_t duration_seconds = fmt_ctx->duration / AV_TIME_BASE;

    if ((av_read_frame(fmt_ctx, pkt)) >= 0) {
        if (pkt->stream_index == audio_stream_index) {
            if (avcodec_send_packet(audio_codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                    play_audio_frame(frame, swr_ctx, 2);
                }
            }
        }

        if (pkt->stream_index == video_stream_index) {
            if (avcodec_send_packet(video_codec_ctx, pkt) == 0) {
                while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                    if (frame->format == AV_PIX_FMT_YUV420P) {
                        AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
                        current_pts_seconds = frame->pts * av_q2d(time_base);

                        SDL_UpdateYUVTexture(texture, NULL,
                            frame->data[0], frame->linesize[0],
                            frame->data[1], frame->linesize[1],
                            frame->data[2], frame->linesize[2]);

                        uint64_t now_ticks = OSGetSystemTime();
                        uint64_t elapsed_ticks = now_ticks - last_frame_ticks;

                        if (elapsed_ticks < ticks_per_frame) {
                            OSSleepTicks(ticks_per_frame - elapsed_ticks);
                        }

                        last_frame_ticks = OSGetSystemTime();

                        SDL_RenderCopy(renderer, texture, NULL, NULL);
                        SDL_RenderPresent(renderer);
                    }
                }
            }
        }

        av_packet_unref(pkt);
    } else {
        printf("Video playback ended.");
        *app_state = STATE_MENU;
        video_player_cleanup();
    }
}

int video_player_cleanup() {
    avcodec_send_packet(audio_codec_ctx, NULL);
    while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
        play_audio_frame(frame, swr_ctx, 2);
    }
    while (ring_buffer_fill > 0) {
        SDL_Delay(100);
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);
    swr_free(&swr_ctx);
    avcodec_free_context(&audio_codec_ctx);
    avcodec_free_context(&video_codec_ctx);
    avformat_close_input(&fmt_ctx);
    return 0;
}