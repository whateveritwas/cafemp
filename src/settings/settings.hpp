#ifndef SETTINGS_HPP
#define SETTINGS_HPP

#include <stdbool.h>

#define MAX_URL_LENGTH 256
#define MAX_API_KEY_LENGTH 128

typedef enum {
    SETTINGS_VERSION,
    SETTINGS_BKG_MUSIC_ENABLED,
    SETTINGS_JELLYFIN_URL,
    SETTINGS_JELLYFIN_API_KEY,
    SETTINGS_COUNT
} settings_keys;

typedef struct {
    int version;
    bool background_music_enabled;
    char jellyfin_url[MAX_URL_LENGTH];
    char jellyfin_api_key[MAX_API_KEY_LENGTH];
} settings_struct;

void settings_save();
void settings_load();
void settings_set(settings_keys key, const void* value);
void settings_get(settings_keys key, void* out_value);

const settings_struct* settings_get_all();

#endif
