# CaféMP – Experimental Media Player for the Nintendo Wii U

## About

**CaféMP** is a lightweight, open-source media player for the Wii U, focused on basic media playback from the SD card. It supports most common video and audio formats, targeting up to **720p at 30 FPS**.

This is a **work-in-progress**—expect occasional crashes, stutters, or other instability. I'm not a specialist in FFmpeg, Wii U homebrew, or C++.

Made with ❤️ in 🇩🇪

> **Note:** 720p30 is the *goal*, not a guarantee. Playback performance depends on encoding complexity.

---

## Installation

1. [Download the latest release](https://github.com/whateveritwas/cafemp/releases/latest).
2. Extract the ZIP file to the **root of your SD card**.
3. Place your media files into:  
   `sd:/wiiu/apps/cafemp/`

**or**

<p align="left">
  <a href="https://hb-app.store/wiiu/cafmediaplayer">
    <img src="branding/hbasbadge-wiiu.png" alt="Get it on the Homebrew App Store!" width="25%">
  </a>
</p>

---

## Using CaféMP

1. Launch **CaféMP** from the Wii U main menu or Homebrew Launcher.
2. Select your desired media type from the sidebar.
3. Use the file browser to locate and select and play your media.

### Controls – Video Player

| Button | Action                |
|--------|-----------------------|
| `A`    | Play / Pause          |
| `B`    | Return to file browser|
| `X`    | Change audio track    |

### Controls – Audio Player

| Button      | Action                 |
|-------------|------------------------|
| `A`         | Play / Pause           |
| `B`         | Return to file browser |
| `D-Pad L/R` | Skip / Rewind          |

### Controls – Photo Viewer

| Button           | Action                     |
|------------------|----------------------------|
| `B`              | Return to file browser     |
| `X`              | Change audio track         |
| `Left Stick L/R` | Show next / previous photo |
| `ZR / RL`        | Zoom in / Zoom out         |
| `Touch`          | Pan                        |

---

## Compatibility Tips

For best results, re-encode your videos using this FFmpeg command:

```bash
ffmpeg -i <input> \
-map 0 \
-c:v libx264 -profile:v baseline -level 3.1 -pix_fmt yuv420p \
-preset ultrafast -tune fastdecode -crf 23 -vf "scale=-2:480" \
-c:a aac -b:a 256k \
-c:s copy \
<output>
````

---

## Features

* Video playback (common formats, up to 720p)
* Audio playback (common formats)
* Image viewer (common formats)
* PDF / EBook viewer (pdf, epub)

---

## Planned Features

* Skip/Rewind support for video
* DLNA / Jellyfin streaming
* USB drive support (ext4, exFAT)
* Audio visualizations
* Playlist support (M3U)
* Wiimote / Pro Controller input
* YouTube (via Invidious) playback

---

## Known Issues

* **Audio/Video Desync**
  Playback may fall out of sync, especially with high-resolution or complex video files. Re-encoding with the recommended FFmpeg settings may help.

* **GamePad Required**
  Currently, the app cannot be used without the Wii U GamePad. Other input methods like Pro Controller or Wiimote are not supported yet.

* **Limited MKV Support**
  Not all `.mkv` files will work. Only those encoded with **H.264 video** and compatible audio formats are expected to play properly.

---

## Credits

* **Ambiance Music**: [LightMister on Freesound](https://freesound.org/people/LightMister/sounds/769925/)
* **devkitPro**: [GitHub](https://github.com/devkitPro)
* **stdout implementation by dkosmari**: [Github](https://github.com/dkosmari/devkitpro-autoconf/blob/main/examples/wiiu/sdl2-swkbd/src/stdout.cpp)
* **srtparser.h**: [Github](https://github.com/saurabhshri/simple-yet-powerful-srt-subtitle-parser-cpp)
* **FFmpeg**: [GitHub](https://github.com/FFmpeg/FFmpeg/)
* **FFmpeg Wii U Configure Script by GaryOderNichts**: [Github](https://github.com/GaryOderNichts/FFmpeg-wiiu/blob/master/configure-wiiu)
* **Nuklear GUI Library**: [GitHub](https://github.com/Immediate-Mode-UI/Nuklear)
* **Wii U Toolchain (WUT)**: [GitHub](https://github.com/devkitPro/wut)
* **mupdf port by hito16**: [GitHub](https://github.com/hito16/mupdf-devkitppc)
* **Helper files from hito16** [Github](https://github.com/hito16/SDLReader/blob/main/ports/wiiu/wiiu_mupdf_hb_wrappers.c) [Github](https://github.com/hito16/SDLReader/blob/main/ports/wiiu/wiiu_time_utils.c)
