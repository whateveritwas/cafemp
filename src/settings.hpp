#ifndef SETTINGS_H
#define SETTINGS_H

struct settings_struct {
    int version = 0;
    int background_music_enabled = 1;
//    const char* path = "/vol/external01/settings.json";
};

enum settings_keys {
    SETTINGS_VERSION,
    SETTINGS_BKG_MUSIC_ENABLED,
//    SETTINGS_PATH
};

void settings_save();
void settings_load();
void settings_set(settings_keys key, void* value);
const settings_struct& settings_get();
void* settings_get_value(settings_keys key);

#endif
