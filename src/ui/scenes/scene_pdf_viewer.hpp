#ifndef UI_PDF_VIEWER_H
#define UI_PDF_VIEWER_H

#include <string>

void scene_pdf_viewer_init(std::string full_path);
void scene_pdf_viewer_render(struct nk_context *ctx);

#endif