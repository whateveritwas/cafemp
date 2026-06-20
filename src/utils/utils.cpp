#include <string>

#include "utils/utils.hpp"

std::string format_time(int seconds) {
    int mins = seconds / 60;
    int secs = seconds % 60;
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%02d:%02d", mins, secs);
    return std::string(buffer);
}
