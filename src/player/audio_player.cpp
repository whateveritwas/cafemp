#include <mutex>
#include <cstdio>
#include <thread>
#include <atomic>
#include <vector>
#include <cstring>
#include <algorithm>
#include <condition_variable>

#include "utils/media_info.hpp"
#include "logger/logger.hpp"
#include "player/audio_player.hpp"

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec audio_spec;

static AVFormatContext* fmt_ctx = nullptr;
static AVCodecContext* audio_codec_ctx = nullptr;
static SwrContext* swr_ctx = nullptr;
static AVFrame* audio_frame = nullptr;

static int audio_stream_index = -1;
static std::thread audio_thread;
static std::atomic<bool> audio_thread_running = false;
static std::atomic<float> current_play_time(0.0f);
bool audio_enabled = false;
static std::atomic<bool> audio_playing = false;

static int out_channels = 2;
static int out_sample_rate = 48000;

static std::mutex audio_mutex;
static std::atomic<bool> switching_audio_stream = false;

static int current_audio_track_id = -1;
static std::vector<AudioTrackInfo> audioTracks;
static std::mutex audioTracksMutex;

static std::atomic<int> audio_decode_users{0};

struct AudioSharedSnapshot {
    AVFormatContext* fmt_ctx;
    AVCodecContext* codec_ctx;
    SwrContext* swr_ctx;
    int stream_index;
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;
    bool enabled;
    bool playing;
};

static void audio_decode_loop() {
    AVPacket* local_packet = av_packet_alloc();
    AVFrame* local_frame = audio_frame ? audio_frame : av_frame_alloc();
    std::vector<uint8_t> out_buf;

    while (audio_thread_running.load(std::memory_order_acquire)) {
        if (switching_audio_stream.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            continue;
        }

        AudioSharedSnapshot snap;
        bool have_user = false;

        {
            std::lock_guard<std::mutex> lock(audio_mutex);
            if (switching_audio_stream.load(std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }

            snap.fmt_ctx = fmt_ctx;
            snap.codec_ctx = audio_codec_ctx;
            snap.swr_ctx = swr_ctx;
            snap.stream_index = audio_stream_index;
            snap.device = audio_device;
            snap.spec = audio_spec;
            snap.enabled = audio_enabled;
            snap.playing = audio_playing.load(std::memory_order_relaxed);

            audio_decode_users.fetch_add(1, std::memory_order_acq_rel);
            have_user = true;
        }

        if (!snap.fmt_ctx || !snap.enabled || !snap.codec_ctx || snap.stream_index < 0) {
            if (have_user) audio_decode_users.fetch_sub(1, std::memory_order_acq_rel);
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }

        int ret = av_read_frame(snap.fmt_ctx, local_packet);
        if (ret < 0) {
            if (have_user) audio_decode_users.fetch_sub(1, std::memory_order_acq_rel);
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }

        if (local_packet->stream_index != snap.stream_index) {
            av_packet_unref(local_packet);
            if (have_user) audio_decode_users.fetch_sub(1, std::memory_order_acq_rel);
            continue;
        }

        if (avcodec_send_packet(snap.codec_ctx, local_packet) == 0) {
            while (avcodec_receive_frame(snap.codec_ctx, local_frame) == 0) {
                int64_t delay = swr_get_delay(snap.swr_ctx, snap.codec_ctx->sample_rate);
                int max_out_samples = av_rescale_rnd(local_frame->nb_samples + delay,
                                                    out_sample_rate,
                                                    snap.codec_ctx->sample_rate,
                                                    AV_ROUND_UP);

                size_t required_bytes = (size_t)max_out_samples * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                if (required_bytes > out_buf.size()) out_buf.resize(required_bytes);

                uint8_t* out_ptrs[1] = { out_buf.data() };

                int out_samples = swr_convert(
                    snap.swr_ctx,
                    out_ptrs,
                    max_out_samples,
                    (const uint8_t**)local_frame->data,
                    local_frame->nb_samples
                );

                if (out_samples <= 0) continue;

                int data_size = out_samples * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                if (snap.playing && snap.device != 0) {
                    SDL_QueueAudio(snap.device, out_buf.data(), data_size);
                }

                if (local_frame->pts != AV_NOPTS_VALUE) {
                    AVRational time_base = snap.fmt_ctx->streams[snap.stream_index]->time_base;
                    double pts_time = (double)local_frame->pts * av_q2d(time_base);
                    if (pts_time >= 0.0) {
                        current_play_time.store((float)pts_time, std::memory_order_release);
                    }
                }
            }
        }

        av_packet_unref(local_packet);
        if (have_user) audio_decode_users.fetch_sub(1, std::memory_order_acq_rel);
    }

    if (!audio_frame && local_frame) av_frame_free(&local_frame);
    if (local_packet) av_packet_free(&local_packet);
}

void collect_audio_tracks(AVFormatContext* in_fmt_ctx) {
    if (!in_fmt_ctx) {
        log_message(LOG_ERROR, "Audio Player", "Format context not initialized");
        return;
    }

    std::lock_guard<std::mutex> lock(audioTracksMutex);
    audioTracks.clear();

    for (unsigned int i = 0; i < in_fmt_ctx->nb_streams; ++i) {
        AVStream* stream = in_fmt_ctx->streams[i];
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

            audioTracks.push_back(info);
        }
    }

    media_info_get()->total_audio_track_count = audioTracks.size();
}

