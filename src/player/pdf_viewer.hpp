#ifndef PDF_VIEWER_H
#define PDF_VIEWER_H

#include <SDL2/SDL.h>

void pdf_viewer_init();
void pdf_viewer_open_file(const char* filepath);
void pdf_texture_zoom(float delta_zoom);
void pdf_viewer_pan(int delta_x, int delta_y);
void pdf_viewer_render();
void pdf_viewer_goto_page(int page_num);
void pdf_viewer_next_page();
void pdf_viewer_prev_page();
void pdf_viewer_fit_to_screen();
void pdf_viewer_cleanup();

#endif
