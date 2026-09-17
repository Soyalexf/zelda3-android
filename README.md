# zelda3-android — enhanced port

A fork of [Waterdish/zelda3-android](https://github.com/Waterdish/zelda3-android),
which in turn builds on [snesrev/zelda3](https://github.com/snesrev/zelda3), the
C reimplementation of *A Link to the Past*.

No ROM or extracted asset data is distributed here. You need your own legally
obtained copy of *The Legend of Zelda: A Link to the Past (USA)*.

## Installing

1. Download the APK from [Releases](https://github.com/Soyalexf/zelda3-android/releases) and install it.
2. Open it. It asks for your ROM and opens the system file picker.
3. Pick your own legally obtained **A Link to the Past (USA)** ROM. Done.

The app copies the ROM into its own folder and builds the game data from it. No
PC, no cable, no Python, and no fighting with `Android/data/`.

## What this fork adds

**Locked widescreen camera** (`WidescreenEdgeMode = 1`)
In the original widescreen view, black bars appeared on reaching the edge of an
area, because the game clips the extended view to the area bounds. Here the
camera locks inside those bounds instead: the picture holds still and Link keeps
moving within the frame. No black bars, and no tilemap wrapping either.
The approach comes from EstebanPdN's 3DS port; see `docs/`.

**On-screen settings menu** (right stick / `R3`, or `F11`)
Configurable without leaving the game and without touching files. Includes quick
save and load, and writes back to the `.ini` preserving its comments and
ordering. This matters on Android: since version 11, `Android/data/` is
restricted and editing the config by hand needs a file manager with special
permissions. The menu is available in English and Spanish.

**Touch controls**
The original port had none: without a physical gamepad the app was unplayable.
By default (`TouchControls = 0`) they appear only when no gamepad is detected,
so a handheld with physical controls stays uncluttered. The directional control
can be a fixed d-pad or a **floating stick** that spawns wherever you put your
finger down (`TouchStick = 1`). Turbo gets its own button, which latches instead
of forcing you to hold a finger on it.

Everything is configurable from the menu: turning the buttons on or off,
choosing d-pad or stick, and a **layout mode** for dragging each button wherever
you want it. Positions are saved to `touch_layout.txt`, and there is an option
to restore the defaults.

**On-device setup, with a ROM picker**
On first run the app asks for your ROM through the system file picker, copies it
into place itself, and builds `zelda3_assets.dat` from it using a bundled 14 KB
BPS patch. This matters more than it sounds: the game reads its files from
`Android/data/`, which since Android 11 the system file manager cannot browse,
so previously getting a ROM in there meant a PC and a USB cable. Files with a
512-byte copier header (common in `.smc` dumps) are handled automatically.

**Configurable turbo speed**
`TurboSpeed = 0` keeps the original behaviour (uncapped). From 2 to 5 you get an
exact multiple, paced by vsync.

**A real message when data is missing**
Previously the app called `Die()`, which on Android becomes a `SIGABRT`: it
vanished without explanation and the reason stayed buried in logcat. Now it
shows a screen naming the problem and the exact folder to copy the ROM into.

## Menu options

Open with the right stick (`R3`), `F11`, or the on-screen `MENU` button. It is
fully usable by touch: tapping a row selects it and advances it, and choices
cycle around, so every value is reachable without a gamepad.

| Option | What it does |
|---|---|
| `LANGUAGE` | Menu language, English or Spanish. Does not affect the game's own text, which comes from the assets file. |
| `LOCKED CAMERA` | The widescreen camera lock described above. Off restores the original behaviour, black bars included. |
| `TURBO SPEED` | `ORIGINAL` is uncapped fast-forward; `X2`–`X5` are exact multiples paced by vsync. |
| `TOUCH BUTTONS` | `AUTO` shows them only when no gamepad is connected; `ON` and `OFF` force it. |
| `D-PAD STYLE` | Fixed d-pad, or a floating stick that spawns under your finger. |
| `MOVE BUTTONS` | Enters layout mode to drag the buttons around. Toggling it again leaves. |
| `RESET LAYOUT` | Restores the factory button positions. |
| `SAVE STATE` / `LOAD STATE` | Quick save and load, slot 0. |

The rest are gameplay options carried over from the upstream zelda3 engine. They
are all off by default, and all change how the original game behaves:

| Option | What it does |
|---|---|
| `ITEMS ON L/R` | Cycle items with the shoulder buttons instead of opening the menu. Also lets you reorder items in the inventory with Y + a direction, and assign items to X, L or R by holding that button in the item screen. If X is reassigned, Select opens the map. |
| `TURN WHILE DASHING` | Lets you steer while dashing with the Pegasus Boots, instead of being locked into a straight line. |
| `MORE BOMBS` | Allows up to four bombs active at once instead of two. |
| `MORE RUPEES` | Raises the rupee cap from 999 to 9999. |
| `BREAK POTS` | The level 2–4 sword can break pots, which normally requires lifting them. |
| `BUG FIXES` | Enables a set of fixes for bugs present in the original game. |

`SAVE CONFIG` writes the changes to the `.ini`, preserving its comments and
ordering. Gameplay options apply immediately but are not persisted, since they
live in the engine's feature flags.

## Building

Needs JDK 11, the Android SDK with **NDK r21e** (the last one supporting
`APP_PLATFORM=android-16`) and **build-tools 30.0.2** (the version AGP 7.0.3
requires).

```sh
export JAVA_HOME=/path/to/jdk-11  ANDROID_HOME=/path/to/android-sdk
./gradlew assembleDebug
```

## Credits

This project would not exist without other people's work. Everything here rests
on:

**[snesrev/zelda3](https://github.com/snesrev/zelda3)** — the C reimplementation
of *A Link to the Past*, built from a decompilation of the original game. It is
the basis for absolutely everything: the engine, the PPU, the logic and the
data. Released under the MIT license. If this port is useful to you, the
underlying credit is theirs.

**[Waterdish/zelda3-android](https://github.com/Waterdish/zelda3-android)** — the
Android port this fork is built on, with all the SDL integration, packaging and
platform startup.

**[EstebanPdN/zelda-alttp-3ds](https://github.com/EstebanPdN/zelda-alttp-3ds)** —
the Nintendo 3DS port, which is where the **idea and the approach of the locked
camera** in widescreen mode come from. Solving the black bars at the edges
without ending up showing the tilemap wrap is their achievement; this port only
carries that approach over to Android.

## A note on development

Development of this fork was assisted by **Claude Code**.
