#ifndef MEDIA_FILES_H
#define MEDIA_FILES_H

std::vector<std::string> get_media_files();
void set_media_files(const std::vector<std::string>& new_files);
void add_media_file(const std::string& file);
void remove_media_file(size_t index);
void clear_media_files();

#endif