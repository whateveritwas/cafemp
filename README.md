# Cafemp: Experimental Media Player for the Wii U

## About
**Cafemp** is an **experimental** media player designed for the **Wii U**. It supports most common video and audio formats at resolutions up to 720p at 30 FPS. However, be aware that not all videos will run smoothly, and there are some limitations due to the experimental nature of the software. Higher resolutions may cause issues like video and audio desynchronization.

### Current Resolution Target:
- **720p at 30 FPS** (not guaranteed to work smoothly with all videos)

## Installation & Usage

### Steps:
1. **Download** the latest release from [here](https://github.com/whateveritwas/cafemp/releases/latest).
2. **Extract** the contents of the zip file to the **root of your SD card**.
3. **Place your media files** in the `cafemp` folder located at `sd:/wiiu/apps/cafemp`.
4. On your **Wii U**, open **Cafemp** from the Home Menu.
5. Navigate through the file browser to select a media file.
6. Press `A` to **play/pause** the media.
7. Press `B` to **return** to the file browser.

---

## Features

### Currently Supported:
- **Video playback**: Most common video formats at lower than 720p resolution.
- **Audio playback**: Most common audio formats.
- **Touch input**: Interact with the app via the Wii U GamePad's touch screen.
  
### Planned Features:
- [ ] Skip/Rewind for audio and video playback
- [ ] Support for network media (DLNA, Jellyfin)
- [ ] USB flash drive media playback (ext4/exFAT)
- [ ] Audio visualization (for better audio feedback)
- [ ] m3u IPTV support
- [ ] Miniplayer for audio
- [ ] Wiimote / Pro Controller input support

---

## Known Issues:
- **Video sync problems**: Higher resolutions may cause audio and video to become out of sync.
- **Crashes**: The app may crash when exiting.
- **General instability**: As an experimental project, the app is still under development and may encounter various bugs.

---

## Credits
- **Ambiance Music**: [Link](https://freesound.org/people/LightMister/sounds/769925/?)
- **devkitPro**: [Link](https://github.com/devkitPro)
- **dkosmari stdout implementation**: [Link](https://github.com/dkosmari/devkitpro-autoconf/blob/main/examples/wiiu/sdl2-swkbd/src/stdout.cpp)
- **FFmpeg**: [Link](https://github.com/FFmpeg/FFmpeg/)
- **GaryOderNichts FFmpeg Configure Script**: [Link](https://github.com/GaryOderNichts/FFmpeg-wiiu/blob/master/configure-wiiu)
- **Nuklear Immediate-Mode GUI**: [Link](https://github.com/Immediate-Mode-UI/Nuklear)
- **WiiU Toolchain**: [Link](https://github.com/devkitPro/wut)