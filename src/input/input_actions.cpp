#include <cstdio>
#include <cmath>

#ifdef __WIIU__
#include <vpad/input.h>
#include <padscore/wpad.h>
#include <padscore/kpad.h>
#include <imgui_impl_wiiu.h>
#endif

#include "logger/logger.hpp"
#include "main.hpp"
#include "input/input_actions.hpp"

static bool s_use_wpad = false;
static bool s_use_kpad = false;
static bool wpad_init = false;
static bool kpad_init = false;

static void update_button_repeat(InputState& state, InputButton btn, bool is_held, int initial_delay = 20, int repeat_rate = 5) {
    auto& repeat = state.repeat_states[btn];

    if (!is_held) {
        repeat.repeating = false;
        repeat.frames_until_repeat = 0;
        return;
    }

    if (input_pressed(state, btn)) {
        set_repeat(state, btn);
        repeat.repeating = true;
        repeat.frames_until_repeat = initial_delay;
        return;
    }

    if (!repeat.repeating)
        return;

    repeat.frames_until_repeat--;

    if (repeat.frames_until_repeat <= 0) {
        set_repeat(state, btn);
        repeat.frames_until_repeat = repeat_rate;
    }
}

void input_poll(InputState &state) {
    uint64_t previous_held = state.held;

    state.pressed = 0;
    state.held = 0;
    state.repeated = 0;

    state.left_stick = StickState{};
    state.right_stick = StickState{};

    state.cursor_position = {0.0f, 0.0f};
    state.valid_cursor = false;

    state.touch.move_x = 0.0f;
    state.touch.move_y = 0.0f;
    state.touch.touched = false;

    state.using_pro_controller = false;

#ifdef __WIIU__

    static VPADStatus vpad {};
    static WPADStatusProController wpad {};
    static KPADStatus kpad {};
    static VPADTouchData touch_calibrated{};

    static WPADExtensionType extType;

    static ImGui_ImplWiiU_ControllerInput wiiu_input;

    if (!wpad_init) {
        WPADInit();
        WPADEnableURCC(true);
        wpad_init = true;
    }

    if (!kpad_init) {
        KPADInit();
        kpad_init = true;
    }

    // ----------------------------
    // Pro Controller
    // ----------------------------
    if (WPADProbe(WPAD_CHAN_0, &extType) == 0 && extType == WPAD_EXT_PRO_CONTROLLER) {
        if (!s_use_wpad) {
            WPADSetDataFormat(WPAD_CHAN_0, WPAD_FMT_PRO_CONTROLLER);
            s_use_wpad = true;
            s_use_kpad = false;

            log_message(LOG_OK, "Input", "Pro Controller connected");
        }

        state.using_pro_controller = true;
        WPADRead(WPAD_CHAN_0, &wpad.core);

        if (wpad.buttons & WPAD_PRO_BUTTON_A) set_hold(state, BTN_A);
        if (wpad.buttons & WPAD_PRO_BUTTON_B) set_hold(state, BTN_B);
        if (wpad.buttons & WPAD_PRO_BUTTON_X) set_hold(state, BTN_X);
        if (wpad.buttons & WPAD_PRO_BUTTON_Y) set_hold(state, BTN_Y);

        if (wpad.buttons & WPAD_PRO_BUTTON_PLUS) set_hold(state, BTN_PLUS);
        if (wpad.buttons & WPAD_PRO_BUTTON_MINUS) set_hold(state, BTN_MINUS);

        if (wpad.buttons & WPAD_PRO_BUTTON_LEFT) set_hold(state, BTN_LEFT);
        if (wpad.buttons & WPAD_PRO_BUTTON_RIGHT) set_hold(state, BTN_RIGHT);
        if (wpad.buttons & WPAD_PRO_BUTTON_UP) set_hold(state, BTN_UP);
        if (wpad.buttons & WPAD_PRO_BUTTON_DOWN) set_hold(state, BTN_DOWN);

        if (wpad.buttons & WPAD_PRO_BUTTON_L) set_hold(state, BTN_L);
        if (wpad.buttons & WPAD_PRO_BUTTON_ZL) set_hold(state, BTN_ZL);
        if (wpad.buttons & WPAD_PRO_BUTTON_R) set_hold(state, BTN_R);
        if (wpad.buttons & WPAD_PRO_BUTTON_ZR) set_hold(state, BTN_ZR);

        state.left_stick.x  = wpad.leftStick.x;
        state.left_stick.y  = wpad.leftStick.y;
        state.right_stick.x = wpad.rightStick.x;
        state.right_stick.y = wpad.rightStick.y;
    }
    else if (s_use_wpad) {
        s_use_wpad = false;
        log_message(LOG_OK, "Input", "Pro Controller disconnected");
    }

    // ----------------------------
    // Wiimote / KPAD
    // ----------------------------
    if (!s_use_wpad && KPADReadEx(WPAD_CHAN_0, &kpad, 1, nullptr) > 0) {
	wiiu_input.kpad[WPAD_CHAN_0] = &kpad;
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

        if (kpad.hold & WPAD_BUTTON_A) set_hold(state, BTN_A);
        if (kpad.hold & WPAD_BUTTON_B) set_hold(state, BTN_B);

        if (kpad.hold & WPAD_BUTTON_1) set_hold(state, BTN_Y);
        if (kpad.hold & WPAD_BUTTON_2) set_hold(state, BTN_X);

        if (kpad.hold & WPAD_BUTTON_PLUS) set_hold(state, BTN_PLUS);
        if (kpad.hold & WPAD_BUTTON_MINUS) set_hold(state, BTN_MINUS);

        if (kpad.hold & WPAD_BUTTON_LEFT) set_hold(state, BTN_LEFT);
        if (kpad.hold & WPAD_BUTTON_RIGHT) set_hold(state, BTN_RIGHT);
        if (kpad.hold & WPAD_BUTTON_UP) set_hold(state, BTN_UP);
        if (kpad.hold & WPAD_BUTTON_DOWN) set_hold(state, BTN_DOWN);

        switch (kpad.extensionType) {
	case WPAD_EXT_CLASSIC:
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_A) set_hold(state, BTN_A);
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_B) set_hold(state, BTN_B);
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_X) set_hold(state, BTN_X);
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_Y) set_hold(state, BTN_Y);

	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_LEFT) set_hold(state, BTN_LEFT);
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_RIGHT) set_hold(state, BTN_RIGHT);
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_UP) set_hold(state, BTN_UP);
	    if (kpad.classic.hold & WPAD_CLASSIC_BUTTON_DOWN) set_hold(state, BTN_DOWN);

	    state.left_stick.x = kpad.classic.leftStick.x;
	    state.left_stick.y = kpad.classic.leftStick.y;
	    state.right_stick.x = kpad.classic.rightStick.x;
	    state.right_stick.y = kpad.classic.rightStick.y;
	    break;
	default:
	    break;
        }
    }
    else if (s_use_kpad && KPADReadEx(WPAD_CHAN_0, &kpad, 1, nullptr) <= 0) {
        s_use_kpad = false;
        log_message(LOG_OK, "Input", "Wii Remote disconnected");
    }

    // ----------------------------
    // Gamepad (VPAD)
    // ----------------------------
    if (VPADRead(VPAD_CHAN_0, &vpad, 1, nullptr)) {
	wiiu_input.vpad = &vpad;
        
        if (vpad.hold & VPAD_BUTTON_A) set_hold(state, BTN_A);
        if (vpad.hold & VPAD_BUTTON_B) set_hold(state, BTN_B);
        if (vpad.hold & VPAD_BUTTON_X) set_hold(state, BTN_X);
        if (vpad.hold & VPAD_BUTTON_Y) set_hold(state, BTN_Y);

        if (vpad.hold & VPAD_BUTTON_PLUS) set_hold(state, BTN_PLUS);
        if (vpad.hold & VPAD_BUTTON_MINUS) set_hold(state, BTN_MINUS);

        if (vpad.hold & VPAD_BUTTON_LEFT) set_hold(state, BTN_LEFT);
        if (vpad.hold & VPAD_BUTTON_RIGHT) set_hold(state, BTN_RIGHT);
        if (vpad.hold & VPAD_BUTTON_UP) set_hold(state, BTN_UP);
        if (vpad.hold & VPAD_BUTTON_DOWN) set_hold(state, BTN_DOWN);

        if (vpad.hold & VPAD_BUTTON_L) set_hold(state, BTN_L);
        if (vpad.hold & VPAD_BUTTON_ZL) set_hold(state, BTN_ZL);
        if (vpad.hold & VPAD_BUTTON_R) set_hold(state, BTN_R);
        if (vpad.hold & VPAD_BUTTON_ZR) set_hold(state, BTN_ZR);

        state.left_stick.x = vpad.leftStick.x;
        state.left_stick.y = vpad.leftStick.y;

        state.right_stick.x = vpad.rightStick.x;
        state.right_stick.y = vpad.rightStick.y;

        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &touch_calibrated, &vpad.tpNormal);

        if (touch_calibrated.touched) {
            float tx = (float)touch_calibrated.x;
            float ty = (float)touch_calibrated.y;

            if (!state.touch.touched) {
                state.touch.x = state.touch.old_x = tx;
                state.touch.y = state.touch.old_y = ty;
            } else {
                state.touch.move_x = tx - state.touch.x;
                state.touch.move_y = ty - state.touch.y;

                state.touch.old_x = state.touch.x;
                state.touch.old_y = state.touch.y;

                state.touch.x = tx;
                state.touch.y = ty;
            }

            state.touch.touched = true;
        }
    }

