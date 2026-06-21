#include <mutex>
#include "utils/media_info.hpp"

static std::mutex media_info_mutex;
static std::unique_ptr<media_info> current_media_info = std::make_unique<media_info>();

media_info* media_info_get() {
    std::lock_guard<std::mutex> lock(media_info_mutex);

    return current_media_info.get();
}

media_info media_info_get_copy() {
    std::lock_guard<std::mutex> lock(media_info_mutex);

    return *current_media_info;
}

void media_info_set(std::unique_ptr<media_info> new_info) {
    std::lock_guard<std::mutex> lock(media_info_mutex);

    if (!new_info) {
        printf("[Media Info] new_info passed to set is null!\n");
        while (1);
    }

    current_media_info = std::move(new_info);
}
