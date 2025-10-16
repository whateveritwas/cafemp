#include <cstdint>
#include <cstdio>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include <new>
#include <cstring>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
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

// ----------- Lock-Free Frame Queue -----------
template <size_t N_PLUS_ONE>
class LFQueue {
public:
    LFQueue() : head(0), tail(0) {}

    bool push(AVFrame* frame) {
        size_t next_head = (head + 1) % N;
        if (next_head == tail.load(std::memory_order_acquire)) {
            return false; // full
        }
        buffer[head] = frame;
        head = next_head;
        return true;
    }

    AVFrame* pop() {
        size_t t = tail.load(std::memory_order_acquire);
        if (t == head) return nullptr; // empty
        AVFrame* f = buffer[t];
        tail.store((t + 1) % N, std::memory_order_release);
        return f;
    }

    void clear_and_unref_all() {
        for (;;) {
            AVFrame* f = pop();
            if (!f) break;
            av_frame_unref(f);
        }
    }

private:
    static constexpr size_t N = N_PLUS_ONE;
    AVFrame* buffer[N];
    size_t head;
    std::atomic<size_t> tail;
};

static LFQueue<MAX_FRAME_QUEUE_SIZE + 1> frame_queue;

// ----------- Globals -----------
static AVFormatContext* fmt_ctx = nullptr;
static AVCodecContext* video_codec_ctx = nullptr;
static SwsContext* sws_ctx = nullptr;
static int video_stream_index = -1;
static std::thread decode_thread;

static AVFrame frame_pool[FRAME_POOL_SIZE];
static std::atomic<bool> thread_running{false};
static std::atomic<bool> playback_running{false};

static frame_info* current_frame_info = nullptr;
static int64_t start_time_us = 0;
static int64_t pause_start_us = 0;
static AVRational video_time_base;

SDL_Rect dest_rect = {0, 0, 0, 0};
bool dest_rect_initialised = false;
static int sws_width = 0;
static int sws_height = 0;

inline int64_t get_time_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()
    ).count();
}

static void clear_frame_queue() {
    frame_queue.clear_and_unref_all();
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
    if (stream->duration != AV_NOPTS_VALUE)
        return stream->duration * av_q2d(stream->time_base);
    else if (fmt_ctx->duration != AV_NOPTS_VALUE)
        return fmt_ctx->duration / (double)AV_TIME_BASE;
    return -1.0;
}

double video_player_get_current_playback_time() {
    if (!fmt_ctx || video_stream_index < 0) return -1.0;
    if (playback_running.load())
        return (get_time_us() - start_time_us) / 1'000'000.0;
    else
        return (pause_start_us - start_time_us) / 1'000'000.0;
}

