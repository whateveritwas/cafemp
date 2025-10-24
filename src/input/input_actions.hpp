#ifndef INPUT_ACTIONS_H
#define INPUT_ACTIONS_H

enum InputButton {
    BTN_A,
    BTN_B,
    BTN_X,
    BTN_Y,
    BTN_PLUS,
    BTN_MINUS,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_ZL,
    BTN_ZR,
    BTN_LSTICK_LEFT,
    BTN_LSTICK_RIGHT,
    BTN_LSTICK_UP,
    BTN_LSTICK_DOWN,
    BTN_RSTICK_LEFT,
    BTN_RSTICK_RIGHT,
    BTN_RSTICK_UP,
    BTN_RSTICK_DOWN,
    BTN_UNKNOWN
};

struct StickState {
    float x = 0.0f;
    float y = 0.0f;
};

struct TouchState {
    bool touched = false;
    float x = 0.0f;
    float y = 0.0f;
    float old_x = 0.0f;
    float old_y = 0.0f;
};

struct InputState {
    uint64_t pressed = 0;
    uint64_t held = 0;
    StickState left_stick;
    StickState right_stick;
    TouchState touch;
    bool using_pro_controller = false;
};

#endif
