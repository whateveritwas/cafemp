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
    if (!src) {
        log_message(LOG_WARNING, "Media Player", "convert_frame_to_yuv420p: null source frame");
        return nullptr;
    }

    AVFrame* dst = av_frame_alloc();
    if (!dst) {
        log_message(LOG_ERROR, "Media Player", "convert_frame_to_yuv420p: failed to allocate frame");
        return nullptr;
    }

    dst->format = PIX_FMT_TARGET;
    dst->width = src->width;
    dst->height = src->height;

    if (av_frame_get_buffer(dst, 32) < 0) {
        log_message(LOG_ERROR, "Media Player", "convert_frame_to_yuv420p: failed to get frame buffer");
        av_frame_free(&dst);
        return nullptr;
    }

    if (!sws_ctx || sws_width != src->width || sws_height != src->height) {
        log_message(LOG_DEBUG, "Media Player", "Creating sws context: %dx%d format %d -> YUV420P",
                    src->width, src->height, src->format);
        if (sws_ctx) sws_freeContext(sws_ctx);
        sws_ctx = sws_getContext(src->width, src->height, (AVPixelFormat)src->format,
                                 dst->width, dst->height, PIX_FMT_TARGET,
                                 SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
        sws_width = src->width;
        sws_height = src->height;
        if (!sws_ctx) {
            log_message(LOG_ERROR, "Media Player", "Failed to create sws context");
            av_frame_free(&dst);
            return nullptr;
        }
    }

    int ret = sws_scale(sws_ctx, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
    if (ret < 0) {
        log_message(LOG_ERROR, "Media Player", "sws_scale failed: %d", ret);
    }
    
    dst->pts = src->pts;
    dst->best_effort_timestamp = src->best_effort_timestamp;
    
    return dst;
}

static bool process_video_packet(AVPacket* pkt) {
    if (!pkt) {
        log_message(LOG_WARNING, "Media Player", "process_video_packet: null packet");
        return false;
    }

    log_message(LOG_DEBUG, "Media Player", "Processing video packet: size=%d, pts=%lld, dts=%lld",
                pkt->size, pkt->pts, pkt->dts);

    int ret = avcodec_send_packet(video_codec_ctx, pkt);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        log_message(LOG_WARNING, "Media Player", "avcodec_send_packet (video) failed: %s", err_buf);
        return false;
    }

    int frames_decoded = 0;
    while (true) {
        AVFrame* out_frame = av_frame_alloc();
        if (!out_frame) {
            log_message(LOG_ERROR, "Media Player", "Failed to allocate output frame");
            break;
        }

        ret = avcodec_receive_frame(video_codec_ctx, out_frame);
        if (ret == AVERROR(EAGAIN)) {
            log_message(LOG_DEBUG, "Media Player", "Video decoder needs more data");
            av_frame_free(&out_frame);
            break;
        } else if (ret == AVERROR_EOF) {
            log_message(LOG_DEBUG, "Media Player", "Video decoder EOF");
            av_frame_free(&out_frame);
            break;
        } else if (ret < 0) {
            char err_buf[128];
            av_strerror(ret, err_buf, sizeof(err_buf));
            log_message(LOG_ERROR, "Media Player", "avcodec_receive_frame (video) failed: %s", err_buf);
            av_frame_free(&out_frame);
            break;
        }

        frames_decoded++;

        AVFrame* frame =
            (out_frame->format == PIX_FMT_TARGET)
                ? out_frame
                : convert_frame_to_yuv420p(out_frame);

        if (frame != out_frame) {
            av_frame_free(&out_frame);
        }

        if (!frame) {
            log_message(LOG_ERROR, "Media Player", "Frame conversion failed");
            continue;
        }

        double pts = frame->best_effort_timestamp * av_q2d(video_time_base);
        log_message(LOG_DEBUG, "Media Player", "Video frame decoded: pts=%.3f, format=%d, size=%dx%d",
                    pts, frame->format, frame->width, frame->height);

        while (thread_running && playback_running &&
               !seeking &&
               pts - get_master_clock() > 0.005) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1));
        }

        if (!video_queue.push(frame, pts)) {
            log_message(LOG_WARNING, "Media Player", "Video queue full, dropping frame at pts=%.3f", pts);
            av_frame_free(&frame);
        }
    }

    if (frames_decoded > 0) {
        log_message(LOG_DEBUG, "Media Player", "Decoded %d video frame(s) from packet", frames_decoded);
    }

    return true;
}

