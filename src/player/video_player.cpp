#include <cstdint>
#include <coreinit/thread.h>
#include <pthread.h>
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

#include "utils/app_state.hpp"
#include "main.hpp"
#include "utils/media_info.hpp"
#include "player/video_player.hpp"
#include "player/audio_player.hpp"
#include "logger/logger.hpp"

int video_stream_index = -1;
AVFormatContext* fmt_ctx = NULL;
AVCodecContext* video_codec_ctx = NULL;
SwrContext* swr_ctx = NULL;
AVPacket* pkt = NULL;
AVFrame* frame = NULL;
AVRational framerate;

frame_info* current_frame_info;

int64_t start_time = 0;

std::queue<AVFrame*> video_frame_queue;
std::mutex video_frame_mutex;
std::condition_variable video_frame_cv;
bool video_thread_running = true;
std::thread video_thread;
std::mutex playback_mutex;
std::condition_variable playback_cv;
int64_t pause_start_time = 0;
int64_t total_paused_duration = 0;

#define FRAME_POOL_SIZE 16
static AVFrame frame_pool[FRAME_POOL_SIZE];
static int pool_index = 0;

AVCodecContext* video_player_create_codec_context(AVFormatContext* fmt_ctx, int stream_index) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);

    if (!codec) {
    	log_message(LOG_ERROR, "Video Player", "No decoder found for codec ID: %d", codecpar->codec_id);
        return NULL;
    }

    #ifdef DEBUG
    log_message(LOG_DEBUG, "Video Player", "Using decoder: %s", codec->name);
    #endif

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
    	log_message(LOG_ERROR, "Video Player", "Failed to allocate codec context");
        return NULL;
    }

    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
    	log_message(LOG_ERROR, "Video Player", "Failed to copy codec parameters to codec context");
        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx->skip_frame = AVDISCARD_NONREF;

    int ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        log_message(LOG_ERROR, "Video Player", "Failed to open codec: %s", errbuf);

        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    #ifdef DEBUG
    log_message(LOG_DEBUG, "Video Player", "Codec opened successfully");
    #endif

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

    video_player_play(false);
    {
        std::unique_lock<std::mutex> lock(playback_mutex);

        int64_t current_time = static_cast<int64_t>(media_info_get()->current_video_playback_time * AV_TIME_BASE);
        int64_t target_time = current_time + static_cast<int64_t>(delta_seconds * AV_TIME_BASE);

        // Clamp to valid range
        if (target_time < 0) target_time = 0;
        if (target_time > fmt_ctx->duration - AV_TIME_BASE)
            target_time = fmt_ctx->duration - AV_TIME_BASE;

        // Seek using flexible flags (can tweak to A/V needs)
        if (av_seek_frame(fmt_ctx, video_stream_index, target_time, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) < 0) {
        	log_message(LOG_ERROR, "Video Player", "Seek failed");
            return;
        }

        log_message(LOG_OK, "Video Player", "Seek success");

        avcodec_flush_buffers(video_codec_ctx);

        clear_video_frames();

        if (current_frame_info) {
            SDL_DestroyTexture(current_frame_info->texture);
            delete current_frame_info;
            current_frame_info = nullptr;
        }

        // Reset playback timestamps
        start_time = av_gettime_relative() - target_time;
        media_info_get()->current_video_playback_time = target_time / static_cast<double>(AV_TIME_BASE);

        audio_player_seek(media_info_get()->current_video_playback_time);
        log_message(LOG_OK, "Video Player", "Seek done. New time: %.2lld seconds", media_info_get()->current_video_playback_time);
    }

    // Resume playback
    video_player_play(true);
}

void video_player_play(bool new_state) {
    std::lock_guard<std::mutex> lock(playback_mutex);
    if (!media_info_get()->playback_status && new_state) {
        int64_t now = av_gettime_relative();
        int64_t pause_duration = now - pause_start_time;
        start_time += pause_duration;
        playback_cv.notify_one();
    } else if (media_info_get()->playback_status && !new_state) {
        pause_start_time = av_gettime_relative();
    }
    #ifdef DEBUG_VIDEO
    log_message(LOG_OK, "Video Player", "Changing playback_status to %s", new_state ? "true" : "false");
    #endif

    media_info_get()->playback_status = new_state;
}

