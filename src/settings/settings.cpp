#include <stdio.h>
#include <string.h>
#include <jansson.h>

#include "main.hpp"
#include "logger/logger.hpp"
#include "settings/settings.hpp"

static settings_struct settings = {
    .version = 1,
    .background_music_enabled = true,
    .jellyfin_url = "",
    .jellyfin_api_key = ""
};

void settings_save() {
    json_t *root = json_object();

    json_object_set_new(root, "version", json_integer(settings.version));
    json_object_set_new(root, "background_music_enabled", json_boolean(settings.background_music_enabled));
    json_object_set_new(root, "jellyfin_url", json_string(settings.jellyfin_url));
    json_object_set_new(root, "jellyfin_api_key", json_string(settings.jellyfin_api_key));

    FILE *file = fopen(SETTINGS_PATH, "w");
    if (file) {
        json_dumpf(root, file, JSON_INDENT(4));
        fclose(file);
    } else {
    	log_message(LOG_ERROR, "Settings", "Settings failed to save");
    }

    json_decref(root);
}

void settings_load() {
    FILE *file = fopen(SETTINGS_PATH, "r");
    if (!file) {
    	log_message(LOG_ERROR, "Settings", "Settings file not found");
        return;
    }

    json_error_t error;
    json_t *root = json_loadf(file, 0, &error);
    fclose(file);

    if (!root) {
    	log_message(LOG_ERROR, "Settings", "Settings failed to load");
        return;
    }

    json_t *j_version = json_object_get(root, "version");
    if (json_is_integer(j_version)) {
    	if(settings.version != (int)json_integer_value(j_version)) log_message(LOG_WARNING, "Settings", "Local settings format v%i differs from expected format v%i", (int)json_integer_value(j_version), settings.version);
    }

    json_t *j_bg = json_object_get(root, "background_music_enabled");
    if (json_is_boolean(j_bg)) {
        settings.background_music_enabled = json_is_true(j_bg);
    }

    json_t *j_jf_url = json_object_get(root, "jellyfin_url");
    if (json_is_string(j_jf_url)) {
        strncpy(settings.jellyfin_url, json_string_value(j_jf_url), MAX_URL_LENGTH);
        settings.jellyfin_url[MAX_URL_LENGTH - 1] = '\0';
    }

    json_t *j_jf_api_key = json_object_get(root, "jellyfin_api_key");
    if (json_is_string(j_jf_api_key)) {
        strncpy(settings.jellyfin_api_key, json_string_value(j_jf_api_key), MAX_API_KEY_LENGTH);
        settings.jellyfin_api_key[MAX_API_KEY_LENGTH - 1] = '\0';
    }

    json_decref(root);
}

void settings_set(settings_keys key, const void* value) {
    switch (key) {
        case SETTINGS_VERSION:
            settings.version = *(const int*)value;
            break;
        case SETTINGS_BKG_MUSIC_ENABLED:
            settings.background_music_enabled = *(const bool*)value;
            break;
        case SETTINGS_JELLYFIN_URL:
            strncpy(settings.jellyfin_url, (const char*)value, MAX_URL_LENGTH);
            settings.jellyfin_url[MAX_URL_LENGTH - 1] = '\0';
            break;
        case SETTINGS_JELLYFIN_API_KEY:
            strncpy(settings.jellyfin_api_key, (const char*)value, MAX_API_KEY_LENGTH);
            settings.jellyfin_api_key[MAX_API_KEY_LENGTH - 1] = '\0';
            break;
        default:
        	log_message(LOG_ERROR, "Settings", "Settings key is invalid");
            break;
    }
}

void settings_get(settings_keys key, void* out_value) {
    if (!out_value) return;

    switch (key) {
        case SETTINGS_VERSION:
            *(int*)out_value = settings.version;
            break;
        case SETTINGS_BKG_MUSIC_ENABLED:
            *(bool*)out_value = settings.background_music_enabled;
            break;
        case SETTINGS_JELLYFIN_URL:
            strncpy((char*)out_value, settings.jellyfin_url, MAX_URL_LENGTH);
            ((char*)out_value)[MAX_URL_LENGTH - 1] = '\0';
            break;
        case SETTINGS_JELLYFIN_API_KEY:
            strncpy((char*)out_value, settings.jellyfin_api_key, MAX_API_KEY_LENGTH);
            ((char*)out_value)[MAX_API_KEY_LENGTH - 1] = '\0';
            break;
        default:
        	log_message(LOG_ERROR, "Settings", "Settings key is invalid");
            break;
    }
}

const settings_struct* settings_get_all() {
    return &settings;
}
