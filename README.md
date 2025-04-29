# cafemp
## About
Cafemp is a **work in progress** media player for the Wii U.

## Usage
- [Download the latest release](https://github.com/whateveritwas/cafemp/releases/latest)
- Extract the zip to the root of your SD card
- Add your own movies/videos to the `cafemp` folder under `sd:/wiiu/apps/cafemp`
- Open the app on the Wii U home menu
- Launch a file from the file browser
- Press `A` to play/pause
- Press `B` to return to the file browser
- `[DPAD L/R]` skip/rewind 5 seconds

## Planned features
- [x] Video player (most common video formats 720p@30)
- [ ] Playing media from network (DLNA, Jellyfin)
- [ ] Playing media from USB flash drive (ext4/exFAT)
- [x] Audio player (most common audio formats)
- [ ] Audio visualization
- [ ] m3u IPTV
- [ ] Tooltips
- [ ] Miniplayer (Audio)
- [ ] Touch input

## Credits
- [Ambiance music](https://freesound.org/people/LightMister/sounds/769925/?)
- [devkitPro](https://github.com/devkitPro)
- [dkosmari stdout implementation](https://github.com/dkosmari/devkitpro-autoconf/blob/main/examples/wiiu/sdl2-swkbd/src/stdout.cpp)
- [exfat](https://github.com/relan/exfat/)
- [FFmpeg](https://github.com/FFmpeg/FFmpeg/)
- [GaryOderNichts FFmpeg configure script](https://github.com/GaryOderNichts/FFmpeg-wiiu/blob/master/configure-wiiu)
- [libiosuhax](https://github.com/dimok789/libiosuhax)
- [Nuklear immediate-mode GUI](https://github.com/Immediate-Mode-UI/Nuklear)
- [WiiU toolchain](https://github.com/devkitPro/wut)