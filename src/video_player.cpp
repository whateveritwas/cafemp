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

#include "app_state.hpp"
#include "main.hpp"
#include "video_player.hpp"
#include "audio_player.hpp"

int video_stream_index = -1;
AVFormatContext* fmt_ctx = NULL;
AVCodecContext* video_codec_ctx = NULL;
SwrContext* swr_ctx = NULL;
AVPacket* pkt = NULL;
AVFrame* frame = NULL;
AVRational framerate;

frame_info* current_frame_info;

int64_t current_pts_seconds = 0;
uint64_t ticks_per_frame = 0;
int64_t start_time = 0;

bool playing_video = false;

std::queue<AVFrame*> video_frame_queue;
std::mutex video_frame_mutex;
std::condition_variable video_frame_cv;
bool video_thread_running = true;
std::thread video_thread;
std::mutex playback_mutex;
std::condition_variable playback_cv;
int64_t pause_start_time = 0;
int64_t total_paused_duration = 0;

AVCodecContext* video_player_create_codec_context(AVFormatContext* fmt_ctx, int stream_index) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        printf("Failed to open codec.\n");
        return NULL;
    }
    return codec_ctx;
}

void clear_video_frames() {
    std::lock_guard<std::mutex> lock(video_frame_mutex);
    while (!video_frame_queue.empty()) {
        AVFrame* f = video_frame_queue.front();
        av_frame_free(&f);
        video_frame_queue.pop();
    }
}

void video_player_seek(float delta_seconds) {
    if (!fmt_ctx || video_stream_index < 0) return;

    int64_t current_time = static_cast<int64_t>(current_pts_seconds * AV_TIME_BASE);
    int64_t target_time = current_time + static_cast<int64_t>(delta_seconds * AV_TIME_BASE);

    // Clamp target_time
    if (target_time < 0) target_time = 0;
    if (target_time > fmt_ctx->duration) target_time = fmt_ctx->duration;

    if (av_seek_frame(fmt_ctx, video_stream_index, target_time, AVSEEK_FLAG_BACKWARD) < 0) {
        printf("Seek failed!\n");
        return;
    }
    printf("Seek success!\n");

    avcodec_flush_buffers(video_codec_ctx);

    clear_video_frames();

    if (current_frame_info) {
        SDL_DestroyTexture(current_frame_info->texture);
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    start_time = av_gettime_relative() - target_time;
    current_pts_seconds = target_time / (double)AV_TIME_BASE;

    audio_player_seek(current_pts_seconds);
    printf("Seek done!\n");
}

bool video_player_is_playing() {
    return playing_video;
}

void video_player_play(bool new_state) {
    std::lock_guard<std::mutex> lock(playback_mutex);

    if (!playing_video && new_state) {
        int64_t now = av_gettime_relative();
        int64_t pause_duration = now - pause_start_time;
        start_time += pause_duration; // Shift start_time forward
        playback_cv.notify_one();
    } else if (playing_video && !new_state) {
        // We are pausing
        pause_start_time = av_gettime_relative();
    }

    playing_video = new_state;
}

int64_t video_player_get_current_time() {
    return current_pts_seconds;
}

frame_info* video_player_get_current_frame_info() {
    if (!current_frame_info) {
        printf("No current_frame_info\n");
        return NULL;
    }
    return current_frame_info;
}

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture) {
    #ifdef DEBUG_VIDEO
    printf("[Video player] Starting Video Player...\n");
    printf("[Video player] Opening file %s\n", filepath);
    #endif

    AVDictionary *options = NULL;
    av_dict_set(&options, "probesize", "10000", 0);
    av_dict_set(&options, "fpsprobesize", "10000", 0);
    av_dict_set(&options, "formatprobesize", "10000", 0);
    if (avformat_open_input(&fmt_ctx, filepath, NULL, &options) != 0) {
    #ifdef DEBUG_VIDEO
        printf("[Video player] Could not open file: %s\n", filepath);
    #endif
        return -1;
    }

    avformat_find_stream_info(fmt_ctx, NULL);

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (video_stream_index < 0) {
    #ifdef DEBUG_VIDEO
        printf("[Video player] Could not find video stream.\n");
    #endif
        return -1;
    }

    #ifdef DEBUG_VIDEO
    printf("[Video player] Video stream found: index %d\n", video_stream_index);
    #endif

    video_codec_ctx = video_player_create_codec_context(fmt_ctx, video_stream_index);
    if (!video_codec_ctx) return -1;

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                 video_codec_ctx->width, video_codec_ctx->height);

    framerate = fmt_ctx->streams[video_stream_index]->r_frame_rate;
    double frameRate = av_q2d(framerate);
    #ifdef DEBUG_VIDEO
    printf("[Video player] FPS: %f\n", frameRate);
    #endif
    ticks_per_frame = frameRate * OSMillisecondsToTicks(1000);

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    #ifdef DEBUG_VIDEO
    printf("[Video player] Codec, packet, and frame initialized\n");
    #endif

    audio_player_init(filepath);

    return 0;
}

