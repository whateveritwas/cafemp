#include <mutex>
#include "media_info.hpp"

static media_info current_media_info;
static std::mutex media_info_mutex;

media_info media_info_get() {
    std::lock_guard<std::mutex> lock(media_info_mutex);
    return current_media_info;
}

void media_info_set(const media_info& new_media_info) {
    std::lock_guard<std::mutex> lock(media_info_mutex);
    current_media_info = new_media_info;
}