#include <cstdio>
#include <cmath>

#include <vpad/input.h>
#include <padscore/wpad.h>
#include <padscore/kpad.h>

#include "logger/logger.hpp"
#include "main.hpp"

#include "input/input_actions.hpp"

static bool s_use_wpad = false;
static bool s_use_kpad = false;
static bool wpad_init = false;
static bool kpad_init = false;

void input_poll(InputState& state) {
#ifdef DEBUG
    static uint64_t last_buttons = 0;
    static bool last_touch = false;
    static float last_touch_x = 0.0f, last_touch_y = 0.0f;
#endif

    if (!wpad_init) {
        WPADInit();
        WPADEnableURCC(true);
        wpad_init = true;
    }

    if (!kpad_init) {
        KPADInit();
        kpad_init = true;
    }

    VPADStatus vpad {};
    WPADStatusProController wpad {};
    KPADStatus kpad {};
    VPADTouchData touch_calibrated {};

    state.pressed = 0;
    state.held = 0;
    state.left_stick = StickState{};
    state.right_stick = StickState{};
    state.touch.move_x = 0.0f;
    state.touch.move_y = 0.0f;
    state.using_pro_controller = false;
    state.cursor_position.x = 0.0f;
    state.cursor_position.y = 0.0f;
    state.valid_cursor = false;

    WPADExtensionType extType;
    if (WPADProbe(WPAD_CHAN_0, &extType) == 0 && extType == WPAD_EXT_PRO_CONTROLLER) {
        if (!s_use_wpad) {
            WPADSetDataFormat(WPAD_CHAN_0, WPAD_FMT_PRO_CONTROLLER);
            s_use_wpad = true;
            s_use_kpad = false;
            log_message(LOG_OK, "Input", "Pro Controller connected");
        }

        state.using_pro_controller = true;
        WPADRead(WPAD_CHAN_0, &wpad.core);

        if (wpad.buttons & WPAD_PRO_BUTTON_A) set_button(state, BTN_A);
        if (wpad.buttons & WPAD_PRO_BUTTON_B) set_button(state, BTN_B);
        if (wpad.buttons & WPAD_PRO_BUTTON_X) set_button(state, BTN_X);
        if (wpad.buttons & WPAD_PRO_BUTTON_Y) set_button(state, BTN_Y);
        if (wpad.buttons & WPAD_PRO_BUTTON_PLUS) set_button(state, BTN_PLUS);
        if (wpad.buttons & WPAD_PRO_BUTTON_MINUS) set_button(state, BTN_MINUS);
        if (wpad.buttons & WPAD_PRO_BUTTON_LEFT) set_button(state, BTN_LEFT);
        if (wpad.buttons & WPAD_PRO_BUTTON_RIGHT) set_button(state, BTN_RIGHT);
        if (wpad.buttons & WPAD_PRO_BUTTON_UP) set_button(state, BTN_UP);
        if (wpad.buttons & WPAD_PRO_BUTTON_DOWN) set_button(state, BTN_DOWN);
        if (wpad.buttons & WPAD_PRO_BUTTON_L) set_button(state, BTN_L);
        if (wpad.buttons & WPAD_PRO_BUTTON_ZL) set_button(state, BTN_ZL);
        if (wpad.buttons & WPAD_PRO_BUTTON_R) set_button(state, BTN_R);
        if (wpad.buttons & WPAD_PRO_BUTTON_ZR) set_button(state, BTN_ZR);

        state.left_stick.x  = wpad.leftStick.x;
        state.left_stick.y  = wpad.leftStick.y;
        state.right_stick.x = wpad.rightStick.x;
        state.right_stick.y = wpad.rightStick.y;

    } else if (s_use_wpad) {
        s_use_wpad = false;
        log_message(LOG_OK, "Input", "Pro Controller disconnected");
    }

    if (!s_use_wpad && KPADReadEx(WPAD_CHAN_0, &kpad, 1, nullptr) > 0) {
        if (!s_use_kpad) {
            s_use_kpad = true;
            const char* ext_name = "None";
            switch (kpad.extensionType) {
                case WPAD_EXT_CLASSIC: ext_name = "Classic Controller"; break;
                case WPAD_EXT_PRO_CONTROLLER: ext_name = "Pro Controller"; break;
                default: break;
            }
            log_message(LOG_OK, "Input", "Wii Remote connected (Extension: %s)", ext_name);
        }

        state.valid_cursor = (kpad.posValid == 1 || kpad.posValid == 2) &&
                             (kpad.pos.x >= -1.0f && kpad.pos.x <= 1.0f) &&
                             (kpad.pos.y >= -1.0f && kpad.pos.y <= 1.0f);
        
        if (state.valid_cursor) {           
            state.cursor_position.x = SCREEN_WIDTH * kpad.pos.x;
            state.cursor_position.y = SCREEN_HEIGHT * kpad.pos.y;
        }

        if (kpad.hold & WPAD_BUTTON_A) set_button(state, BTN_A);
        if (kpad.hold & WPAD_BUTTON_B) set_button(state, BTN_B);
        if (kpad.hold & WPAD_BUTTON_1) set_button(state, BTN_Y); // Map 1 to Y
        if (kpad.hold & WPAD_BUTTON_2) set_button(state, BTN_X); // Map 2 to X
        if (kpad.hold & WPAD_BUTTON_PLUS) set_button(state, BTN_PLUS);
        if (kpad.hold & WPAD_BUTTON_MINUS) set_button(state, BTN_MINUS);
        if (kpad.hold & WPAD_BUTTON_LEFT) set_button(state, BTN_LEFT);
        if (kpad.hold & WPAD_BUTTON_RIGHT) set_button(state, BTN_RIGHT);
        if (kpad.hold & WPAD_BUTTON_UP) set_button(state, BTN_UP);
        if (kpad.hold & WPAD_BUTTON_DOWN) set_button(state, BTN_DOWN);

        switch (kpad.extensionType) {
            case WPAD_EXT_CLASSIC:
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_A) set_button(state, BTN_A);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_B) set_button(state, BTN_B);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_X) set_button(state, BTN_X);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_Y) set_button(state, BTN_Y);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_PLUS) set_button(state, BTN_PLUS);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_MINUS) set_button(state, BTN_MINUS);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_LEFT) set_button(state, BTN_LEFT);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_RIGHT) set_button(state, BTN_RIGHT);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_UP) set_button(state, BTN_UP);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_DOWN) set_button(state, BTN_DOWN);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_L) set_button(state, BTN_L);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_ZL) set_button(state, BTN_ZL);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_R) set_button(state, BTN_R);
                if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_ZR) set_button(state, BTN_ZR);

                state.left_stick.x = kpad.classic.leftStick.x;
                state.left_stick.y = kpad.classic.leftStick.y;
                state.right_stick.x = kpad.classic.rightStick.x;
                state.right_stick.y = kpad.classic.rightStick.y;
                break;

            default:
                break;
        }
    } else if (s_use_kpad && KPADReadEx(WPAD_CHAN_0, &kpad, 1, nullptr) <= 0) {
        s_use_kpad = false;
        log_message(LOG_OK, "Input", "Wii Remote disconnected");
    }

    if (VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr)) {
        if (vpad.hold & VPAD_BUTTON_A) set_button(state, BTN_A);
        if (vpad.hold & VPAD_BUTTON_B) set_button(state, BTN_B);
        if (vpad.hold & VPAD_BUTTON_X) set_button(state, BTN_X);
        if (vpad.hold & VPAD_BUTTON_Y) set_button(state, BTN_Y);
        if (vpad.hold & VPAD_BUTTON_PLUS) set_button(state, BTN_PLUS);
        if (vpad.hold & VPAD_BUTTON_MINUS) set_button(state, BTN_MINUS);
        if (vpad.hold & VPAD_BUTTON_LEFT) set_button(state, BTN_LEFT);
        if (vpad.hold & VPAD_BUTTON_RIGHT) set_button(state, BTN_RIGHT);
        if (vpad.hold & VPAD_BUTTON_UP) set_button(state, BTN_UP);
        if (vpad.hold & VPAD_BUTTON_DOWN) set_button(state, BTN_DOWN);
        if (vpad.hold & VPAD_BUTTON_L) set_button(state, BTN_L);
        if (vpad.hold & VPAD_BUTTON_ZL) set_button(state, BTN_ZL);
        if (vpad.hold & VPAD_BUTTON_R) set_button(state, BTN_R);
        if (vpad.hold & VPAD_BUTTON_ZR) set_button(state, BTN_ZR);

        state.left_stick.x  = vpad.leftStick.x;
        state.left_stick.y  = vpad.leftStick.y;
        state.right_stick.x = vpad.rightStick.x;
        state.right_stick.y = vpad.rightStick.y;

        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch_calibrated, &vpad.tpNormal);

        if (touch_calibrated.touched) {
            float tx = (float)touch_calibrated.x;
            float ty = (float)touch_calibrated.y;

            if (!state.touch.touched) {
                state.touch.x = state.touch.old_x = tx;
                state.touch.y = state.touch.old_y = ty;
                state.touch.move_x = 0.0f;
                state.touch.move_y = 0.0f;
            } else {
                state.touch.old_x = state.touch.x;
                state.touch.old_y = state.touch.y;
                state.touch.x = tx;
                state.touch.y = ty;
                state.touch.move_x = state.touch.x - state.touch.old_x;
                state.touch.move_y = state.touch.y - state.touch.old_y;
            }
            state.touch.touched = true;
        } else {
            state.touch.touched = false;
            state.touch.move_x = 0.0f;
            state.touch.move_y = 0.0f;
        }
    }