std::vector<AudioTrackInfo> get_audio_tracks() {
    std::lock_guard<std::mutex> lock(audioTracksMutex);
    return audioTracks;
}

int audio_player_init(const char* filepath) {
    log_message(LOG_OK, "Audio Player", "Starting Audio Player");

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            log_message(LOG_ERROR, "Audio Player", "SDL_InitSubSystem failed: %s", SDL_GetError());
            return -1;
        }
    }

    if (avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr) != 0) {
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        return -1;
    }

    int found_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (found_index < 0) {
        audio_enabled = false;
        return 0;
    }

    std::lock_guard<std::mutex> lock(audio_mutex);

    audio_stream_index = found_index;
    current_audio_track_id = found_index;
    audio_enabled = true;

    AVCodecParameters* codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) return -1;

    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(audio_codec_ctx, codecpar) < 0) return -1;
    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) return -1;

    SDL_AudioSpec wanted_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.samples = 4096;
    wanted_spec.callback = nullptr;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &audio_spec, 0);
    if (!audio_device) return -1;

    swr_ctx = swr_alloc_set_opts(
        nullptr,
        av_get_default_channel_layout(out_channels),
        AV_SAMPLE_FMT_S16,
        out_sample_rate,
        av_get_default_channel_layout(audio_codec_ctx->channels),
        audio_codec_ctx->sample_fmt,
        audio_codec_ctx->sample_rate,
        0, nullptr
    );
    if (!swr_ctx || swr_init(swr_ctx) < 0) return -1;

    audio_frame = av_frame_alloc();

    collect_audio_tracks(fmt_ctx);

    SDL_ClearQueuedAudio(audio_device);
    SDL_PauseAudioDevice(audio_device, 0);

    audio_thread_running = true;
    audio_thread = std::thread(audio_decode_loop);

    media_info_get()->total_audio_playback_time = (int64_t)audio_player_get_total_play_time();

    return 0;
}

bool audio_player_switch_audio_stream(int new_stream_index) {
    if (!fmt_ctx) return false;
    if (new_stream_index < 0 || new_stream_index >= (int)fmt_ctx->nb_streams) return false;

    AVStream* new_stream = fmt_ctx->streams[new_stream_index];
    if (new_stream->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) return false;

    double current_time = audio_player_get_current_play_time();

    switching_audio_stream.store(true, std::memory_order_release);

    // wait until decoder is not using old ctx
    while (audio_decode_users.load(std::memory_order_acquire) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::lock_guard<std::mutex> lock(audio_mutex);

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
        switching_audio_stream.store(false);
        return false;
    }

    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx) {
        switching_audio_stream.store(false);
        return false;
    }

    if (avcodec_parameters_to_context(audio_codec_ctx, new_stream->codecpar) < 0) {
        avcodec_free_context(&audio_codec_ctx);
        switching_audio_stream.store(false);
        return false;
    }

    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        avcodec_free_context(&audio_codec_ctx);
        switching_audio_stream.store(false);
        return false;
    }

    swr_ctx = swr_alloc_set_opts(
        nullptr,
        av_get_default_channel_layout(out_channels),
        AV_SAMPLE_FMT_S16,
        out_sample_rate,
        av_get_default_channel_layout(audio_codec_ctx->channels),
        audio_codec_ctx->sample_fmt,
        audio_codec_ctx->sample_rate,
        0, nullptr
    );

    if (!swr_ctx || swr_init(swr_ctx) < 0) {
        if (swr_ctx) swr_free(&swr_ctx);
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
        switching_audio_stream.store(false);
        return false;
    }

    audio_stream_index = new_stream_index;
    current_audio_track_id = new_stream_index;

    AVRational tb = fmt_ctx->streams[audio_stream_index]->time_base;
    int64_t seek_target = static_cast<int64_t>(current_time / av_q2d(tb));

    if (av_seek_frame(fmt_ctx, audio_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
        log_message(LOG_ERROR, "Audio Player", "Seek after stream switch failed");
    }

    if (audio_codec_ctx) avcodec_flush_buffers(audio_codec_ctx);
    if (swr_ctx) swr_init(swr_ctx);

    SDL_ClearQueuedAudio(audio_device);
    current_play_time.store((float)current_time);

    if (audio_playing.load(std::memory_order_relaxed)) SDL_PauseAudioDevice(audio_device, 0);

    switching_audio_stream.store(false);
    return true;
}

