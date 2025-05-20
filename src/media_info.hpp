

#ifndef MADIA_INFO_H
#define MADIA_INFO_H

#include <string>

struct media_info {
    std::string path = "";
    char type = 0;
    double framerate = 30.0;
    int64_t current_playback_time = 0;
    int64_t total_playback_time = 0;
    int current_audio_track_id = 1;
    int total_audio_track_count = 1;
    int current_caption_id = 0;
    int total_caption_count = 0;
    bool playback_status = false;
};

media_info media_info_get();
void media_info_set(media_info& new_media_info);

#endif