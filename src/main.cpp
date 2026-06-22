#include "main.hpp"

#include "logger/logger.hpp"
#include "ui/menu.hpp"
#include "utils/display.hpp"
#include "utils/power_manager.hpp"

#include <sndcore2/core.h>
#include <whb/gfx.h>
#include <whb/proc.h>

int main(void) {
    WHBProcInit();
    WHBGfxInit();
    power_manager_sleep_enable(false);

    AXInit();
    AXQuit();

    log_message(LOG_OK, "Main", "\x1b[2J\x1b[HApplication Start");

    display_setup();
    ui_init();

    while (WHBProcIsRunning()) {
        ui_render();
    }

    ui_shutdown();

    log_message(LOG_OK, "Main", "Application End");

    power_manager_sleep_enable(true);
    WHBGfxShutdown();
    WHBProcShutdown();

    return 0;
}