frame_info* video_player_get_current_frame_info() {
    if (!current_frame_info) {
    	log_message(LOG_WARNING, "Video Player", "No current_frame_info");
        return NULL;
    }
    return current_frame_info;
}

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture) {
    #ifdef DEBUG_VIDEO
	log_message(LOG_DEBUG, "Video Player", "Starting Video Player");
	log_message(LOG_DEBUG, "Video Player", "file %s", filepath);
    #endif

    AVDictionary *options = NULL;
    av_dict_set(&options, "probesize", "10000", 0);
    av_dict_set(&options, "fpsprobesize", "10000", 0);
    av_dict_set(&options, "formatprobesize", "10000", 0);
    if (avformat_open_input(&fmt_ctx, filepath, NULL, &options) != 0) {
    #ifdef DEBUG_VIDEO
    	log_message(LOG_DEBUG, "Video Player", "Could not open file: %s", filepath);
    #endif
        return -1;
    }

    avformat_find_stream_info(fmt_ctx, NULL);

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);

    if (video_stream_index < 0) {
    	log_message(LOG_ERROR, "Video Player", "Could not find video stream.");
        return -1;
    }

    #ifdef DEBUG_VIDEO
    log_message(LOG_DEBUG, "Video Player", "Video stream found: index %d", video_stream_index);
    #endif

    video_codec_ctx = video_player_create_codec_context(fmt_ctx, video_stream_index);
    if (!video_codec_ctx) return -1;

    SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                 video_codec_ctx->width, video_codec_ctx->height);

    framerate = fmt_ctx->streams[video_stream_index]->r_frame_rate;
    media_info_get()->framerate = av_q2d(framerate);
    media_info_get()->total_video_playback_time = video_player_get_total_play_time();

    #ifdef DEBUG_VIDEO
    log_message(LOG_DEBUG, "Video Player", "FPS: %f", media_info_get()->framerate);
    #endif

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    #ifdef DEBUG_VIDEO
    log_message(LOG_DEBUG, "Video Player", "Codec, packet, and frame initialized\n");
    #endif

    return 0;
}

