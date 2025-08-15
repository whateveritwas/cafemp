#include <mutex>
#include "utils/media_info.hpp"

static std::mutex media_info_mutex;
static std::unique_ptr<media_info> current_media_info = std::make_unique<media_info>();

media_info* media_info_get() {
    std::lock_guard<std::mutex> lock(media_info_mutex);

    if (!current_media_info) {
        printf("[Media Info] current_media_info is null!\n");
        while (1);  // Crash intentionally for debugging
    }

    #if defined(DEBUG_AUDIO) && defined(DEBUG_VIDEO)
    printf("=== Media Info Debug ===\n");
    printf("Path: %s\n", current_media_info->path.c_str());
    printf("Type: %c\n", current_media_info->type);
    printf("Framerate: %.2f fps\n", current_media_info->framerate);
    printf("Current Video Playback Time: %lld s\n", static_cast<long long>(current_media_info->current_video_playback_time));
    printf("Current Audio Playback Time: %lld s\n", static_cast<long long>(current_media_info->current_audio_playback_time));
    printf("Total Video Playback Time: %lld s\n", static_cast<long long>(current_media_info->total_video_playback_time));
    printf("Total Audio Playback Time: %lld s\n", static_cast<long long>(current_media_info->total_audio_playback_time));
    printf("Current Audio Track ID: %d\n", current_media_info->current_audio_track_id);
    printf("Total Audio Track Count: %d\n", current_media_info->total_audio_track_count);
    printf("Current Caption ID: %d\n", current_media_info->current_caption_id);
    printf("Total Caption Count: %d\n", current_media_info->total_caption_count);
    printf("Playback Status: %s\n", current_media_info->playback_status ? "Playing" : "Stopped");
    printf("========================\n");
    #endif

    return current_media_info.get();
}

media_info media_info_get_copy() {
    std::lock_guard<std::mutex> lock(media_info_mutex);

    if (!current_media_info) {
        printf("[Media Info] current_media_info is null!\n");
        while (1);
    }

    #if defined(DEBUG_AUDIO) && defined(DEBUG_VIDEO)
    printf("=== Media Info Debug ===\n");
    printf("Path: %s\n", current_media_info->path.c_str());
    printf("Type: %c\n", current_media_info->type);
    printf("Framerate: %.2f fps\n", current_media_info->framerate);
    printf("Current Video Playback Time: %lld s\n", static_cast<long long>(current_media_info->current_video_playback_time));
    printf("Current Audio Playback Time: %lld s\n", static_cast<long long>(current_media_info->current_audio_playback_time));
    printf("Total Video Playback Time: %lld s\n", static_cast<long long>(current_media_info->total_video_playback_time));
    printf("Total Audio Playback Time: %lld s\n", static_cast<long long>(current_media_info->total_audio_playback_time));
    printf("Current Audio Track ID: %d\n", current_media_info->current_audio_track_id);
    printf("Total Audio Track Count: %d\n", current_media_info->total_audio_track_count);
    printf("Current Caption ID: %d\n", current_media_info->current_caption_id);
    printf("Total Caption Count: %d\n", current_media_info->total_caption_count);
    printf("Playback Status: %s\n", current_media_info->playback_status ? "Playing" : "Stopped");
    printf("========================\n");
    #endif

    return *current_media_info; // Return a copy
}

void media_info_set(std::unique_ptr<media_info> new_info) {
    std::lock_guard<std::mutex> lock(media_info_mutex);

    if (!new_info) {
        printf("[Media Info] new_info passed to set is null!\n");
        while (1);
    }

    current_media_info = std::move(new_info);
}
