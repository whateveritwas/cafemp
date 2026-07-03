#include "input/input_actions.hpp"

#include "input/devices/vpad.hpp"
#include "input/input_device.hpp"
#include "logger/logger.hpp"

static input_device input_device_current;
static input_device input_device_previous;

void input_init() {
    input_device_current = {};
    input_device_previous = {};

    input_device_vpad_init();

    log_message(LOG_OK, "Input", "Initialised input");
}

void input_poll() {
    input_device_previous = input_device_current;

    input_device_current = {};

    input_device_vpad_poll();

    input_device *vpad = input_device_vpad_get();

    if (vpad && vpad->connected) {
        input_device_current = *vpad;

        if (!vpad->pointer.valid) {
            input_device_current.pointer.valid = false;
        }
    }
}

bool input_pressed(buttons button) { return (input_device_current.buttons & button) && !(input_device_previous.buttons & button); }

bool input_held(buttons button) { return (input_device_current.buttons & button); }

bool input_released(buttons button) { return !(input_device_current.buttons & button) && (input_device_previous.buttons & button); }

input_device *input_get() { return &input_device_current; }

const input_device *input_get_previous() { return &input_device_previous; }

void input_shutdown() {
    log_message(LOG_OK, "Input", "Shutdown input");
    input_device_vpad_shutdown();
}