void video_player_start(const char* path, SDL_Renderer& renderer, SDL_Texture*& texture) {
    #ifdef DEBUG_VIDEO
    printf("[Video player] Starting video playback\n");
    #endif

    current_pts_seconds = 0;
    video_thread_running = true;

    if (current_frame_info) {
        SDL_DestroyTexture(current_frame_info->texture);
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    video_player_init(path, &renderer, texture);
    start_video_decoding_thread();
    app_state_set(STATE_PLAYING_VIDEO);
}

void start_video_decoding_thread() {
    #ifdef DEBUG_VIDEO
    printf("[Video player] Starting video decoding thread...\n");
    #endif
    video_thread = std::thread(process_video_frame_thread);
}

void process_video_frame_thread() {
    AVFrame* local_frame = av_frame_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    start_time = av_gettime_relative();  // microseconds
    total_paused_duration = 0;
    pause_start_time = 0;

    while (video_thread_running) {
        {
            std::unique_lock<std::mutex> lock(playback_mutex);
            playback_cv.wait(lock, [] { return playing_video || !video_thread_running; });
        }

        AVPacket pkt;
        if (av_read_frame(fmt_ctx, &pkt) >= 0) {
            if (pkt.stream_index == video_stream_index) {
                if (!avcodec_send_packet(video_codec_ctx, &pkt)) {
                    while (!avcodec_receive_frame(video_codec_ctx, local_frame)) {
                        int64_t pts_us = local_frame->pts * av_q2d(time_base) * 1e6;
                        int64_t now_us = av_gettime_relative() - start_time;
                        int64_t delay = pts_us - now_us;

                        if (delay > 0) {
                            av_usleep(delay);
                        }

                        current_pts_seconds = local_frame->pts * av_q2d(time_base);

                        {
                            std::lock_guard<std::mutex> lock(video_frame_mutex);
                            if (video_frame_queue.size() < 10) { // Buffer limit
                                video_frame_queue.push(local_frame); // Directly push the frame
                                local_frame = av_frame_alloc(); // Allocate a new frame to receive next one
                            } else {
                                av_frame_unref(local_frame); // Frame too old, discard it
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

int64_t video_player_get_total_play_time() {
    if (!fmt_ctx || video_stream_index < 0) return 0;

    AVStream* stream = fmt_ctx->streams[video_stream_index];
    if (!stream) return 0;

    int64_t duration = stream->duration;
    if (duration <= 0) {
        duration = fmt_ctx->duration * stream->time_base.den / (stream->time_base.num * AV_TIME_BASE);
    } else {
        duration = duration * av_q2d(stream->time_base);
    }

    return static_cast<int64_t>(duration);
}

void render_video_frame(SDL_Renderer* renderer) {
    AVFrame* frame = nullptr;

    {
        std::lock_guard<std::mutex> lock(video_frame_mutex);
        if (!video_frame_queue.empty()) {
            frame = video_frame_queue.front();
            video_frame_queue.pop();
        }
    }

    if (!frame) return; // Nothing to render safely

    if (!current_frame_info) {
        current_frame_info = new frame_info;
        current_frame_info->texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, frame->width, frame->height);
        current_frame_info->frame_width = frame->width;
        current_frame_info->frame_height = frame->height;
    }

    if (current_frame_info->texture) {
        SDL_UpdateYUVTexture(current_frame_info->texture, NULL,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]);
    }

    av_frame_free(&frame); // Free the cloned frame
}

void video_player_update(SDL_Renderer* renderer) {
    if (!playing_video) return;

    render_video_frame(renderer);
}

void stop_video_decoding_thread() {
    #ifdef DEBUG_VIDEO
    printf("[Video player] Stopping video decoding thread...\n");
    #endif
    video_thread_running = false;
    playback_cv.notify_all();
    if (video_thread.joinable()) {
        video_thread.join();
    #ifdef DEBUG_VIDEO
        printf("[Video player] Video decoding thread joined\n");
    #endif
    }
}


int video_player_cleanup() {
    #ifdef DEBUG_VIDEO
    printf("[Video player] Stopping Video Player...\n");
    #endif

    audio_player_cleanup();
    stop_video_decoding_thread();

    {
        std::lock_guard<std::mutex> lock(video_frame_mutex);
        while (!video_frame_queue.empty()) {
            AVFrame* f = video_frame_queue.front();
            av_frame_free(&f);
            video_frame_queue.pop();
        }
    #ifdef DEBUG_VIDEO
        printf("[Video player] Cleared video frame queue\n");
    #endif
    }

    if (current_frame_info) {
        if (current_frame_info->texture) {
            SDL_DestroyTexture(current_frame_info->texture);
        }
        delete current_frame_info;
        current_frame_info = nullptr;
    #ifdef DEBUG_VIDEO
        printf("[Video player] Freed current_frame_info\n");
    #endif
    }

    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    #ifdef DEBUG_VIDEO
        printf("[Video player] Freed last video frame\n");
    #endif
    }

    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    #ifdef DEBUG_VIDEO
        printf("[Video player] Freed packet\n");
    #endif
    }

    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
    #ifdef DEBUG_VIDEO
        printf("[Video player] Freed codec context\n");
    #endif
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    #ifdef DEBUG_VIDEO
        printf("[Video player] Closed input file\n");
    #endif
    }

    current_pts_seconds = 0;
    video_stream_index = -1;
    playing_video = false;
    total_paused_duration = 0;
    pause_start_time = 0;

    #ifdef DEBUG_VIDEO
    printf("[Video player] Cleanup complete\n");
    #endif

    return 0;
}
