#include <mutex>
#include <cstdio>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <algorithm>
#include <condition_variable>
#include <chrono>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#include <SDL2/SDL.h>

#include "utils/utils.hpp"
#include "utils/media_info.hpp"
#include "utils/sdl.hpp"
#include "logger/logger.hpp"
#include "player/media_player.hpp"

#define MAX_FRAME_QUEUE_SIZE 12
#define PIX_FMT_TARGET AV_PIX_FMT_YUV420P
#define AUDIO_OUT_CHANNELS 2
#define AUDIO_OUT_SAMPLE_RATE 48000
#define OUT_BUF_THRESHOLD (256 * 1024)

static AVFormatContext* fmt_ctx = nullptr;
static std::atomic<bool> media_initialized{false};
static std::atomic<bool> thread_running{false};
static std::atomic<bool> playback_running{false};

struct VideoFrame {
    AVFrame* av_frame;
    double pts_seconds;
};

class VideoFrameQueue {
public:
    explicit VideoFrameQueue(size_t capacity) : cap(capacity) {
        buffer.resize(cap, {nullptr, 0.0});
        head = tail = 0;
    }

    bool push(AVFrame* f, double pts) {
        std::lock_guard<std::mutex> lk(mtx);
        size_t next = (head + 1) % cap;
        if (next == tail) return false;
        buffer[head] = {f, pts};
        head = next;
        return true;
    }

    VideoFrame pop() {
        std::lock_guard<std::mutex> lk(mtx);
        if (tail == head) return {nullptr, 0.0};
        VideoFrame f = buffer[tail];
        buffer[tail] = {nullptr, 0.0};
        tail = (tail + 1) % cap;
        return f;
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx);
        while (tail != head) {
            VideoFrame f = buffer[tail];
            buffer[tail] = {nullptr, 0.0};
            tail = (tail + 1) % cap;
            if (f.av_frame) av_frame_free(&f.av_frame);
        }
    }

    bool empty() {
        std::lock_guard<std::mutex> lk(mtx);
        return tail == head;
    }

private:
    size_t cap;
    std::vector<VideoFrame> buffer;
    size_t head, tail;
    std::mutex mtx;
};

static VideoFrameQueue video_queue(MAX_FRAME_QUEUE_SIZE + 1);
static AVCodecContext* video_codec_ctx = nullptr;
static SwsContext* sws_ctx = nullptr;
static int video_stream_index = -1;
static AVRational video_time_base;
static frame_info* current_frame_info = nullptr;
static SDL_Rect dest_rect = {0, 0, 0, 0};
static bool dest_rect_initialised = false;
static int sws_width = 0, sws_height = 0;

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec audio_spec;
static AVCodecContext* audio_codec_ctx = nullptr;
static SwrContext* swr_ctx = nullptr;
static int audio_stream_index = -1;
static bool audio_enabled = false;
static int current_audio_track_id = -1;
static std::vector<AudioTrackInfo> audio_tracks;
static std::mutex audio_tracks_mutex;

static int64_t start_time_us = 0;
static int64_t pause_start_us = 0;
static std::atomic<float> audio_pts{0.0f};
static std::atomic<bool> seeking{false};
static std::atomic<int> decode_users{0};
static std::condition_variable decode_cv;
static std::mutex decode_mutex;
static std::thread decode_thread;

inline int64_t get_time_us() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
        steady_clock::now().time_since_epoch()
	).count();
}

static double get_master_clock() {
    if (audio_enabled && audio_device != 0) {
        float pts = audio_pts.load(std::memory_order_acquire);
        if (pts <= 0.0f) pts = 0.0f;
        
        Uint32 queued_bytes = SDL_GetQueuedAudioSize(audio_device);
        double queued_seconds = (double)queued_bytes /
            (audio_spec.freq * audio_spec.channels * (SDL_AUDIO_BITSIZE(audio_spec.format) / 8));
        
        double corrected_time = (double)pts - queued_seconds;
        return corrected_time < 0.0 ? 0.0 : corrected_time;
    } else {
        if (playback_running.load())
            return (get_time_us() - start_time_us) / 1'000'000.0;
        else
            return (pause_start_us - start_time_us) / 1'000'000.0;
    }
}

