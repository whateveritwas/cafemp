#ifndef UI_FILEBROWSER_H
#define UI_FILEBROWSER_H

#include <string>
#include <unordered_set>

static const std::unordered_set<std::string> valid_video_endings = {"mp4", "mov", "avi", "mkv"};
static const std::unordered_set<std::string> valid_audio_endings = {"mp3", "wav", "ogg", "flac", "aac"};
static const std::unordered_set<std::string> valid_image_endings = {"png", "jpg", "gif", "tga", "bmp"};
static const std::unordered_set<std::string> valid_pdf_ending = {"pdf", "epub", "cbz"};

void scene_file_browser_scan_directory(const char* path);

void scene_file_browser_render();

#endif
