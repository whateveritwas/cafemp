#include <cstdint>
#include <cstdio>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>

extern "C" {
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
	#include <libavutil/imgutils.h>
	#include <libavutil/time.h>
}

#include "utils/utils.hpp"
#include "utils/media_info.hpp"
#include "utils/sdl.hpp"
#include "logger/logger.hpp"
#include "player/audio_player.hpp"
#include "player/video_player.hpp"

#include <SDL2/SDL.h>

#define FRAME_POOL_SIZE 16
#define MAX_FRAME_QUEUE_SIZE 12
#define PIX_FMT_TARGET AV_PIX_FMT_YUV420P
#define SWS_FLAGS SWS_FAST_BILINEAR

static AVFormatContext* fmt_ctx = nullptr;
static AVCodecContext* video_codec_ctx = nullptr;
static int video_stream_index = -1;
static SwsContext* sws_ctx = nullptr;

static std::thread decode_thread;
static std::mutex queue_mutex;
static std::condition_variable queue_cv;
static std::condition_variable playback_cv;
static std::mutex playback_mutex;

static std::queue<AVFrame*> frame_queue;
static AVFrame frame_pool[FRAME_POOL_SIZE];
static int pool_index = 0;

static std::atomic<bool> thread_running{false};
static std::atomic<bool> playback_running{false};

static frame_info* current_frame_info = nullptr;

static int64_t start_time_us = 0;
static int64_t pause_start_us = 0;

static AVRational video_time_base;

SDL_Rect dest_rect = (SDL_Rect){0, 0, 0, 0};
bool dest_rect_initialised = false;

//static bool first_video_frame = true;

static AVFrame* get_frame_from_pool() {
    AVFrame* f = &frame_pool[pool_index];
    av_frame_unref(f);
    pool_index = (pool_index + 1) % FRAME_POOL_SIZE;
    return f;
}

inline int64_t get_time_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}


static void clear_frame_queue() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    while (!frame_queue.empty()) {
        AVFrame* f = frame_queue.front();
        frame_queue.pop();
        av_frame_unref(f);
    }
}

static void free_current_frame_info() {
    if (current_frame_info) {
        if (current_frame_info->texture) {
            SDL_DestroyTexture(current_frame_info->texture);
            current_frame_info->texture = nullptr;
        }
        delete current_frame_info;
        current_frame_info = nullptr;
    }
}

double video_player_get_total_playback_time() {
    if (!fmt_ctx || video_stream_index < 0) return 0.0;

    AVStream* stream = fmt_ctx->streams[video_stream_index];

    if (stream->duration != AV_NOPTS_VALUE) {
        return stream->duration * av_q2d(stream->time_base);
    } else if (fmt_ctx->duration != AV_NOPTS_VALUE) {
        return fmt_ctx->duration / (double)AV_TIME_BASE;
    }

    return -1.0; // unknown duration
}

double video_player_get_current_playback_time() {
    if (!fmt_ctx || video_stream_index < 0) return -1.0;

    if (playback_running.load()) {
        return (get_time_us() - start_time_us) / 1'000'000.0;
    } else {
        return (pause_start_us - start_time_us) / 1'000'000.0;
    }
}


