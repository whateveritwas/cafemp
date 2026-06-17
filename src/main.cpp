#ifdef __WIIU__
#include <whb/proc.h>
#include <whb/gfx.h>
#include <sndcore2/core.h>
#endif

#include "main.hpp"
#include "ui/menu.hpp"
#include "utils/power_manager.hpp"
#include "logger/logger.hpp"

int main(void) {
#ifdef __WIIU__
    WHBProcInit();
    WHBGfxInit();
    power_manager_sleep_enable(false);

    AXInit();
    AXQuit();
#endif

    log_message(LOG_OK, "Main", "\x1b[2J\x1b[HApplication Start");    
    
    ui_init();

    while (WHBProcIsRunning()) {
        ui_render();
    }

    ui_shutdown();

    log_message(LOG_OK, "Main", "Application End");

#ifdef __WIIU__
    power_manager_sleep_enable(true);
    WHBGfxShutdown();    
    WHBProcShutdown();
#endif    
    return 0;
}
