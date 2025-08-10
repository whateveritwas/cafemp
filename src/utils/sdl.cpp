#include <stdlib.h>
#include "logger/logger.hpp"
#include "utils/sdl.hpp"
#include "main.hpp"

static sdl_instance_struct* sdl_instance = NULL;
static int sdl_initialized = 0;

int sdl_init() {
    if (sdl_initialized) {
        log_message(LOG_WARNING, "SDL", "SDL already initialized, skipping re-init.");
        return 0;
    }

    log_message(LOG_OK, "SDL", "Starting SDL...");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        log_message(LOG_ERROR, "SDL", "Failed to init SDL: %s", SDL_GetError());
        return -1;
    }

    sdl_instance = (sdl_instance_struct*)calloc(1, sizeof(sdl_instance_struct));
    if (!sdl_instance) {
        log_message(LOG_ERROR, "SDL", "Failed to allocate SDL struct");
        SDL_Quit();
        return -1;
    }

    sdl_instance->sdl_window = SDL_CreateWindow(
        "",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        0
    );
    if (!sdl_instance->sdl_window) {
        log_message(LOG_ERROR, "SDL", "Failed to create SDL window: %s", SDL_GetError());
        sdl_cleanup();
        return -1;
    }

    sdl_instance->sdl_renderer = SDL_CreateRenderer(
        sdl_instance->sdl_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!sdl_instance->sdl_renderer) {
        log_message(LOG_ERROR, "SDL", "Failed to create SDL renderer: %s", SDL_GetError());
        sdl_cleanup();
        return -1;
    }

    SDL_RenderSetLogicalSize(sdl_instance->sdl_renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    sdl_initialized = 1;
    return 0;
}

void sdl_render() {
	SDL_RenderPresent(sdl_instance->sdl_renderer);
}

sdl_instance_struct* sdl_get() {
    return sdl_instance;
}

int sdl_cleanup() {
    if (!sdl_initialized) {
        log_message(LOG_WARNING, "SDL", "SDL cleanup called but SDL was not initialized.");
        return 0;
    }

    if (sdl_instance->sdl_texture) SDL_DestroyTexture(sdl_instance->sdl_texture);
    if (sdl_instance->sdl_renderer) SDL_DestroyRenderer(sdl_instance->sdl_renderer);
    if (sdl_instance->sdl_window) SDL_DestroyWindow(sdl_instance->sdl_window);

    free(sdl_instance);
    sdl_instance = NULL;

    SDL_Quit();
    sdl_initialized = 0;

    log_message(LOG_OK, "SDL", "SDL cleaned up successfully.");
    return 0;
}