static AVFrame* convert_frame_to_yuv420p(AVFrame* src) {
    if (!src) return nullptr;

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
    dst->pts = src->pts;
    dst->best_effort_timestamp = src->best_effort_timestamp;
    
    return dst;
}

static bool process_video_packet(AVPacket* pkt) {
    if (avcodec_send_packet(video_codec_ctx, pkt) < 0)
        return false;

    while (true) {
        AVFrame* out_frame = av_frame_alloc();
        if (!out_frame) break;
        
        int ret = avcodec_receive_frame(video_codec_ctx, out_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&out_frame);
            break;
        }

        AVFrame* frame_to_queue = (out_frame->format == PIX_FMT_TARGET)
	    ? out_frame
	    : convert_frame_to_yuv420p(out_frame);
        
        if (!frame_to_queue) {
            av_frame_free(&out_frame);
            continue;
        }

        double pts_sec = (frame_to_queue->pts != AV_NOPTS_VALUE)
	    ? frame_to_queue->pts * av_q2d(video_time_base)
	    : frame_to_queue->best_effort_timestamp * av_q2d(video_time_base);

        while (thread_running.load() && playback_running.load() && !seeking.load()) {
            double now = get_master_clock();
            double diff = pts_sec - now;
            if (diff <= 0.005) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (!video_queue.push(frame_to_queue, pts_sec)) {
            av_frame_free(&frame_to_queue);
        }
    }

    return true;
}