void video_player_start(const char* path, SDL_Renderer& renderer, SDL_Texture*& texture) {
    video_thread_running = true;

    if (current_frame_info) {
        SDL_DestroyTexture(current_frame_info->texture);
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    video_player_init(path, &renderer, texture);
    audio_player_init(path);

    start_video_decoding_thread();
    app_state_set(STATE_PLAYING_VIDEO);
}

void set_wiiu_thread_priority(std::thread& t, int priority) {
    OSThread* os_thread = reinterpret_cast<OSThread*>(t.native_handle());
    if (os_thread) {
        OSSetThreadPriority(os_thread, priority);
    }
}

void start_video_decoding_thread() {
    #ifdef DEBUG_VIDEO
	log_message(LOG_DEBUG, "Video Player", "Starting video decoding thread");
    #endif
    video_thread = std::thread(process_video_frame_thread);
    set_wiiu_thread_priority(video_thread, 31);
}

std::mutex info_mutex;

inline AVFrame* get_next_frame_from_pool() {
    AVFrame* f = &frame_pool[pool_index];
    av_frame_unref(f);
    pool_index = (pool_index + 1) % FRAME_POOL_SIZE;
    return f;
}

void process_video_frame_thread() {
    AVRational tb = fmt_ctx->streams[video_stream_index]->time_base;
    int64_t tb_num = tb.num;
    int64_t tb_den = tb.den;

    start_time = av_gettime_relative();  // microseconds
    total_paused_duration = 0;
    pause_start_time = 0;

    while (video_thread_running) {
        // Wait for playback resume
        {
            std::unique_lock<std::mutex> lock(playback_mutex);
            playback_cv.wait(lock, [] {
                std::lock_guard<std::mutex> info_lock(info_mutex);
                return media_info_get()->playback_status || !video_thread_running;
            });
        }
        if (!video_thread_running) break;

        AVPacket pkt;
        if (av_read_frame(fmt_ctx, &pkt) < 0) {
            video_thread_running = false;
            break;
        }

        if (pkt.stream_index == video_stream_index) {
            if (avcodec_send_packet(video_codec_ctx, &pkt) == 0) {
                while (true) {

                    {
                        std::unique_lock<std::mutex> lock(playback_mutex);
                        playback_cv.wait(lock, [] {
                            std::lock_guard<std::mutex> info_lock(info_mutex);
                            return media_info_get()->playback_status || !video_thread_running;
                        });

                        if (!video_thread_running) break;
                    }


                    AVFrame* frame = get_next_frame_from_pool();
                    if (avcodec_receive_frame(video_codec_ctx, frame) < 0) {
                        break;
                    }

                    int64_t pts_us = (frame->pts * tb_num * 1000000LL) / tb_den;
                    int64_t now_us = av_gettime_relative() - start_time;
                    int64_t delay = pts_us - now_us;

                    if (delay > 0) {
                        if (delay > 500) av_usleep(delay);
                    }

                    {
                        std::lock_guard<std::mutex> info_lock(info_mutex);
                        media_info_get()->current_video_playback_time =
                            (double)(frame->pts * tb_num) / tb_den;
                    }

                    {
                        std::lock_guard<std::mutex> lock(video_frame_mutex);
                        if (video_frame_queue.size() < FRAME_POOL_SIZE) {
                            video_frame_queue.push(frame);
                        } else {
                            AVFrame* old = video_frame_queue.front();
                            video_frame_queue.pop();
                            av_frame_unref(old);
                            video_frame_queue.push(frame);
                        }
                    }
                }
            }
        }
        av_packet_unref(&pkt);
    }
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

void video_player_update(SDL_Renderer* renderer) {
    if (!media_info_get_copy().playback_status) return;

    AVFrame* frame = nullptr;

    {
        std::lock_guard<std::mutex> lock(video_frame_mutex);
        if (!video_frame_queue.empty()) {
            frame = video_frame_queue.front();
            video_frame_queue.pop();
        }
    }

    if (!frame) return;

    if (!current_frame_info) {
        current_frame_info = new frame_info;
        current_frame_info->texture = SDL_CreateTexture(
            renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
            frame->width, frame->height
        );
        current_frame_info->frame_width = frame->width;
        current_frame_info->frame_height = frame->height;
    }

    // Update texture (no reallocation)
    if (current_frame_info->texture) {
        SDL_UpdateYUVTexture(
            current_frame_info->texture, nullptr,
            frame->data[0], frame->linesize[0],
            frame->data[1], frame->linesize[1],
            frame->data[2], frame->linesize[2]
        );
    }

    av_frame_unref(frame);
}

void stop_video_decoding_thread() {
    #ifdef DEBUG_VIDEO
	log_message(LOG_DEBUG, "Video Player", "Stopping video decoding thread");
    #endif
    video_thread_running = false;
    playback_cv.notify_all();
    if (video_thread.joinable()) {
        video_thread.join();
    #ifdef DEBUG_VIDEO
        log_message(LOG_DEBUG, "Video Player", "Video decoding thread joined");
    #endif
    }
}

int video_player_cleanup() {
    #ifdef DEBUG_VIDEO
	log_message(LOG_DEBUG, "Video Player", "Stopping Video Player");
    #endif

    audio_player_cleanup();

    // 1) Stop decode thread and wait for it to finish
    stop_video_decoding_thread();

    // 2) Clear video frame queue safely
    {
        std::lock_guard<std::mutex> lock(video_frame_mutex);
        while (!video_frame_queue.empty()) {
            AVFrame* f = video_frame_queue.front();
            video_frame_queue.pop();
            av_frame_unref(f);  // Unref only, no free!
        }
    }

    // 3) Destroy current frame texture safely
    if (current_frame_info) {
        if (current_frame_info->texture) {
            SDL_DestroyTexture(current_frame_info->texture);
            current_frame_info->texture = nullptr;
        }
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    // 4) Free last frame and packet only if allocated with av_frame_alloc/av_packet_alloc
    if (frame) {
        av_frame_free(&frame);
        frame = nullptr;
    }
    if (pkt) {
        av_packet_free(&pkt);
        pkt = nullptr;
    }

    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }

    video_stream_index = -1;
    media_info_get()->current_video_playback_time = 0;
    total_paused_duration = 0;
    pause_start_time = 0;

    media_info_get()->playback_status = false;

    #ifdef DEBUG_VIDEO
    log_message(LOG_DEBUG, "Video Player", "Cleanup complete");
    #endif

    return 0;
}
