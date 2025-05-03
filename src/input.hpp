#ifndef INPUT_H
#define INPUT_H

#include <vpad/input.h>
#include <padscore/wpad.h>

void ui_menu_input(VPADStatus* vpad_status, 
                   WPADStatusProController* wpad_status, 
                   int& current_page_file_browser, 
                   int& selected_index);

void ui_settings_input(VPADStatus* vpad_status, WPADStatusProController* wpad_status);
void ui_video_player_input(VPADStatus* vpad_status, WPADStatusProController* wpad_status);
void ui_audio_player_input(VPADStatus* vpad_status, WPADStatusProController* wpad_status);
void ui_handle_vpad_input(int& current_page_file_browser, int& selected_index, nk_context *ctx);

#endif