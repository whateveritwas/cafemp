#ifndef SUBTITLE_H
#define SUBTITLE_H

#include <string>

void subtitle_start(const std::string& filename);
void subtitle_update(double currentTime);
void subtitle_render(struct nk_context* ctx);
void subtitle_cleanup();

#endif
