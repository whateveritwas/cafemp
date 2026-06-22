#ifndef UI_PDF_VIEWER_HPP
#define UI_PDF_VIEWER_HPP

#include "input/input_actions.hpp"

#include <string>

void scene_pdf_viewer_init(std::string full_path);
void scene_pdf_viewer_render();
void scene_pdf_viewer_input(InputState &input);

#endif
