#include <whb/proc.h>
#include <whb/gfx.h>
#include <nn/ac.h>
#include <sys/socket.h>

#include "main.hpp"
#include "ui/menu.hpp"
#include "utils/sdl.hpp"
#include "utils/power_manager.hpp"
#include "logger/logger.hpp"

uint32_t ip = 0;

int main(int argc, char **argv) {
	nn::ac::ConfigIdNum configId;

	nn::ac::Initialize();
	nn::ac::GetStartupId(&configId);
	nn::ac::Connect(configId);

    WHBProcInit();

    log_message(LOG_OK, "Main", "Application Start");

    if (!nn::ac::GetAssignedAddress(&ip)) {
    	log_message(LOG_WARNING, "Main", "Failed to get an IP address assigned");
    } else {
        log_message(LOG_OK, "Main", "IP is: %u.%u.%u.%u",
                     (ip >> 24) & 0xFF,
                     (ip >> 16) & 0xFF,
                     (ip >> 8) & 0xFF,
                     (ip >> 0) & 0xFF);
    }

    if (sdl_init() != 0) {
        log_message(LOG_ERROR, "Main", "Failed to initialize SDL window or renderer.");
        SDL_Quit();
        return -1;
    }

    ui_init();

//    WHBGfxInit();

	power_manager_sleep_enable(false);

    while (WHBProcIsRunning()) {
        ui_render();
        sdl_render();
    }

	power_manager_sleep_enable(true);

    ui_shutdown();

    if (sdl_cleanup() != 0) {
    	log_message(LOG_ERROR, "Main", "Failed to clean up sdl some how");
    	return -1;
    }

    log_message(LOG_OK, "Main", "Application End");

    WHBProcShutdown();
    // WHBGfxShutdown();
    nn::ac::Finalize();
    return 0;
}
