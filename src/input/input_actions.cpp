#include "input/input_actions.hpp"

#include "input/devices/kpad.hpp"
#include "input/devices/vpad.hpp"
#include "input/input_device.hpp"
#include "logger/logger.hpp"

static input_device input_device_current;
static input_device input_device_previous;

void input_init() {
    input_device_current = {};
    input_device_previous = {};

    input_device_vpad_init();
    input_device_kpad_init();

    log_message(LOG_OK, "Input", "Initialised input");
}

void input_poll() {
    input_device_previous = input_device_current;

    input_device_current = {};

    input_device_vpad_poll();
    input_device_kpad_poll();

    input_device *vpad = input_device_vpad_get();

    if (vpad && vpad->connected) {
        input_device_current = *vpad;
    }

    input_device *kpad = input_device_kpad_get();

    if (kpad && kpad->connected) {
        input_device_current = *kpad;
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