static bool process_audio_packet(AVPacket* pkt) {
    if (avcodec_send_packet(audio_codec_ctx, pkt) != 0)
        return false;

    AVFrame* frame = av_frame_alloc();
    std::vector<uint8_t> out_buf;

    while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
        if (seeking.load(std::memory_order_acquire)) {
            av_frame_unref(frame);
            break;
        }

        int delay = swr_get_delay(swr_ctx, audio_codec_ctx->sample_rate);
        int max_samples = av_rescale_rnd(frame->nb_samples + delay,
                                         AUDIO_OUT_SAMPLE_RATE,
                                         audio_codec_ctx->sample_rate,
                                         AV_ROUND_UP);

        int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        size_t needed_bytes = (size_t)max_samples * AUDIO_OUT_CHANNELS * bytes_per_sample;
        if (out_buf.size() < needed_bytes) out_buf.resize(needed_bytes);

        uint8_t* out_ptr = out_buf.data();
        int out_samples = swr_convert(swr_ctx, &out_ptr, max_samples,
                                      (const uint8_t**)frame->data, frame->nb_samples);

        if (out_samples > 0 && playback_running.load() && audio_device != 0) {
            int bytes = out_samples * AUDIO_OUT_CHANNELS * bytes_per_sample;
            SDL_QueueAudio(audio_device, out_buf.data(), bytes);
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        int64_t best_pts = av_frame_get_best_effort_timestamp(frame);
#pragma GCC diagnostic pop
        
        if (best_pts != AV_NOPTS_VALUE) {
            AVRational tb = fmt_ctx->streams[audio_stream_index]->time_base;
            double pts_sec = best_pts * av_q2d(tb);
            audio_pts.store((float)pts_sec, std::memory_order_release);
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    return true;
}

static void decode_loop() {
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        log_message(LOG_ERROR, "Media Player", "Failed to allocate packet");
        return;
    }

    while (thread_running.load()) {
        while (!playback_running.load() && thread_running.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

        if (!thread_running.load()) break;

        if (seeking.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (audio_enabled && audio_device != 0) {
            while (SDL_GetQueuedAudioSize(audio_device) > OUT_BUF_THRESHOLD &&
                   thread_running.load() && !seeking.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        }

        decode_users.fetch_add(1, std::memory_order_acq_rel);
        
        if (seeking.load(std::memory_order_acquire)) {
            decode_users.fetch_sub(1, std::memory_order_acq_rel);
            decode_cv.notify_all();
            continue;
        }

        int ret = av_read_frame(fmt_ctx, pkt);
        if (ret < 0) {
            decode_users.fetch_sub(1, std::memory_order_acq_rel);
            decode_cv.notify_all();

            if (video_codec_ctx) avcodec_send_packet(video_codec_ctx, nullptr);
            if (audio_codec_ctx) avcodec_send_packet(audio_codec_ctx, nullptr);
            
            thread_running = false;
            break;
        }

        if (pkt->stream_index == video_stream_index && video_codec_ctx) {
            process_video_packet(pkt);
        } else if (pkt->stream_index == audio_stream_index && audio_codec_ctx) {
            process_audio_packet(pkt);
        }

        av_packet_unref(pkt);
        decode_users.fetch_sub(1, std::memory_order_acq_rel);
        decode_cv.notify_all();
    }

    av_packet_free(&pkt);
}

static void collect_audio_tracks() {
    if (!fmt_ctx) return;

    std::lock_guard<std::mutex> lock(audio_tracks_mutex);
    audio_tracks.clear();

    for (unsigned int i = 0; i < fmt_ctx->nb_streams; ++i) {
        AVStream* stream = fmt_ctx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            AVDictionaryEntry* lang = av_dict_get(stream->metadata, "language", nullptr, 0);
            const char* language = lang ? lang->value : "und";

            AudioTrackInfo info = {
                static_cast<int>(i),
                avcodec_get_name(stream->codecpar->codec_id),
                stream->codecpar->channels,
                stream->codecpar->sample_rate,
                language
            };

            audio_tracks.push_back(info);
        }
    }

    media_info_get()->total_audio_track_count = audio_tracks.size();
}

std::vector<AudioTrackInfo> media_player_get_audio_tracks() {
    std::lock_guard<std::mutex> lock(audio_tracks_mutex);
    return audio_tracks;
}

static int init_video_stream() {
    video_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        log_message(LOG_WARNING, "Media Player", "No video stream found");
        return -1;
    }

    AVStream* stream = fmt_ctx->streams[video_stream_index];
    video_time_base = stream->time_base;

    const AVCodec* codec = nullptr;
    if (stream->codecpar->codec_id == AV_CODEC_ID_H264 && stream->codecpar->height == 720) {
        codec = avcodec_find_decoder_by_name("h264_wiiu");
        if (codec) log_message(LOG_OK, "Media Player", "Using hardware H.264 decoding");
    }
    if (!codec) codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, "Media Player", "Video codec not found");
        return -1;
    }

    video_codec_ctx = avcodec_alloc_context3(codec);
    if (!video_codec_ctx || 
        avcodec_parameters_to_context(video_codec_ctx, stream->codecpar) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to setup video codec context");
        return -1;
    }

    video_codec_ctx->get_format = [](AVCodecContext*, const enum AVPixelFormat* pix_fmts) -> enum AVPixelFormat {
        for (int i = 0; pix_fmts[i] != -1; ++i)
            if (pix_fmts[i] == PIX_FMT_TARGET) return pix_fmts[i];
        for (int i = 0; pix_fmts[i] != -1; ++i)
            if (pix_fmts[i] == AV_PIX_FMT_NV12) return AV_PIX_FMT_NV12;
        return pix_fmts[0];
    };

    if (avcodec_open2(video_codec_ctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to open video codec");
        return -1;
    }

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
        log_message(LOG_ERROR, "Media Player", "Failed to create SDL texture");
        return -1;
    }

    log_message(LOG_OK, "Media Player", "Video stream initialized");
    return 0;
}

