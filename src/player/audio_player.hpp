#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include "main.hpp"
#include <string>
#include <vector>
#include <SDL2/SDL.h>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/frame.h>
	#include <libavutil/time.h>
}

struct AudioTrackInfo {
    int streamIndex;
    std::string codecName;
    int channels;
    int sampleRate;
    std::string language;
};

void collect_audio_tracks(AVFormatContext* in_fmt_ctx);
std::vector<AudioTrackInfo> get_audio_tracks();
int audio_player_init(const char* filepath);
bool audio_player_switch_audio_stream(int new_stream_index);
void audio_player_play(bool state);
bool audio_player_get_audio_play_state();
void audio_player_seek(float delta_time);
double audio_player_get_current_play_time();
double audio_player_get_total_play_time();
void audio_player_cleanup();
int audio_player_get_current_track_id();

#endif
