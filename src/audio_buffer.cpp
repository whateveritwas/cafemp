#include <vector>
#include <mutex>

#include "audio_buffer.hpp"

void audio_buffer_init(AudioBuffer& audio_buffer, size_t buffer_size) {
    audio_buffer.buffer.resize(buffer_size);
    audio_buffer.write_index = 0;
    audio_buffer.read_index = 0;
    audio_buffer.buffer_size = buffer_size;
}

bool audio_buffer_write(AudioBuffer& audio_buffer, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(audio_buffer.mutex);

    size_t available_space = audio_buffer.buffer_size - audio_buffer_available_data(audio_buffer);
    if (available_space < size) {
        return false;
    }

    const uint8_t* bytes = (const uint8_t*)data;
    for (size_t i = 0; i < size; i++) {
        audio_buffer.buffer[audio_buffer.write_index] = bytes[i];
        audio_buffer.write_index = (audio_buffer.write_index + 1) % audio_buffer.buffer_size;
    }
    return true;
}

size_t audio_buffer_read(AudioBuffer& audio_buffer, void* data, size_t size) {
    std::lock_guard<std::mutex> lock(audio_buffer.mutex);
    size_t bytes_read = 0;

    while (bytes_read < size && audio_buffer_available_data(audio_buffer) > 0) {
        ((uint8_t*)data)[bytes_read] = audio_buffer.buffer[audio_buffer.read_index];
        audio_buffer.read_index = (audio_buffer.read_index + 1) % audio_buffer.buffer_size;
        bytes_read++;
    }
    return bytes_read;
}

size_t audio_buffer_available_space(const AudioBuffer& audio_buffer) {
    if (audio_buffer.write_index >= audio_buffer.read_index) {
        return audio_buffer.buffer_size - (audio_buffer.write_index - audio_buffer.read_index);
    } else {
        return audio_buffer.read_index - audio_buffer.write_index;
    }
}

size_t audio_buffer_available_data(const AudioBuffer& audio_buffer) {
    if (audio_buffer.write_index >= audio_buffer.read_index) {
        return audio_buffer.write_index - audio_buffer.read_index;
    } else {
        return audio_buffer.buffer_size - (audio_buffer.read_index - audio_buffer.write_index);
    }
}