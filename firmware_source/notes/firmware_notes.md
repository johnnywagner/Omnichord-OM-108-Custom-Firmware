# Raw notes from the development (copied from the project handbook as written; some paths refer to the
# original dev setup, which is not included)

## 3. Hardware and flashing
- **MCU:** NXP **i.MX RT1021** (Cortex-M7, no internal flash).
- **Flash:** Winbond **W25Q128JV**, 16 MB QSPI NOR at `0x60000000` (XIP; the MPU makes all of it read-only and executable).
- **UPDATE mode always works:** the rear PLAY/UPDATE switch puts the unit in the boot ROM's serial download mode (mask ROM, so any mistake is recoverable). HAB is off, and there are no checksums or signatures anywhere.
- **USB:**
  - boot ROM `0x1fc9:0x0130` (`sdphost`);
  - NXP flashloader K2.8.0 in RAM `0x15a2:0x0073` (`blhost`);
  - both come from SPSDK, installed via Thonny.
- **Touch sensor:** onsemi **LC717A10PJ** on the strum plate PCB, on LPSPI3 (§5).
- **Analog voices:** chord (3 notes) + bass (1) = Quad Timer TMR1 (`0x401DC000`) ch 0–3 square waves into analog circuitry.
  - The Omni 1/2 main voice ("Mellow Pulse") gates the chord oscillators per plate through GPIO2.
  - Firmware can change only their pitch.
- **Flashing** (`make_package.py` generates all of it):
  - **Connect sequence:** `nxpdevscan`; `sdphost … write-file 0x20209000 assets/ivt_flashloader_rt1020.bin`; `jump-address 0x20209000 || true`; `blhost get-property 1` (→ K2.8.0); `write-memory 0x2000 assets/omnichord_rw_fcb.bin`; `configure-memory 9 0x2000`.
  - **Per region:** `flash-erase-region <addr> <len ≤ 0x8000> 9`, `write-memory`, `read-memory` → readback, then a hash check. Everything is chained with `&&`.
  - **The write FCB** (`omnichord_rw_fcb.bin`): 1-bit SPI at 30 MHz; 4 KB sector erase `0x20` and page program `0x02` only. Its block and chip erase are empty, hence rule 6.
  - **Package rules:**
    - a read-only pre-check: identity sectors must match, and each target sector must hold the installed, an accepted, the new or blank content, so a re-run after an interruption is safe;
    - order: data, then our code (entry table last), then stock patch sites; rollback is the exact reverse;
    - every write is read back and hash-checked.
  - **Files:** `ivt_flashloader_rt1020.bin`, `omnichord_rw_fcb.bin`, `omnichord_readonly_fcb.bin` (dumps only), `flashloader.srec`, `MIMXRT1020.json`.

## 4. Stock firmware map (what we rely on)
- **Decompile:** `ghidra/stock_decompiled.c` (Ghidra, literal pools resolved, 599 functions). Search it first.
  - Ghidra misses the mixer `0x6000DE3C`: use `lib/da.py ADDR N`.
  - Recipe: `ghidra/*.java` + `resolve_literals.py` (import the first `0x40000` bytes at `0x60000000`, add the RAM blocks from the init copies).
- **Flash:**

  | Range | Content |
  |---|---|
  | `0x000000` | FCB, IVT `0x1000`, vectors `0x2000` |
  | `0x002000`–`0x03881D` | program + RAM-init images |
  | `0x039000` | mod 1 |
  | `0x03A000`–`0x047FFF` | our code |
  | `0x0486A0` | percussion bank |
  | `0x148620` | strum bank |
  | `0xDA0000`–`0xFFE000` | mostly our samples |
  | `0xFFE000` | our records |
  | `0xFFF000` | stock settings |

  Details: `firmware_tables.md`.
