#ifndef APP_STATE_H
#define APP_STATE_H

enum AppState {
    STATE_MENU,
    STATE_MENU_FILES,
    STATE_MENU_NETWORK_FILES,
    STATE_MENU_VIDEO_FILES,
    STATE_MENU_AUDIO_FILES,
    STATE_MENU_IMAGE_FILES,
    STATE_MENU_SETTINGS,
    STATE_PLAYING_VIDEO,
    STATE_PLAYING_AUDIO,
    STATE_VIEWING_PHOTO
};

AppState app_state_get();
void app_state_set(AppState new_app_state);

#endif