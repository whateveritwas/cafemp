#include "input/devices/vpad.hpp"

#include "input/input_device.hpp"
#include "logger/logger.hpp"

#include <imgui/backends/imgui_impl_wiiu.h>
#include <imgui/imgui.h>
#include <vpad/input.h>

static VPADStatus vpad_raw;
static VPADTouchData vpad_raw_touch;
static input_device vpad_device;
static ImGui_ImplWiiU_ControllerInput vpad_imgui_input;

void input_device_vpad_init() {
    VPADInit();

    vpad_device = {};
    vpad_raw = {};
    vpad_raw_touch = {};

    log_message(LOG_OK, "Vpad", "Initialised Vpad");
}

void input_device_vpad_poll() {
    if (VPADRead(VPAD_CHAN_0, &vpad_raw, 1, nullptr)) {
        if (!vpad_device.connected) {
            log_message(LOG_OK, "Vpad", "Connected Vpad");
            vpad_device.connected = true;
        }

        vpad_imgui_input.vpad = &vpad_raw;
        ImGui_ImplWiiU_ProcessInput(&vpad_imgui_input);

        VPADGetTPCalibratedPoint(VPAD_CHAN_0, &vpad_raw_touch, &vpad_raw.tpNormal);

        if (vpad_raw.hold & VPAD_BUTTON_A) vpad_device.buttons |= BUTTON_A;
        if (vpad_raw.hold & VPAD_BUTTON_B) vpad_device.buttons |= BUTTON_B;
        if (vpad_raw.hold & VPAD_BUTTON_X) vpad_device.buttons |= BUTTON_X;
        if (vpad_raw.hold & VPAD_BUTTON_Y) vpad_device.buttons |= BUTTON_Y;

        if (vpad_raw.hold & VPAD_BUTTON_PLUS) vpad_device.buttons |= BUTTON_PLUS;
        if (vpad_raw.hold & VPAD_BUTTON_MINUS) vpad_device.buttons |= BUTTON_MINUS;

        if (vpad_raw.hold & VPAD_BUTTON_UP) vpad_device.buttons |= BUTTON_UP;
        if (vpad_raw.hold & VPAD_BUTTON_DOWN) vpad_device.buttons |= BUTTON_DOWN;
        if (vpad_raw.hold & VPAD_BUTTON_LEFT) vpad_device.buttons |= BUTTON_LEFT;
        if (vpad_raw.hold & VPAD_BUTTON_RIGHT) vpad_device.buttons |= BUTTON_RIGHT;

        if (vpad_raw.hold & VPAD_BUTTON_L) vpad_device.buttons |= BUTTON_L;
        if (vpad_raw.hold & VPAD_BUTTON_ZL) vpad_device.buttons |= BUTTON_ZL;
        if (vpad_raw.hold & VPAD_BUTTON_R) vpad_device.buttons |= BUTTON_R;
        if (vpad_raw.hold & VPAD_BUTTON_ZR) vpad_device.buttons |= BUTTON_ZR;

        if (vpad_raw.hold) vpad_device.active = true;

        vpad_device.left.x = vpad_raw.leftStick.x;
        vpad_device.left.y = vpad_raw.leftStick.y;

        vpad_device.right.x = vpad_raw.rightStick.x;
        vpad_device.right.y = vpad_raw.rightStick.y;

        if (vpad_raw_touch.touched) {
            vpad_device.buttons |= BUTTON_TOUCH;

            vpad_device.pointer.valid = true;
            vpad_device.pointer.x = vpad_raw_touch.x;
            vpad_device.pointer.y = vpad_raw_touch.y;

            vpad_device.active = true;
        } else {
            vpad_device.pointer.valid = false;
        }
    } else {
        vpad_device.active = false;

        if (vpad_device.connected) {
            log_message(LOG_OK, "Vpad", "Disconnected Vpad");
            vpad_device.connected = false;
        }
    }
}

input_device *input_device_vpad_get() { return &vpad_device; }

void input_device_vpad_shutdown() {
    vpad_device.connected = false;

    VPADShutdown();

    log_message(LOG_OK, "Vpad", "Shutdown Vpad");
}
