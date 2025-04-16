#ifndef VIDEO_PLAYER_H
#define VIDEO_PLAYER_H

void audio_callback(void *userdata, Uint8 *stream, int len);
void play_audio_frame(AVFrame* frame, SwrContext* swr_ctx, int out_channels);
SDL_AudioSpec create_audio_spec();
AVCodecContext* create_codec_context(AVFormatContext* fmt_ctx, int stream_index);

#endif