#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <cstddef>
#include <mutex>

struct AudioBuffer {
    std::vector<uint8_t> buffer;
    size_t write_index;
    size_t read_index;
    size_t buffer_size;
    std::mutex mutex;
};

void audio_buffer_init(AudioBuffer& audio_buffer, size_t buffer_size);
bool audio_buffer_write(AudioBuffer& audio_buffer, const void* data, size_t size);
size_t audio_buffer_read(AudioBuffer& audio_buffer, void* data, size_t size);
size_t audio_buffer_available_space(const AudioBuffer& audio_buffer);
size_t audio_buffer_available_data(const AudioBuffer& audio_buffer);
#endif