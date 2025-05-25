#include <string>
#include <vector>
#include <dirent.h>
#include <unordered_set>
#include <SDL2/SDL_image.h>

#include "utils.hpp"
#include "app_state.hpp"
#include "media_files.hpp"
#include "audio_player.hpp"
#include "video_player.hpp"

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}

std::string truncate_filename(const std::string& name, size_t max_length) {
    if (name.length() <= max_length) {
        return name;
    }
    return name.substr(0, max_length - 3) + "...";
}

bool valid_file_ending(const std::string& file_ending) {
    return valid_video_endings.count(file_ending) > 0 || valid_audio_endings.count(file_ending) > 0;
}

void scan_directory(const char* path) {
    clear_media_files();
    printf("[Menu] Opening folder %s\n", path);
    DIR* dir = opendir(path);
    if (!dir) return;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name(ent->d_name);
        if (name.length() > 4) {
            std::string ext = name.substr(name.find_last_of(".") + 1);
            for (auto& c : ext) c = std::tolower(c);
            if (valid_file_ending(ext)) {
                add_media_file(name);
            }
        }
    }    
    closedir(dir);
}