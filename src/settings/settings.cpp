#include <jansson.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "main.hpp"
#include "settings/settings.hpp"

static settings_struct settings;

void settings_save() {
    json_t *root = json_object();

    json_object_set_new(root, "version", json_integer(settings.version));
    json_object_set_new(root, "background_music_enabled", json_boolean(settings.background_music_enabled));
    //json_object_set_new(root, "path", json_string(settings.path));

    FILE *file = fopen(SETTINGS_PATH, "w");
    if (file) {
        json_dumpf(root, file, JSON_INDENT(4));
        fclose(file);
    } else {
        printf("[Settings] Failed to save settings to file.\n");
    }

    json_decref(root);
}

void settings_load() {
    FILE *file = fopen(SETTINGS_PATH, "r");
    if (file) {
        json_error_t error;
        json_t *root = json_loadf(file, 0, &error);
        fclose(file);

        if (root) {
            json_t *version = json_object_get(root, "version");
            if (json_is_integer(version)) {
                settings.version = json_integer_value(version);
            }

            json_t *bg_music = json_object_get(root, "background_music_enabled");
            if (json_is_boolean(bg_music)) {
                settings.background_music_enabled = json_is_true(bg_music);
            }
            /*
            json_t *path = json_object_get(root, "path");
            if (json_is_string(path)) {
                free((void*)settings.path);
                settings.path = strdup(json_string_value(path));
            }
            */
            json_decref(root);
        } else {
            printf("[Settings] Failed to load settings: %s\n", error.text);
        }
    } else {
        printf("[Settings] No settings file found, using default settings.\n");
    }
}

void settings_set(settings_keys key, void* value) {
    switch (key) {
        case SETTINGS_VERSION:
            settings.version = *reinterpret_cast<int*>(value);
            break;
        case SETTINGS_BKG_MUSIC_ENABLED:
            settings.background_music_enabled = *reinterpret_cast<int*>(value);
            break;
        /*
        case SETTINGS_PATH:
            free((void*)settings.path);
            settings.path = strdup(reinterpret_cast<const char*>(value));
            break;
        */
    }
}

const settings_struct& settings_get() {
    return settings;
}

void* settings_get_value(settings_keys key) {
    switch (key) {
        case SETTINGS_VERSION:
            return (void*)&settings.version;
        case SETTINGS_BKG_MUSIC_ENABLED:
            return (void*)&settings.background_music_enabled;
        // case SETTINGS_PATH:
        //    return (void*)settings.path;
        default:
            return nullptr;
    }
}
