#ifndef UI_PHOTO_VIEWER_H
#define UI_PHOTO_VIEWER_H

#include <string>

void widget_photo_viewer_init(std::string full_path);
void widget_photo_viewer_render(struct nk_context *ctx);

#endif
