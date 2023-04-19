# ChimeraSNES

A Super Nintendo emulator core using the libretro API.

Based on multiple snes9x forks, primarily uosnes and the snes9x2005 core.

## Recommended changes to Retroarch settings

For better performance, go to settings and change these values:
- Video > Threaded Video = ON (default is OFF)
- Audio > Output > Audio Latency (ms) = 128 (default is 64)
- Audio > Resampler > Audio Resampler = sinc (this is the default)
- Audio > Resampler > Resampler Quality = Lowest (default is Lower)

## Building the core for PS Vita

The easiest way to build the core for vita is to download the [libretro-super](https://github.com/libretro/libretro-super) repo, copy `build-chimerasnes-vita.sh` to your copy of the repo and then run it from that directory.

You will need to have vitasdk and p7zip installed.

This will generate the vita2d and piglet versions of the core and compress them to chimerasnes.zip.

## Support me:

[![liberapay](https://liberapay.com/assets/widgets/donate.svg)](https://liberapay.com/jamsilva/donate)
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/M4M7KJV70)