void audio_player_play(bool state) {
    media_info_get()->playback_status = state;

    audio_playing.store(state);
    if (audio_device != 0) {
        if (state) SDL_PauseAudioDevice(audio_device, 0);
        else SDL_PauseAudioDevice(audio_device, 1);
    }
}

bool audio_player_get_audio_play_state() {
    return audio_playing.load(std::memory_order_relaxed);
}

void audio_player_seek(float delta_time) {
    if (!fmt_ctx || !audio_enabled) return;

    switching_audio_stream.store(true, std::memory_order_release);

    while (audio_decode_users.load(std::memory_order_acquire) > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::lock_guard<std::mutex> lock(audio_mutex);

    double base_time = audio_player_get_current_play_time();
    double target_time = std::clamp(base_time + delta_time, 0.0, audio_player_get_total_play_time());

    AVRational tb = fmt_ctx->streams[audio_stream_index]->time_base;
    int64_t seek_target = (int64_t)llround(target_time / av_q2d(tb));

    if (av_seek_frame(fmt_ctx, audio_stream_index, seek_target, AVSEEK_FLAG_BACKWARD) < 0) {
        log_message(LOG_ERROR, "Audio Player", "Seek failed");
    } else {
        avformat_flush(fmt_ctx);
        if (audio_codec_ctx) avcodec_flush_buffers(audio_codec_ctx);
        SDL_ClearQueuedAudio(audio_device);
        if (swr_ctx) swr_init(swr_ctx);
        current_play_time.store((float)target_time, std::memory_order_release);
    }

    switching_audio_stream.store(false, std::memory_order_release);
}

double audio_player_get_current_play_time() {
    float pts = current_play_time.load(std::memory_order_acquire);

    Uint32 queued_bytes = 0;
    if (audio_device != 0) queued_bytes = SDL_GetQueuedAudioSize(audio_device);

    double queued_seconds = 0.0;
    if (audio_device != 0 && audio_spec.freq > 0 && audio_spec.channels > 0) {
        queued_seconds = (double)queued_bytes / (audio_spec.freq * audio_spec.channels * (SDL_AUDIO_BITSIZE(audio_spec.format) / 8));
    }

    double corrected_time = (double)pts - queued_seconds;
    if (corrected_time < 0.0) corrected_time = 0.0;
    return corrected_time;
}

double audio_player_get_total_play_time() {
    if (!fmt_ctx || audio_stream_index < 0) return 0.0;
    int64_t duration = fmt_ctx->streams[audio_stream_index]->duration;
    double total_time = (double)duration * av_q2d(fmt_ctx->streams[audio_stream_index]->time_base);
    return total_time;
}

void audio_player_cleanup() {
    if (!audio_enabled) return;

    audio_thread_running = false;
    if (audio_thread.joinable()) audio_thread.join();

    std::lock_guard<std::mutex> lock(audio_mutex);

    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_ClearQueuedAudio(audio_device);
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }

    if (audio_frame) { av_frame_free(&audio_frame); audio_frame = nullptr; }
    if (swr_ctx) { swr_free(&swr_ctx); swr_ctx = nullptr; }
    if (audio_codec_ctx) { avcodec_free_context(&audio_codec_ctx); audio_codec_ctx = nullptr; }
    if (fmt_ctx) { avformat_close_input(&fmt_ctx); fmt_ctx = nullptr; }

    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);

    audio_enabled = false;
    audio_playing.store(false);
    audio_stream_index = -1;
    current_audio_track_id = -1;
    current_play_time.store(0.0f);
}

int audio_player_get_current_track_id() {
    return current_audio_track_id;
}
