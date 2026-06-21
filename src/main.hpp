#ifndef MAIN_HPP
#define MAIN_HPP

#ifdef LEGACY
#define PLATFORM_WIIU_LEGACY
#else
#define PLATFORM_WIIU
#endif

#ifdef PLATFORM_WIIU

#define BASE_PATH        "/vol/external01/wiiu/apps/cafemp/"
#define CONTENT_PATH     "/vol/content/"

#define FONT_PATH        CONTENT_PATH "Roboto-Regular.ttf"
#define AMBIANCE_PATH    CONTENT_PATH "cafebgdemo.mp3"

#define MEDIA_PATH_AUDIO  "/vol/external01/wiiu/apps/cafemp/Audio/"
#define MEDIA_PATH_VIDEO  "/vol/external01/wiiu/apps/cafemp/Video/"
#define MEDIA_PATH_PHOTO  "/vol/external01/wiiu/apps/cafemp/Photo/"
#define MEDIA_PATH_PDF    "/vol/external01/wiiu/apps/cafemp/Library/"

#define SETTINGS_PATH     "/vol/external01/wiiu/apps/cafemp/settings.json"

#endif

#ifdef PLATFORM_WIIU_LEGACY

#define BASE_PATH        "/vol/external01/wiiu/apps/cafemp/"
#define CONTENT_PATH     "/vol/external01/wiiu/apps/cafemp/content/"

#define FONT_PATH        CONTENT_PATH "Roboto-Regular.ttf"
#define AMBIANCE_PATH    CONTENT_PATH "cafebgdemo.mp3"

#define MEDIA_PATH_AUDIO  BASE_PATH "Audio/"
#define MEDIA_PATH_VIDEO  BASE_PATH "Video/"
#define MEDIA_PATH_PHOTO  BASE_PATH "Photo/"
#define MEDIA_PATH_PDF    BASE_PATH "Library/"

#define SETTINGS_PATH     BASE_PATH "settings.json"

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
