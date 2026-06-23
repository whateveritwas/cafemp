#ifndef APP_STATE_HPP
#define APP_STATE_HPP

enum AppState {
    STATE_MENU,
    STATE_MENU_FILES,
    STATE_MENU_SETTINGS,
    STATE_PLAYING_VIDEO,
    STATE_PLAYING_AUDIO,
    STATE_VIEWING_PHOTO,
    STATE_VIEWING_PDF
};

AppState app_state_get();
void app_state_set(AppState new_app_state);

#endif
