#ifndef SDL_H
#define SDL_H

#include <SDL2/SDL.h>

typedef struct {
	SDL_Window* sdl_window;
	SDL_Renderer* sdl_renderer;
	SDL_Texture* sdl_texture;
} sdl_instance_struct;

int sdl_init();
void sdl_render();
sdl_instance_struct* sdl_get();
int sdl_cleanup();

#endif