#ifdef DEBUG
    uint64_t changed = state.pressed ^ last_buttons;
    if (changed) {
        for (int i = 0; i < 64; ++i) {
            if (changed & (1ull << i)) {
                bool now = state.pressed & (1ull << i);
                log_message(LOG_DEBUG, "Input", "Button %d %s", i, now ? "pressed" : "released");
            }
        }
        last_buttons = state.pressed;
    }

    auto log_stick = [&](const char* name, const StickState& stick) {
        if (fabs(stick.x) > 0.15f || fabs(stick.y) > 0.15f)
            log_message(LOG_DEBUG, "Input", "%s Stick: X=%.2f Y=%.2f", name, stick.x, stick.y);
    };
    log_stick("Left", state.left_stick);
    log_stick("Right", state.right_stick);

    if (state.touch.touched && !last_touch) {
        log_message(LOG_DEBUG, "Input", "Touch START at (%.1f, %.1f)", state.touch.x, state.touch.y);
    } else if (!state.touch.touched && last_touch) {
        log_message(LOG_DEBUG, "Input", "Touch END at (%.1f, %.1f)", last_touch_x, last_touch_y);
    } else if (state.touch.touched) {
        if (fabs(state.touch.move_x) || fabs(state.touch.move_y))
            log_message(LOG_DEBUG, "Input", "Touch MOVE (%.1f, %.1f) Δ(%.1f, %.1f)",
                        state.touch.x, state.touch.y, state.touch.move_x, state.touch.move_y);
    }

    last_touch = state.touch.touched;
    last_touch_x = state.touch.x;
    last_touch_y = state.touch.y;

    if (state.valid_cursor) {
        log_message(LOG_DEBUG, "Input", "Cursor: (%.1f, %.1f)", state.cursor_position.x, state.cursor_position.y);
    }
#endif
}