- **RAM init copies** (table `0x60002278`):
  - DTCM `0x20000000` ← `0x60037E3C` (`0x9E4`, includes the voice table);
  - ITCM `0x0` ← `0x6003617C` (`0xA40`, flash-write routines);
  - OCRAM `0x20200000` ← `0x60036BBC` (`0x1280`, sound-engine hot code).
- **Veneers** `0x60018688`–`0x60018730` reach OCRAM / ITCM:
  - note-on `0x600186D8`;
  - all-off `0x600186C8`;
  - instrument loader `0x600186B8`;
  - drum hit `0x600186B0` (→ `0x20200344`);
  - Normal rhythm hit `0x60018728` (→ `0x20200488`);
  - master tune `0x600186E0`;
  - flash erase `0x60018720`, program `0x600186E8`.
- **RTOS:** FreeRTOS, 1 kHz tick `0x2000DB84`.
  - Tasks (priority): MIDI `0x6000EC10` (4), touch `0x6000FA1C` (3), control `0x6000E9AC` (2: power manager, button handler `0x6000C274`, strum scanner `0x60009B20`, battery check), LED / I2C `0x6000F5FC` (1, the lowest). A 1 ms software timer (`0x6000E95C`) gives each its binary semaphore. SPI / UART / I2C transfers are the RTOS drivers (they block, yielding). No watchdog (SystemInit disables them); no WFI anywhere.
  - The rhythm runs from TMR2 (200 µs).
- **Settings** `0x60FFF000` (7 bytes → `0x20000280`): MIDI preset, auto-off, factory test (keep 1), Volume CC, **rhythm tone (byte 4)**, battery, Decay CC. Saved by `0x600093B0`.
- **Power-on setup order:** two stock key scans (`0x6000C274`, KEYS_EN 0) → factory-reset check `0x6000B2C4` (→ `DE6B`) → load → `B204` presets → `B290` auto-off → `B2FC` → `B350` rhythm tone (our `rhythm_setup` → `restore_early`) → `B384` battery → save if changed (overwrites the page buffers `0x20002AD4`-`0x20002CD3`) → tasks start. Then: any setting flag (DE6B / DE62 / DE6F / DE66 / DE63) or DE2C already set → the "setting changed" blink (`0x60004EF0`, DE2C; LED phase DE2F, 21 phases ~2.1 s; restores the LED flags from `0x20000110` and sets KEYS_EN DEA2 at its end); else the startup animation (DE2A, `0x60004758`). (Keys 0 + 11 held, DE87: a stock test mode, no animation, `0x60004028` sets the default flags, the stock ignores panel presses.) Our power-on settings (slow rock/funk, the bass choice) set DE2C + a BOOTF bit and draw their LEDs in blink_after. Key-held flags are at `0x20006B90 + 7·key + 4`.
- **Power:**
  - power button = GPIO1 bit 10;
  - power is cut only by `GPIO1 DR_CLEAR = 0x8000`, at `0x600083B0` / `0x60008418`; both now go through our `latch_off`;
  - auto power-off after 30 min.
