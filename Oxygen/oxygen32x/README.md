# oxygen32x

Sega 32X support for the Oxygen engine (work in progress).

Goal: run Knuckles' Chaotix on Oxygen, keeping Oxygen's strengths: widescreen, mods and improved rendering.

## Contents

* `src/cpu/m68k`, `src/cpu/sh2`, `src/cpu/z80`: CPU cores (decoders, shared instruction semantics, reference interpreters).
* `tests/`: unit tests for the 68000 and SH-2 cores. They need no ROM.

## Provenance and license

The CPU cores and their tests come from [YuutaTsubasa/ChaotixRecompiled](https://github.com/YuutaTsubasa/ChaotixRecompiled), commit `09fcd6b82f3cb25f08a2b993db3593af473de058`, published under the MIT license (see `LICENSE-ChaotixRecompiled`). The files are unmodified copies. The rest of this repository is GPLv3 (see the top-level `LICENSE`).

No game data is included. Running Chaotix will require your own legally obtained ROM.

## Build and test

```bash
cmake -S Oxygen/oxygen32x -B build32x -DCMAKE_BUILD_TYPE=Release
cmake --build build32x
ctest --test-dir build32x --output-on-failure
```

## Roadmap

1. CPU cores + tests (this directory).
2. 32X bus and memory map (32-bit address space, frame buffer, palette, COMM registers) and boot without BIOS.
3. VDP layer that maps Mega Drive VDP state onto Oxygen's render parts, plus a render item for the 32X frame buffer.
4. Audio: reuse Oxygen's YM2612 and SN76489, add Z80 and PWM.
5. Hooks from lemon scripts on interpreter addresses (mods, widescreen).

## Running it

- `oxygen32x_run rom.32x [frames] [dump.ppm]`: headless run, optional picture dump.
- `oxygen32x_player rom.32x`: SDL2 player (video, sound, keyboard / gamepad / touch).
  Keys: arrows, Z/X/C = A/B/C, A/S/D = X/Y/Z, Enter = Start, Tab = Mode, F11 = fullscreen.
- Android: `android/` is a Gradle project (SDL2 activity + ROM picker). No ROM is bundled;
  the app asks for your own `.32x`/`.bin`/`.zip`. The `oxygen32x Android APK` GitHub Actions
  workflow builds a debug APK (artifact `oxygen32x-debug-apk`).

Not included yet: widescreen, mods and enhanced renders (Oxygen integration, step 5).
