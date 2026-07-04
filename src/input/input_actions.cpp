#include "input/input_actions.hpp"

#include "input/devices/kpad.hpp"
#include "input/devices/vpad.hpp"
#include "input/devices/wpad.hpp"
#include "input/input_device.hpp"
#include "logger/logger.hpp"

#include <cstdio>

struct input_backend {
    const char *name;
    void (*init)();
    void (*poll)();
    input_device *(*get)();
    void (*shutdown)();
};

enum input_backend_id {
    INPUT_BACKEND_KPAD = 0,
    INPUT_BACKEND_WPAD,
    INPUT_BACKEND_VPAD,

    INPUT_BACKEND_COUNT,
    INPUT_BACKEND_NONE = INPUT_BACKEND_COUNT
};

static const input_backend input_backends[INPUT_BACKEND_COUNT] = {
    {"Kpad", input_device_kpad_init, input_device_kpad_poll, input_device_kpad_get, input_device_kpad_shutdown},
    {"Wpad", input_device_wpad_init, input_device_wpad_poll, input_device_wpad_get, input_device_wpad_shutdown},
    {"Vpad", input_device_vpad_init, input_device_vpad_poll, input_device_vpad_get, input_device_vpad_shutdown},
};

static input_device input_device_current;
static input_device input_device_previous;

static input_backend_id input_current_source = INPUT_BACKEND_NONE;
static bool input_backend_was_active[INPUT_BACKEND_COUNT] = {};

#ifdef DEBUG
static bool input_debug_enabled = false;
static input_backend_id input_debug_last_source = INPUT_BACKEND_NONE;
static bool input_debug_last_pointer_valid = false;

void input_debug_set_enabled(bool enabled) { input_debug_enabled = enabled; }

bool input_debug_is_enabled() { return input_debug_enabled; }

struct input_debug_button_name {
    buttons button;
    const char *name;
};

static const input_debug_button_name input_debug_button_names[] = {
    {BUTTON_A, "A"}, {BUTTON_B, "B"}, {BUTTON_X, "X"}, {BUTTON_Y, "Y"}, {BUTTON_PLUS, "PLUS"}, {BUTTON_MINUS, "MINUS"}, {BUTTON_UP, "UP"}, {BUTTON_DOWN, "DOWN"}, {BUTTON_LEFT, "LEFT"}, {BUTTON_RIGHT, "RIGHT"}, {BUTTON_L, "L"}, {BUTTON_ZL, "ZL"}, {BUTTON_R, "R"}, {BUTTON_ZR, "ZR"}, {BUTTON_LSTICK_LEFT, "LSTICK_LEFT"}, {BUTTON_LSTICK_RIGHT, "LSTICK_RIGHT"}, {BUTTON_LSTICK_UP, "LSTICK_UP"}, {BUTTON_LSTICK_DOWN, "LSTICK_DOWN"}, {BUTTON_RSTICK_LEFT, "RSTICK_LEFT"}, {BUTTON_RSTICK_RIGHT, "RSTICK_RIGHT"}, {BUTTON_RSTICK_UP, "RSTICK_UP"}, {BUTTON_RSTICK_DOWN, "RSTICK_DOWN"}, {BUTTON_TOUCH, "TOUCH"},
};

static void input_debug() {
    char buffer[96];

    if (input_current_source != input_debug_last_source) {
        const char *from = (input_debug_last_source == INPUT_BACKEND_NONE) ? "None" : input_backends[input_debug_last_source].name;
        const char *to = (input_current_source == INPUT_BACKEND_NONE) ? "None" : input_backends[input_current_source].name;

        std::snprintf(buffer, sizeof(buffer), "Source: %s -> %s", from, to);
        log_message(LOG_DEBUG, "Input", buffer);

        input_debug_last_source = input_current_source;
    }

    for (const input_debug_button_name &entry : input_debug_button_names) {
        if (input_pressed(entry.button)) {
            std::snprintf(buffer, sizeof(buffer), "Pressed: %s", entry.name);
            log_message(LOG_DEBUG, "Input", buffer);
        } else if (input_released(entry.button)) {
            std::snprintf(buffer, sizeof(buffer), "Released: %s", entry.name);
            log_message(LOG_DEBUG, "Input", buffer);
        }
    }

    bool pointer_valid_now = input_device_current.pointer.valid;
    if (pointer_valid_now != input_debug_last_pointer_valid) {
        std::snprintf(buffer, sizeof(buffer), "Pointer: %s", pointer_valid_now ? "valid" : "invalid");
        log_message(LOG_DEBUG, "Input", buffer);

        input_debug_last_pointer_valid = pointer_valid_now;
    }
}
#endif

void input_init() {
    input_device_current = {};
    input_device_previous = {};

    input_current_source = INPUT_BACKEND_NONE;
    for (bool &was_active : input_backend_was_active)
        was_active = false;

#ifdef DEBUG
    input_debug_last_source = INPUT_BACKEND_NONE;
    input_debug_last_pointer_valid = false;
#endif

    for (const input_backend &backend : input_backends)
        backend.init();

    log_message(LOG_OK, "Input", "Initialised input");
}

void input_poll() {
    input_device_previous = input_device_current;
    input_device_current = {};

    for (const input_backend &backend : input_backends)
        backend.poll();

    if (input_current_source != INPUT_BACKEND_NONE) {
        input_device *current = input_backends[input_current_source].get();
        if (!current || !current->connected || !current->active) {
            input_current_source = INPUT_BACKEND_NONE;
        }
    }

    for (int i = 0; i < INPUT_BACKEND_COUNT; ++i) {
        input_device *device = input_backends[i].get();
        bool is_active_now = device && device->connected && device->active;
        bool became_active = is_active_now && !input_backend_was_active[i];

        if (became_active && i != input_current_source) {
            input_current_source = static_cast<input_backend_id>(i);
            break;
        }
    }

    if (input_current_source == INPUT_BACKEND_NONE) {
        for (int i = 0; i < INPUT_BACKEND_COUNT; ++i) {
            input_device *device = input_backends[i].get();
            if (device && device->connected && device->active) {
                input_current_source = static_cast<input_backend_id>(i);
                break;
            }
        }
    }

    if (input_current_source != INPUT_BACKEND_NONE) {
        input_device *device = input_backends[input_current_source].get();
        if (device) input_device_current = *device;
    }

    for (int i = 0; i < INPUT_BACKEND_COUNT; ++i) {
        input_device *device = input_backends[i].get();
        input_backend_was_active[i] = device && device->connected && device->active;
    }

#ifdef DEBUG
    input_debug();
#endif
}

bool input_pressed(buttons button) { return (input_device_current.buttons & button) && !(input_device_previous.buttons & button); }

bool input_held(buttons button) { return (input_device_current.buttons & button); }

bool input_released(buttons button) { return !(input_device_current.buttons & button) && (input_device_previous.buttons & button); }

input_device *input_get() { return &input_device_current; }

const input_device *input_get_previous() { return &input_device_previous; }

void input_shutdown() {
    for (int i = INPUT_BACKEND_COUNT - 1; i >= 0; --i)
        input_backends[i].shutdown();
    log_message(LOG_OK, "Input", "Shutdown input");
}