static AVFrame* convert_frame_to_yuv420p(AVFrame* src) {
    AVFrame* dst = av_frame_alloc();
    if (!dst) return nullptr;

    dst->format = PIX_FMT_TARGET;
    dst->width = src->width;
    dst->height = src->height;

    if (av_frame_get_buffer(dst, 32) < 0) {
        av_frame_free(&dst);
        return nullptr;
    }

    if (!sws_ctx || sws_width != src->width || sws_height != src->height) {
        if (sws_ctx) sws_freeContext(sws_ctx);
        sws_ctx = sws_getContext(src->width, src->height, (AVPixelFormat)src->format,
                                 dst->width, dst->height, PIX_FMT_TARGET,
                                 SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        sws_width = src->width;
        sws_height = src->height;
        if (!sws_ctx) {
            av_frame_free(&dst);
            return nullptr;
        }
    }

    sws_scale(sws_ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
    return dst;
}

static void decode_loop() {
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        log_message(LOG_ERROR, "Video Player", "Failed to allocate packet");
        return;
    }

    while (thread_running.load()) {
        while (!playback_running.load() && thread_running.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

        if (!thread_running.load()) break;

        int ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
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
                AVFrame* out_frame = av_frame_alloc();
                ret = avcodec_receive_frame(video_codec_ctx, out_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    av_frame_free(&out_frame);
                    break;
                } else if (ret < 0) {
                    char errbuf[128];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    log_message(LOG_ERROR, "Video Player", "avcodec_receive_frame failed: %s", errbuf);
                    av_frame_free(&out_frame);
                    break;
                }

                AVFrame* frame_to_queue = nullptr;
                if (out_frame->format == PIX_FMT_TARGET) {
                    frame_to_queue = out_frame;
                } else if (out_frame->format == AV_PIX_FMT_NV12) {
                    frame_to_queue = convert_frame_to_yuv420p(out_frame);
                    av_frame_free(&out_frame);
                    if (!frame_to_queue) continue;
                } else {
                    static std::atomic<bool> warned{false};
                    if (!warned.exchange(true))
                        log_message(LOG_WARNING, "Video Player", "Unsupported pixel format: %d", out_frame->format);
                    av_frame_free(&out_frame);
                    continue;
                }

                if (!frame_queue.push(frame_to_queue)) {
                    AVFrame* old = frame_queue.pop();
                    if (old) av_frame_unref(old);
                    if (!frame_queue.push(frame_to_queue))
                        av_frame_unref(frame_to_queue);
                }
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

    const AVCodec* codec = nullptr;
    if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
    	if(stream->codecpar->height == 720) {
    		codec = avcodec_find_decoder_by_name("h264_wiiu");
    		log_message(LOG_OK, "Video Player", "Using hardware decoding");
    	} else {
    		codec = avcodec_find_decoder_by_name("h264");
    		log_message(LOG_OK, "Video Player", "Using software decoding");
    	}
    }
    if (!codec) codec = avcodec_find_decoder(stream->codecpar->codec_id);
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

    video_codec_ctx->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) -> enum AVPixelFormat {
        for (int i = 0; pix_fmts[i] != -1; ++i)
            if (pix_fmts[i] == PIX_FMT_TARGET) return pix_fmts[i];
        for (int i = 0; pix_fmts[i] != -1; ++i)
            if (pix_fmts[i] == AV_PIX_FMT_NV12) return AV_PIX_FMT_NV12;
        return pix_fmts[0];
    };

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
    current_frame_info->texture = SDL_CreateTexture(
        sdl_get()->sdl_renderer,
        SDL_PIXELFORMAT_IYUV,
        SDL_TEXTUREACCESS_STREAMING,
        video_codec_ctx->width, video_codec_ctx->height
    );

    if (!current_frame_info->texture) {
        log_message(LOG_ERROR, "Video Player", "SDL_CreateTexture failed");
        video_player_cleanup();
        return -1;
    }

    clear_frame_queue();
    sws_ctx = nullptr;
    start_time_us = get_time_us();
    pause_start_us = 0;
    thread_running = true;
    playback_running = false;

    media_info_get()->playback_status = true;
    media_info_get()->total_video_playback_time = video_player_get_total_playback_time();

    audio_player_init(filepath);
    decode_thread = std::thread(decode_loop);

    return 0;
}

void video_player_play(bool play) {
    if (play && pause_start_us != 0) {
        int64_t paused_duration = get_time_us() - pause_start_us;
        start_time_us += paused_duration;
        pause_start_us = 0;
    } else if (!play) {
        pause_start_us = get_time_us();
    }
    playback_running = play;
    media_info_get()->playback_status = play;
    audio_player_play(play);
}

void video_player_seek(double seconds) {
    if (!fmt_ctx || video_stream_index < 0) return;

    video_player_play(false);

    int64_t seek_target = static_cast<int64_t>(seconds * AV_TIME_BASE);
    int64_t seek_ts = av_rescale_q(seek_target, AVRational{1, AV_TIME_BASE}, fmt_ctx->streams[video_stream_index]->time_base);

    if (av_seek_frame(fmt_ctx, video_stream_index, seek_ts, AVSEEK_FLAG_BACKWARD) < 0)
        log_message(LOG_ERROR, "Video Player", "Seek failed");
    else {
        avcodec_flush_buffers(video_codec_ctx);
        clear_frame_queue();
        start_time_us = get_time_us() - static_cast<int64_t>(seconds * 1'000'000);
    }

    video_player_play(true);
}

void video_player_update() {
    media_info_get()->current_video_playback_time = video_player_get_current_playback_time();
    if (!playback_running.load()) return;

    AVFrame* frame = frame_queue.pop();
    if (!frame) return;

    if (!dest_rect_initialised) {
        dest_rect = calculate_aspect_fit_rect(frame->width, frame->height);
        dest_rect_initialised = true;
    }

    SDL_UpdateYUVTexture(current_frame_info->texture, nullptr,
                         frame->data[0], frame->linesize[0],
                         frame->data[1], frame->linesize[1],
                         frame->data[2], frame->linesize[2]);
    SDL_RenderCopy(sdl_get()->sdl_renderer, current_frame_info->texture, nullptr, &dest_rect);

    av_frame_unref(frame);
}

void video_player_cleanup() {
    audio_player_cleanup();
    thread_running = false;
    playback_running = false;
    dest_rect_initialised = false;

    if (decode_thread.joinable())
        decode_thread.join();

    clear_frame_queue();

    for (int i = 0; i < FRAME_POOL_SIZE; ++i) {
        av_frame_unref(&frame_pool[i]);
        if (frame_pool[i].data[0]) av_freep(&frame_pool[i].data[0]);
    }

    if (video_codec_ctx) { avcodec_free_context(&video_codec_ctx); video_codec_ctx = nullptr; }
    if (fmt_ctx) { avformat_close_input(&fmt_ctx); fmt_ctx = nullptr; }
    free_current_frame_info();
    if (sws_ctx) { sws_freeContext(sws_ctx); sws_ctx = nullptr; }
    avformat_network_deinit();

    log_message(LOG_OK, "Video Player", "Cleaned up");
}
