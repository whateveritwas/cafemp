# Cafemp – Experimental Media Player for the Nintendo Wii U

## About

Cafemp is a lightweight, open-source media player for the Wii U, focused on basic media playback from the SD card. It's a work-in-progress and may be unstable. It supports most common video and audio formats at resolutions up to 720p at 30 FPS.

> ⚠️ **Note:** 720p at 30 FPS is the *target*, but not guaranteed. Playback may stutter or desync depending on video complexity and encoding.

## Installation

1. [Download the latest release](https://github.com/whateveritwas/cafemp/releases/latest).
2. Extract the ZIP file to the **root of your SD card**.
3. Copy your media files into: `sd:/wiiu/apps/cafemp/`

## Running Cafemp

1. Launch Cafemp from the **Wii U Homebrew Launcher**.
2. Use the file browser to select your media file.
3. Controls:

   * `A` → Play/Pause
   * `B` → Return to file browser

## ⚙️ Compatibility Notice

For best results, re-encode video files with FFmpeg using the following command:

```bash
ffmpeg -i <input> \
-map 0 \
-c:v libx264 -profile:v baseline -level 3.1 -pix_fmt yuv420p -preset ultrafast -tune fastdecode -crf 23 -vf "scale=-2:480" \
-c:a aac -b:a 128k \
-c:s copy \
<output>
```

## ✅ Features

* 💼 Video playback (common formats, ≤720p)
* 🎵 Audio playback
* ✋ Touch input via GamePad

## 🛠️ Planned Features

* ⏩ Skip/Rewind support
* 🌐 DLNA / Jellyfin streaming
* 📀 USB drive support (ext4, exFAT)
* 📊 Audio visualization
* 📺 IPTV (`.m3u`) playlist support
* 🎧 Miniplayer for audio
* 🎮 Wiimote / Pro Controller input
* ▶️ YouTube video playback

## ⚠️ Known Issues

* ❗ **Video/audio desync** with high-res files
* ❗ **App may crash** when exiting
* ❗ **Unstable behavior** due to experimental state

## Credits

* **Ambiance Music**: [Link](https://freesound.org/people/LightMister/sounds/769925/?)
* **devkitPro**: [Link](https://github.com/devkitPro)
* **dkosmari stdout implementation**: [Link](https://github.com/dkosmari/devkitpro-autoconf/blob/main/examples/wiiu/sdl2-swkbd/src/stdout.cpp)
* **FFmpeg**: [Link](https://github.com/FFmpeg/FFmpeg/)
* **GaryOderNichts FFmpeg Configure Script**: [Link](https://github.com/GaryOderNichts/FFmpeg-wiiu/blob/master/configure-wiiu)
* **Nuklear Immediate-Mode GUI**: [Link](https://github.com/Immediate-Mode-UI/Nuklear)
* **WiiU Toolchain**: [Link](https://github.com/devkitPro/wut)
