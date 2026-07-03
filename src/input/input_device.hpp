#ifndef INPUT_DEVICE_HPP
#define INPUT_DEVICE_HPP

#include <cstdint>
enum buttons : uint32_t {
    BUTTON_A = 1u << 0,
    BUTTON_B = 1u << 1,
    BUTTON_X = 1u << 2,
    BUTTON_Y = 1u << 3,

    BUTTON_PLUS = 1u << 4,
    BUTTON_MINUS = 1u << 5,

    BUTTON_UP = 1u << 6,
    BUTTON_DOWN = 1u << 7,
    BUTTON_LEFT = 1u << 8,
    BUTTON_RIGHT = 1u << 9,

    BUTTON_L = 1u << 10,
    BUTTON_ZL = 1u << 11,
    BUTTON_R = 1u << 12,
    BUTTON_ZR = 1u << 13,

    BUTTON_LSTICK_LEFT = 1u << 14,
    BUTTON_LSTICK_RIGHT = 1u << 15,
    BUTTON_LSTICK_UP = 1u << 16,
    BUTTON_LSTICK_DOWN = 1u << 17,

    BUTTON_RSTICK_LEFT = 1u << 18,
    BUTTON_RSTICK_RIGHT = 1u << 19,
    BUTTON_RSTICK_UP = 1u << 20,
    BUTTON_RSTICK_DOWN = 1u << 21,

    BUTTON_TOUCH = 1u << 22
};

typedef struct input_stick {
    float x = 0.0f;
    float y = 0.0f;
} input_stick;

typedef struct input_pointer {
    bool valid = false;

    float x = 0.0f;
    float y = 0.0f;

    float delta_x = 0.0f;
    float delta_y = 0.0f;
} input_pointer;

typedef struct input_device {
    uint32_t buttons = 0;

    input_stick left = {};
    input_stick right = {};

    input_pointer pointer = {};

    bool connected = false;
} input_device;

#endif
