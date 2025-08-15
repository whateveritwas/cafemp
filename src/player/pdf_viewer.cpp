#include <SDL2/SDL.h>

#include "main.hpp"
#include "utils/sdl.hpp"
#include "utils/utils.hpp"
#include "logger/logger.hpp"
#include "player/pdf_viewer.hpp"

void pdf_viewer_init() {
	log_message(LOG_OK, "Pdf Viewer", "Starting Pdf Viewer");
}

void pdf_viewer_open_file(const char* filepath) {
}

void pdf_texture_zoom(float delta_zoom) {
}

void pdf_viewer_pan(int delta_x, int delta_y) {
}

void pdf_viewer_render() {
	draw_checkerboard_pattern(sdl_get()->sdl_renderer, SCREEN_WIDTH, SCREEN_HEIGHT, 40);
}

void pdf_viewer_cleanup() {
	log_message(LOG_OK, "Pdf Viewer", "Cleaned up");
}

