#include "utils/sdl.hpp"

#include "ui/widgets/widget_cursor.hpp"

void widget_cursor_render(float cx, float cy, float radius) {
    SDL_SetRenderDrawColor(sdl_get()->sdl_renderer, 255, 172, 28, 255);

    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy <= radius*radius) {
                SDL_RenderDrawPoint(sdl_get()->sdl_renderer,
                                    (int)(cx + dx),
                                    (int)(cy + dy));
            }
        }
    }

	SDL_SetRenderDrawColor(sdl_get()->sdl_renderer, 0, 0, 0, 255);
}
