#ifdef __WIIU__
#include <whb/proc.h>
#include <whb/gfx.h>
#include <nn/ac.h>
#include <sys/socket.h>
#endif

#include "main.hpp"
#include "ui/menu.hpp"
#include "utils/sdl.hpp"
#include "utils/power_manager.hpp"
#include "logger/logger.hpp"

uint32_t ip = 0;

#ifndef __WIIU__
#include <SDL2/SDL.h>

bool WHBProcIsRunning() {
    SDL_Event e;
    SDL_PollEvent(&e);

    if (e.type == SDL_QUIT)
        return false;
    return true;
}
#endif

int main(int argc, char **argv) {
    static const int out_channels = 2;
    static const int out_sample_rate = 48000;

#ifdef __WIIU__    
    nn::ac::ConfigIdNum configId;

    nn::ac::Initialize();
    nn::ac::GetStartupId(&configId);
    nn::ac::Connect(configId);

    WHBProcInit();
#endif

    log_message(LOG_OK, "Main", "\x1b[2J\x1b[HApplication Start");
    
#ifdef __WIIU__
    if (!nn::ac::GetAssignedAddress(&ip)) {
    	log_message(LOG_WARNING, "Main", "Failed to get an IP address assigned");
    } else {
        log_message(LOG_OK, "Main", "IP is: %u.%u.%u.%u",
		    (ip >> 24) & 0xFF,
		    (ip >> 16) & 0xFF,
		    (ip >> 8) & 0xFF,
		    (ip >> 0) & 0xFF);
    }    
#endif
    
    if (sdl_init() != 0) {
        log_message(LOG_ERROR, "Main", "Failed to initialize SDL window or renderer.");
        SDL_Quit();
        return -1;
    }

    ui_init();
    
#ifdef __WIIU__
//    WHBGfxInit();

    power_manager_sleep_enable(false);
#endif
    
    SDL_AudioSpec wanted_spec;
    SDL_AudioSpec audio_spec;
    SDL_zero(wanted_spec);
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.samples = 4096;
    wanted_spec.callback = nullptr;

    SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &wanted_spec, &audio_spec, 0);
    if (!audio_device)
        return -1;

    if (audio_device != 0) {
        SDL_PauseAudioDevice(audio_device, 1);
        SDL_ClearQueuedAudio(audio_device);
        SDL_CloseAudioDevice(audio_device);
        audio_device = 0;
    }    
    

    while (WHBProcIsRunning()) {
        ui_render();
        sdl_render();
    }

#ifdef __WIIU__    
    power_manager_sleep_enable(true);
#endif
    
    ui_shutdown();

    if (sdl_cleanup() != 0) {
    	log_message(LOG_ERROR, "Main", "Failed to clean up sdl some how");
    	return -1;
    }

    log_message(LOG_OK, "Main", "Application End");

#ifdef __WIIU__    
    WHBProcShutdown();
    nn::ac::Finalize();
#endif    
    return 0;
}
