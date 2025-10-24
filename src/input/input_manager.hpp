#ifndef INPUT_H
#define INPUT_H

#include <vpad/input.h>
#include <padscore/wpad.h>

void input_menu(VPADStatus* vpad_status, WPADStatusProController* wpad_status, int& current_page_file_browser, int& selected_index);
void input_use_wpad(bool state);
bool input_use_wpad();
void input_check_wpad_pro_connection();
bool input_is_vpad_touched();
void input_settings(VPADStatus* vpad_status, WPADStatusProController* wpad_status);
void input_video_player(VPADStatus* vpad_status, WPADStatusProController* wpad_status);
void input_audio_player(VPADStatus* vpad_status, WPADStatusProController* wpad_status);
void input_update(int& current_page_file_browser, int& selected_index, struct nk_context *ctx);

#endif