#include "audio_player.hpp"
#include <cstdio>
#include <cstring>
#include <thread>
#include <atomic>

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec audio_spec;

static AVFormatContext* fmt_ctx = nullptr;
static AVCodecContext* audio_codec_ctx = nullptr;
static SwrContext* swr_ctx = nullptr;
static AVFrame* audio_frame = nullptr;
static AVPacket* audio_packet = nullptr;

static int audio_stream_index = -1;
static std::thread audio_thread;
static std::atomic<bool> audio_thread_running = false;
static std::atomic<float> current_play_time(0.0f);
bool audio_enabled = false;
static bool audio_playing = false;

static int out_channels = 2;
static int out_sample_rate = 48000;

static void audio_decode_loop() {
    while (audio_thread_running.load()) {
        if (!fmt_ctx || !audio_enabled || !audio_playing) {
            SDL_Delay(10);
            continue;
        }

        if (av_read_frame(fmt_ctx, audio_packet) >= 0) {
            if (audio_packet->stream_index == audio_stream_index) {
                if (avcodec_send_packet(audio_codec_ctx, audio_packet) == 0) {
                    while (avcodec_receive_frame(audio_codec_ctx, audio_frame) == 0) {
                        uint8_t temp_buffer[8192];
                        uint8_t* out_buffers[] = { temp_buffer };

                        int out_samples = swr_convert(
                            swr_ctx,
                            out_buffers,
                            sizeof(temp_buffer) / (out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)),
                            (const uint8_t**)audio_frame->data,
                            audio_frame->nb_samples
                        );

                        if (out_samples <= 0) continue;

                        int data_size = out_samples * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                        if (audio_playing) {
                            SDL_QueueAudio(audio_device, temp_buffer, data_size);
                        }

                        if (audio_frame->pts != AV_NOPTS_VALUE) {
                            AVRational time_base = fmt_ctx->streams[audio_stream_index]->time_base;
                            double pts_time = (double)audio_frame->pts * av_q2d(time_base);
                            if (pts_time >= 0) {
                                current_play_time.store((float)pts_time);
                            }
                        }
                    }
                }
            }
            av_packet_unref(audio_packet);
        } else {
            SDL_Delay(10);
        }
    }
}

int audio_player_init(const char* filepath) {
    printf("Starting Audio Player...\n");
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    }

    printf("Opening file %s\n", filepath);
    if (avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr) != 0) {
        printf("Could not open input: %s\n", filepath);
        return -1;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        printf("Could not find stream info\n");
        return -1;
    }

    audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index < 0) {
        printf("No audio stream found\n");
        audio_enabled = false;
        return 0;
    }

    audio_enabled = true;

    AVCodecParameters* codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        printf("Unsupported codec\n");
        return -1;
    }

    audio_codec_ctx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(audio_codec_ctx, codecpar) < 0) {
        printf("Failed to copy codec parameters\n");
        return -1;
    }

    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        printf("Failed to open codec\n");
        return -1;
    }

    SDL_AudioSpec wanted_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.samples = 4096;
    wanted_spec.callback = nullptr;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &audio_spec, 0);
    if (!audio_device) {
        printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return -1;
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
    swr_init(swr_ctx);

    audio_frame = av_frame_alloc();
    audio_packet = av_packet_alloc();

    SDL_ClearQueuedAudio(audio_device);
    SDL_PauseAudioDevice(audio_device, 0);

    audio_thread_running = true;
    audio_thread = std::thread(audio_decode_loop);

    return 0;
}

void audio_player_cleanup() {
    if (!audio_enabled) return;
    printf("Stopping Audio Player...\n");

    audio_thread_running = false;
    if (audio_thread.joinable()) audio_thread.join();

    SDL_PauseAudioDevice(audio_device, 1);
    SDL_ClearQueuedAudio(audio_device);
    SDL_CloseAudioDevice(audio_device);
    audio_device = 0;

    if (audio_frame) {
        av_frame_free(&audio_frame);
        audio_frame = nullptr;
    }

    if (audio_packet) {
        av_packet_free(&audio_packet);
        audio_packet = nullptr;
    }

    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = nullptr;
    }

    if (audio_codec_ctx) {
        avcodec_free_context(&audio_codec_ctx);
        audio_codec_ctx = nullptr;
    }

    if (fmt_ctx) {
        avformat_close_input(&fmt_ctx);
        fmt_ctx = nullptr;
    }
}

double audio_player_get_current_play_time() {
    if (!audio_enabled) return 0.0;

    float pts = current_play_time.load();

    Uint32 queued_bytes = SDL_GetQueuedAudioSize(audio_device);

    double queued_seconds = (double)queued_bytes / (audio_spec.freq * audio_spec.channels * (SDL_AUDIO_BITSIZE(audio_spec.format) / 8));

    double corrected_time = (double)pts - queued_seconds;

    if (corrected_time < 0.0)
        corrected_time = 0.0;

    return corrected_time;
}

double audio_player_get_total_play_time() {
    if (!fmt_ctx || audio_stream_index < 0) return 0.0;

    int64_t duration = fmt_ctx->streams[audio_stream_index]->duration;

    double total_time = (double)duration * av_q2d(fmt_ctx->streams[audio_stream_index]->time_base);
    return total_time;
}

void audio_player_audio_play(bool state) {
    if (!audio_enabled) return;

    audio_playing = state;

    if (state) SDL_PauseAudioDevice(audio_device, 0);
    else SDL_PauseAudioDevice(audio_device, 1);
}

bool audio_player_get_audio_play_state() {
    return audio_playing;
}

void audio_player_seek(float delta_time) { }
