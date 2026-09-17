# Technical notes

## Why the camera is locked instead of preloading neighbouring screens

The obvious idea for the black bars at area edges is to preload the surrounding
areas. It does not work, and the reason is not device performance:

- The PPU tilemap is **512x512 with wrap-around** (`wx & 511`). The map buffer
  `dung_bg2` is 64x64 entries and the game indexes it by **masking**, not
  bounds-checking (`overworld_offset_mask_x/y`, which are `0x3f0` for a large
  area and `0x1f0` for a small one).
- Consequence: looking past the edge of an area makes **both the VRAM and the
  map buffer wrap around and return the opposite side of that same area**. That
  is what shows up as "the left edge appearing on the right". The neighbouring
  area's data is simply not in memory.
- Forcing the full extension removes the black bars but shows that wrap instead.
  Measured with instrumentation: up to **80% of frames** had regions with no
  real data behind them.
- Repairing VRAM from `dung_bg2` only fixes columns the streamer recycled; it
  cannot invent the neighbouring area.

Hence the locked camera. In indoor rooms 256 px wide the edges stay black by
design: there is genuinely nothing to show, and `WideCamera_ClampToBounds`
returns the camera untouched when the area is narrower than two margins.

Note also that adjacent overworld areas can use a different tileset and palette,
so even a larger map buffer would not be enough on its own.

## Traps worth remembering

- **The port does not use the current directory.** Every file is opened through
  `SDL_RWFromFileInExternal()`, which prefixes `SDL_AndroidGetExternalStoragePath()`.
  A relative `fopen("zelda3.ini")` fails silently.
- **Names in `variables.h` are macros onto RAM.** Declaring a local variable
  called `frame_counter` breaks the build with baffling errors.
- **`F10` was already taken** by `Load` (save states use F1-F10).
- **Android 13 interposes a permission review screen** when installing a debug
  build; the game will not start until CONTINUE is tapped.
- **`adb shell input tap` is useless for testing touch input**: the coordinates
  it delivers do not match what SDL receives. Test with a finger.
- **`HandleCommand` discards input while the menu is open**, so any menu action
  must close the menu *before* running.
- **Watch the compiler warnings.** Passing an array of strings through a
  `const char *` helper is only a warning in C, and it produced a garbage
  pointer that crashed the menu.

## Build environment

- NDK **r21e** — the last release supporting `APP_PLATFORM=android-16`, which
  this project declares. r24 and later raise the minimum to API 19.
- build-tools **30.0.2** — AGP 7.0.3 asks for this exact version; 31 will not do.
- JDK **11** for Gradle/AGP.
