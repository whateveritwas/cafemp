#include "subtitle.hpp"

#include "vendor/srtparser.h"
#include <nuklear.h>
#include <vector>
#include <string>
#include <iostream>
#include "logger/logger.hpp"

std::vector<SubtitleItem*> subtitles;
SubtitleParserFactory* subParserFactory = nullptr;
SubtitleParser* parser = nullptr;
SubtitleItem* currentSubtitle = nullptr;

void subtitle_start(const std::string& filename) {
    subParserFactory = new SubtitleParserFactory(filename);
    parser = subParserFactory->getParser();

    if (!parser) {
    	log_message(LOG_ERROR, "Subtitle", "Failed to create subtitle parser for %s", filename.c_str());
        return;
    }

    subtitles = parser->getSubtitles();
    if (subtitles.empty()) {
    	log_message(LOG_ERROR, "Subtitle", "No subtitles loaded from %s", filename.c_str());
    }
}

void subtitle_update(double currentTime) {
    currentSubtitle = nullptr;

    for (auto* item : subtitles) {
        if (item->getStartTime() <= currentTime && currentTime <= item->getEndTime()) {
            currentSubtitle = item;
            break;
        }
    }
}

void subtitle_render(struct nk_context* ctx) {
    if (!currentSubtitle) return;

    const char* text = currentSubtitle->getDialogue().c_str();

    struct nk_rect screen = nk_window_get_content_region(ctx);

    nk_begin(ctx, "SubtitlesOverlay",
             nk_rect(0, screen.h - 60, screen.w, 60),
             NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND);

    nk_layout_row_dynamic(ctx, 40, 1);
    nk_label(ctx, text, NK_TEXT_CENTERED);

    nk_end(ctx);
}

void subtitle_cleanup() {
    for (auto* item : subtitles) delete item;
    subtitles.clear();

    delete parser;
    delete subParserFactory;
    parser = nullptr;
    subParserFactory = nullptr;
}
