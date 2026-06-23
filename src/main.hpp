#ifndef MAIN_HPP
#define MAIN_HPP

#ifdef LEGACY
#define PLATFORM_WIIU_LEGACY
#else
#define PLATFORM_WIIU
#endif

#ifdef PLATFORM_WIIU

#define SYMBOLS_FONT "content:/NerdFontsSymbolsOnly/SymbolsNerdFont-Regular.ttf"
#define AMBIANCE_PATH "content:/cafebgdemo.mp3"

#define BASE_PATH_RAW "/vol/external01/wiiu/apps/cafemp/"
#define MEDIA_PATH_AUDIO_RAW BASE_PATH_RAW "Audio/"
#define MEDIA_PATH_VIDEO_RAW BASE_PATH_RAW "Video/"
#define MEDIA_PATH_PHOTO_RAW BASE_PATH_RAW "Photo/"
#define MEDIA_PATH_PDF_RAW BASE_PATH_RAW "Library/"

#define BASE_PATH ""
#define MEDIA_PATH_AUDIO "audio:/"
#define MEDIA_PATH_PDF "library:/"
#define MEDIA_PATH_PHOTO "photo:/"
#define MEDIA_PATH_VIDEO "video:/"
#define MEDIA_PATH_USB "usb:/"

#define SETTINGS_PATH "settings:/settings.json"

#endif

#ifdef PLATFORM_WIIU_LEGACY

#define BASE_PATH "/vol/external01/wiiu/apps/cafemp/"
#define CONTENT_PATH "/vol/external01/wiiu/apps/cafemp/content/"

#define FONT_PATH CONTENT_PATH "Roboto-Regular.ttf"
#define AMBIANCE_PATH CONTENT_PATH "cafebgdemo.mp3"

#define MEDIA_PATH_AUDIO BASE_PATH "Audio/"
#define MEDIA_PATH_VIDEO BASE_PATH "Video/"
#define MEDIA_PATH_PHOTO BASE_PATH "Photo/"
#define MEDIA_PATH_PDF BASE_PATH "Library/"

#define SETTINGS_PATH BASE_PATH "settings.json"

#endif
#define VERSION_STRING_NUMBER "v0.6.0.this.is.pain"

#ifdef DEBUG
#define VERSION_STRING "CaféMP " VERSION_STRING_NUMBER " (Build: " __DATE__ " " __TIME__ ")"
#elif defined(LEGACY)
#define VERSION_STRING "CaféMP Legacy " VERSION_STRING_NUMBER
#else
#define VERSION_STRING "CaféMP " VERSION_STRING_NUMBER
#endif

#define TOOLTIP_BAR_HEIGHT 48

#endif
