#include <cstdint>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/frame.h>
}

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "config.hpp"
#include "video_player.hpp"

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

frame_info* current_frame_info;

uint8_t ring_buffer[RING_BUFFER_SIZE];
int ring_buffer_write_pos = 0;
int ring_buffer_read_pos = 0;
int ring_buffer_fill = 0;

int64_t current_pts_seconds = 0;
uint64_t ticks_per_frame = 0;

bool playing_video = false;

std::queue<AVFrame*> video_frame_queue;  // Queue to hold decoded video frames
std::mutex video_frame_mutex;            // Mutex to protect the queue
std::condition_variable video_frame_cv;  // Condition variable for synchronization
bool video_thread_running = true;        // Flag to control the video thread
std::thread video_thread;                // Thread for video decoding

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

AVCodecContext* create_codec_context(AVFormatContext* fmt_ctx, int stream_index, bool video = false) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    if(video) {
        codec_ctx->skip_frame = AVDISCARD_NONREF;
    }

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        printf("Failed to open codec.\n");
        return NULL;
    }
    return codec_ctx;
}

inline AVFrame* convert_nv12_to_yuv420p(AVFrame* src_frame) {
    struct SwsContext* sws_ctx = sws_getContext(
        src_frame->width, src_frame->height, AV_PIX_FMT_NV12,
        src_frame->width, src_frame->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL
    );

    if (!sws_ctx) {
        printf("Failed to create sws context.\n");
        return NULL;
    }

    AVFrame* dst_frame = av_frame_alloc();
    if (!dst_frame) {
        sws_freeContext(sws_ctx);
        return NULL;
    }

    dst_frame->format = AV_PIX_FMT_YUV420P;
    dst_frame->width = src_frame->width;
    dst_frame->height = src_frame->height;

    if (av_frame_get_buffer(dst_frame, 32) < 0) {
        printf("Could not allocate buffer for destination frame.\n");
        av_frame_free(&dst_frame);
        sws_freeContext(sws_ctx);
        return NULL;
    }

    sws_scale(
        sws_ctx,
        (const uint8_t* const*)src_frame->data, src_frame->linesize,
        0, src_frame->height,
        dst_frame->data, dst_frame->linesize
    );

    sws_freeContext(sws_ctx);
    return dst_frame;
}

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture) {
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
    video_codec_ctx = create_codec_context(fmt_ctx, video_stream_index, true);

    if (!audio_codec_ctx || !video_codec_ctx) return -1;

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        video_codec_ctx->width,
        video_codec_ctx->height
    );

    framerate = fmt_ctx->streams[video_stream_index]->r_frame_rate;
    double frameRate = av_q2d(framerate);
    printf("FPS: %f\n", frameRate);

    ticks_per_frame = frameRate * OSMillisecondsToTicks(1000);

    // Allocate and set up SwrContext
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2); // stereo (2 channels)

    int res = swr_alloc_set_opts2(
        &swr_ctx,
        &out_ch_layout,
        AV_SAMPLE_FMT_S16,
        AUDIO_SAMPLE_RATE,
        &audio_codec_ctx->ch_layout,
        audio_codec_ctx->sample_fmt,
        audio_codec_ctx->sample_rate,
        0,
        nullptr
    );

    if (res < 0 || !swr_ctx || swr_init(swr_ctx) < 0) {
        printf("Failed to initialize SwrContext\n");
        return -1;
    }

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    SDL_PauseAudio(0);

    return 0;
}

void video_player_start(const char* path, AppState* app_state, SDL_Renderer& renderer, SDL_Texture*& texture, SDL_mutex& _audio_mutex, SDL_AudioSpec wanted_spec) {
    current_pts_seconds = 0;
    audio_mutex = &_audio_mutex;
    video_player_init(path, &renderer, texture);
    start_video_decoding_thread();
    *app_state = STATE_PLAYING;
}

void video_player_scrub(int dt) {
    int64_t seek_target_seconds = current_pts_seconds + dt;

    int64_t seek_target = seek_target_seconds * AV_TIME_BASE;

    if(dt > 0) {
        av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_ANY);
    }
    else {
        av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD);
    }

    SDL_LockMutex(audio_mutex);
    ring_buffer_fill = 0;
    ring_buffer_read_pos = 0;
    ring_buffer_write_pos = 0;
    SDL_UnlockMutex(audio_mutex);

    avcodec_flush_buffers(audio_codec_ctx);
    avcodec_flush_buffers(video_codec_ctx);
}

bool video_player_is_playing() {
    return playing_video;
}

void video_player_play(bool new_state) {
    playing_video = new_state;
}

int64_t video_player_get_current_time() {
    return current_pts_seconds;
}

frame_info* video_player_get_current_frame_info() {
    return current_frame_info;
}

void process_audio_frame(AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture, AVPacket* pkt) {
    if (pkt->stream_index == audio_stream_index) {
        if (avcodec_send_packet(audio_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
                play_audio_frame(frame, swr_ctx, 2);
            }
        }
    }
}

