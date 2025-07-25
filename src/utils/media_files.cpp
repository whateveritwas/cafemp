#include <vector>
#include <string>
#include <mutex>

static std::vector<std::string> media_files;
static std::mutex media_files_mutex;

std::vector<std::string> get_media_files() {
    std::lock_guard<std::mutex> lock(media_files_mutex);
    return media_files;
}

void set_media_files(const std::vector<std::string>& new_files) {
    std::lock_guard<std::mutex> lock(media_files_mutex);
    media_files = new_files;
}

void add_media_file(const std::string& file) {
    std::lock_guard<std::mutex> lock(media_files_mutex);
    media_files.push_back(file);
}

void remove_media_file(size_t index) {
    std::lock_guard<std::mutex> lock(media_files_mutex);
    if (index < media_files.size()) {
        media_files.erase(media_files.begin() + index);
    }
}

void clear_media_files() {
    std::lock_guard<std::mutex> lock(media_files_mutex);
    media_files.clear();
}