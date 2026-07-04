#include "input/devices/wpad.hpp"

#include "input/input_device.hpp"
#include "logger/logger.hpp"

#include <padscore/wpad.h>

static input_device wpad_device;
static WPADStatusProController wpad_raw;
static WPADExtensionType wpad_ext;

void input_device_wpad_init() {
    WPADInit();
    WPADEnableURCC(true);

    wpad_raw = {};
    wpad_ext = WPAD_EXT_UNKNOWN;

    log_message(LOG_OK, "Wpad", "Initialised Wpad");
}

void input_device_wpad_poll() {
    if (WPADProbe(WPAD_CHAN_0, &wpad_ext) == 0 && wpad_ext == WPAD_EXT_PRO_CONTROLLER) {
        WPADRead(WPAD_CHAN_0, &wpad_raw.core);

        if (!wpad_device.connected) {
            WPADSetDataFormat(WPAD_CHAN_0, WPAD_FMT_PRO_CONTROLLER);
            log_message(LOG_OK, "Wpad", "Connected Wpad");
            wpad_device.connected = true;
        }

        if (wpad_raw.buttons & WPAD_PRO_BUTTON_A) wpad_device.buttons |= BUTTON_A;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_B) wpad_device.buttons |= BUTTON_B;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_X) wpad_device.buttons |= BUTTON_X;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_Y) wpad_device.buttons |= BUTTON_Y;

        if (wpad_raw.buttons & WPAD_PRO_BUTTON_PLUS) wpad_device.buttons |= BUTTON_PLUS;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_MINUS) wpad_device.buttons |= BUTTON_MINUS;

        if (wpad_raw.buttons & WPAD_PRO_BUTTON_UP) wpad_device.buttons |= BUTTON_UP;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_DOWN) wpad_device.buttons |= BUTTON_DOWN;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_LEFT) wpad_device.buttons |= BUTTON_LEFT;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_RIGHT) wpad_device.buttons |= BUTTON_RIGHT;

        if (wpad_raw.buttons & WPAD_PRO_BUTTON_L) wpad_device.buttons |= BUTTON_L;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_ZL) wpad_device.buttons |= BUTTON_ZL;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_R) wpad_device.buttons |= BUTTON_R;
        if (wpad_raw.buttons & WPAD_PRO_BUTTON_ZR) wpad_device.buttons |= BUTTON_ZR;

        if (wpad_raw.buttons) wpad_device.active = true;

        wpad_device.left.x = wpad_raw.leftStick.x;
        wpad_device.left.y = wpad_raw.leftStick.y;
        wpad_device.right.x = wpad_raw.rightStick.x;
        wpad_device.right.y = wpad_raw.rightStick.y;
    } else {
        wpad_device.active = false;

        if (wpad_device.connected) {
            log_message(LOG_OK, "Wpad", "Disconnected Wpad");
            wpad_device.connected = false;
        }
    }
}

input_device *input_device_wpad_get() { return &wpad_device; }

void input_device_wpad_shutdown() {
    wpad_device.connected = false;

    WPADEnableURCC(false);
    WPADShutdown();

    log_message(LOG_OK, "Wpad", "Shutdown Wpad");
}
