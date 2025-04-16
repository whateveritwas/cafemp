#include <cstdint>
#include "config.hpp"

void audio_callback(void *userdata, Uint8 *stream, int len) {
    SDL_LockMutex(audio_mutex);

    int bytes_to_copy = (len > ring_buffer_fill) ? ring_buffer_fill : len;

    int first_chunk = RING_BUFFER_SIZE - ring_buffer_read_pos;
    if (first_chunk > bytes_to_copy) first_chunk = bytes_to_copy;

    SDL_memcpy(stream, ring_buffer + ring_buffer_read_pos, first_chunk);
    SDL_memcpy(stream + first_chunk, ring_buffer, bytes_to_copy - first_chunk);

    ring_buffer_read_pos = (ring_buffer_read_pos + bytes_to_copy) % RING_BUFFER_SIZE;
    ring_buffer_fill -= bytes_to_copy;

    if (bytes_to_copy < len) {
        SDL_memset(stream + bytes_to_copy, 0, len - bytes_to_copy);
    }

    SDL_UnlockMutex(audio_mutex);
}

void play_audio_frame(AVFrame* frame, SwrContext* swr_ctx, int out_channels) {
    uint8_t temp_buffer[8192];
    uint8_t* out_buffers[1] = { temp_buffer };

    int out_samples = swr_convert(
        swr_ctx,
        out_buffers,
        sizeof(temp_buffer) / (out_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16)),
        (const uint8_t**)frame->data,
        frame->nb_samples
    );

    if (out_samples < 0) {
        fprintf(stderr, "Error while converting audio.\n");
        return;
    }

    int bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    int data_size = out_samples * out_channels * bytes_per_sample;

    SDL_LockMutex(audio_mutex);

    if (data_size <= RING_BUFFER_SIZE - ring_buffer_fill) {
        int first_chunk = RING_BUFFER_SIZE - ring_buffer_write_pos;
        if (first_chunk > data_size) first_chunk = data_size;

        memcpy(ring_buffer + ring_buffer_write_pos, temp_buffer, first_chunk);
        memcpy(ring_buffer, temp_buffer + first_chunk, data_size - first_chunk);

        ring_buffer_write_pos = (ring_buffer_write_pos + data_size) % RING_BUFFER_SIZE;
        ring_buffer_fill += data_size;
    }

    SDL_UnlockMutex(audio_mutex);
}

SDL_AudioSpec create_audio_spec() {
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = audio_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = 2;
    wanted_spec.samples = 1024;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = NULL;
    return wanted_spec;
}

AVCodecContext* create_codec_context(AVFormatContext* fmt_ctx, int stream_index) {
    AVCodecParameters* codecpar = fmt_ctx->streams[stream_index]->codecpar;
    AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codecpar);
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        printf("Failed to open codec.\n");
        return NULL;
    }
    return codec_ctx;
}