- **Panel:**
  - **Button handler `0x6000C274`:** scans the matrix itself (GPIO3 read at `0x6000C2F4`); key id = row·9 + col.
    - Per key, 7 bytes at `0x20006B90 + 7·id`: [0] raw, [3] released, [4] pressed.
    - Release needs 30 scans. Events are acted on only if `DEA2` ≠ 0.
    - The control task calls it only after `DE20` reaches 2000.
  - **Key ids:**
    - voice slots `0x24 0x25 0x26 0x33 0x34`;
    - VOICE bank `0x27`; patterns `0x28`–`0x2C`; PATTERN bank `0x2D`;
    - Start/Sync `0x2E`, Manual/Auto `0x2F`, Hold `0x30`, KEYBOARD `0x31`;
    - major row 0–11 (T 0, P 1, ↓ 2, ↑ 3), minor row 12–23 (Db minor 12), 7th row 24–35 (Gb 7th `0x35`), Gb minor `0x32`;
    - at power-on: 40 / 41 auto-off, 42 slow rock, 43 country, 44 swing.
  - **LED flags `0x200000F0 + i`:** 2–6 voice slot, 7/8 bank, 9–13 pattern, `0x0E`/`0x0F` pattern bank, `0x10` Hold, `0x11`/`0x12` Manual/Auto, `0x13`/`0x14` Start/Sync, **`0x15` keyboard mode** (use this as the keyboard test).
    - **LED output is queued (2.78 finding):** every LED write (`0x6000F518(port, ~bits)`, ports 4 / 5 / 6, shadows `0x20002A54`..56) goes into a 512-entry ring (`0x2000AB8C`, write index `0x2000AF8C`, read `0x2000AB88`) that the lowest-priority LED task sends to the I2C LED driver (address 0x23) **one write per millisecond**. Anything that writes LEDs repeatedly delays every LED change (the stock battery check did: 2.78 `bat_led` / `bat_row`).
    - **At power-on the panel's flags live in LEDSAVE `0x20000110`** (0x16 bytes, DTCM init = the stock defaults: voice 1, bank 1, pattern 1, Hold on, Auto, Sync): the live flags are zero until the startup animation / setting blink ends and copies LEDSAVE in (the stock button code reads Hold / Manual / Start from the flags).
    - **2.79 finding: right after that copy, both stock routines switched on the default flags one by one** (2, 7, 0x0E, 9, 0x12, 0x10, 0x13 off, 0x14), a no-op on a stock unit, but it lit the defaults over 2.78's restored panel. Both now `bl panel_shown` right after the copy and return (sites `0x600047AA`, `0x60004F70`).
    - The stock rhythm player reads the **Auto flag 0x12** (chord row; without it, it cuts the bass gate every step); the button code keeps Manual / Auto in `0x200009DA` (0 = Manual: held chord). Both set = the third state (2.79).
    - `0x60003CE4(i,on)` sets a flag; `0x60003C14` redraws from the flags.
    - `0x60003AD0(&s8)` shows −6..+6 on the pattern row; `0x60003984(&s8)` on the voice row.
  - **Voice-row displays need `DE78`:** the battery check `0x600081C0` redraws the voice row (`0x60003A5C`) many times a second, from `0x20000108` if `DE73`, else `0x2000010C` if `DE78`, else from the flags.
  - **Never set `DE73` from our code:** it turns keys 4–11 into P and can leave keyboard notes silent (`DE7C`).
  - **Variables:**
    - `DE88` voice bank, `DE89` voice (bank·5 + slot), `DE80` keyboard mode;
    - `DE84` / `DE82` pattern bank / pattern, `DE47` rhythm running;
    - `DE72` master tune, `DE79` transpose, `0x200009DC` octave;
    - `DE65` rhythm tone (runtime).
  - **Voice select:** `0x6000E7DC(bank·5 + kb·12 + slot)`; slot 12 = the tuning reference; slots 10 / 11 are unreachable.
- **Sound engine:**
  - **Format:** output 48 kHz; samples 16-bit mono at 44.1 kHz.
  - **Pitch step** = 2^((note·100 − zone_cents − 146 + tune)/1200). **Global sample tune** = s16 `0x20007606`, re-read for every melodic channel each block (how our vibrato works). Drums are fixed-step.
  - **Renderer `0x202005EC`:** 4-tap interpolation, no anti-aliasing (so band-limited octave copies). 78 melodic channels at `0x20007640` (stride `0x34`); drums use channels 78 + instrument.
  - **Mixer `0x6000DE3C`** saturates. Per-voice gain = envelope × layer volume/127, latched at note-on. The zone amp byte does nothing.
  - **Banks ("HZ0211Info", pointers at `0x200000E8`):** header B+0x20 = sample-table offset (relative to B+0x20), +0x24 = instrument count, +0x28+4i = record offset (relative to B+0x14).
    - Record = 16-byte name, int 1, zone count, then 28-byte zones: +4 end, +8 loop length, +0xC sample index, +0x10 s16 tune, +0x15 key high, +0x1B root.
    - Strum samples are stored as end+2 frames; percussion as end+1.
  - **Voice table:** RAM `0x200003AC` (init data at `0x600381E8`), 22 voices × 4 layers × 16 B; [1] instrument, [13] volume, [14] bus. Voices v / v+12 = normal / keyboard copy. Layer 3 = percussion: 0x30 / 0x32 / 0x33 / 0x3E are read for the selected voice.
