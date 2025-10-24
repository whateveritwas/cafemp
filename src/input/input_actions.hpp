#ifndef INPUT_ACTIONS_H
#define INPUT_ACTIONS_H

#include <cstdint>

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
	BTN_L,
    BTN_ZL,
	BTN_R,
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
	float move_x = 0.0f;
	float move_y = 0.0f;
};

struct InputState {
    uint64_t pressed = 0;
    uint64_t held = 0;
    StickState left_stick;
    StickState right_stick;
    TouchState touch;
    bool using_pro_controller = false;
};

void input_poll(InputState& state);

static inline void set_button(InputState& s, InputButton btn) {
    s.pressed |= (1ull << btn);
}

static inline void set_hold(InputState& s, InputButton btn) {
    s.held |= (1ull << btn);
}

inline bool input_pressed(const InputState& s, InputButton btn) {
    return s.pressed & (1ull << btn);
}

inline bool input_held(const InputState& s, InputButton btn) {
    return s.held & (1ull << btn);
}

inline bool input_touched(const InputState& s) {
    return s.touch.touched;
}

#endif
