# World of Illusion — PS Vita

An unofficial PS Vita port of **World of Illusion Starring Mickey Mouse and Donald Duck**
(Sega Mega Drive / Genesis) by Loki D.S.

The port uses the emulation core and enhancement module from
[Mickey & Donald Collection](https://github.com/Darhkfox/MickeyAndDonaldCollection):
widescreen patches, HUD re-anchoring and cheats. The game is not included — you
need your own ROM dump.

## Features

* **Widescreen:** Off, 16:10, 16:9 and 2:1. The width can be changed in the
  menu, including mid-level.
* **HUD anchoring:** the status bar and counters are pinned to the screen edges.
* **Cheats:** invincibility, infinite hearts, infinite tries, maximum hearts.
* **Saves:** one save-state slot (`saves/woi.state`).
* **Settings:** stored in `settings.ini` and kept across restarts.
* **LiveArea art:** custom icon, background and a startup screen with
  transparency.
* **Controls:** D-pad/stick, X, □, ○, Start and Select, mapped for the Vita.

## Requirements

* PS Vita with HENkaku/ensō.
* VitaShell to install the VPK.
* Your own game dump. The port targets the USA version:

  `World of Illusion Starring Mickey Mouse and Donald Duck (U) [!].bin`

  CRC32 `921EBD1C`, size 1,048,576 bytes (1 MB). The ROM can be kept in a `.zip`.

## Installation

1. Install `WorldOfIllusion.vpk` with VitaShell.
2. Put the ROM in `ux0:data/MickeyAndDonald/roms/`.

   It is also searched for in `ux0:data/MickeyAndDonald/`, `app0:roms/` and
   `app0:`.
3. Launch “World of Illusion” from the LiveArea.

On first launch, `ux0:data/MickeyAndDonald/` is created (`settings.ini`,
`log.txt`; saves go to `saves/woi.state`).

## Controls

| Vita | Mega Drive | Action |
|---|---|---|
| D-pad / stick | D-pad | Move |
| **X** | C | Jump |
| **□** | B | Cape attack / magic |
| **△** | A | (unused in-game) |
| **○** | C | Jump (alternative) |
| **Start** | Start | In-game pause |
| **Select** | — | Port menu |

In menus: D-pad to select, **X** to confirm, **○** to go back/resume.

## Menu (Select)

* Resume / Save State / Load State / Reset Game — one save slot
  (`saves/woi.state`).
* Widescreen — Off, 16:10, 16:9, 2:1 (the game is not patched beyond 2:1).
* Cheats — Invincibility, Infinite Hearts, Infinite Tries, Maximum Hearts.
  Achievements are not part of this port, so cheats are available.
* Exit.

Settings are saved to `settings.ini` when changed and on exit.

## Limitations

* Single player only. Not ported: 2P, level select, achievements, MD+
  soundtracks, launcher, shaders and rewind.
* Wider than 2:1 is not supported by the widescreen patches.
* If you see frame drops or audio problems, include
  `ux0:data/MickeyAndDonald/log.txt`: it logs the fps, frame time (avg/max)
  and the minimum audio queue level every 5 seconds.

## Building

Requires [VitaSDK](https://vitasdk.org/) in `C:\vitasdk`:

```powershell
$env:VITASDK="C:\vitasdk"; $env:PATH="C:\vitasdk\bin;$env:PATH"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_TOOLCHAIN_FILE="C:/vitasdk/share/vita.toolchain.cmake"
cmake --build build
```

The package is written to `build/WorldOfIllusion.vpk` (Title ID `WOLI00001`).

LiveArea images: `icon0.png` (128×128) and `bg.png` (840×500) are 8-bit
indexed PNGs (color type 3, no alpha); `startup.png` (280×158) is RGBA
(color type 6) with transparency.

The emulation core and patches come from Mickey & Donald Collection (see
`LICENSE`).

## Credits

* **PS Vita port:** Loki D.S.
* [Darhkfox / Mickey & Donald Collection](https://github.com/Darhkfox/MickeyAndDonaldCollection) —
  emulation core, widescreen patches and the enhancement module.
* © Disney. © Sega. Unofficial fan project, not affiliated with Sega or Disney.
  No game code or assets are included.