static void decode_loop() {
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        log_message(LOG_ERROR, "Video Player", "Failed to allocate packet");
        return;
    }

    while (thread_running.load()) {
        {
            std::unique_lock<std::mutex> play_lock(playback_mutex);
            playback_cv.wait(play_lock, [] {
                return playback_running.load() == true || thread_running.load() == false;
            });
            if (!thread_running.load()) break;
        }

        int ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
            // EOF or error
            thread_running = false;
            break;
        }

        if (pkt->stream_index == video_stream_index) {
            ret = avcodec_send_packet(video_codec_ctx, pkt);
            if (ret < 0) {
                char errbuf[128];
                av_strerror(ret, errbuf, sizeof(errbuf));
                log_message(LOG_ERROR, "Video Player", "avcodec_send_packet failed: %s", errbuf);
                av_packet_unref(pkt);
                continue;
            }

            while (ret >= 0) {
                AVFrame* frame = get_frame_from_pool();

                ret = avcodec_receive_frame(video_codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    char errbuf[128];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    log_message(LOG_ERROR, "Video Player", "avcodec_receive_frame failed: %s", errbuf);
                    break;
                }

                if (frame->format != PIX_FMT_TARGET) {
                    AVFrame* converted = get_frame_from_pool();
                    av_image_alloc(converted->data, converted->linesize,
                                   frame->width, frame->height, PIX_FMT_TARGET, 1);

                    if (!sws_ctx) {
                        sws_ctx = sws_getContext(frame->width, frame->height,
                                                 (AVPixelFormat)frame->format,
                                                 frame->width, frame->height,
                                                 PIX_FMT_TARGET, SWS_FLAGS, nullptr, nullptr, nullptr);
                        if (!sws_ctx) {
                            log_message(LOG_ERROR, "Video Player", "sws_getContext failed");
                            av_frame_unref(frame);
                            av_freep(&converted->data[0]);
                            continue;
                        }
                    }

                    sws_scale(sws_ctx, frame->data, frame->linesize,
                              0, frame->height,
                              converted->data, converted->linesize);

                    converted->width = frame->width;
                    converted->height = frame->height;
                    converted->format = PIX_FMT_TARGET;
                    converted->pts = frame->pts;

                    av_frame_unref(frame);
                    frame = converted;
                }

                {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (frame_queue.size() >= MAX_FRAME_QUEUE_SIZE) {
                        AVFrame* old = frame_queue.front();
                        frame_queue.pop();
                        av_frame_unref(old);
                    }
                    frame_queue.push(frame);
                }
                queue_cv.notify_one();
            }
        }
        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
}

