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
#include "media_info.hpp"
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

AVCodecContext* video_player_create_codec_context(AVFormatContext* fmt_ctx, int stream_index) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);

    if (!codec) {
        printf("[Video Player] No decoder found for codec ID: %d\n", codecpar->codec_id);
        return NULL;
    }

    printf("[Video Player] Using decoder: %s\n", codec->name);

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        printf("[Video Player] Failed to allocate codec context\n");
        return NULL;
    }

    // Optional: Set pixel format early
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_parameters_to_context(codec_ctx, codecpar) < 0) {
        printf("[Video Player] Failed to copy codec parameters to codec context\n");
        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codec_ctx->skip_frame = AVDISCARD_NONREF;

    int ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        char errbuf[256];
        av_strerror(ret, errbuf, sizeof(errbuf));
        printf("[Video Player] Failed to open codec: %s\n", errbuf);

        avcodec_free_context(&codec_ctx);
        return NULL;
    }

    printf("[Video Player] Codec opened successfully\n");

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

    // Pause decoding thread safely
    video_player_play(false);
    {
        std::unique_lock<std::mutex> lock(playback_mutex);

        media_info info = media_info_get_copy();
        int64_t current_time = static_cast<int64_t>(info.current_video_playback_time * AV_TIME_BASE);
        int64_t target_time = current_time + static_cast<int64_t>(delta_seconds * AV_TIME_BASE);

        // Clamp to valid range
        if (target_time < 0) target_time = 0;
        if (target_time > fmt_ctx->duration - AV_TIME_BASE)
            target_time = fmt_ctx->duration - AV_TIME_BASE;

        // Seek using flexible flags (can tweak to A/V needs)
        if (av_seek_frame(fmt_ctx, video_stream_index, target_time, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) < 0) {
            printf("[Video Player] Seek failed!\n");
            return;
        }

        printf("[Video Player] Seek success!\n");

        // Flush codec buffers after seek
        avcodec_flush_buffers(video_codec_ctx);

        // Clear buffered frames
        clear_video_frames();

        // Reset current frame info
        if (current_frame_info) {
            SDL_DestroyTexture(current_frame_info->texture);
            delete current_frame_info;
            current_frame_info = nullptr;
        }

        // Reset playback timestamps
        start_time = av_gettime_relative() - target_time;
        info.current_video_playback_time = target_time / static_cast<double>(AV_TIME_BASE);

        // Sync audio with video
        audio_player_seek(info.current_video_playback_time);
        media_info_set(std::make_unique<media_info>(info));
        printf("[Video Player] Seek done! New time: %.2lld seconds\n", info.current_video_playback_time);
    }

    // Resume playback
    video_player_play(true);
}

void video_player_play(bool new_state) {
    std::lock_guard<std::mutex> lock(playback_mutex);

    media_info info = media_info_get_copy();
    if (!info.playback_status && new_state) {
        int64_t now = av_gettime_relative();
        int64_t pause_duration = now - pause_start_time;
        start_time += pause_duration; // Shift start_time forward
        playback_cv.notify_one();
    } else if (info.playback_status && !new_state) {
        // We are pausing
        pause_start_time = av_gettime_relative();
    }

    info.playback_status = new_state;
    media_info_set(std::make_unique<media_info>(info));
}

frame_info* video_player_get_current_frame_info() {
    if (!current_frame_info) {
        printf("[Video Player] No current_frame_info\n");
        return NULL;
    }
    return current_frame_info;
}

int video_player_init(const char* filepath, SDL_Renderer* renderer, SDL_Texture*& texture) {
    media_info info = media_info_get_copy();
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
    info.framerate = av_q2d(framerate);
    #ifdef DEBUG_VIDEO
    printf("[Video player] FPS: %f\n", info.framerate);
    #endif

    pkt = av_packet_alloc();
    frame = av_frame_alloc();

    #ifdef DEBUG_VIDEO
    printf("[Video player] Codec, packet, and frame initialized\n");
    #endif

    audio_player_init(info.path.c_str());
    media_info_set(std::make_unique<media_info>(info));

    return 0;
}

void video_player_start(const char* path, SDL_Renderer& renderer, SDL_Texture*& texture) {
    media_info info = media_info_get_copy();
    #ifdef DEBUG_VIDEO
    printf("[Video player] Starting video playback\n");
    #endif

    info.path = path;
    info.current_video_playback_time = 0;
    video_thread_running = true;

    if (current_frame_info) {
        SDL_DestroyTexture(current_frame_info->texture);
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    media_info_set(std::make_unique<media_info>(info));
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

std::mutex info_mutex;

void process_video_frame_thread() {
    AVFrame* local_frame = av_frame_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    start_time = av_gettime_relative();  // microseconds
    total_paused_duration = 0;
    pause_start_time = 0;

    while (video_thread_running) {
        {
            std::unique_lock<std::mutex> lock(playback_mutex);
            playback_cv.wait(lock, [] {
                std::lock_guard<std::mutex> info_lock(info_mutex);
                return media_info_get()->playback_status || !video_thread_running;
            });
        }

        AVPacket pkt;
        if (av_read_frame(fmt_ctx, &pkt) >= 0) {
            if (pkt.stream_index == video_stream_index) {
                if (!avcodec_send_packet(video_codec_ctx, &pkt)) {
                    while (!avcodec_receive_frame(video_codec_ctx, local_frame)) {
                        // Update playback time
                        {
                            std::lock_guard<std::mutex> info_lock(info_mutex);
                            media_info info = media_info_get_copy();
                            info.current_video_playback_time = local_frame->pts * av_q2d(time_base);
                            media_info_set(std::make_unique<media_info>(info));
                        }

                        // Push frame to queue
                        {
                            std::lock_guard<std::mutex> lock(video_frame_mutex);
                            if (video_frame_queue.size() < 10) { // Buffer limit
                                video_frame_queue.push(local_frame);
                                local_frame = av_frame_alloc(); // Prepare for next
                            } else {
                                av_frame_unref(local_frame); // Discard old
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
    if (!media_info_get_copy().playback_status) return;

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

    video_stream_index = -1;
    media_info info = media_info_get_copy();
    info.current_video_playback_time = 0;
    info.playback_status = false;
    media_info_set(std::make_unique<media_info>(info));
    total_paused_duration = 0;
    pause_start_time = 0;

    #ifdef DEBUG_VIDEO
    printf("[Video player] Cleanup complete\n");
    #endif

    return 0;
}