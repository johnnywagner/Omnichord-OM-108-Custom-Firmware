# Development notes: modifying the OM-108 firmware

Everything learned while reverse engineering the **Suzuki Omnichord OM-108** firmware and building this modification,
so that further work can start from here instead of from scratch. Written for anyone with embedded background (i.MX RT,
Cortex-M, Thumb-2), and equally usable as context for an AI coding assistant.

## What exists
- **The Custom Firmware Tool** (repo root, `om108_gui.py` + `assets/om108_patcher.py`) reads the unit, saves its stock
  firmware, and installs / restores firmware over USB. Its recipe `assets/om108_mod.patch` holds this project's bytes only.
- **The modification's source** (this folder): `mod2.c` + `entry.s` + `mod1.s` / `mod1_syms.s` (+ `link.ld`, `build.sh`:
  clang / LLVM 18, Cortex-M7). Stock addresses are written `FL(0x6000xxxx)` / `RA(0x2000xxxx)` as 1.0.2.9 addresses;
  `stock.py` (addresses only) maps them to each factory version, and `build.sh` builds one binary per version.
- **Raw notes from the reverse engineering** (`notes/`): hardware and flashing, the stock firmware map (addresses,
  tasks, variables, the sound engine, MIDI), the touch chip, this mod's architecture, lessons learned, and the flash
  tables. The notes were written during development:
  some paths and file names in them refer to the original dev setup, which isn't included.
- **Ghidra scripts** (`ghidra/`) used to decompile the stock firmware. The decompile itself isn't included (it's
  Suzuki's code); rebuild it from your own unit's firmware. Paths inside the scripts are from the original setup.

## What you need first
1. **Your unit's stock firmware.** Run the tool's **Check/Dump Firmware** on a stock unit; it saves
   `my_unit/backups/om108_backup_v29.bin` (factory firmware 1.0.2.9) or `om108_backup_v30.bin` (1.0.3.0). The two
   supported versions are listed by hash in `assets/om108_patcher.py` (`KNOWN_STOCK`); any other is unsupported until
   verified.
2. **Reference images**, rebuilt from that file and the recipe (run from the repo root):
   ```python
   import sys; sys.path.insert(0, 'assets'); import om108_patcher as P
   stock = open('my_unit/backups/om108_backup_v29.bin', 'rb').read()   # or _v30
   rec = P.load_recipe('assets/om108_mod.patch', stock)              # the variant for this factory version
   suggested = P.build_image(stock, rec, P.default_settings())   # what Install writes with the default settings
   for name, img in P.legacy_images(stock, rec): print(name)      # earlier releases the tool recognises (1.0.2.9)
   ```

## How the modification works (short)
- MCU NXP i.MX RT1021 (Cortex-M7), firmware runs in place from a 16 MB QSPI flash at `0x60000000`. No signatures or
  checksums; the boot ROM's USB download mode (rear PLAY/UPDATE switch) always works, so a bad flash is recoverable.
- Stock code is changed only at small **patch sites**: a `bl` to a fixed **entry table** at `0x6003A000` (`entry.s`),
  which leads into `mod2.c`. The code lives in `0x6003A000`-`0x60046FFF`; `0x60047000` is the settings block the tool
  writes (read by `mod2.c`, see "settings block").
- `mod2.c` has no data / bss: every RAM variable is an absolute address in a stock buffer known to be free
  (`notes/firmware_notes.md`, "RAM we use"); stock functions are called through absolute addresses.
- Stock is FreeRTOS: MIDI, touch, control (buttons, strum plate) and LED tasks. MIDI may only be posted from the
  control task.

## Rules (learned the hard way; keep them)
1. Never modify or lose the unit's stock firmware file. Keep a second copy.
2. Never erase or write `0x60FFE000` (this mod's records) or `0x60FFF000` (stock settings). Never write the whole chip.
3. Erase in pieces of at most `0x8000`: the flashloader silently skips 64 KB block erases.
4. Read back and hash-check every write; stop at the first failure. Make every install resumable if it is cut off.
5. Order: data first, then code (entry table last), then the stock patch sites; roll back in reverse.
6. Change as little stock behaviour as possible, and test before flashing: we ran the real stock code in an emulator
   (unicorn) with the modification on top.
7. The strum plate's touch sensor is fragile: don't change its global chip settings, and avoid opening the unit.

## Getting a change onto the unit
The tool installs only what its recipe describes, so a changed firmware needs either a new recipe (format: see
`load_recipe()` / `build_image()` in `assets/om108_patcher.py`) or a direct flash with NXP's `blhost`, following the
rules above (`notes/firmware_notes.md`, "Hardware and flashing"). Before installing a self-made recipe over a unit that
already runs a tool-installed firmware, restore stock with the tool version that installed it (or hold **F** while
clicking Restore Stock Firmware: the Full Factory Restore). Always have a tested way back to stock first, and never write to someone's instrument without their OK.