void start_video_decoding_thread() {
    printf("Starting video decoding thread...\n");
    video_thread = std::thread(process_video_frame_thread);
}

void stop_video_decoding_thread() {
    printf("Stopping video decoding thread...\n");
    video_thread_running = false;
    if (video_thread.joinable()) {
        video_thread.join();  // Ensure the thread finishes before exiting
    }
}

void process_video_frame_thread() {
    AVFrame* local_frame = av_frame_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    int64_t start_time = av_gettime_relative(); // microseconds

    while (video_thread_running) {
        AVPacket pkt;
        if (av_read_frame(fmt_ctx, &pkt) >= 0) {
            if (pkt.stream_index == video_stream_index) {
                if (avcodec_send_packet(video_codec_ctx, &pkt) == 0) {
                    while (avcodec_receive_frame(video_codec_ctx, local_frame) == 0) {
                        if (local_frame->format == AV_PIX_FMT_YUV420P) {
                            // Convert PTS to real time
                            int64_t pts_us = local_frame->pts * av_q2d(time_base) * 1e6;
                            int64_t now_us = av_gettime_relative() - start_time;

                            int64_t delay = pts_us - now_us;
                            if (delay > 0) {
                                av_usleep(delay);
                            }

                            current_pts_seconds = local_frame->pts * av_q2d(time_base);

                            AVFrame* cloned_frame = av_frame_clone(local_frame);
                            if (cloned_frame) {
                                std::lock_guard<std::mutex> lock(video_frame_mutex);
                                video_frame_queue.push(cloned_frame);
                            }
                        }
                    }
                }
            }
            av_packet_unref(&pkt);
        } else {
            video_thread_running = false;
        }
    }

    av_frame_free(&local_frame);
}

void render_video_frame(AppState* app_state, SDL_Renderer* renderer) {
    AVFrame* frame = nullptr;

    {
        // Lock the mutex and check if there are frames in the queue
        std::lock_guard<std::mutex> lock(video_frame_mutex);
        if (!video_frame_queue.empty()) {
            frame = video_frame_queue.front();
            video_frame_queue.pop();
        }
    }

    if (frame) {
        // Create a texture if needed and render the frame
        if (!current_frame_info) {
            current_frame_info = new frame_info;
            current_frame_info->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
            current_frame_info->frame_width = frame->width;
            current_frame_info->frame_height = frame->height;
        }

        SDL_UpdateYUVTexture(current_frame_info->texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);

        // SDL_RenderCopy(renderer, current_frame_info->texture, NULL, NULL);
        //SDL_RenderPresent(renderer);
    }
}
/*
void process_video_frame(AppState* app_state, SDL_Renderer* renderer, SDL_Texture* texture, AVPacket* pkt, uint64_t* last_frame_ticks) {
    if (pkt->stream_index == video_stream_index) {
        if (avcodec_send_packet(video_codec_ctx, pkt) == 0) {
            while (avcodec_receive_frame(video_codec_ctx, frame) == 0) {
                if (frame->format == AV_PIX_FMT_YUV420P) {
                    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
                    current_pts_seconds = frame->pts * av_q2d(time_base);

                    if (!current_frame_info) {
                        current_frame_info = new frame_info;
                        current_frame_info->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
                        current_frame_info->frame_width = frame->width;
                        current_frame_info->frame_height = frame->height;
                        current_frame_info->total_time = 0;
                    }
 
                    SDL_UpdateYUVTexture(current_frame_info->texture, NULL,
                        frame->data[0], frame->linesize[0],
                        frame->data[1], frame->linesize[1],
                        frame->data[2], frame->linesize[2]);

                    uint64_t elapsed_ticks = OSGetSystemTime() - *last_frame_ticks;
                    if (elapsed_ticks < ticks_per_frame) {
                        // printf("%llu\n", OSTicksToMilliseconds((ticks_per_frame - elapsed_ticks) / 1000));
                        OSSleepTicks((ticks_per_frame - elapsed_ticks)/1000);
                    }

                    *last_frame_ticks = OSGetSystemTime();
                }
            }
        }
    }
}
*/
void video_player_update(AppState* app_state, SDL_Renderer* renderer) {
    if (!playing_video)
        return;

    // Process audio separately in the main thread or use another thread for audio
    // process_audio_frame(app_state, renderer, nullptr, pkt); 

    render_video_frame(app_state, renderer);  // Render the video frame
}

int video_player_cleanup() {

    stop_video_decoding_thread();

    avcodec_send_packet(audio_codec_ctx, NULL);
    while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
        play_audio_frame(frame, swr_ctx, 2);
    }

    while (ring_buffer_fill > 0) {
        SDL_Delay(100);
    }

    if (current_frame_info) {
        SDL_DestroyTexture(current_frame_info->texture);
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }

    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }

    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }

    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
    }

    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }

    ring_buffer_fill = 0;
    ring_buffer_read_pos = 0;
    ring_buffer_write_pos = 0;
    current_pts_seconds = 0;
    audio_stream_index = -1;
    video_stream_index = -1;
    playing_video = false;

    return 0;
}