static bool process_audio_packet(AVPacket* pkt) {
    if (!pkt) {
        log_message(LOG_WARNING, "Media Player", "process_audio_packet: null packet");
        return false;
    }

    log_message(LOG_DEBUG, "Media Player", "Processing audio packet: size=%d, pts=%lld, dts=%lld",
                pkt->size, pkt->pts, pkt->dts);

    int ret = avcodec_send_packet(audio_codec_ctx, pkt);
    if (ret != 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        log_message(LOG_WARNING, "Media Player", "avcodec_send_packet (audio) failed: %s", err_buf);
        return false;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        log_message(LOG_ERROR, "Media Player", "Failed to allocate audio frame");
        return false;
    }

    std::vector<uint8_t> out_buf;
    int frames_decoded = 0;

    while (avcodec_receive_frame(audio_codec_ctx, frame) == 0) {
        if (seeking.load(std::memory_order_acquire)) {
            log_message(LOG_DEBUG, "Media Player", "Seeking in progress, skipping audio frame");
            av_frame_unref(frame);
            break;
        }

        frames_decoded++;

        int delay = swr_get_delay(swr_ctx, audio_codec_ctx->sample_rate);
        int max_samples = av_rescale_rnd(frame->nb_samples + delay,
                                         AUDIO_OUT_SAMPLE_RATE,
                                         audio_codec_ctx->sample_rate,
                                         AV_ROUND_UP);

        int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        size_t needed_bytes = (size_t)max_samples * AUDIO_OUT_CHANNELS * bytes_per_sample;
        if (out_buf.size() < needed_bytes) {
            log_message(LOG_DEBUG, "Media Player", "Resizing audio buffer: %zu -> %zu bytes",
                        out_buf.size(), needed_bytes);
            out_buf.resize(needed_bytes);
        }

        uint8_t* out_ptr = out_buf.data();
        int out_samples = swr_convert(swr_ctx, &out_ptr, max_samples,
                                      (const uint8_t**)frame->data, frame->nb_samples);

        if (out_samples < 0) {
            log_message(LOG_ERROR, "Media Player", "swr_convert failed: %d", out_samples);
        } else if (out_samples > 0 && playback_running.load() && audio_device != 0) {
            int bytes = out_samples * AUDIO_OUT_CHANNELS * bytes_per_sample;
            int queued_before = SDL_QueueAudio(audio_device, out_buf.data(), bytes);
            if (queued_before < 0) {
                log_message(LOG_ERROR, "Media Player", "SDL_QueueAudio failed");
            } else {
                log_message(LOG_DEBUG, "Media Player", "Queued %d audio bytes (%d samples)",
                            bytes, out_samples);
            }
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        int64_t best_pts = av_frame_get_best_effort_timestamp(frame);
#pragma GCC diagnostic pop
        
        if (best_pts != AV_NOPTS_VALUE) {
            AVRational tb = fmt_ctx->streams[audio_stream_index]->time_base;
            double pts_sec = best_pts * av_q2d(tb);
            audio_pts.store((float)pts_sec, std::memory_order_release);
            log_message(LOG_DEBUG, "Media Player", "Audio PTS updated: %.3f", pts_sec);
        } else {
            log_message(LOG_DEBUG, "Media Player", "Audio frame has no PTS");
        }

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    
    if (frames_decoded > 0) {
        log_message(LOG_DEBUG, "Media Player", "Decoded %d audio frame(s) from packet", frames_decoded);
    }

    return true;
}

static void decode_loop() {
    log_message(LOG_OK, "Media Player", "Decode thread started");

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        log_message(LOG_ERROR, "Media Player", "Failed to allocate packet");
        return;
    }

    int packet_count = 0;
    int video_packet_count = 0;
    int audio_packet_count = 0;

    while (thread_running.load()) {
        if (!media_initialized.load(std::memory_order_acquire)) {
            log_message(LOG_DEBUG, "Media Player", "Decode loop waiting for initialization");
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        while (!playback_running.load() && thread_running.load()) {
            if (packet_count > 0) {
                log_message(LOG_DEBUG, "Media Player", "Playback paused (processed %d packets: %d video, %d audio)",
                            packet_count, video_packet_count, audio_packet_count);
                packet_count = 0;
                video_packet_count = 0;
                audio_packet_count = 0;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        if (!thread_running.load()) {
            log_message(LOG_DEBUG, "Media Player", "Decode thread stopping");
            break;
        }

        if (seeking.load(std::memory_order_acquire)) {
            log_message(LOG_DEBUG, "Media Player", "Decode loop paused for seeking");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        if (audio_enabled && audio_device != 0) {
            Uint32 queued = SDL_GetQueuedAudioSize(audio_device);
            if (queued > OUT_BUF_THRESHOLD) {
                log_message(LOG_DEBUG, "Media Player", "Audio buffer full (%u bytes), throttling", queued);
                while (SDL_GetQueuedAudioSize(audio_device) > OUT_BUF_THRESHOLD &&
                       thread_running.load() &&
                       !seeking.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
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

            if (ret == AVERROR_EOF) {
                log_message(LOG_OK, "Media Player", "End of file reached (processed %d packets total)",
                            packet_count);
            } else {
                char err_buf[128];
                av_strerror(ret, err_buf, sizeof(err_buf));
                log_message(LOG_ERROR, "Media Player", "av_read_frame failed: %s", err_buf);
            }

            if (video_codec_ctx) {
                log_message(LOG_DEBUG, "Media Player", "Flushing video decoder");
                avcodec_send_packet(video_codec_ctx, nullptr);
            }
            if (audio_codec_ctx) {
                log_message(LOG_DEBUG, "Media Player", "Flushing audio decoder");
                avcodec_send_packet(audio_codec_ctx, nullptr);
            }

            thread_running = false;
            break;
        }

        packet_count++;

        if (pkt->stream_index == video_stream_index && video_codec_ctx) {
            video_packet_count++;
            log_message(LOG_DEBUG, "Media Player", "Read video packet #%d (total %d)",
                        video_packet_count, packet_count);
            process_video_packet(pkt);
        } else if (pkt->stream_index == audio_stream_index && audio_codec_ctx) {
            audio_packet_count++;
            log_message(LOG_DEBUG, "Media Player", "Read audio packet #%d (total %d)",
                        audio_packet_count, packet_count);
            process_audio_packet(pkt);
        } else {
            log_message(LOG_DEBUG, "Media Player", "Skipping packet from stream %d", pkt->stream_index);
        }

        av_packet_unref(pkt);

        decode_users.fetch_sub(1, std::memory_order_acq_rel);
        decode_cv.notify_all();
    }

    av_packet_free(&pkt);
    log_message(LOG_OK, "Media Player", "Decode thread finished (processed %d total packets: %d video, %d audio)",
                packet_count, video_packet_count, audio_packet_count);
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
    video_stream_index =
        av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_stream_index < 0) {
        log_message(LOG_WARNING, "Media Player", "No video stream found");
        return -1;
    }

    AVStream* stream = fmt_ctx->streams[video_stream_index];
    video_time_base = stream->time_base;

    const AVCodec* codec =
        avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, "Media Player", "Video codec not found");
        return -1;
    }

    if (stream->codecpar->codec_id == AV_CODEC_ID_H264 &&
        stream->codecpar->height == 720) {
        const AVCodec* hw =
            avcodec_find_decoder_by_name("h264_wiiu");
        if (hw) {
            codec = hw;
            log_message(LOG_OK, "Media Player", "Using hardware H.264 decoder");
        }
    }

    video_codec_ctx = avcodec_alloc_context3(codec);
    if (!video_codec_ctx) {
        log_message(LOG_ERROR, "Media Player", "Failed to allocate video codec context");
        return -1;
    }

    if (avcodec_parameters_to_context(video_codec_ctx,
                                      stream->codecpar) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to copy codec params");
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
        return -1;
    }

    if (avcodec_open2(video_codec_ctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to open video codec");
        avcodec_free_context(&video_codec_ctx);
        video_codec_ctx = nullptr;
        return -1;
    }

    current_frame_info = new frame_info();
    current_frame_info->width  = video_codec_ctx->width;
    current_frame_info->height = video_codec_ctx->height;
    current_frame_info->texture =
        SDL_CreateTexture(
            sdl_get()->sdl_renderer,
            SDL_PIXELFORMAT_IYUV,
            SDL_TEXTUREACCESS_STREAMING,
            video_codec_ctx->width,
            video_codec_ctx->height
        );

    if (!current_frame_info->texture) {
        log_message(LOG_ERROR, "Media Player", "Failed to create texture");
        delete current_frame_info;
        current_frame_info = nullptr;
        return -1;
    }

    log_message(LOG_OK, "Media Player", "Video stream initialized (%dx%d)",
                video_codec_ctx->width, video_codec_ctx->height);
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
    if (!audio_codec_ctx) {
        log_message(LOG_ERROR, "Media Player", "Failed to allocate audio codec context");
        return -1;
    }

    if (avcodec_parameters_to_context(audio_codec_ctx, codecpar) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to copy audio codec params");
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        return -1;
    }

    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to open audio codec");
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
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
    
    log_message(LOG_OK, "Media Player", "Audio stream initialized (%d Hz, %d ch)", audio_codec_ctx->sample_rate, audio_codec_ctx->channels);
    return 0;
}

int media_player_init(const char* filepath) {
    if (media_initialized.load()) {
        log_message(LOG_ERROR, "Media Player", "Already initialized");
        return -1;
    }

    log_message(LOG_OK, "Media Player", "Initializing: %s", filepath);

    avformat_network_init();

    if (avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to open input file");
        return -1;
    }

    fmt_ctx->flags |= AVFMT_FLAG_NOBUFFER;
    fmt_ctx->probesize = 32 * 1024;
    fmt_ctx->max_analyze_duration = AV_TIME_BASE / 2;

    log_message(LOG_DEBUG, "Media Player", "Finding stream info...");
    int ret = avformat_find_stream_info(fmt_ctx, nullptr);
    
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        log_message(LOG_ERROR, "Media Player", "Failed to find stream info: %s (code %d)", 
                    err_buf, ret);
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
        return -1;
    }

    log_message(LOG_DEBUG, "Media Player", "Stream info found, initializing codecs...");

    bool has_video = (init_video_stream() == 0);
    bool has_audio = (init_audio_stream() == 0);

    if (!has_video && !has_audio) {
        log_message(LOG_ERROR, "Media Player", "No usable streams found");
        media_player_cleanup();
        return -1;
    }

    log_message(LOG_OK, "Media Player", "Streams initialized - Video: %s, Audio: %s",
                has_video ? "yes" : "no", has_audio ? "yes" : "no");

    start_time_us = get_time_us();
    pause_start_us = start_time_us;
    audio_pts.store(0.0f);

    thread_running = true;
    playback_running = false;
    
    media_initialized = true;
    decode_thread = std::thread(decode_loop);

    media_info_get()->playback_status = false;
    if (has_video && video_stream_index >= 0) {
        AVStream* vs = fmt_ctx->streams[video_stream_index];
        media_info_get()->total_video_playback_time = 
            (vs->duration != AV_NOPTS_VALUE) ? 
            vs->duration * av_q2d(vs->time_base) : 0.0;
    }
    if (has_audio && audio_stream_index >= 0) {
        AVStream* as = fmt_ctx->streams[audio_stream_index];
        media_info_get()->total_audio_playback_time = 
            (as->duration != AV_NOPTS_VALUE) ? 
            (int64_t)(as->duration * av_q2d(as->time_base)) : 0;
    }

    log_message(LOG_OK, "Media Player", "Initialization complete");
    return 0;
}

void media_player_play(bool play) {
    log_message(LOG_OK, "Media Player", "Playback %s requested", play ? "START" : "PAUSE");

    if (play && pause_start_us != 0) {
        int64_t paused_duration = get_time_us() - pause_start_us;
        start_time_us += paused_duration;
        log_message(LOG_DEBUG, "Media Player", "Resuming from pause (was paused for %.3f seconds)",
                    paused_duration / 1'000'000.0);
        pause_start_us = 0;
    } else if (!play) {
        pause_start_us = get_time_us();
        double current_time = get_master_clock();
        log_message(LOG_DEBUG, "Media Player", "Pausing at time %.3f", current_time);
    }
    
    playback_running = play;
    media_info_get()->playback_status = play;
    
    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, play ? 0 : 1);
        if (play) {
            Uint32 queued = SDL_GetQueuedAudioSize(audio_device);
            log_message(LOG_DEBUG, "Media Player", "Audio resumed with %u bytes queued", queued);
        }
    }

    log_message(LOG_OK, "Media Player", "Playback state changed to: %s", play ? "PLAYING" : "PAUSED");
}

void media_player_seek(double seconds) {
    if (!fmt_ctx) {
        log_message(LOG_ERROR, "Media Player", "Seek failed: format context is null");
        return;
    }

    log_message(LOG_OK, "Media Player", "SEEKING to %.3f seconds", seconds);

    log_message(LOG_DEBUG, "Media Player", "Pausing playback");
    media_player_play(false);

    {
        std::lock_guard<std::mutex> lock(decode_mutex);
        seeking.store(true, std::memory_order_release);
        log_message(LOG_DEBUG, "Media Player", "Seeking flag set");
    }

    {
        log_message(LOG_DEBUG, "Media Player", "Waiting for decode users to finish...");
        std::unique_lock<std::mutex> lock(decode_mutex);
        decode_cv.wait(lock, [] { 
            int users = decode_users.load(std::memory_order_acquire);
            if (users > 0) {
                log_message(LOG_DEBUG, "Media Player", "Still waiting... %d decode users active", users);
            }
            return users == 0; 
        });
        log_message(LOG_DEBUG, "Media Player", "All decode users finished");
    }

    std::lock_guard<std::mutex> lock(decode_mutex);

    log_message(LOG_DEBUG, "Media Player", "Performing seek operation");
    int64_t seek_target = static_cast<int64_t>(seconds * AV_TIME_BASE);
    
    int ret = av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        log_message(LOG_ERROR, "Media Player", "av_seek_frame failed: %s", err_buf);
    } else {
        log_message(LOG_DEBUG, "Media Player", "Flushing buffers");
        avformat_flush(fmt_ctx);
        
        if (video_codec_ctx) {
            avcodec_flush_buffers(video_codec_ctx);
            log_message(LOG_DEBUG, "Media Player", "Video codec buffers flushed");
        }
        if (audio_codec_ctx) {
            avcodec_flush_buffers(audio_codec_ctx);
            log_message(LOG_DEBUG, "Media Player", "Audio codec buffers flushed");
        }
        
        log_message(LOG_DEBUG, "Media Player", "Clearing frame queue");
        video_queue.clear();
        
        if (audio_device != 0) {
            SDL_ClearQueuedAudio(audio_device);
            log_message(LOG_DEBUG, "Media Player", "SDL audio queue cleared");
        }
        
        if (swr_ctx) {
            swr_init(swr_ctx);
            log_message(LOG_DEBUG, "Media Player", "Resampler reinitialized");
        }
        
        log_message(LOG_DEBUG, "Media Player", "Updating timestamps");
        audio_pts.store((float)seconds, std::memory_order_release);
        start_time_us = get_time_us() - static_cast<int64_t>(seconds * 1'000'000);
        pause_start_us = start_time_us;
        
        log_message(LOG_OK, "Media Player", "Seek completed successfully to %.3f", seconds);
    }

    log_message(LOG_DEBUG, "Media Player", "Clearing seeking flag");
    seeking.store(false, std::memory_order_release);
    decode_cv.notify_all();

    log_message(LOG_DEBUG, "Media Player", "Resuming playback");
    media_player_play(true);
    
    log_message(LOG_OK, "Media Player", "SEEK OPERATION COMPLETE");
}

bool media_player_switch_audio_track(int new_stream_index) {
    log_message(LOG_OK, "Media Player", "SWITCHING AUDIO TRACK to stream %d", new_stream_index);

    if (!fmt_ctx || !audio_enabled) {
        log_message(LOG_ERROR, "Media Player", "Track switch failed: fmt_ctx=%p audio_enabled=%d",
                    fmt_ctx, audio_enabled);
        return false;
    }
    
    if (new_stream_index < 0 || new_stream_index >= (int)fmt_ctx->nb_streams) {
        log_message(LOG_ERROR, "Media Player", "Invalid stream index: %d (valid range: 0-%d)",
                    new_stream_index, fmt_ctx->nb_streams - 1);
        return false;
    }

    AVStream* new_stream = fmt_ctx->streams[new_stream_index];
    if (new_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
        log_message(LOG_ERROR, "Media Player", "Stream %d is not audio (type=%d)",
                    new_stream_index, new_stream->codecpar->codec_type);
        return false;
    }

    double current_time = get_master_clock();
    log_message(LOG_DEBUG, "Media Player", "Current time: %.3f, switching from stream %d to %d",
                current_time, audio_stream_index, new_stream_index);

    {
        std::lock_guard<std::mutex> lock(decode_mutex);
        seeking.store(true, std::memory_order_release);
        log_message(LOG_DEBUG, "Media Player", "Seeking flag set for track switch");
    }

    {
        log_message(LOG_DEBUG, "Media Player", "Waiting for decode users...");
        std::unique_lock<std::mutex> lock(decode_mutex);
        decode_cv.wait(lock, []{ 
            return decode_users.load(std::memory_order_acquire) == 0; 
        });
        log_message(LOG_DEBUG, "Media Player", "All decode users finished");
    }

    std::lock_guard<std::mutex> lock(decode_mutex);

    if (audio_device != 0) {
        log_message(LOG_DEBUG, "Media Player", "Pausing and clearing audio device");
        SDL_PauseAudioDevice(audio_device, 1);
        Uint32 queued_before = SDL_GetQueuedAudioSize(audio_device);
        SDL_ClearQueuedAudio(audio_device);
        log_message(LOG_DEBUG, "Media Player", "Cleared %u bytes from audio queue", queued_before);
    }

    if (audio_codec_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Freeing old audio codec context");
        avcodec_free_context(&audio_codec_ctx); 
        audio_codec_ctx = nullptr; 
    }
    if (swr_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Freeing old resampler context");
        swr_free(&swr_ctx); 
        swr_ctx = nullptr; 
    }

    log_message(LOG_DEBUG, "Media Player", "Finding decoder for codec ID %d",
                new_stream->codecpar->codec_id);
    const AVCodec* codec = avcodec_find_decoder(new_stream->codecpar->codec_id);
    if (!codec) {
        log_message(LOG_ERROR, "Media Player", "Codec not found for stream %d", new_stream_index);
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    log_message(LOG_DEBUG, "Media Player", "Allocating new codec context for %s",
                avcodec_get_name(new_stream->codecpar->codec_id));
    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx) {
        log_message(LOG_ERROR, "Media Player", "Failed to allocate codec context");
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    if (avcodec_parameters_to_context(audio_codec_ctx, new_stream->codecpar) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to copy codec parameters");
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        log_message(LOG_ERROR, "Media Player", "Failed to open codec");
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    log_message(LOG_OK, "Media Player", "New audio codec opened: %s, %d Hz, %d channels",
                avcodec_get_name(new_stream->codecpar->codec_id),
                audio_codec_ctx->sample_rate,
                audio_codec_ctx->channels);

    log_message(LOG_DEBUG, "Media Player", "Creating new resampler context");
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
        if (swr_ctx) swr_free(&swr_ctx);
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        seeking.store(false);
        decode_cv.notify_all();
        return false;
    }

    audio_stream_index = new_stream_index;
    current_audio_track_id = new_stream_index;
    log_message(LOG_OK, "Media Player", "Audio track switched to stream %d", new_stream_index);

    log_message(LOG_DEBUG, "Media Player", "Seeking to current position %.3f", current_time);
    int64_t seek_target = static_cast<int64_t>(current_time * AV_TIME_BASE);
    int ret = av_seek_frame(fmt_ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_ANY);
    if (ret < 0) {
        char err_buf[128];
        av_strerror(ret, err_buf, sizeof(err_buf));
        log_message(LOG_WARNING, "Media Player", "Seek after track switch failed: %s", err_buf);
    } else {
        log_message(LOG_DEBUG, "Media Player", "Flushing format and codec buffers");
        avformat_flush(fmt_ctx);
        avcodec_flush_buffers(audio_codec_ctx);
        audio_pts.store((float)current_time, std::memory_order_release);
    }

    if (playback_running.load() && audio_device != 0) {
        log_message(LOG_DEBUG, "Media Player", "Resuming audio device");
        SDL_PauseAudioDevice(audio_device, 0);
    }

    seeking.store(false, std::memory_order_release);
    decode_cv.notify_all();

    log_message(LOG_OK, "Media Player", "AUDIO TRACK SWITCH COMPLETE");
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
    log_message(LOG_OK, "Media Player", "CLEANUP STARTING");

    log_message(LOG_DEBUG, "Media Player", "Stopping threads");
    thread_running = false;
    playback_running = false;
    media_initialized = false;

    if (decode_thread.joinable()) {
        log_message(LOG_DEBUG, "Media Player", "Waiting for decode thread to finish...");
        decode_thread.join();
        log_message(LOG_DEBUG, "Media Player", "Decode thread joined");
    }

    log_message(LOG_DEBUG, "Media Player", "Clearing video queue");
    video_queue.clear();

    if (audio_device != 0) {
        log_message(LOG_DEBUG, "Media Player", "Cleaning up audio device");
        SDL_PauseAudioDevice(audio_device, 1);
        Uint32 queued = SDL_GetQueuedAudioSize(audio_device);
        log_message(LOG_DEBUG, "Media Player", "Clearing %u bytes from audio queue", queued);
        SDL_ClearQueuedAudio(audio_device);
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
        log_message(LOG_DEBUG, "Media Player", "Audio device closed");
    }

    if (video_codec_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Freeing video codec context");
        avcodec_free_context(&video_codec_ctx); 
        video_codec_ctx = nullptr; 
    }
    
    if (audio_codec_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Freeing audio codec context");
        avcodec_free_context(&audio_codec_ctx); 
        audio_codec_ctx = nullptr; 
    }

    if (sws_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Freeing scaler context");
        sws_freeContext(sws_ctx); 
        sws_ctx = nullptr; 
    }
    
    if (swr_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Freeing resampler context");
        swr_free(&swr_ctx); 
        swr_ctx = nullptr; 
    }

    if (current_frame_info) {
        log_message(LOG_DEBUG, "Media Player", "Freeing frame info");
        if (current_frame_info->texture) {
            SDL_DestroyTexture(current_frame_info->texture);
            current_frame_info->texture = nullptr;
        }
        delete current_frame_info;
        current_frame_info = nullptr;
    }

    if (fmt_ctx) { 
        log_message(LOG_DEBUG, "Media Player", "Closing format context");
        avformat_close_input(&fmt_ctx); 
        fmt_ctx = nullptr; 
    }

    if (SDL_WasInit(SDL_INIT_AUDIO)) {
        log_message(LOG_DEBUG, "Media Player", "Step 10: Shutting down SDL audio");
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    log_message(LOG_DEBUG, "Media Player", "Step 11: Deinitializing network");
    avformat_network_deinit();

    log_message(LOG_DEBUG, "Media Player", "Step 12: Resetting state variables");
    audio_enabled = false;
    audio_stream_index = -1;
    video_stream_index = -1;
    current_audio_track_id = -1;
    dest_rect_initialised = false;

    log_message(LOG_OK, "Media Player", "CLEANUP COMPLETE");
}
