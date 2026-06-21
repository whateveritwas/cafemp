#include <imgui.h>

#include "widget_tooltip.hpp"

#include "utils/font.hpp"
#include "main.hpp"
#include "utils/app_state.hpp"

void widget_tooltip_render() {
    ImGui::SetNextWindowPos(ImVec2(0, display_get().height - TOOLTIP_BAR_HEIGHT * display_get().scale), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(display_get().width, TOOLTIP_BAR_HEIGHT * display_get().scale), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    if (ImGui::Begin("tooltip_bar", nullptr, flags)) {
        switch (app_state_get()) {
	case STATE_MENU: {
	    ImGui::Text("%s Select | %s Open", FONT_GLYPH_LEFT_ANALOG_STICK, FONT_GLYPH_A_BUTTON);
	    break;
	}
	case STATE_MENU_FILES:
	case STATE_MENU_NETWORK_FILES:
	case STATE_MENU_VIDEO_FILES:
	case STATE_MENU_AUDIO_FILES:
	case STATE_MENU_IMAGE_FILES:
	case STATE_MENU_PDF_FILES: {
	    ImGui::Text("%s Select | %s Open | %s Back", FONT_GLYPH_LEFT_ANALOG_STICK, FONT_GLYPH_A_BUTTON, FONT_GLYPH_B_BUTTON);
	    break;
        }
	case STATE_MENU_SETTINGS: {
	    break;
	}
	case STATE_MENU_EASTER_EGG: {
	    break;
	}
	case STATE_PLAYING_VIDEO: {
	    break;
	}
	case STATE_PLAYING_AUDIO: {
	    break;
	}
        case STATE_VIEWING_PHOTO:            
	case STATE_VIEWING_PDF: {
	    ImGui::Text("%s Change Page | %s Zoom + | %s Zoom - | (Touch) Pan | %s Back", FONT_GLYPH_LEFT_ANALOG_STICK, FONT_GLYPH_ZL_BUTTON, FONT_GLYPH_ZR_BUTTON, FONT_GLYPH_B_BUTTON);
	    break;
	}
        }
    }

    ImGui::End();
}
