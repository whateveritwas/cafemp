#include "audio_player.hpp"
#include <cstdio>
#include <cstring>

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_BUFFER_SIZE 192000

void audio_callback(void* userdata, Uint8* stream, int len) {
    AudioPlayer* player = (AudioPlayer*)userdata;

    if (player->audio_buf_index >= player->audio_buf_size) {
        audio_player_decode_audio_frame(player);
        if (player->audio_buf_index >= player->audio_buf_size) {
            // Still no data? Fill stream with silence.
            SDL_memset(stream, 0, len);
            return;
        }
    }    

    // Fill the audio stream with data
    int bytes_to_copy = std::min(len, player->audio_buf_size - player->audio_buf_index);
    SDL_memcpy(stream, player->audio_buf + player->audio_buf_index, bytes_to_copy);

    player->audio_buf_index += bytes_to_copy;
}

int audio_player_init(AudioPlayer* player, const char* filepath) {
    printf("Starting Audio Player...\n");

    // Audio initialization
    if (avformat_open_input(&player->fmt_ctx, filepath, nullptr, nullptr) < 0) {
        printf("Failed to open audio file: %s\n", filepath);
        return -1;
    }

    if (avformat_find_stream_info(player->fmt_ctx, nullptr) < 0) {
        printf("Failed to find stream information\n");
        return -1;
    }

    player->audio_stream_index = av_find_best_stream(player->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (player->audio_stream_index < 0) {
        printf("Failed to find audio stream\n");
        return -1;
    }

    AVStream* stream = player->fmt_ctx->streams[player->audio_stream_index];
    const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
    player->audio_codec_ctx = avcodec_alloc_context3(codec);
    if (!player->audio_codec_ctx) {
        printf("Failed to allocate codec context\n");
        return -1;
    }

    if (avcodec_parameters_to_context(player->audio_codec_ctx, stream->codecpar) < 0) {
        printf("Failed to copy codec parameters\n");
        return -1;
    }

    if (avcodec_open2(player->audio_codec_ctx, codec, nullptr) < 0) {
        printf("Failed to open audio codec\n");
        return -1;
    }

    player->swr_ctx = nullptr;
    AVChannelLayout out_ch_layout;
    av_channel_layout_default(&out_ch_layout, 2); // stereo output

    swr_alloc_set_opts2(
        &player->swr_ctx,
        &out_ch_layout,
        AV_SAMPLE_FMT_S16,
        AUDIO_SAMPLE_RATE,
        &player->audio_codec_ctx->ch_layout,
        player->audio_codec_ctx->sample_fmt,
        AUDIO_SAMPLE_RATE,
        0,
        nullptr
    );

    if (swr_init(player->swr_ctx) < 0) {
        printf("Failed to initialize resampler\n");
        return -1;
    }

    av_channel_layout_uninit(&out_ch_layout);

    player->audio_frame = av_frame_alloc();
    player->audio_packet = av_packet_alloc();
    player->audio_buf = (uint8_t*)av_malloc(AUDIO_BUFFER_SIZE);
    if (!player->audio_buf) {
        printf("Failed to allocate audio buffer\n");
        return -1;
    }

    player->audio_buf_size = 0;
    player->audio_buf_index = 0;
    player->playing = false;

    SDL_AudioSpec spec;
    spec.freq = AUDIO_SAMPLE_RATE;
    spec.format = AUDIO_S16SYS;
    spec.channels = 2;
    spec.samples = 1024;
    spec.callback = audio_callback;
    spec.userdata = player;

    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (dev == 0) {
        printf("Failed to open audio device: %s\n", SDL_GetError());
        return -1;
    }

    SDL_PauseAudio(0);
    SDL_PauseAudioDevice(dev, 0);

    printf("Audio player initialized successfully.\n");
    return 0;
}

void audio_player_play(AudioPlayer* player, bool play) {
    player->playing = play;
    SDL_PauseAudioDevice(player->device_id, play ? 0 : 1);
}

void audio_player_decode_audio_frame(AudioPlayer* player) {
    int ret;
    while ((ret = av_read_frame(player->fmt_ctx, player->audio_packet)) >= 0) {
        if (player->audio_packet->stream_index != player->audio_stream_index) {
            av_packet_unref(player->audio_packet);
            continue;
        }

        ret = avcodec_send_packet(player->audio_codec_ctx, player->audio_packet);
        if (ret < 0) {
            av_packet_unref(player->audio_packet);
            continue;
        }

        ret = avcodec_receive_frame(player->audio_codec_ctx, player->audio_frame);
        if (ret < 0) {
            av_packet_unref(player->audio_packet);
            continue;
        }

        av_rescale_rnd(
            swr_get_delay(player->swr_ctx, AUDIO_SAMPLE_RATE) + player->audio_frame->nb_samples,
            AUDIO_SAMPLE_RATE,
            AUDIO_SAMPLE_RATE,
            AV_ROUND_UP
        );

        player->audio_buf_size = swr_convert(
            player->swr_ctx,
            &player->audio_buf,
            AUDIO_BUFFER_SIZE / 2,
            (const uint8_t**)player->audio_frame->data,
            player->audio_frame->nb_samples
        ) * 2 * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

        player->audio_buf_index = 0;
        av_packet_unref(player->audio_packet);
        break;
    }
}

void audio_player_cleanup(AudioPlayer* player) {
    printf("Stopping Audio Player...\n");

    SDL_CloseAudio();

    if (player->audio_buf) av_free(player->audio_buf);
    if (player->audio_frame) av_frame_free(&player->audio_frame);
    if (player->audio_packet) av_packet_free(&player->audio_packet);
    if (player->swr_ctx) swr_free(&player->swr_ctx);
    if (player->audio_codec_ctx) avcodec_free_context(&player->audio_codec_ctx);
    if (player->fmt_ctx) avformat_close_input(&player->fmt_ctx);
}
