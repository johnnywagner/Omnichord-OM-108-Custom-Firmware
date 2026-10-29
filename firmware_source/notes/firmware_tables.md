# Firmware tables for mod 2.59 (generated from the 2.59 image)

**2.79:** for the code, the patch sites and the data areas use `changes_vs_stock_281.md` (sections 1-2 here are 2.59's; 2.60-2.79 added patch sites and grew the code). Sections 4+ (banks, voice table) are unchanged. Free flash in 2.78 (`audio/free_map.py images/mod2_78.bin`): the same gaps as below, but the ones that held our old leftovers are back to the stock bytes (blank where stock was blank), except a 64-byte guard next to each sample in use.

Generated with the dev kit (`audio/bank_dump.py`, `audio/free_map.py`) plus a diff against the stock dump. Regenerate after any data change. All addresses are flash addresses (XIP at `0x60000000`).

## 1. Every stock code byte we changed (stock dump vs 2.59, code area < 0x60039000; identical to the 2.37 list)

| Address | Stock instruction | Now | Purpose |
|---|---|---|---|
| `0x600083B0` | `str.w r2,[r3,#0x88] (power latch off)` | `b.w 0x6003A00C latch_off` | save panel/settings records, then the original latch-off (manual power-off) |
| `0x60008418` | `str.w r2,[r3,#0x88]` | `b.w 0x6003A00C latch_off` | same, auto power-off / low-battery path |
| `0x600096CA / 0x600096F8 / 0x60009728` | `bl 0x6000DB28 (MIDI queue post) in the 3 MIDI builders` | `bl 0x60039000` (CC builder); 2.73: note-on / note-off `bl 0x6003A03C midi_hook` (which then calls `0x60039000`) | mod 1 MIDI hook: routing per preset, drum note map, drops all CCs; 2.73: midi_hook moves the chord channel's notes in jazz mode |
| `0x6000AF1A` | `bl 0x6000E7DC (voice select, bank toggle path)` | `bl 0x6003A004 voice_other` | voice select without alt toggle |
| `0x6000B22C` | `movs r1,#2` | `movs r1,#1` | mod 1: Volume CC always saved off |
| `0x6000B25E (+0x6000B270)` | `ldr r2,=0x2000DE6F; movs r0,#0x1E / movs r3,#1` | `b.w 0x600390C0 / movs r3,#0` | mod 1: A. piano held at power-on = MIDI preset 5; Decay CC saved off |
| `0x6000C2F4` | `ldr r4,[r1]; ldr r2,[r2] (key-matrix read)` | `bl 0x6003A010 row_read` | injects key presses (panel restore) and hides Up/Down during our menus |
| `0x6000D3BE / D406 / D492 / D4D6 / D51A` | `bl 0x6000E7DC (5 voice buttons)` | `bl 0x6003A000 voice_btn` | re-pressing the selected voice toggles primary/alt sub |
| `0x6000E1B4` | `b.w 0x600181D6 (mixer's final memcpy)` | `b.w 0x6003A01C mix_tail` | adds spoken prompts / beeps to the audio block |
| `0x6000EA9E` | `bl 0x60004EF0 (power-on blink)` | `bl 0x6003A020 blink_wrap` | blinks the slow rock/funk LED when it is held at power-on (reserved key) |
| `0x6000EB3C` | `bl 0x6000C274 (button handler)` | `bl 0x6003A008 c274_wrap` | panel restore, snapshot, keyboard-mode UI (mode selector, menus), then the stock handler |
| `0x6000EB50` | `bl 0x60009B20 (strumplate scanner)` | `bl 0x6003A018 scan_wrap` | modulation engine (vib_tick); the effect modes clear the Instant OFF bit (normal play) |
| `0x6000F284` | `bl 0x6000B2C4 (factory-reset check)` | `bl 0x6003A014 boot_b2c4` | factory reset also erases our records; loads our settings |
| `0x6000F2A0` | `bl 0x6000B350 (rhythm-tone setup)` | `bl 0x6003A024 rhythm_setup` | stock rhythm-tone keys (swing = Classic 84, country = Normal 108) + spoken prompt |
| `0x6000EC70` | `ldrb r3,[r7]; cmp r3,#4` (MIDI task: internal preset 4 skips clock / Start / Stop) | `bl 0x6003A05C clk_gate` | 2.82: clock / Start / Stop only in user presets P4 / P5 (`0x20000280`) |
| `0x6000F2D4` | `ldrb r4,[r3] (load MIDI preset)` | `movs r4,#3` | mod 1: internal preset forced to All Out; routing done in the MIDI hook |
| `0x6000FB76` | `bl 0x6000F90C (touch task palm check)` | `bl 0x6003A02C touch_hook` | palm check, then (effect modes only) the Instant OFF 16x gain boost; Note Off puts the stock set-up back |
| `0x6000C090` | `bl 0x6000BD8C (strum-plate notes from the chord)` | `bl 0x6003A040 strum_wrap` (2.73) | jazz mode: the chord classes moved before the stock computes the 13 plate notes |
| `0x600166D0` | `cmp r2,#0x64; bhi (QTMR_SetupPwm entry)` | `b.w 0x6003A028 pwm_hook` | tells the analog vibrato when stock code reprograms an oscillator |

Also changed (data, not code): the voice table init data `0x60038278`–`0x60038740` (layer instruments/volumes; see section 4).

## 2. Flash areas that differ from stock (plus the two runtime record sectors)

| Range | What |
|---|---|
| `0x60038279`–`0x60038739` | voice table init data (layer instruments / volumes; section 4) |
| `0x60039000`–`0x600391FF` | mod 1 MIDI hook + routing table (`0x60039100`) + drum map (`0x60039180`) + preset-5 trampoline (`0x600390C0`) |
| `0x6003A000`–`0x6003CC9A` | our code (mod2.c + entry.s), 11,419 bytes; entry table at `0x6003A000`. Blank after it up to `0x60047FFF` (about 45 KB) |
| `0x600B6AA4`–`0x600BD01B` | hand clap = clap4 (2.59), 12,988 frames incl. zero padding (sound = first 5,513). Bank header `0x60048000` sector is stock again since 2.55 |
| `0x60148644`–`0x6014A50E` | strum bank: instrument offset table, instrument records, sample table (233 samples) |
| `0x60359000`–`0x605A2FFB` | replacement / new sample data (over unused stock sample data) |
| `0x60BEC000`–`0x60D6095F` | replacement / new sample data |
| `0x60DA0000`–`0x60FE6FE7` | replacement / new sample data, plus the spoken-prompt blob at `0x60F40000` (32.9 KB used, 60 KB reserved) |
| `0x60FFE000` | our panel + settings records (append-only, written by our code at power-off; changes only at runtime) |
| `0x60FFF000` | stock settings (7 bytes, written by the stock code) |

## 3. Free flash (2.59)

Our code area: **`0x6003CC9B`–`0x60047FFF` blank (about 45 KB)**. Sample area (`audio/free_map.py`; "old unused data" = stock or earlier samples nothing references any more, safe to overwrite). Keep the 64 KB below `0x60FFE000` blank as a reserve, so the last line really offers 28 KB.

```
0x60151546-0x60161544    63 KB old unused data
0x6049C318-0x6049FC64    14 KB old unused data
0x605A2FFC-0x605A87C8    21 KB old unused data
0x60D60960-0x60D64D48    16 KB old unused data
0x60D96DCA-0x60DA0000    36 KB blank
0x60DBD624-0x60DBEF80     6 KB old unused data
0x60DDE500-0x60DE1000    10 KB old unused data
0x60EE18AC-0x60EE487C    11 KB old unused data
0x60F16830-0x60F22B7C    48 KB old unused data
0x60FE6FE8-0x60FFE000    92 KB blank
total 323 KB
```
(The 2.37 list wrongly showed `0x60F47000`–`0x60F4F990` as free; that is the prompt blob area since 2.53. `free_map.py` now reserves `0x60F40000`–`0x60F4EFFF`.)

## 4. Percussion bank (`0x600486A0`, 25 instruments, 25 samples, sample base `0x60048FB4`)

Every instrument has one zone, loop length 16, tune 0, root 60, key high 127. The samples are packed back to back: each stores end+1 frames (16-bit mono, 44.1 kHz) and the next starts right after (no guard frame here). Sample data ends at `0x60145D10`; the strum bank starts at `0x60148620`. WAV exports: `audio/drum_export.py <image> <dir>`.

| # | Name | Data | Frames | Length | Record |
|---|---|---|---|---|---|
| 0 | AnMMV_Agogo 1    | `0x6004CFB4` | 11086 | 251 ms | `0x60048AB4` |
| 1 | AnMMV_Agogo 2    | `0x60052650` | 11716 | 266 ms | `0x60048AE8` |
| 2 | AnSF_Bass Drum   | `0x600581D8` | 8492 | 193 ms | `0x60048B1C` |
| 3 | AnMMV_Claves     | `0x6005C430` | 6636 | 150 ms | `0x60048B50` |
| 4 | AnMMV_Conga 2    | `0x6005F808` | 24168 | 548 ms | `0x60048B84` |
| 5 | AnMMV_Conga 3    | `0x6006B4D8` | 24536 | 556 ms | `0x60048BB8` |
| 6 | AnMSF_Rim Shot   | `0x60077488` | 2032 | 46 ms | `0x60048BEC` |
| 7 | AnMMV_Cowbell    | `0x60078468` | 22972 | 521 ms | `0x60048C20` |
| 8 | AnMMV_Cymbal     | `0x600837E0` | 104802 | 2376 ms | `0x60048C54` |
| 9 | AnMMV_Hand Claps | `0x600B6AA4` | 12988 | 295 ms | `0x60048C88` |
| 10 | AnMMV_HH Closed  | `0x600BD01C` | 3060 | 69 ms | `0x60048CBC` |
| 11 | AnMMV_HH Open    | `0x600BE804` | 58816 | 1334 ms | `0x60048CF0` |
| 12 | AnMMV_Maracas    | `0x600DB384` | 5660 | 128 ms | `0x60048D24` |
| 13 | AnTX8_Rim Shot   | `0x600DDFBC` | 2802 | 64 ms | `0x60048D58` |
| 14 | AnMMV_Snare Drum | `0x600DF5A0` | 11046 | 250 ms | `0x60048D8C` |
| 15 | AnTX8_Tambourine | `0x600E4BEC` | 20644 | 468 ms | `0x60048DC0` |
| 16 | AnMMV_Tom 2      | `0x600EED34` | 24928 | 565 ms | `0x60048DF4` |
| 17 | AnMMV_Tom 3      | `0x600FAFF4` | 26300 | 596 ms | `0x60048E28` |
| 18 | AnMMV_Tom 4      | `0x60107D6C` | 35782 | 811 ms | `0x60048E5C` |
| 19 | FMPSG_Ride       | `0x601194F8` | 65536 | 1486 ms | `0x60048E90` |
| 20 | OM84_Bass Drum   | `0x601394F8` | 7068 | 160 ms | `0x60048EC4` |
| 21 | OM84_Claves      | `0x6013CC30` | 770 | 17 ms | `0x60048EF8` |
| 22 | OM84_Cymbal      | `0x6013D234` | 14010 | 318 ms | `0x60048F2C` |
| 23 | OM84_Hi-Hat      | `0x60143FA8` | 2228 | 51 ms | `0x60048F60` |
| 24 | OM84_SD Skin     | `0x60145110` | 1536 | 35 ms | `0x60048F94` |

Zone end (the clap's is at `0x60048C90`) = frames − 1.

**Where each drum sound is used (stock tables, read 2026-10-13):**

| Use | Normal (108) | Classic (84) |
|---|---|---|
| keyboard-mode drum keys 4–11 (table `0x60033248`, 8 per tone; MIDI notes `0x60033258` = 36 38 48 41 42 46 49 39) | 2 14 16 18 10 11 8 9 | 20 24 16 18 23 22 8 9 |
| keyboard-mode strum plates 1–13 + Instant OFF (table `0x600331E0`, 14 per tone) | 2 2 14 14 16 16 17 17 10 10 11 11 8 9 | 20 20 24 24 16 16 17 17 23 23 22 22 8 9 |
| rhythm rows 0–17 (switch in `0x60007574`; row notes `0x60018C78` = 36 62 38 39 45 48 49 42 46 70 56 63 64 37 67 68 75 54) | 2 13 14 9 17 16 8 10 11 12 7 5 4 6 1 0 3 15 | as Normal, except BD 20, snare 24 (+23), toms 24, cymbal 22, closed HH 23, open HH 22, claves 21 |

- Sample 19 (FMPSG Ride) is not used by any of these.
- Normal-tone rhythm hits go through `0x20200488(instr, choke, velocity)`; Classic hits and the drum keys / plates through `0x20200344(instr, choke)` (veneer `0x600186B0`), fixed level. Drum voices use engine channels 78 + instrument.
- The plate-drum MIDI notes are `0x6003320C` (3 sets of 14, chosen by `DE61`, which the pattern keys set).
- Runtime rhythm tone = `DE65` (copied from settings byte `0x20000284` at power-on). Its users: `0x60007574` (row → instrument), `0x60007C8C` (rhythm tick: extra Classic-only logic), `0x60009B20` (strum scanner, plate drums), `0x6000BCCC` (drum keys).

## 5. Strum bank and voice table (`audio/bank_dump.py`)

Zone notation `keyhi:sample@zone_cents` (zone_cents = root·100 − tune = the sample's true pitch in MIDI cents at 44.1 kHz; samples stored at 22.05 kHz use +1200). Instrument names are labels only (the loader ignores them). Strum-bank samples are stored as end+2 frames (a guard frame).

```
## Strum bank at 0x60148620: 22 instruments, 233 samples
   0 Sine               rec 0x60148A34  zones(keyhi:sample@zone_cents): 35:0@2877, 47:1@4077, 59:2@5277, 71:3@6477, 83:4@7677, 127:5@8877
   1 evolve_sub         rec 0x60148CFC  zones(keyhi:sample@zone_cents): 54:228@5997, 66:229@7200, 78:230@8401, 90:231@9600, 127:232@10801
   2 WG Pad Mellow      rec 0x60148B28  zones(keyhi:sample@zone_cents): 53:7@5277, 59:8@5277, 65:9@6477, 71:10@6477, 77:11@7677, 83:12@7677, 89:13@8877, 95:14@8877, 101:15@10077, 127:16@10077
   3 pad_sub            rec 0x60148C58  zones(keyhi:sample@zone_cents): 53:177@4810, 65:178@6010, 77:179@7210, 89:180@8410, 127:181@9610
   4 OrchHarp_Mono      rec 0x60148DF8  zones(keyhi:sample@zone_cents): 53:182@4800, 65:207@6000, 77:208@7200, 89:209@8400, 127:210@9600
   5 Steel-Str Gt.      rec 0x60148FB4  zones(keyhi:sample@zone_cents): 43:45@4077, 46:46@4077, 49:47@5277, 51:48@5277, 54:49@5277, 57:50@5277, 62:51@6477, 65:52@6477, 67:53@6477, 71:54@6477, 75:55@7677, 80:56@7677, 86:57@8877, 89:58@8877, 94:59@8877, 100:60@10077, 103:61@10077, 107:62@10077, 111:63@11277, 127:5@8877
   6 Vibraphone YV      rec 0x601491FC  zones(keyhi:sample@zone_cents): 58:64@5277, 61:65@6477, 64:66@6477, 67:67@6477, 70:68@7677, 73:69@7677, 76:70@7677, 79:71@7677, 82:72@8877, 85:73@8877, 115:74@8877, 127:74@10077
   7 churchbell         rec 0x60149364  zones(keyhi:sample@zone_cents): 65:200@6000, 77:201@7200, 89:211@8400, 127:212@9600
   8 Detuned_EOrg_2     rec 0x60149424  zones(keyhi:sample@zone_cents): 47:75@5277, 59:76@6477, 65:77@6477, 71:78@7677, 77:79@7677, 83:80@8877, 89:81@8877, 95:82@10077, 103:83@11277, 127:83@12477
   9 GPf Y CF Mono      rec 0x60149554  zones(keyhi:sample@zone_cents): 50:84@5277, 56:85@6477, 62:86@6477, 68:87@7677, 74:88@7677, 80:89@8877, 86:90@8877, 92:91@10077, 127:92@10077
  10 Banjo              rec 0x60149668  zones(keyhi:sample@zone_cents): 52:93@5277, 55:94@6477, 58:95@5277, 61:96@6477, 64:97@6477, 67:98@6477, 70:99@6477, 73:100@7677, 76:101@7677, 79:102@7677, 82:103@8877, 85:104@8876, 89:105@8877, 93:106@8877, 97:107@10077, 101:108@10077, 106:109@10077, 110:110@11277, 127:111@10077
  11 rezy_sub           rec 0x60149894  zones(keyhi:sample@zone_cents): 64:218@5915, 73:219@7054, 79:220@7555, 90:221@8416, 127:222@9617
  12 E.Piano FM         rec 0x60149954  zones(keyhi:sample@zone_cents): 51:112@5277, 58:113@6477, 63:114@6477, 70:115@6477, 75:116@7677, 82:117@7677, 87:118@8877, 94:119@8877, 99:120@8877, 106:121@10077, 127:122@8877
  13 pad_sub octave     rec 0x60149AA0  zones(keyhi:sample@zone_cents): 53:177@4810, 65:178@6010, 77:179@7210, 89:180@8410, 127:181@9610
  14 Celesta WG         rec 0x60149C24  zones(keyhi:sample@zone_cents): 61:134@6477, 64:135@6477, 68:136@6477, 73:137@7677, 76:138@7677, 79:139@7677, 82:140@7677, 86:141@8877, 90:142@8877, 94:143@8877, 98:144@8877, 102:145@10077, 106:146@10077, 109:147@10077, 120:148@7677, 127:148@10077
  15 vibes_sub_alt      rec 0x60149DFC  zones(keyhi:sample@zone_cents): 53:213@4823, 65:214@6016, 77:215@7206, 89:216@8397, 127:217@9602
  16 OM-84 Keys         rec 0x60149EBC  zones(keyhi:sample@zone_cents): 46:149@4077, 49:150@5277, 52:151@5277, 55:152@5277, 58:153@5277, 61:154@6477, 64:155@6477, 67:156@6477, 70:157@6477, 73:158@7677, 76:159@7677, 79:160@7677, 82:161@7677, 85:162@8877, 88:163@8877, 91:164@8877, 94:165@8877, 97:166@8877, 100:167@8877, 103:168@8877, 106:169@8877, 109:170@8877, 127:5@8877
  17 harp_sub           rec 0x601486B4  zones(keyhi:sample@zone_cents): 53:171@4800, 65:172@6000, 77:173@7200, 89:174@8400, 127:175@9600
  18 string_sub         rec 0x60148E9C  zones(keyhi:sample@zone_cents): 48:187@5499, 54:188@6696, 60:223@6696, 66:189@7902, 72:224@7902, 80:190@9099, 89:225@9099, 95:226@9099, 127:227@9099
  19 string2_sub        rec 0x60148978  zones(keyhi:sample@zone_cents): 53:202@4802, 65:203@6004, 77:204@7200, 89:205@8399, 127:206@9604
  20 organ_sub_alt      rec 0x60148814  zones(keyhi:sample@zone_cents): 127:176@6000
  21 organ_sub          rec 0x60148848  zones(keyhi:sample@zone_cents): 50:192@4788, 56:193@5403, 62:194@6006, 68:195@6601, 74:196@7192, 83:197@7806, 95:198@9006, 127:199@10206
  sample addresses (index: start, bytes) for samples >= 171:
    171: 0x60359000
    172: 0x6039A1CC
    173: 0x603CC998
    174: 0x603FEB64
    175: 0x60432F30
    176: 0x6049FC64
    177: 0x60E0B000
    178: 0x60E2B584
    179: 0x60E4B784
    180: 0x60E6C2CC
    181: 0x60E8D0B0
    182: 0x60EADB8C
    183: 0x60ECE36C
    184: 0x60EEECA8
    185: 0x60F0F75C
    186: 0x60F2F954
    187: 0x60F4F990
    188: 0x60F57E00
    189: 0x60F5F91C
    190: 0x6045E000
    191: 0x60F7A07C
    192: 0x60BEC000
    193: 0x60C099D4
    194: 0x60C28254
    195: 0x60C41728
    196: 0x60C598FC
    197: 0x60C72FD0
    198: 0x60C90750
    199: 0x60C9F318
    200: 0x60CA7000
    201: 0x60CD8E14
    202: 0x60EE487C
    203: 0x60F01EFC
    204: 0x60F22B7C
    205: 0x60DA0000
    206: 0x60DBEF80
    207: 0x60DE1000
    208: 0x60DEB5D4
    209: 0x60DF08C0
    210: 0x60DF3238
    211: 0x60CF1D28
    212: 0x60CFE4B4
    213: 0x604E2000
    214: 0x604FD974
    215: 0x6051A3E8
    216: 0x6053695C
    217: 0x605522D0
    218: 0x60D05000
    219: 0x60D163A4
    220: 0x60D2CD14
    221: 0x60D3EABC
    222: 0x60D5547C
    223: 0x60572508
    224: 0x6057A024
    225: 0x60583E70
    226: 0x6047D18C
    227: 0x60EC2720
    228: 0x60F69768
    229: 0x60F88568
    230: 0x60DF46F8
    231: 0x60FA64E8
    232: 0x60FC71E8
   0 AnMMV_Agogo 1      smp [0]
   1 AnMMV_Agogo 2      smp [1]
   2 AnSF_Bass Drum     smp [2]
   3 AnMMV_Claves       smp [3]
   4 AnMMV_Conga 2      smp [4]
   5 AnMMV_Conga 3      smp [5]
   6 AnMSF_Rim Shot     smp [6]
   7 AnMMV_Cowbell      smp [7]
   8 AnMMV_Cymbal       smp [8]
   9 AnMMV_Hand Claps   smp [9]
  10 AnMMV_HH Closed    smp [10]
  11 AnMMV_HH Open      smp [11]
  12 AnMMV_Maracas      smp [12]
  13 AnTX8_Rim Shot     smp [13]
  14 AnMMV_Snare Drum   smp [14]
  15 AnTX8_Tambourine   smp [15]
  16 AnMMV_Tom 2        smp [16]
  17 AnMMV_Tom 3        smp [17]
  18 AnMMV_Tom 4        smp [18]
  19 FMPSG_Ride         smp [19]
  20 OM84_Bass Drum     smp [20]
  21 OM84_Claves        smp [21]
  22 OM84_Cymbal        smp [22]
  23 OM84_Hi-Hat        smp [23]
  24 OM84_SD Skin       smp [24]
## voice table (flash 0x600381E8), voices 0-21: layer: on/instr/vol/bus
  v 0 L3:ins0 vol127 bus2
  v 1 L1:ins3 vol127 bus1  L3:ins0 vol127 bus2
  v 2 L0:ins4 vol127 bus0  L1:ins17 vol127 bus1  L2:ins1 vol0 bus1  L3:ins0 vol127 bus2
  v 3 L0:ins14 vol56 bus0  L1:ins3 vol127 bus1  L3:ins0 vol127 bus2
  v 4 L0:ins9 vol127 bus0  L1:ins3 vol127 bus1  L3:ins0 vol127 bus2
  v 5 L0:ins5 vol127 bus0  L2:ins13 vol112 bus1  L3:ins0 vol127 bus2
  v 6 L0:ins12 vol52 bus0  L1:ins12 vol52 bus0  L2:ins2 vol127 bus1  L3:ins0 vol127 bus2
  v 7 L0:ins8 vol120 bus0  L1:ins21 vol127 bus1  L3:ins0 vol127 bus2
  v 8 L0:ins6 vol88 bus0  L1:ins18 vol124 bus1  L3:ins0 vol127 bus2
  v 9 L0:ins10 vol127 bus0  L1:ins18 vol127 bus1  L3:ins0 vol127 bus2
  v10 L0:ins11 vol127 bus0  L1:ins3 vol127 bus1  L3:ins0 vol127 bus2
  v11 L0:ins0 vol127 bus0  L1:ins1 vol127 bus1  L3:ins0 vol127 bus2
  v12 L0:ins16 vol112 bus0  L1:ins16 vol112 bus1  L3:ins0 vol127 bus2
  v13 L0:ins16 vol112 bus0  L1:ins16 vol112 bus1  L3:ins0 vol127 bus2
  v14 L0:ins4 vol127 bus0  L1:ins17 vol127 bus1  L2:ins1 vol0 bus1  L3:ins0 vol127 bus2
  v15 L0:ins14 vol56 bus0  L1:ins3 vol127 bus1  L3:ins0 vol127 bus2
  v16 L0:ins9 vol127 bus0  L1:ins3 vol127 bus1  L3:ins0 vol127 bus2
  v17 L0:ins5 vol127 bus0  L2:ins13 vol112 bus1  L3:ins0 vol127 bus2
  v18 L0:ins12 vol52 bus0  L1:ins12 vol52 bus0  L2:ins2 vol127 bus1  L3:ins0 vol127 bus2
  v19 L0:ins8 vol120 bus0  L1:ins21 vol127 bus1  L3:ins0 vol127 bus2
  v20 L0:ins6 vol88 bus0  L1:ins18 vol124 bus1  L3:ins0 vol127 bus2
  v21 L0:ins10 vol127 bus0  L1:ins18 vol127 bus1  L3:ins0 vol127 bus2
```
