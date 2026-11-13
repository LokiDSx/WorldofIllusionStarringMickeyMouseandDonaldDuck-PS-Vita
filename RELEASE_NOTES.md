An unofficial PS Vita port of **World of Illusion Starring Mickey Mouse and Donald Duck** (Sega Mega Drive / Genesis) by Loki D.S.

The game runs on a built-in Mega Drive emulation core with widescreen patches: the view is genuinely widened — enemies, items and the HUD are all included — rather than stretched. The game is not included; you need your own ROM dump.

## What's new

First public release.

## Features

* **Widescreen:** Off, 16:10, 16:9 and 2:1. The width can be changed in the menu, including mid-level.
* **HUD anchoring:** the status bar and counters are pinned to the screen edges.
* **Cheats:** invincibility, infinite hearts, infinite tries, maximum hearts.
* **Saves:** one save-state slot.
* **Settings:** stored in `settings.ini` and kept across restarts.
* **LiveArea art:** custom icon, background and a startup screen with transparency.
* **Controls:** D-pad/stick, X, □, ○, Start and Select, mapped for the Vita.

## Installation

1. Install `WorldOfIllusion.vpk` with VitaShell (HENkaku/ensō required).
2. Put the ROM in:

   `ux0:data/MickeyAndDonald/roms/`

   It is also searched for in `ux0:data/MickeyAndDonald/`, `app0:roms/` and `app0:`. The ROM can be kept in a `.zip`.
3. Launch “World of Illusion” from the LiveArea.

On first launch, `ux0:data/MickeyAndDonald/` is created (`settings.ini`, `log.txt`; saves go to `saves/woi.state`).

**You need your own game dump.** The port targets the USA version:

`World of Illusion Starring Mickey Mouse and Donald Duck (U) [!].bin`

CRC32 `921EBD1C`, size 1,048,576 bytes (1 MB).

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

* Resume / Save State / Load State / Reset Game — one save slot (`saves/woi.state`).
* Widescreen — Off, 16:10, 16:9, 2:1.
* Cheats — Invincibility, Infinite Hearts, Infinite Tries, Maximum Hearts.
* Exit.

Settings are saved when changed and on exit.

## Limitations

* Single player only.
* Not ported: 2P, level select, achievements, MD+ soundtracks, launcher, shaders and rewind.
* Wider than 2:1 is not supported by the widescreen patches.
* If you see frame drops or audio problems, include `ux0:data/MickeyAndDonald/log.txt`: it logs the fps, frame time (avg/max) and the minimum audio queue level every 5 seconds.

## Credits

* **PS Vita port:** Loki D.S.
* [Darhkfox / Mickey & Donald Collection](https://github.com/Darhkfox/MickeyAndDonaldCollection) — emulation core, widescreen patches and the enhancement module.
* The port's own code is released under the MIT licence (see `LICENSE`).
* © Disney. © Sega. Unofficial fan project, not affiliated with Sega or Disney. No game code or assets are included.

## File

* `WorldOfIllusion.vpk` — Title ID `WOLI00001`.
* Size: 1,465,493 bytes.
* SHA-256: `10b37942044ee6d8773ceef709ebe8b1947333cb27acf2a1f99bf81a854de17a`.
