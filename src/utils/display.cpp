#include <gx2/display.h>
#include <mutex>
#include <algorithm>

#include "logger/logger.hpp"
#include "display.hpp"

static scan_mode current_scan_mode{};
static std::mutex current_scan_mode_mutex;

scan_mode display_get() {
    std::lock_guard<std::mutex> lock(current_scan_mode_mutex);
    return current_scan_mode;
}

rect display_calculate_aspect_fit(int width, int height) {
    scan_mode mode = display_get();

    float scale_x = static_cast<float>(mode.width) / width;
    float scale_y = static_cast<float>(mode.height) / height;
    float scale = std::min(scale_x, scale_y);

    rect r;

    r.w = width * scale;
    r.h = height * scale;

    r.x = (mode.width - r.w) * 0.5f;
    r.y = (mode.height - r.h) * 0.5f;

    return r;
}

void display_setup() {
    std::lock_guard<std::mutex> lock(current_scan_mode_mutex);

    GX2TVScanMode scanMode = GX2GetSystemTVScanMode();

    switch (scanMode) {
        case GX2_TV_SCAN_MODE_NONE:
            current_scan_mode.width = 0;
            current_scan_mode.height = 0;
            break;

        case GX2_TV_SCAN_MODE_576I:
            current_scan_mode.width = 720;
            current_scan_mode.height = 576;
            break;

        case GX2_TV_SCAN_MODE_480I:
        case GX2_TV_SCAN_MODE_480P:
            current_scan_mode.width = 854;
            current_scan_mode.height = 480;
            break;

        case GX2_TV_SCAN_MODE_720P:
            current_scan_mode.width = 1280;
            current_scan_mode.height = 720;
            break;

        case GX2_TV_SCAN_MODE_1080I:
        case GX2_TV_SCAN_MODE_1080P:
            current_scan_mode.width = 1920;
            current_scan_mode.height = 1080;
            break;

        default:
            current_scan_mode.width = 1280;
            current_scan_mode.height = 720;
            break;
    }

    current_scan_mode.scale = static_cast<float>(current_scan_mode.height) / 720.0f;

    log_message(LOG_DEBUG, "Display", "Found scan mode %ix%i with scale %.2f", current_scan_mode.width, current_scan_mode.height, current_scan_mode.scale);
}
