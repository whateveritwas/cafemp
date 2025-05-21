

#ifndef MADIA_INFO_H
#define MADIA_INFO_H

#include <string>
#include <memory>

struct media_info {
    std::string path = "";
    char type = 'U';
    double framerate = 30.0;
    int64_t current_video_playback_time = 0;
    int64_t current_audio_playback_time = 0;
    int64_t total_video_playback_time = 0;
    int64_t total_audio_playback_time = 0;
    int current_audio_track_id = 1;
    int total_audio_track_count = 1;
    int current_caption_id = 0;
    int total_caption_count = 0;
    bool playback_status = false;
};

media_info* media_info_get();
media_info media_info_get_copy();
void media_info_set(std::unique_ptr<media_info> new_info);
#endif