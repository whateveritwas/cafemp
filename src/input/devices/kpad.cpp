#include "input/devices/kpad.hpp"

#include "input/input_device.hpp"
#include "logger/logger.hpp"
#include "utils/display.hpp"

#include <imgui/imgui.h>
#include <padscore/kpad.h>

static input_device kpad_device;
static KPADStatus kpad_raw;

void input_device_kpad_init() {
    KPADInit();
    log_message(LOG_OK, "Kpad", "Initialised Kpad");
}

void input_device_kpad_poll() {
    kpad_device = {};

    if (KPADReadEx(WPAD_CHAN_0, &kpad_raw, 1, nullptr) > 0) {
        kpad_device.pointer.valid = (kpad_raw.posValid == 1 || kpad_raw.posValid == 2) && (kpad_raw.pos.x >= -1.0f && kpad_raw.pos.x <= 1.0f) && (kpad_raw.pos.y >= -1.0f && kpad_raw.pos.y <= 1.0f);

        if (kpad_device.pointer.valid) {
            kpad_device.pointer.x = kpad_raw.pos.x * display_get().width;
            kpad_device.pointer.delta_x = kpad_raw.posDiff.x * display_get().width;

            kpad_device.pointer.y = kpad_raw.pos.y * display_get().height;
            kpad_device.pointer.delta_y = kpad_raw.posDiff.y * display_get().height;
        }

        switch (kpad_raw.extensionType) {
            case WPAD_EXT_CORE:
            case WPAD_EXT_MPLUS:
                if (kpad_raw.hold & WPAD_BUTTON_A) kpad_device.buttons |= BUTTON_A;
                if (kpad_raw.hold & WPAD_BUTTON_B) kpad_device.buttons |= BUTTON_B;

                if (kpad_raw.hold & WPAD_BUTTON_1) kpad_device.buttons |= BUTTON_X;
                if (kpad_raw.hold & WPAD_BUTTON_2) kpad_device.buttons |= BUTTON_Y;

                if (kpad_raw.hold & WPAD_BUTTON_PLUS) kpad_device.buttons |= BUTTON_PLUS;
                if (kpad_raw.hold & WPAD_BUTTON_MINUS) kpad_device.buttons |= BUTTON_MINUS;

                if (kpad_raw.hold & WPAD_BUTTON_LEFT) kpad_device.buttons |= BUTTON_UP;
                if (kpad_raw.hold & WPAD_BUTTON_RIGHT) kpad_device.buttons |= BUTTON_DOWN;
                if (kpad_raw.hold & WPAD_BUTTON_UP) kpad_device.buttons |= BUTTON_LEFT;
                if (kpad_raw.hold & WPAD_BUTTON_DOWN) kpad_device.buttons |= BUTTON_RIGHT;

                if (kpad_device.pointer.valid) {
                    ImGuiIO &io = ImGui::GetIO();
                    io.MousePos = ImVec2(kpad_device.pointer.x, kpad_device.pointer.y);

                    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, kpad_raw.hold & WPAD_BUTTON_A);
                }

                break;

            case WPAD_EXT_CLASSIC:
            case WPAD_EXT_MPLUS_CLASSIC:
            case WPAD_EXT_PRO_CONTROLLER:
                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_A) kpad_device.buttons |= BUTTON_A;
                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_B) kpad_device.buttons |= BUTTON_B;

                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_X) kpad_device.buttons |= BUTTON_X;
                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_Y) kpad_device.buttons |= BUTTON_Y;

                if (kpad_raw.hold & WPAD_CLASSIC_BUTTON_PLUS) kpad_device.buttons |= BUTTON_PLUS;
                if (kpad_raw.hold & WPAD_CLASSIC_BUTTON_MINUS) kpad_device.buttons |= BUTTON_MINUS;

                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_LEFT) kpad_device.buttons |= BUTTON_UP;
                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_RIGHT) kpad_device.buttons |= BUTTON_DOWN;
                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_UP) kpad_device.buttons |= BUTTON_LEFT;
                if (kpad_raw.classic.hold & WPAD_CLASSIC_BUTTON_DOWN) kpad_device.buttons |= BUTTON_RIGHT;

                kpad_device.left.x = kpad_raw.classic.leftStick.x;
                kpad_device.left.y = kpad_raw.classic.leftStick.y;
                kpad_device.right.x = kpad_raw.classic.rightStick.x;
                kpad_device.right.y = kpad_raw.classic.rightStick.y;

                break;

            default:
                break;
        }

        kpad_device.connected = true;
    } else {
        kpad_device.connected = false;
    }
}

input_device *input_device_kpad_get() { return &kpad_device; }

void input_device_kpad_shutdown() {
    KPADShutdown();
    log_message(LOG_OK, "Kpad", "Shutdown Kpad");
}