int video_player_init(const char* filepath) {
    if (thread_running.load()) {
        log_message(LOG_ERROR, "Video Player", "Already initialized");
        return -1;
    }

    avformat_network_init();

    fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr) < 0) {
        log_message(LOG_ERROR, "Video Player", "Failed to open input file");
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        log_message(LOG_ERROR, "Video Player", "Failed to find stream info");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        log_message(LOG_ERROR, "Video Player", "No video stream found");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    AVStream* stream = fmt_ctx->streams[video_stream_index];
    video_time_base = stream->time_base;

    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, "Video Player", "Codec not found");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    video_codec_ctx = avcodec_alloc_context3(codec);
    if (!video_codec_ctx) {
        log_message(LOG_ERROR, "Video Player", "Failed to allocate codec context");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    if (avcodec_parameters_to_context(video_codec_ctx, stream->codecpar) < 0) {
        log_message(LOG_ERROR, "Video Player", "Failed to copy codec parameters");
        avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    if (avcodec_open2(video_codec_ctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, "Video Player", "Failed to open codec");
        avcodec_free_context(&video_codec_ctx);
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    free_current_frame_info();

    current_frame_info = new frame_info();
    current_frame_info->width = video_codec_ctx->width;
    current_frame_info->height = video_codec_ctx->height;
    current_frame_info->texture = SDL_CreateTexture(sdl_get()->sdl_renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        video_codec_ctx->width,
        video_codec_ctx->height);

    if (!current_frame_info->texture) {
        log_message(LOG_ERROR, "Video Player", "SDL_CreateTexture failed");
        video_player_cleanup();
        return -1;
    }

    clear_frame_queue();

    pool_index = 0;
    thread_running = true;
    playback_running = false;
    start_time_us = get_time_us();
    pause_start_us = 0;

    media_info_get()->playback_status = true;
    media_info_get()->total_video_playback_time = video_player_get_total_playback_time();

    audio_player_init(filepath);

    decode_thread = std::thread(decode_loop);

    return 0;
}

void video_player_play(bool play) {
    {
        std::lock_guard<std::mutex> lock(playback_mutex);
        if (play) {
            if (pause_start_us != 0) {
                int64_t paused_duration = get_time_us() - pause_start_us;
                start_time_us += paused_duration;
                pause_start_us = 0;
            }
        } else {
            pause_start_us = get_time_us();
        }
        playback_running = play;
    }
    playback_cv.notify_one();

	#ifdef DEBUG
	log_message(LOG_DEBUG, "Video Player", "Updating play state to: %s", play ? "playing" : "paused");
	#endif
	media_info_get()->playback_status = play;
	audio_player_play(play);
}

void video_player_seek(double seconds) {
    if (!fmt_ctx || video_stream_index < 0) return;

    video_player_play(false);

    std::lock_guard<std::mutex> lock(playback_mutex);

    int64_t seek_target = static_cast<int64_t>(seconds * AV_TIME_BASE);
    int64_t seek_ts = av_rescale_q(seek_target, AVRational{1, AV_TIME_BASE}, fmt_ctx->streams[video_stream_index]->time_base);

    if (av_seek_frame(fmt_ctx, video_stream_index, seek_ts, AVSEEK_FLAG_BACKWARD) < 0) {
        log_message(LOG_ERROR, "Video Player", "Seek failed");
    } else {
        avcodec_flush_buffers(video_codec_ctx);
        clear_frame_queue();
        start_time_us = get_time_us() - static_cast<int64_t>(seconds * 1000000);
    }

    video_player_play(true);
}

//static bool frame_due_now(const AVFrame* f) {
//    if (f->pts == AV_NOPTS_VALUE) {
//        log_message(LOG_DEBUG, "Video Player",
//                    "[frame_due_now] No PTS → render immediately");
//        return true;
//    }
//
//    // Convert PTS to microseconds
//    int64_t pts_us = av_rescale_q(f->pts, video_time_base, AVRational{1, 1000000});
//
//    // On first frame, align start_time_us so elapsed_us matches this frame’s PTS
//    if (first_video_frame) {
//        start_time_us = get_time_us() - pts_us;
//        log_message(LOG_DEBUG, "Video Player",
//                    "[frame_due_now] First frame detected → start_time_us aligned to PTS (%" PRId64 " us)",
//                    pts_us);
//        first_video_frame = false;
//    }
//
//    int64_t now_us = get_time_us();
//    int64_t elapsed_us = now_us - start_time_us;
//    int64_t delta_us = pts_us - elapsed_us;
//
//    log_message(LOG_DEBUG, "Video Player",
//                "[frame_due_now] raw_pts=%" PRId64
//                "  pts_us=%" PRId64 " (%.3f s)"
//                "  elapsed_us=%" PRId64 " (%.3f s)"
//                "  delta_us=%" PRId64 " (%.3f s)"
//                "  decision=%s",
//                f->pts,
//                pts_us, pts_us / 1e6,
//                elapsed_us, elapsed_us / 1e6,
//                delta_us, delta_us / 1e6,
//                (delta_us <= 0 ? "RENDER" : "WAIT"));
//
//    return delta_us <= 0;
//}

void video_player_update() {
	media_info_get()->current_video_playback_time = video_player_get_current_playback_time();

	if (!playback_running.load()) return;


    AVFrame* frame = nullptr;

    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        if (frame_queue.empty()) return;
        frame = frame_queue.front();
//        if (!frame_due_now(frame)) return;
        frame_queue.pop();
    }

    if (!frame) return;

    SDL_UpdateYUVTexture(current_frame_info->texture, nullptr,
                         frame->data[0], frame->linesize[0],
                         frame->data[1], frame->linesize[1],
                         frame->data[2], frame->linesize[2]);

    if (!dest_rect_initialised) {
        dest_rect = calculate_aspect_fit_rect(current_frame_info->width, current_frame_info->height);
        dest_rect_initialised = true;
    }

    SDL_RenderCopy(sdl_get()->sdl_renderer, current_frame_info->texture, nullptr, &dest_rect);

    av_frame_unref(frame);
}

void video_player_cleanup() {
	audio_player_cleanup();

    thread_running = false;
    playback_running = false;
    playback_cv.notify_one();

    dest_rect_initialised = false;

    if (decode_thread.joinable()) {
        decode_thread.join();
    }

    clear_frame_queue();

    if (video_codec_ctx) {
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }

    if (sws_ctx) {
        sws_freeContext(sws_ctx);
        sws_ctx = nullptr;
    }

    free_current_frame_info();

    avformat_network_deinit();

    log_message(LOG_OK, "Video Player", "Cleaned up");
}
