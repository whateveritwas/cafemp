#include "audio_player.hpp"
#include <cstdio>
#include <cstring>
#include <algorithm>

static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioSpec audio_spec;
static SDL_mutex* audio_mutex = nullptr;

static uint8_t ring_buffer[RING_BUFFER_SIZE];
static int ring_buffer_write_pos = 0;
static int ring_buffer_read_pos = 0;
static int ring_buffer_fill = 0;

static AVFormatContext* fmt_ctx = nullptr;
static AVCodecContext* audio_codec_ctx = nullptr;
static SwrContext* swr_ctx = nullptr;
static AVFrame* audio_frame = nullptr;
static AVPacket* audio_packet = nullptr;

static int audio_stream_index = -1;
static bool audio_playing = false;

static int out_channels = 2;
static int out_sample_rate = 48000;

static void audio_callback(void* userdata, Uint8* stream, int len) {
    SDL_LockMutex(audio_mutex);

    int bytes_to_copy = std::min(len, ring_buffer_fill);
    int first_chunk = std::min(bytes_to_copy, RING_BUFFER_SIZE - ring_buffer_read_pos);

    SDL_memcpy(stream, ring_buffer + ring_buffer_read_pos, first_chunk);
    SDL_memcpy(stream + first_chunk, ring_buffer, bytes_to_copy - first_chunk);

    ring_buffer_read_pos = (ring_buffer_read_pos + bytes_to_copy) % RING_BUFFER_SIZE;
    ring_buffer_fill -= bytes_to_copy;

    if (bytes_to_copy < len) {
        SDL_memset(stream + bytes_to_copy, 0, len - bytes_to_copy);
    }

    SDL_UnlockMutex(audio_mutex);
}

int audio_player_init(const char* filepath) {
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    }

    audio_mutex = SDL_CreateMutex();

    if (avformat_open_input(&fmt_ctx, filepath, nullptr, nullptr) != 0) {
        fprintf(stderr, "Could not open input: %s\n", filepath);
        return -1;
    }

    avformat_find_stream_info(fmt_ctx, nullptr);

    audio_stream_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (audio_stream_index < 0) {
        fprintf(stderr, "Could not find audio stream\n");
        return -1;
    }

    AVCodecParameters* codecpar = fmt_ctx->streams[audio_stream_index]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec) {
        fprintf(stderr, "Unsupported codec\n");
        return -1;
    }

    audio_codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(audio_codec_ctx, codecpar);
    if (avcodec_open2(audio_codec_ctx, codec, nullptr) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        return -1;
    }

    // Create audio spec
    SDL_AudioSpec wanted_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.samples = 1024;
    wanted_spec.callback = audio_callback;

    audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &audio_spec, 0);
    if (!audio_device) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return -1;
    }

    swr_ctx = swr_alloc_set_opts(nullptr,
        av_get_default_channel_layout(out_channels), AV_SAMPLE_FMT_S16, out_sample_rate,
        audio_codec_ctx->channel_layout ? audio_codec_ctx->channel_layout : av_get_default_channel_layout(audio_codec_ctx->channels),
        audio_codec_ctx->sample_fmt, audio_codec_ctx->sample_rate, 0, nullptr);
    swr_init(swr_ctx);

    audio_frame = av_frame_alloc();
    audio_packet = av_packet_alloc();

    ring_buffer_fill = 0;
    ring_buffer_read_pos = 0;
    ring_buffer_write_pos = 0;

    SDL_PauseAudioDevice(audio_device, 0);
    audio_playing = true;

    return 0;
}

void audio_player_update() {
    if (!audio_playing || !fmt_ctx) return;

    while (av_read_frame(fmt_ctx, audio_packet) >= 0) {
        if (audio_packet->stream_index == audio_stream_index) {
            if (avcodec_send_packet(audio_codec_ctx, audio_packet) == 0) {
                while (avcodec_receive_frame(audio_codec_ctx, audio_frame) == 0) {
                    uint8_t temp_buffer[8192];
                    uint8_t* out_buffers[] = { temp_buffer };

                    int out_samples = swr_convert(swr_ctx, out_buffers,
                        sizeof(temp_buffer) / (out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)),
                        (const uint8_t**)audio_frame->data, audio_frame->nb_samples);

                    if (out_samples <= 0) continue;

                    int data_size = out_samples * out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

                    SDL_LockMutex(audio_mutex);

                    if (data_size <= RING_BUFFER_SIZE - ring_buffer_fill) {
                        int first_chunk = std::min(data_size, RING_BUFFER_SIZE - ring_buffer_write_pos);
                        memcpy(ring_buffer + ring_buffer_write_pos, temp_buffer, first_chunk);
                        memcpy(ring_buffer, temp_buffer + first_chunk, data_size - first_chunk);

                        ring_buffer_write_pos = (ring_buffer_write_pos + data_size) % RING_BUFFER_SIZE;
                        ring_buffer_fill += data_size;
                    }

                    SDL_UnlockMutex(audio_mutex);
                }
            }
        }

        av_packet_unref(audio_packet);
        break; // Limit to one frame per update
    }
}

void audio_player_cleanup() {
    if (!audio_playing) return;

    audio_playing = false;

    SDL_PauseAudioDevice(audio_device, 1);
    SDL_CloseAudioDevice(audio_device);
    audio_device = 0;

    if (audio_mutex) {
        SDL_DestroyMutex(audio_mutex);
        audio_mutex = nullptr;
    }

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

    ring_buffer_fill = 0;
    ring_buffer_write_pos = 0;
    ring_buffer_read_pos = 0;
}