#else
    // ==================================================
    // PC INPUT (SDL)
    // ==================================================

    const uint8_t *keys = SDL_GetKeyboardState(nullptr);

    if (keys[SDL_SCANCODE_Z]) set_hold(state, BTN_A);
    if (keys[SDL_SCANCODE_X]) set_hold(state, BTN_B);
    if (keys[SDL_SCANCODE_C]) set_hold(state, BTN_X);
    if (keys[SDL_SCANCODE_V]) set_hold(state, BTN_Y);

    if (keys[SDL_SCANCODE_RETURN]) set_hold(state, BTN_PLUS);
    if (keys[SDL_SCANCODE_ESCAPE]) set_hold(state, BTN_MINUS);

    if (keys[SDL_SCANCODE_LEFT]) set_hold(state, BTN_LEFT);
    if (keys[SDL_SCANCODE_RIGHT]) set_hold(state, BTN_RIGHT);
    if (keys[SDL_SCANCODE_UP]) set_hold(state, BTN_UP);
    if (keys[SDL_SCANCODE_DOWN]) set_hold(state, BTN_DOWN);

    if (keys[SDL_SCANCODE_Q]) set_hold(state, BTN_L);
    if (keys[SDL_SCANCODE_E]) set_hold(state, BTN_R);
    if (keys[SDL_SCANCODE_1]) set_hold(state, BTN_ZL);
    if (keys[SDL_SCANCODE_2]) set_hold(state, BTN_ZR);

    int mx, my;
    uint32_t mouse = SDL_GetMouseState(&mx, &my);

    state.cursor_position = {(float)mx, (float)my};
    state.valid_cursor = true;

    if (mouse & SDL_BUTTON(SDL_BUTTON_LEFT)) {
        state.touch.touched = true;

        state.touch.x = (float)mx;
        state.touch.y = (float)my;

        state.touch.move_x = state.touch.x - state.touch.old_x;
        state.touch.move_y = state.touch.y - state.touch.old_y;

        state.touch.old_x = state.touch.x;
        state.touch.old_y = state.touch.y;
    }

#endif

    // ----------------------------
    // Common finalization
    // ----------------------------
    state.pressed = state.held & ~previous_held;

    for (int i = 0; i < INPUT_BUTTON_COUNT; ++i) {
        update_button_repeat(state, static_cast<InputButton>(i), input_held(state, static_cast<InputButton>(i)));
    }

    ImGui_ImplWiiU_ProcessInput(&wiiu_input);

#ifdef DEBUG
    static uint64_t last_buttons = 0;

    uint64_t changed = state.held ^ last_buttons;

    if (changed) {
        for (int i = 0; i < 64; ++i) {
            if (changed & (1ull << i)) {
                log_message(LOG_DEBUG, "Input", "Button %d %s",
			    i, (state.held & (1ull << i)) ? "pressed" : "released");
            }
        }
        last_buttons = state.held;
    }
#endif
}