static int init_audio_stream() {
    audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index < 0) {
        log_message(LOG_WARNING, "Media Player", "No audio stream found");
        audio_enabled = false;
        return 0;
    }

    AVCodecParameters* codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, "Media Player", "Audio codec not found");
        return -1;
    }

    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx ||
        avcodec_parameters_to_context(audio_codec_ctx, codecpar) < 0 ||
        avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to setup audio codec");
        return -1;
    }

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            log_message(LOG_ERROR, "Media Player", "SDL audio init failed: %s", SDL_GetError());
            return -1;
        }
    }

    SDL_AudioSpec wanted_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = AUDIO_OUT_SAMPLE_RATE;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = AUDIO_OUT_CHANNELS;
    wanted_spec.samples = 4096;
    wanted_spec.callback = nullptr;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &audio_spec, 0);
    if (!audio_device) {
        log_message(LOG_ERROR, "Media Player", "Failed to open audio device");
        return -1;
    }

    swr_ctx = swr_alloc_set_opts(
        nullptr,
        av_get_default_channel_layout(AUDIO_OUT_CHANNELS),
        AV_SAMPLE_FMT_S16,
        AUDIO_OUT_SAMPLE_RATE,
        av_get_default_channel_layout(audio_codec_ctx->channels),
        audio_codec_ctx->sample_fmt,
        audio_codec_ctx->sample_rate,
        0, nullptr
	);

    if (!swr_ctx || swr_init(swr_ctx) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to initialize resampler");
        return -1;
    }

    current_audio_track_id = audio_stream_index;
    audio_enabled = true;
    
    SDL_ClearQueuedAudio(audio_device);
    SDL_PauseAudioDevice(audio_device, 0);

    collect_audio_tracks();
    
    log_message(LOG_OK, "Media Player", "Audio stream initialized");
    return 0;
}

int media_player_init(const char* filepath) {
    if (media_initialized.load()) {
        log_message(LOG_ERROR, "Media Player", "Already initialized");
        return -1;
    }

    log_message(LOG_OK, "Media Player", "Initializing media player");

    avformat_network_init();

    if (avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to open input file");
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to find stream info");
        avformat_close_input(&fmt_ctx);
        return -1;
    }

    bool has_video = (init_video_stream() == 0);
    bool has_audio = (init_audio_stream() == 0);

    if (!has_video && !has_audio) {
        log_message(LOG_ERROR, "Media Player", "No usable streams found");
        media_player_cleanup();
        return -1;
    }

    start_time_us = get_time_us();
    pause_start_us = start_time_us;
    audio_pts.store(0.0f);

    thread_running = true;
    playback_running = false;
    decode_thread = std::thread(decode_loop);

    media_info_get()->playback_status = false;
    if (has_video) {
        media_info_get()->total_video_playback_time = 
            video_stream_index >= 0 ? 
            fmt_ctx->streams[video_stream_index]->duration * av_q2d(fmt_ctx->streams[video_stream_index]->time_base) : 
            0.0;
    }
    if (has_audio) {
        media_info_get()->total_audio_playback_time = 
            audio_stream_index >= 0 ? 
            (int64_t)(fmt_ctx->streams[audio_stream_index]->duration * av_q2d(fmt_ctx->streams[audio_stream_index]->time_base)) : 
            0;
    }

    media_initialized = true;
    log_message(LOG_OK, "Media Player", "Initialization complete");
    return 0;
}

void media_player_play(bool play) {
    if (play && pause_start_us != 0) {
        int64_t paused_duration = get_time_us() - pause_start_us;
        start_time_us += paused_duration;
        pause_start_us = 0;
    } else if (!play) {
        pause_start_us = get_time_us();
    }
    
    playback_running = play;
    media_info_get()->playback_status = play;
    
    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, play ? 0 : 1);
    }
}