- **MIDI:**
  - **Builders:** note-on `0x600096A0`, note-off `0x600096D4`, CC `0x60009704`. They post with `0x6000DB28`, which isn't thread-safe: only the control task may post. Hook the builders, never `DB28`.
  - **Internal channels:** 0 Main, 1 Chord, 2 Bass, 3 Sub, 9 Drum.
  - **Mod 1:** route table `0x60039100`, drum map `0x60039180`, internal preset forced to 3 at `0x6000F2D4`.
  - **Message:** {u8 type, u8 ch, u16 len, u8* data}; type → status table `0x60033460` (4 = CC, 7 = bend). Queue `0x2000681C`.
- **Rhythm engine and drum tables:** `NEXT_PHASE_rhythm_mode.md` §3 and `firmware_tables.md` §4.

## 5. Touch chip (LC717A10PJ), short
Full details: `touch_chip_LC717A10.md`. Measurements: `reference/plate_measurements.md`.
- **Bus:** LPSPI3, driven only by the stock touch task `0x6000FA1C` via `0x60012888(0x2000AFA8, xfer 0x2000AF98)`. Write = {reg, val}; read = {reg|0x80, 0}.
- **Readings:** about 250 cycles/s; levels `0x1A`–`0x28` → `0x2000AB78[0..14]`; touch masks `DEA0` / `DEA1`.
  - **Channel 14 = Instant OFF:** level `0x2000AB86`, `DEA1` bit 6, threshold reg `0x18`, gain = reg `0x09` low nibble.
- **Stock set-up `0x6000ED18`** (at boot and from the error recovery `0x6000F858`): halt, then registers 00–3D (02–09 = 0x77 = 8× gain, thresholds 20), then `0x2F = 0x8F` (load + static calibration).
- **Ours (`touch_hook`, effect modes only):** replay that set-up with `0x09 = 0xFF` (16×) and `0x18 = 40`.
  - It checks about once a second and re-applies only after two wrong reads, only while nothing is touched, with at most 3 failures.
  - Note Off mode replays the stock values the same way.
  - **The user does not want global chip settings changed** (they affect the strum plate).

---

## 6. Our modifications: architecture
### 6.1 Code and entry table
- **Source:** `mod2.c` (~1,900 lines of C) + `entry.s` (entry table + 5 asm wrappers).
- **Placement:** linked at `0x6003A000`; 26,696 bytes (2.86 + the settings firmware; ends `0x60040848`); must stay below
  `0x60047000` (the settings block; mkimg asserts it). (2.85 alone: 18,770 bytes.)
- **Mod 1** (MIDI routing) is separate, at `0x60039000`: assembled, no source; its tables are documented in `firmware_tables.md` §2.
- **Build:** clang `-Os`, thumbv7em, soft-float, **no data / bss**.
  - Every RAM variable is an absolute address (§6.3). Stock functions are called through absolute pointers (addr | 1).
  - Every struct is magic-checked.
- **Entry table** (fixed: stock patch sites jump here):

