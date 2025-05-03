#include <jansson.h>

#include "main.hpp"
#include "settings.hpp"

void save_settings(int background_music_enabled) {
    json_t *root = json_object();

    json_object_set_new(root, "background_music_enabled", json_boolean(background_music_enabled));

    FILE *file = fopen(SETTINGS_PATH, "w");
    if (file) {
        json_dumpf(root, file, JSON_INDENT(4));
        fclose(file);
    } else {
        printf("[Settings] Failed to save settings to file.\n");
    }

    json_decref(root);
}

void load_settings(int background_music_enabled) {
    FILE *file = fopen(SETTINGS_PATH, "r");
    if (file) {
        json_error_t error;
        json_t *root = json_loadf(file, 0, &error);
        fclose(file);

        if (root) {
            json_t *bg_music = json_object_get(root, "background_music_enabled");
            if (json_is_boolean(bg_music)) {
                background_music_enabled = json_is_true(bg_music);
            }

            json_decref(root);  // Free the JSON object
        } else {
            printf("[Settings] Failed to load settings: %s\n", error.text);
        }
    } else {
        printf("[Settings] No settings file found, using default settings.\n");
    }
}