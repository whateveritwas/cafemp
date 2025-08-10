#include <utils/sdl.hpp>
#include <whb/proc.h>
#include "main.hpp"
#include "ui/menu.hpp"
#include "logger/logger.hpp"

int main(int argc, char **argv) {
    WHBProcInit();

    log_message(LOG_OK, "Main", "Application Start.");

    if (sdl_init() != 0) {
        log_message(LOG_ERROR, "Main", "Failed to initialize SDL window or renderer.");
        SDL_Quit();
        return -1;
    }

    ui_init();

    while (WHBProcIsRunning()) {
        ui_render();
        sdl_render();
    }

    ui_shutdown();

    if (sdl_cleanup() != 0) {
    	log_message(LOG_ERROR, "Main", "Failed to clean up sdl some how");
    	return -1;
    }

    log_message(LOG_OK, "Main", "Application End.");

    WHBProcShutdown();
    return 0;
}
