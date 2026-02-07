#ifndef MEDIA_PLAYER_HPP
#define MEDIA_PLAYER_HPP

#include <vector>
#include <string>

struct AudioTrackInfo {
    int stream_index;
    const char* codec_name;
    int channels;
    int sample_rate;
    const char* language;
};

struct frame_info {
    int width;
    int height;
    SDL_Texture* texture;
};

int media_player_init(const char* filepath);
void media_player_cleanup();
void media_player_play(bool play);
void media_player_seek(double seconds);
bool media_player_is_playing();
void media_player_update();
std::vector<AudioTrackInfo> media_player_get_audio_tracks();
bool media_player_switch_audio_track(int new_stream_index);
int media_player_get_current_audio_track();
double media_player_get_current_time();
double media_player_get_total_time();

#endif // MEDIA_PLAYER_HPP