void media_player_seek(double seconds) {
    if (!fmt_ctx) return;

    log_message(LOG_DEBUG, "Media Player", "Seeking to %.3f seconds", seconds);

    media_player_play(false);

    {
        std::lock_guard<std::mutex> lock(decode_mutex);
        seeking.store(true, std::memory_order_release);
    }

    {
        std::unique_lock<std::mutex> lock(decode_mutex);
        decode_cv.wait(lock, [] { 
            return decode_users.load(std::memory_order_acquire) == 0; 
        });
    }

    std::lock_guard<std::mutex> lock(decode_mutex);

    int64_t seek_target = static_cast<int64_t>(seconds * AV_TIME_BASE);
    
    if (av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY) < 0) {
        log_message(LOG_ERROR, "Media Player", "Seek failed");
    } else {
        avformat_flush(fmt_ctx);
        
        if (video_codec_ctx) avcodec_flush_buffers(video_codec_ctx);
        if (audio_codec_ctx) avcodec_flush_buffers(audio_codec_ctx);
        
        video_queue.clear();
        
        if (audio_device != 0) SDL_ClearQueuedAudio(audio_device);
        if (swr_ctx) swr_init(swr_ctx);
        
        audio_pts.store((float)seconds, std::memory_order_release);
        start_time_us = get_time_us() - static_cast<int64_t>(seconds * 1'000'000);
        pause_start_us = start_time_us;
    }

    seeking.store(false, std::memory_order_release);
    decode_cv.notify_all();

    media_player_play(true);
}

bool media_player_switch_audio_track(int new_stream_index) {
    if (!fmt_ctx || !audio_enabled) return false;
    if (new_stream_index < 0 || new_stream_index >= (int)fmt_ctx->nb_streams) return false;

    AVStream* new_stream = fmt_ctx->streams[new_stream_index];
    if (new_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) return false;

    double current_time = get_master_clock();

    {
        std::lock_guard<std::mutex> lock(decode_mutex);
        seeking.store(true, std::memory_order_release);
    }

    {
        std::unique_lock<std::mutex> lock(decode_mutex);
        decode_cv.wait(lock, []{ 
            return decode_users.load(std::memory_order_acquire) == 0; 
        });
    }

    std::lock_guard<std::mutex> lock(decode_mutex);

    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_ClearQueuedAudio(audio_device);
    }

    if (audio_codec_ctx) { 
        avcodec_free_context(&audio_codec_ctx); 
        audio_codec_ctx = nullptr; 
    }
    if (swr_ctx) { 
        swr_free(&swr_ctx); 
        swr_ctx = nullptr; 
    }

    const AVCodec* codec = avcodec_find_decoder(new_stream->codecpar->codec_id);
    if (!codec) {
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx ||
        avcodec_parameters_to_context(audio_codec_ctx, new_stream->codecpar) < 0 ||
        avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        if (audio_codec_ctx) avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    swr_ctx = swr_alloc_set_opts(
        nullptr,
        av_get_default_channel_layout(AUDIO_OUT_CHANNELS),
        AV_SAMPLE_FMT_S16,
        AUDIO_OUT_SAMPLE_RATE,
        av_get_default_channel_layout(audio_codec_ctx->channels),
        audio_codec_ctx->sample_fmt,
        audio_codec_ctx->sample_rate,
        0, nullptr
	);

    if (!swr_ctx || swr_init(swr_ctx) < 0) {
        if (swr_ctx) swr_free(&swr_ctx);
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    audio_stream_index = new_stream_index;
    current_audio_track_id = new_stream_index;

    int64_t seek_target = static_cast<int64_t>(current_time * AV_TIME_BASE);
    av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
    avformat_flush(fmt_ctx);
    avcodec_flush_buffers(audio_codec_ctx);
    audio_pts.store((float)current_time, std::memory_order_release);

    if (playback_running.load() && audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 0);
    }

    seeking.store(false, std::memory_order_release);
    decode_cv.notify_all();

    return true;
}

