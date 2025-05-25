#include <mutex>
#include "app_state.hpp"

static AppState app_state = STATE_MENU;
static std::mutex app_state_mutex;

AppState app_state_get() {
    std::lock_guard<std::mutex> lock(app_state_mutex);
    return app_state;
}

void app_state_set(AppState new_app_state) {
    std::lock_guard<std::mutex> lock(app_state_mutex);
    app_state = new_app_state;
}