| Offset | Entry | Patched stock site |
|---|---|---|
| +0x00 | `voice_btn` | 5 voice-button `bl 0x6000E7DC` (`0x6000D3BE` …) |
| +0x04 | `voice_other` | bank-toggle voice select `0x6000AF1A` |
| +0x08 | `c274_wrap` | button handler call `0x6000EB3C` |
| +0x0C | `latch_off` | power latch-off `0x600083B0`, `0x60008418` |
| +0x10 | `row_read` | key-matrix read `0x6000C2F4` |
| +0x14 | `boot_b2c4` | factory-reset check `0x6000F284` |
| +0x18 | `scan_wrap` | strum scanner call `0x6000EB50` |
| +0x1C | `mix_tail` | mixer's final memcpy `0x6000E1B4` |
| +0x20 | `blink_wrap` | power-on blink `0x6000EA9E` |
| +0x24 | `rhythm_setup` | rhythm-tone setup `0x6000F2A0` |
| +0x28 | `pwm_hook` | QTMR_SetupPwm entry `0x600166D0` |
| +0x2C | `touch_hook` | palm check in the touch task `0x6000FB76` |
| +0x30 | `r27_step_wrap` | 2.60: rhythm tick, once per stock step `0x60007E58` |
| +0x34 | `r27_row` | 2.60: rhythm row player call `0x60007E98` |
| +0x38 | `r27_bass` | 2.60: rhythm bass call `0x60007FFE` |
| +0x3C | `midi_hook` | 2.73: MIDI note-on / note-off builders `0x600096CA`, `0x600096F8` (then mod 1) |
| +0x40 | `strum_wrap` | 2.73: strum-plate notes `0x6000C090` |
| +0x44 | `bat_led` | 2.78: battery check LED writes `0x6000825C`, `0x6000828C`, `0x600082F2` |
| +0x48 | `bat_row` | 2.78: battery check voice-row redraw `0x6000829C` |
| +0x4C | `panel_shown` | 2.79: end of the startup LED animation `0x600047AA` and of the setting blink `0x60004F70` (bl + return: no forced default flags) |
| +0x50 | `ma_bass_auto` | 2.81: the auto bass MIDI's Auto check `0x60008EE6`, `0x60008F2A`, `0x60008F7E`, `0x60008FF4`, `0x60009114` |
| +0x54 | `ma_hold_on` | 2.81: the held Manual chord's MIDI bass note-on `0x60008BAE` |
| +0x58 | `ma_hold_off` | 2.81: its note-off in `0x60008B0C`, `0x60008C02` |
| +0x5C | `clk_gate` | 2.82: the MIDI task's realtime-message check `0x6000EC70` (asm in `entry.s`: skip clock / Start / Stop unless preset P4 / P5) |
| +0x60 | `ma_chord_off` | 2.84: the stock Manual branch's call of 0x60008E2C at `0x6000CFCC` (the rhythm chord's MIDI note-offs without its Auto test) |
| +0x64 | `led_prio` | 2.85: main's `strd r4,r3,[sp]` at `0x6000F390` (xTaskCreate's stack arguments for the LED task): priority 3 instead of 1 (asm in `entry.s`) |
| +0x68 | `bass_note` | 2.86: bass pitch calls in the rhythm tick `0x60007BD6`, `0x60007C1E`, `0x60007C46`, `0x60007E0E` (jazz bass map; the final static bass passes them through) |
| +0x6C | `plate_on` | 2.86: strum-plate note-on in the plate scanner `0x6000A2E4`, `0x6000A47E` (jazz plate note; Omni 1 / 2 oscillator retune) |
| +0x70 | `plate_off` | 2.86: strum-plate release in the plate scanner `0x6000A506`, `0x6000A52E` |
| +0x74 | `chord_dec` | 2.86: the chord-button decoder's calls `0x6000CCFE`, `0x6000D750` (the 4 extra jazz combinations) |