void media_player_update() {
    double current_time = get_master_clock();
    media_info_get()->current_video_playback_time = current_time;

    if (!playback_running.load() || !video_codec_ctx)
        return;

    VideoFrame f = video_queue.pop();
    if (!f.av_frame)
        return;

    double frame_time = f.pts_seconds;
    double diff = frame_time - current_time;

    if (diff < -0.050) {
        log_message(LOG_WARNING, "Media Player", "Dropping late frame: %.3f ms late", -diff * 1000.0);
        av_frame_free(&f.av_frame);
        return;
    }

    if (!dest_rect_initialised) {
        dest_rect = calculate_aspect_fit_rect(f.av_frame->width, f.av_frame->height);
        dest_rect_initialised = true;
    }

    if (current_frame_info && current_frame_info->texture) {
        int tex_w = 0, tex_h = 0;
        SDL_QueryTexture(current_frame_info->texture, nullptr, nullptr, &tex_w, &tex_h);
        if (tex_w != f.av_frame->width || tex_h != f.av_frame->height) {
            SDL_DestroyTexture(current_frame_info->texture);
            current_frame_info->texture = SDL_CreateTexture(
                sdl_get()->sdl_renderer,
                SDL_PIXELFORMAT_IYUV,
                SDL_TEXTUREACCESS_STREAMING,
                f.av_frame->width, f.av_frame->height
		);
        }
    }

    if (current_frame_info && current_frame_info->texture) {
        SDL_UpdateYUVTexture(
            current_frame_info->texture,
            nullptr,
            f.av_frame->data[0], f.av_frame->linesize[0],
            f.av_frame->data[1], f.av_frame->linesize[1],
            f.av_frame->data[2], f.av_frame->linesize[2]
	    );

        SDL_RenderCopy(
            sdl_get()->sdl_renderer,
            current_frame_info->texture,
            nullptr,
            &dest_rect
	    );
    }

    av_frame_free(&f.av_frame);
}

double media_player_get_current_time() {
    return get_master_clock();
}

double media_player_get_total_time() {
    if (!fmt_ctx) return 0.0;
    
    if (video_stream_index >= 0) {
        AVStream* stream = fmt_ctx->streams[video_stream_index];
        if (stream->duration != AV_NOPTS_VALUE)
            return stream->duration * av_q2d(stream->time_base);
    }
    
    if (audio_stream_index >= 0) {
        AVStream* stream = fmt_ctx->streams[audio_stream_index];
        if (stream->duration != AV_NOPTS_VALUE)
            return stream->duration * av_q2d(stream->time_base);
    }
    
    if (fmt_ctx->duration != AV_NOPTS_VALUE)
        return fmt_ctx->duration / (double)AV_TIME_BASE;
    
    return 0.0;
}

bool media_player_is_playing() {
    return playback_running.load();
}

int media_player_get_current_audio_track() {
    return current_audio_track_id;
}

void media_player_cleanup() {
    log_message(LOG_OK, "Media Player", "Cleaning up");

    thread_running = false;
    playback_running = false;

    if (decode_thread.joinable())
        decode_thread.join();

    video_queue.clear();

    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_ClearQueuedAudio(audio_device);
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }

    if (video_codec_ctx) { 
        avcodec_free_context(&video_codec_ctx); 
        video_codec_ctx = nullptr; 
    }
    
    if (audio_codec_ctx) { 
        avcodec_free_context(&audio_codec_ctx); 
        audio_codec_ctx = nullptr; 
    }

    if (sws_ctx) { 
        sws_freeContext(sws_ctx); 
        sws_ctx = nullptr; 
    }
    
    if (swr_ctx) { 
        swr_free(&swr_ctx); 
        swr_ctx = nullptr; 
    }

    if (current_frame_info) {
        if (current_frame_info->texture) {
            SDL_DestroyTexture(current_frame_info->texture);
            current_frame_info->texture = nullptr;
        }
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    if (fmt_ctx) { 
        avformat_close_input(&fmt_ctx); 
        fmt_ctx = nullptr; 
    }

    if (SDL_WasInit(SDL_INIT_AUDIO)) 
        SDL_QuitSubSystem(SDL_INIT_AUDIO);

    avformat_network_deinit();

    audio_enabled = false;
    audio_stream_index = -1;
    video_stream_index = -1;
    current_audio_track_id = -1;
    dest_rect_initialised = false;
    media_initialized = false;

    log_message(LOG_OK, "Media Player", "Cleanup complete");
}
