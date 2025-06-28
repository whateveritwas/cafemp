# CaféMP – Experimental Media Player for the Nintendo Wii U

## About

CaféMP is a lightweight, open-source media player for the Wii U, focused on basic media playback from the SD card. It's a work-in-progress and may be unstable. It supports most common video and audio formats at resolutions up to 720p at 30 FPS.

Made with ❤️ in 🇩🇪

> ⚠️ **Note:** 720p at 30 FPS is the *target*, but not guaranteed. Playback may stutter or desync depending on video complexity and encoding.  

## Installation

1. [Download the latest release](https://github.com/whateveritwas/cafemp/releases/latest).
2. Extract the ZIP file to the **root of your SD card**.
3. Copy your media files into: `sd:/wiiu/apps/cafemp/`

**or**

<p align="left">
  <a href="https://hb-app.store/wiiu/cafmediaplayer">
    <img src="branding/hbasbadge-wiiu.png" alt="Get it on the Homebrew App Store!" width="50%" height="50%">
  </a>
</p>

## Using CaféMP

1. Launch CaféMP from the **Wii U Main Menu**.
2. Select wnated media type from the side bar
3. Use the file browser to select your media file.
3. Controls:

   * `A` → Play/Pause
   * `B` → Return to file browser
   * `DPAD L/R` → Skip / Rewind (Audio only)

## ⚙️ Compatibility Notice

For best results, re-encode video files with FFmpeg using the following command:

```bash
ffmpeg -i <input> \
-map 0 \
-c:v libx264 -profile:v baseline -level 3.1 -pix_fmt yuv420p -preset ultrafast -tune fastdecode -crf 23 -vf "scale=-2:480" \
-c:a aac -b:a 256k \
-c:s copy \
<output>
```

## ✅ Features

* 💼 Video playback (common formats, ≤720p)
* 🎵 Audio playback
* 🖼️ Photo viewer (common formats, no animated gifs yet)
* ✋ Touch input via GamePad

## 🛠️ Planned Features

* ⏩ Skip/Rewind support (video)
* 🌐 DLNA / Jellyfin streaming
* 📀 USB drive support (ext4, exFAT)
* 📊 Audio visualization
* 📺 Playlist support (m3u)
* 🎧 Miniplayer for audio
* 🎮 Wiimote / Pro Controller input
* ▶️ YouTube(`invidious`) video playback

## ⚠️ Known Issues

* ❗ **Video/audio desync** with high-res files
* ❗ **App may crash** when exiting
* ❗ **Unstable behavior** due to experimental state
* ❗ **Not usable without gamepad** 
* ❗ **MKV** not all mkv files will work only h264

## Credits

* **Ambiance Music**: [Link](https://freesound.org/people/LightMister/sounds/769925/?)
* **devkitPro**: [Link](https://github.com/devkitPro)
* **dkosmari stdout implementation**: [Link](https://github.com/dkosmari/devkitpro-autoconf/blob/main/examples/wiiu/sdl2-swkbd/src/stdout.cpp)
* **FFmpeg**: [Link](https://github.com/FFmpeg/FFmpeg/)
* **GaryOderNichts FFmpeg Configure Script**: [Link](https://github.com/GaryOderNichts/FFmpeg-wiiu/blob/master/configure-wiiu)
* **Nuklear Immediate-Mode GUI**: [Link](https://github.com/Immediate-Mode-UI/Nuklear)
* **WiiU Toolchain**: [Link](https://github.com/devkitPro/wut)