### 6.2 Where each feature lives
- **`c274_wrap`** (control task, each tick), in this order:
  1. `restore_tick` (2.78: only what `restore_early` left: settings whose buttons were held from power-on, pressed through `row_read` → `inj_bits` once let go; the player's own press wins; then the voice reload), then `ma_tick` (2.79: the third Manual/Auto state);
  2. `r27_tick` (27 mode buttons / LEDs), `pluck_tick` (jazz voicing, alternate bass pluck);
  3. `snapshot` (the record to save at power-off);
  4. `ui_tick` (startup prompt, the Gb 7th mode selector and blink, the menus and keyboard effect);
  5. the stock handler.
- **`row_filter`** (via `row_read`): restore presses; 2.79 `ma_keys` (row 5: Manual/Auto held + KEYBOARD = the third-state gesture, KEYBOARD hidden until let go); 27 mode's PATTERN buttons (`r27_keys`); Gb 7th + voice button = mode pick (while Gb 7th is physically down). While a config menu or the Gb 7th hold is up, it also hides Up/Down from the stock handler and hides held note keys until they're let go (`ui_t.hid`). During `restore_early` (ST phase `PH_EARLY`) only the injected presses.
- **`scan_wrap` → `vib_tick`** (control task): the whole modulation engine. It writes the pitch, the tremolo gain, the analog vibrato and the MIDI bend. In the effect modes during normal play it clears `DEA1` bit 6, so the stock Instant OFF never fires.
- **`mix_tail`** (audio mixer):
  - `trem_mix` = tremolo gain on bus A (strum voices);
  - `prompt_mix` = spoken clips from the blob at `0x60F40000` (8 kHz μ-law: 108, 84, mode, note off, vibrato, tremolo, auto, manual, 27).
- **`touch_hook`:** the chip gain (§5).
- **`pwm_hook`:** flags stock oscillator reprogramming for `an_apply`.
- **`boot_b2c4` / `boot_mode`:** factory reset (erases our sector, sets defaults), or loads the records.
- **`rhythm_setup`:** rhythm-mode keys (stock tone keys + 27 mode), bass, jazz, prompt / blink flags; then `restore_early` (the saved panel through the stock handler, before the panel is shown; see the comment above it in mod2.c).
- **`blink_wrap`:** our LEDs in the stock setting blink (27 mode, bass choice).
- **`bat_led` / `bat_row`:** the stock battery check's LED writes, only when they change (+ every 128th call).
- **`latch_off`:** `save_state`, then the stock latch-off.
- **`voice_btn` / `voice_other`:** alt sub toggle.
  - Tables `SUB_OFF` / `SUB_NRM` / `SUB_ALT` give the layer byte and instrument per voice 1–9.
  - Harp alt also sets a layer-2 evolve_sub copy to volume 52 (+3 dB).

### 6.3 RAM we use
| Address | What |
|---|---|
| voice-table slot 11 (`0x2000066C` + off) | persistent settings, never read by the engine (slot unreachable): VMODE +0x1F (`0x2000068B`), EARLY +0x20 (2.78: restore steps left), BLPH +0x2D, BOOTF +0x2E, SPD +0x2F, PDEP[4] +0x34, PSPD[4] +0x38, R27 +0x3C (27 mode), BSTY +0x3D (bass), JAZZ +0x3E (stock 2 / 0x4A), MODEV +0x3F (`0x200006AB`) |
| `0x20002AD4` PAGEBUF (256 B) | stock page buffer; our record writes use it at power-off only |
| `0x20002AD8` `sl_t` (56) | 2.86: jazz strum slot owners / MIDI (runtime; page buffer) |
| `0x20002B10` `bro_t` (0x38) + bass MIDI map (2) | 2.86: jazz state: the voicing sounding (`jv`, `jk`), the extra combination (`xt`), Omni plate notes (`mel`), MIDI (runtime; page buffer) |
| `0x20002B60` `an_t` (0x30) | analog oscillators (vibrato, jazz shifts) |
| `0x20002B90` `vib_t` (≤ 0x44) | modulation engine state |
| `0x20002BD4` `cs_t` (8) | chip gain state (touch task) |
| `0x20002BDC` `fx_t` (20) | tremolo gain, MIDI bend, RPN state |
| `0x20002BF0` `mt_t` (16) | mode selector / blink |
| `0x20002C00` `mm_t` (8) | 2.73: jazz MIDI note map (in / out per chord note) |
| `0x20002C08` `ui_t` (24) | keyboard UI |
| `0x20002C20` `pl_t` (32) | prompt player |
| `0x20002C50` `bat_t` (4) | 2.78: battery LED de-duplication |
| `0x20002C54` `ST` (48) | panel restore state + snapshot |
| `0x20002C84` `r27_t` (72) | 27 mode, alternate bass pluck, jazz shifts, analog vibrato copy, Gb 7th + voice pick, 2.79 Manual/Auto gesture (`mpre`, `mfrom`, `mflag`, `moff`) |

- **Free:** `0x20002C40`–`0x20002C4F`, `0x20002CCC`–`0x20002CD3`. `0x20002AD4`–`0x20002B5F` is free at runtime, but the page buffer is overwritten at power-on / power-off. (`0x20002CD4`+ is a stock MIDI queue's buffer.)
- **These are stock buffers** used only by the stock settings save before the tasks start.
- **MODEV:** bit 0 = effect mode on, bits 2–5 = current depth step, bits 1 / 6 unused.
- **VMODE:** 1 vibrato auto, 2 vibrato manual, 3 tremolo auto, 4 tremolo manual.

### 6.4 Records (`0x60FFE000`, append-only 16-byte slots, the sector erased when full)
- **Panel record v2** `[0]=A5 [1]=2`:
  - [2] voice bank, [3] voice, [4] pattern bank, [5] pattern (FF none);
  - [6] Hold, [7] Manual (2.79: 2 / 3 = the third state, turned on from Auto / Manual; older versions read it as Auto; 2.83: kept live in keyboard mode, [2..6] / [8] keep their normal-mode values), [8] Start;
  - [9] tune, [10] transpose, [11] octave;
  - [12] alt mask voices 1–8;
  - [13] bit 0 = alt voice 9 | (MODEV & 0x7F) << 1;
  - [14] sum = 0x5A + Σ[0..13];
  - [15] 0.
- **Settings record v3** `[1]=3`:
  - [2] VMODE, [3] its speed;
  - [4..7] depth per mode, [8..11] speed per mode, [12] = `0x5D` (the per-mode bytes are valid);
  - [13] 0x27 = 27 mode (bits 0-5) | bass style << 6, [14] sum, [15] 0.
- **Rules:**
  - The newest valid record of each kind wins.
  - Write only if changed; writes run with interrupts off.
  - Erase and re-append when full. After an erase that is read back, invalidate the D-cache (`0x60012C9C`).
  - Saved only at power-off (`latch_off`).

### 6.5 Modulation engine (`vib_tick`, ~1 kHz)
- **Input:** L = Instant OFF reading × 16 / the real chip gain, clamped to 80. It's smoothed adaptively (15 ms for movement, 60 ms for jitter).
- **Gate:** L ≥ 15 for 20 ms on; off < 11 for 20 ms, or after the 0.35 s roll-off hold. A quick-lift detector ends it early. Menus force the gate.
- **Depth:** coverage 15 → 68 on a soft curve, × the step's cents (or %). It rises at once and falls no faster than 0.8 s; 40 ms anti-click attack; 0.2 s fade (menus 0.6 s in / 0.88 s out); legato re-touch.
- **Speed:** `SPEED_MHZ[]` + `SWELL_MHZ[]` touch swell; glides 100 ms.
- **Manual modes:** output = (reading − home) / 25 counts, clamped ±1, slewed 10 ms.
- **Output:**
  - vibrato: `0x20007606` = base + cents (base re-taken when someone else changes it; exact base back when idle); `an_apply` on TMR1 ch 0–2 only with Omni 1/2; MIDI bend;
  - tremolo: `fx_t.gain` (Q15) for `trem_mix`.
- **The constants are named at the top of the vibrato section of `mod2.c`.**

## 8. Lessons learned (don't repeat these)
- **Flash and records:**
  - Large erases fail silently: 64 KB-aligned pieces are skipped. Always erase ≤ `0x8000`.
  - The factory-reset flag `DE6B` is cleared before the control loop runs, so hook the check itself (`boot_b2c4`).
  - The stock power-on settings save overwrites the stock buffers after `boot_b2c4`; persistent RAM values live in voice-table slot 11.
  - After a flash write or erase, invalidate the D-cache before reading the sector back.
- **Panel:**
  - Inject key presses only by ORing column bits into the matrix read; setting the debounce bytes only makes release events.
  - Voice-row displays need `DE78`; **never `DE73`** (§4).
- **MIDI:** never post from the touch or audio tasks; rate-limit bursts, or the stock builders stall.
- **Sound:**
  - The zone amp byte does nothing; layer volume is latched at note-on; a disabled layer never gets its note-off.
  - Check sample / instrument sharing before repointing (samples 0–5 and 15–16, instrument 13).
  - The user's files can be an octave off their names: always run a harmonic-sum fundamental check.
  - **Limiter:** `limiter.gain_env` must get a padded signal (≥ 10× the release), or the start is dipped. 2.54/2.55 clap attacks were softened this way (`reference/audio_methods.md`).
  - The last 5–10 % of an S-curve fade is inaudible: make fades ~10 % longer than the audible requirement.
- **LEDs (2.78):** LED writes are queued and sent one per millisecond by the lowest-priority task: never write LEDs every tick, and redraw only on change. At power-on, the panel's flags live in LEDSAVE until the startup animation / blink ends.
- **Hand-overs to stock code (2.79):** 2.78's power-on test copied LEDSAVE into the flags itself instead of running the stock animation end, which then forced its default flags over the restored panel on the unit (Manual + Auto both lit). When our state passes through a stock routine, drive that routine in the test (test_final `go_live`), and check what the user sees after it.
- **Packages (2.78):** a data region erased in more than one piece could be refused by the pre-check after a cut; `make_package.py` now caps regions at 0x8000. Test a new package by cutting it at every step (see the 2.78 hand-off notes in `reference/REVIEW_2.77_done.md`).
- **Tests:** `run_tests.sh` trusts exit codes. Make every check fail loudly, and read the counts. Never add a global `UC_HOOK_CODE` (every instruction): it made the suite 6× slower. Hook single addresses with `e.stub` or `hook_add(..., begin=a, end=a)`.
- **Python bytecode:** a same-size edit to a `.py` within the same second can leave a stale `.pyc` (bit `release.py installed` once; it now clears `lib/__pycache__`).
- **Don't state measurements as facts without data.** Build a read-only diagnostic and measure (e.g. 2.34dev / 2.42dev streamed plate readings over MIDI; `audio/mmon_parse.py`).
- **The plate:** it senses coverage, not pressure or hover. Rocking mid-pad barely changes the reading; rocking at the edge does.
- **The user prefers simple, predictable behaviour** over clever detection, and decides by playing. Offer a few simple variations of what works, side by side, and remove the losers afterwards.
- **MIDI Monitor:** raise its history to 500,000 events; ask them to show all channels and hide Clock.

## 10. Sources
- OM-108 Owner's Guide: https://www.omnichord-heaven.com/downloads/manuals/OM-108_Owners_Guide.html.
- NXP AN12238 (i.MX RT flashloader); i.MX RT1020 Reference Manual; SPSDK docs; Winbond W25Q128JV datasheet.
- onsemi LC717A10 datasheet (no register map); **AND9346JP-D** (LC717A30 application note, full register map): https://www.onsemi.com/pub/Collateral/AND9346JP-D.PDF; LC717A design note: https://static5.arrow.com/pdfs/2013/10/21/7/10/15/3/ons_/manual/132dnlc717a-d.pdf
