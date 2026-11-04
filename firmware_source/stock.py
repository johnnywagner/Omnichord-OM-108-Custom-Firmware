"""The factory firmware versions the mod runs on, and every Suzuki address it uses in each: ONE table.

The mod is written once, against 1.0.2.9 addresses (the decompile, the handbook and every comment use those). This table
gives each of them its address in every supported version:
- FLASH: every stock code / data address the mod calls, patches or checks (fw/mod2.c FL() / FN(), fw/entry.s, fw/mod1.s,
  mkimg.py's patch sites), 1.0.2.9 -> 1.0.3.0;
- RAM: 1.0.3.0 moved most of its RAM by a few bytes (new data / variables); the rule (ranges + offsets) maps any
  1.0.2.9 RAM address (fw sources RA());
- CONST: values (not addresses) that differ;
- INIT: where each version's RAM-init images are (the emulator loads them).
fw/build.sh writes fw/stock_vNN.h from this table (python3 ../lib/stock.py header NN <sources>) and builds the code for
each version; mkimg.py --fw NN patches each version's sites from it; the emulator (lib/emu.py) runs the same tests on
each version through it (tests keep their 1.0.2.9 addresses, lib/fwmap30.json maps all of the stock program).

How the 1.0.3.0 column was found and checked: port/README.md (port/check_table.py re-checks every entry against both
images: same code around each site / at each function, literal pools and branch targets compared through the maps).
A new factory version = a new column here + its map, then every suite on it (OM108_FW=NN bash run_tests.sh)."""
import os, sys, re, json

VERSIONS = {   # SHA-256 of 0x60000000-0x60FFDFFF (= the tool's KNOWN_STOCK / STOCK_VERSIONS); the image in images/
    '29': dict(name='1.0.2.9', fw_sha='ed36679655347814183e36f69bacba8f3a317b30074a7538df55d3bf46de157f', image='stock.bin'),
    '30': dict(name='1.0.3.0', fw_sha='97de517e44c4700190d314480203f535590220271f56afa74085028d0d9e016f', image='stock30.bin'),
}
DEFAULT = '29'

# RAM-init images (table 0x60002278 in both): flash offset, size
INIT = {'29': dict(dtcm=(0x37E3C, 0x9E4), itcm=(0x3617C, 0xA40), ocram=(0x36BBC, 0x1280)),
        '30': dict(dtcm=(0x37F64, 0xA08), itcm=(0x362A4, 0xA40), ocram=(0x36CE4, 0x1280))}

# RAM relocation, 1.0.2.9 -> 1.0.3.0 ([lo, hi) 1.0.2.9 address, + offset). Checked against every RAM literal of the code
# matched between the two (port/check_table.py): DTCM data +0x20 (a new 0x20-byte block at 0x200000F0: the low-volume
# analog mute's sample ring), +0x24 from the version digits' block; BSS +0x20, then +1 / +2 for two new variables
# (0x2000DE44 analog mute flag, 0x2000DE4B its ring index, in 1.0.3.0 addresses). ITCM / OCRAM code and data: unchanged.
# (Nothing of ours uses 0x20000381-0x20000393, the one stretch the literals can't pin down: 0x20000380 is +0x20,
# 0x20000394 +0x24.) The BSS ends at 0x2000DEA4 (1.0.2.9); the rule goes on to 0x2000E000 (free RAM, so a dump of
# 0x20000000-0x2000DFFF reads the same in both); above it (the stack, the emulator's stack) nothing moved.
RAM = {'29': [],
       '30': [(0x200000F0, 0x20000394, 0x20), (0x20000394, 0x200009E4, 0x24), (0x200009E4, 0x2000DE24, 0x20),
              (0x2000DE24, 0x2000DE2A, 0x21), (0x2000DE2A, 0x2000E000, 0x22)]}

# Values that differ between versions (fw sources: the name, from stock_vNN.h)
CONST = {
    # the lowest of the bass player's special row codes (a code below it is a note value). 1.0.3.0's bass player
    # 0x60007C14 treats 0x7B as a rest (MIDI note-off, as 0x7E) and its patterns use it (issue-4 notes item 3)
    'BASS_CODE_LO': {'29': 0x7C, '30': 0x7B},
}

# Flash: 1.0.2.9 address -> 1.0.3.0. 'site' = a stock patch site (mkimg.py), 'fn' = a function / data we call or read,
# 'chk' = an instruction mkimg checks stays stock. (hand) = placed by reading both versions' code (the function changed
# around it), see port/README.md; every other one is the automatic code match, checked by port/check_table.py.
FLASH = {
    # LED / panel display (fn)
    # sensors (our analog mute, fw/mod2.c sense_hook; 1.0.3.0: am_hook at its own 0x60003834, mkimg.py SITES30)
    0x600035A8: 0x6000366C,   # fn store a sensor reading (the sensor loop's call 0x6000376C -> sense_hook on 1.0.2.9)
    0x60003984: 0x60003A4C,   # fn transpose display on the voice row
    0x60003A5C: 0x60003B24,   # fn voice row redraw
    0x60003AD0: 0x60003B98,   # fn master tune display on the pattern row
    0x60003C14: 0x60003CDC,   # fn redraw the LEDs from their flags
    0x60003CE4: 0x60003DAC,   # fn set an LED flag
    0x600047AA: 0x60004872,   # site end of the startup LED animation -> panel_shown
    0x60004EF0: 0x60004FB8,   # fn power-on setting blink (blink_wrap calls it)
    0x60004F70: 0x60005038,   # site end of the setting blink -> panel_shown
    # rhythm player (1.0.3.0 changed the bass player and the tick's code checks: port/README.md)
    0x60007574: 0x6000763C,   # fn row player
    0x60007B34: 0x60007C14,   # fn bass player (hand: push.w r8 added at the start)
    0x60007BD6: 0x60007CFC,   # site bass pitch call 1 in the bass player -> bass_note (hand: the code before it reordered)
    0x60007C1E: 0x60007D1E,   # site bass pitch call 2 -> bass_note (hand-checked)
    0x60007C46: 0x60007D44,   # site bass pitch call 3 -> bass_note (hand-checked)
    0x60007E0E: 0x60007F14,   # site bass pitch call in the tick -> bass_note (hand-checked: uxtb r0 added before it)
    0x60007E58: 0x60007F60,   # site the tick's ldr r4,=DE42 / ldrb -> r27_step_wrap
    0x60007E98: 0x60007FA0,   # site the tick's row player call -> r27_row
    0x60007FFE: 0x60008100,   # site the tick's bass player call -> r27_bass (hand-checked)
    # battery check, power latch
    0x6000825C: 0x60008368,   # site LED write -> bat_led
    0x6000828C: 0x60008398,   # site LED write -> bat_led
    0x6000829C: 0x600083A8,   # site voice row redraw -> bat_row
    0x600082C2: 0x600083CE,   # chk the low-battery shutdown's LED write stays stock
    0x600082F2: 0x600083FE,   # site LED write -> bat_led
    0x600083B0: 0x600084BC,   # site power latch-off -> latch_off
    0x60008418: 0x60008524,   # site power latch-off -> latch_off
    0x600085B0: 0x600086BC,   # fn tune tables A
    0x6000873C: 0x60008848,   # fn tune tables B
    # chords / bass MIDI, Manual / Auto
    0x60008B0C: 0x60008C18,   # fn start the held Manual chord
    0x60008B1A: 0x60008C26,   # chk its Auto check stays stock
    0x60008BAE: 0x60008CBA,   # site its MIDI bass note-on -> ma_hold_on
    0x60008C02: 0x60008D0E,   # site its MIDI bass note-off -> ma_hold_off
    0x60008D38: 0x60008E50,   # fn held Manual chord off
    0x60008D9C: 0x60008EB4,   # chk Auto check stays stock
    0x60008E2C: 0x60008F44,   # fn the rhythm chord's MIDI note-offs
    0x60008E34: 0x60008F4C,   # chk its Auto check stays stock
    0x60008E74: 0x60008F8C,   # fn bass pitch
    0x60008EE6: 0x60008FFE,   # site auto bass MIDI Auto check -> ma_bass_auto
    0x60008F2A: 0x60009042,   # site "
    0x60008F7E: 0x60009096,   # site "
    0x60008FB0: 0x600090C8,   # fn auto bass note off
    0x60008FF4: 0x6000910C,   # site auto bass MIDI Auto check -> ma_bass_auto
    0x600090BE: 0x600091D6,   # chk Auto check stays stock
    0x60009114: 0x6000922C,   # site auto bass MIDI Auto check -> ma_bass_auto
    0x60009260: 0x600093A0,   # fn busy-wait (touch chip set-up)
    # MIDI builders
    0x600096A0: 0x600097E0,   # fn MIDI note-on
    0x600096CA: 0x6000980A,   # site its post -> midi_hook
    0x600096D4: 0x60009814,   # fn MIDI note-off
    0x600096F8: 0x60009838,   # site its post -> midi_hook
    0x60009728: 0x60009868,   # site the CC builder's post -> mod 1
    # strum plates
    0x60009B20: 0x60009C60,   # fn strum scanner (scan_wrap tail-calls it)
    0x6000A2E4: 0x6000A424,   # site plate note-on -> plate_on
    0x6000A47E: 0x6000A5BE,   # site plate note-on -> plate_on
    0x6000A506: 0x6000A646,   # site plate release -> plate_off
    0x6000A52E: 0x6000A66E,   # site plate release -> plate_off
    0x6000AF1A: 0x6000B05A,   # site bank-toggle voice select -> voice_other
    # MIDI presets (mod 1)
    0x6000B22C: 0x6000B36C,   # site preset byte
    0x6000B242: 0x6000B382,   # fn mod 1 returns here
    0x6000B25E: 0x6000B39E,   # site -> mod 1 (b.w 0x600390C0)
    0x6000B262: 0x6000B3A2,   # fn mod 1 returns here
    0x6000B270: 0x6000B3B0,   # site preset byte
    0x6000B2C4: 0x6000B404,   # fn power-on factory-reset check
    0x6000B350: 0x6000B490,   # fn rhythm-tone setup
    0x6000B468: 0x6000B5A8,   # fn chord root set-up (chord decoder)
    # keyboard notes (issue #2)
    0x6000B760: 0x6000B8A0,   # site keyboard note-on -> kb_noteon
    0x6000B9BA: 0x6000BAFA,   # site keyboard note-on -> kb_noteon
    0x6000B9DE: 0x6000BB1E,   # site keyboard MIDI note-on -> kb_mon
    0x6000BA04: 0x6000BB44,   # site keyboard MIDI note-on -> kb_mon
    0x6000BB6A: 0x6000BCAA,   # site keyboard MIDI note-off -> kb_moff
    0x6000BB8C: 0x6000BCCC,   # site "
    0x6000BC6E: 0x6000BDAE,   # site "
    0x6000BC94: 0x6000BDD4,   # site "
    0x6000BD8C: 0x6000BECC,   # fn strum-plate notes from the chord (strum_wrap)
    0x6000C090: 0x6000C1D0,   # site its call -> strum_wrap
    0x6000C1A4: 0x6000C2E4,   # fn P / T release display (1.0.3.0: also redraws the bank in normal mode, port/README.md)
    # the button handler (1.0.3.0 changed it most: every site here hand-checked)
    0x6000C274: 0x6000C3C0,   # fn button handler
    0x6000C2F4: 0x6000C444,   # site key-matrix read -> row_read (its [sp,#0x1c] = the row, as in 1.0.2.9)
    0x6000CBD2: 0x6000CCB0,   # site keyboard note-on -> kb_noteon (hand: one of the two "eor #1" note-ons, code moved)
    0x6000CCFE: 0x6000CEB6,   # site chord decoder call -> chord_dec
    0x6000CFCC: 0x6000D19E,   # site the Manual branch's 0x60008E2C call -> ma_chord_off (hand: same branch, r7 for r8)
    0x6000D284: 0x6000D3CE,   # site keyboard note-on -> kb_noteon
    0x6000D3BE: 0x6000D5A2,   # site voice button 2 -> voice_btn
    0x6000D406: 0x6000D5EA,   # site voice button 1 -> voice_btn
    0x6000D492: 0x6000D676,   # site voice button 5 -> voice_btn
    0x6000D4D6: 0x6000D6BA,   # site voice button 4 -> voice_btn
    0x6000D51A: 0x6000D55E,   # site voice button 3 -> voice_btn (hand: its block moved before button 2's)
    0x6000D71E: 0x6000CE84,   # site keyboard note-on -> kb_noteon (hand: the other "eor #1" note-on, code moved)
    0x6000D750: 0x6000D894,   # site chord decoder call -> chord_dec
    0x6000D8D0: 0x6000D9F8,   # fn Auto?
    0x6000D8F8: 0x6000DA20,   # fn MIDI notes off for chord / bass?
    0x6000DB28: 0x6000DC50,   # fn MIDI queue post
    0x6000E1B4: 0x6000E2DC,   # site the mixer's final memcpy -> mix_tail
    0x6000E7DC: 0x6000E904,   # fn select voice slot
    0x6000E850: 0x6000E978,   # fn layer release mode for all voices
    # control task, MIDI task, main, touch task
    0x6000EA9E: 0x6000EBC6,   # site power-on blink call -> blink_wrap
    0x6000EB3C: 0x6000EC64,   # site button handler call -> c274_wrap
    0x6000EB50: 0x6000EC78,   # site strum scanner call -> scan_wrap
    0x6000EC70: 0x6000ED98,   # site realtime-message check (ldrb r3,[r7]; cmp r3,#4) -> clk_gate
    0x6000EC74: 0x6000ED9C,   # chk its beq stays
    0x6000F284: 0x6000F3AC,   # site factory-reset check call -> boot_b2c4
    0x6000F2A0: 0x6000F3C8,   # site rhythm-tone setup call -> rhythm_setup
    0x6000F2D4: 0x6000F3FC,   # site internal MIDI preset forced to 3 (mod 1)
    0x6000F390: 0x6000F4B8,   # site LED task's xTaskCreate stack args -> led_prio
    0x6000F394: 0x6000F4BC,   # chk its bl xTaskCreate stays
    0x6000F518: 0x6000F640,   # fn LED port write
    0x6000F90C: 0x6000FA34,   # fn palm check (touch task)
    0x6000FB76: 0x6000FC9E,   # site its call -> touch_hook
    0x60011564: 0x6001168C,   # fn xTaskCreate
    0x60012888: 0x600129B0,   # fn LPSPI transfer
    0x60012C9C: 0x60012DC4,   # fn D-cache invalidate
    0x600166D0: 0x600167F8,   # site QTMR_SetupPwm entry -> pwm_hook
    0x600166D4: 0x600167FC,   # fn QTMR_SetupPwm after the replaced 4 bytes
    0x600181D6: 0x600182FE,   # fn memcpy
    # veneers to ITCM / OCRAM (1.0.3.0 orders them differently; same targets)
    0x600186C8: 0x600187B0,   # fn all strum channels off
    0x600186D8: 0x60018800,   # fn note-on
    0x600186E0: 0x60018838,   # fn master tune
    0x600186E8: 0x60018848,   # fn flash page program
    0x60018708: 0x60018808,   # fn strum slot release
    0x60018720: 0x60018810,   # fn flash sector erase
    # data
    0x600332C0: 0x600333E8,   # fn key-matrix column bit table
}
COLUMN = {'29': None, '30': FLASH}

def fl(a, v=DEFAULT):
    """1.0.2.9 flash address a (0x6000xxxx, even) in version v"""
    if v == '29' or not (0x60002000 <= a < 0x60039000): return a
    if a not in COLUMN[v]: raise KeyError('0x%08X has no 1.0.%s.%s address in lib/stock.py' % (a, v[0], v[1]))
    return COLUMN[v][a]
def ra(a, v=DEFAULT):
    """1.0.2.9 RAM address a in version v"""
    for lo, hi, d in RAM[v]:
        if lo <= a < hi: return a + d
    return a
def version_of(img):
    """the factory firmware version an image (stock or modded) is built on, from its RAM-init table (0x60002278)"""
    import struct
    for v, d in INIT.items():
        if struct.unpack_from('<3I', img, 0x2278) == (0x60000000 + d['dtcm'][0], 0x20000000, d['dtcm'][1]): return v
    raise ValueError('not a known OM-108 factory firmware version')
def addr(a, v=DEFAULT):
    """any 1.0.2.9 address (stock program flash 0x60002000-0x60038FFF, or RAM) in version v; others unchanged"""
    if v == '29': return a
    if 0x60002000 <= a < 0x60039000:
        t = fmap((a & ~1) - 0x60000000, v)
        if t is None: raise KeyError('0x%08X has no counterpart in factory firmware %s' % (a, VERSIONS[v]['name']))
        return t + 0x60000000 | (a & 1)
    return ra(a, v)
_INV = {}
def back(a, v=DEFAULT):
    """a version v RAM or stock program address -> the 1.0.2.9 one (None: one of v's own new bytes / unmatched code);
    anything else unchanged"""
    if v == '29': return a
    if 0x60002000 <= a < 0x60039000:
        if v not in _INV:
            import bisect
            inv = sorted((lo + d, hi + d, -d) for lo, hi, d in prog_map(v))
            _INV[v] = (inv, [r[0] for r in inv], {t: f for f, t in COLUMN[v].items()})
        inv, keys, tab = _INV[v]; o = (a & ~1) - 0x60000000
        if (a & ~1) in tab: return tab[a & ~1] | (a & 1)
        import bisect
        k = bisect.bisect_right(keys, o) - 1
        return (o + inv[k][2] + 0x60000000) | (a & 1) if k >= 0 and inv[k][0] <= o < inv[k][1] else None
    if not (0x20000000 <= a < 0x20020000): return a
    for lo, hi, d in RAM[v]:
        if lo + d <= a < hi + d: return a - d
    if any(lo <= a < lo + d for lo, hi, d in RAM[v]) and a >= RAM[v][0][0]: return None
    return a
def chunks(a, n, v=DEFAULT):
    """the 1.0.2.9 range [a, a + n) as [(version v address, length)]: split where the RAM rule's offset changes, so a
    block read of a 1.0.2.9 range gives its bytes in 1.0.2.9 order (a version's new bytes in between left out)"""
    if v == '29' or not RAM[v] or not (0x20000000 <= a < 0x20020000): return [(addr(a, v), n)]
    cuts = sorted({lo for lo, hi, d in RAM[v]} | {hi for lo, hi, d in RAM[v]})
    out = []
    while n > 0:
        nxt = next((c for c in cuts if c > a), a + n); k = min(n, nxt - a)
        out.append((ra(a, v), k)); a += k; n -= k
    return out
def const(name, v=DEFAULT): return CONST[name][v]
def dtcm_flash(a, v=DEFAULT):
    """flash offset of DTCM init data for 1.0.2.9 RAM address a (e.g. the voice table 0x200003AC) in version v"""
    o, n = INIT[v]['dtcm']; r = ra(a, v) - 0x20000000; assert 0 <= r < n, hex(a)
    return o + r
VT_RAM = 0x200003AC                 # the voice table (22 voices x 4 layers x 16 B), copied from flash at power-on
def vt_flash(v=DEFAULT): return dtcm_flash(VT_RAM, v)

# ---- the whole program map (the emulator, tests): any 1.0.2.9 flash offset -> version v (port/mkmap.py writes it)
_MAPS = {}
def prog_map(v):
    """[(lo, hi, delta)] of flash offsets 0x2000-0x39000 (1.0.2.9) for version v; None for 1.0.2.9"""
    if v == '29': return None
    if v not in _MAPS:
        _MAPS[v] = [tuple(r) for r in json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fwmap%s.json' % v)))['runs']]
    return _MAPS[v]
def fmap(o, v):
    """any 1.0.2.9 flash offset o in version v (the table first, then the map; None if it has no counterpart)"""
    if v == '29' or not (0x2000 <= o < 0x39000): return o
    if (o + 0x60000000) in COLUMN[v]: return COLUMN[v][o + 0x60000000] - 0x60000000
    import bisect
    runs = prog_map(v); k = bisect.bisect_right(runs, (o, 1 << 40, 0)) - 1
    if k >= 0 and runs[k][0] <= o < runs[k][1]: return o + runs[k][2]
    return None

# ---- the build header: fw/stock_vNN.h
def header(v, sources):
    """#defines for every FL(x) / FN(x) / RA(x) the sources use, and CONST, for version v"""
    txt = ''.join(open(p).read() for p in sources)
    fls = sorted(set(re.findall(r'\bF[LN]\(([0-9A-F]{8})\)', txt))); ras = sorted(set(re.findall(r'\bRA\(([0-9A-F]{8})\)', txt)))
    out = ['/* generated by lib/stock.py for factory firmware %s: do not edit */' % VERSIONS[v]['name'],
           '#ifdef __ASSEMBLER__', '#define U_(x) x', '#else', '#define U_(x) x##u', '#endif',
           '#define FL(a) FL_##a', '#define FN(a) (FL_##a | U_(1))', '#define RA(a) RA_##a', '#define FW_VERSION %s' % v]
    for a in fls:
        x = int(a, 16); assert not x & 1, a
        out.append('#define FL_%s U_(0x%08X)' % (a, fl(x, v)))
    for a in ras: out.append('#define RA_%s U_(0x%08X)' % (a, ra(int(a, 16), v)))
    for k, d in sorted(CONST.items()): out.append('#define %s 0x%X' % (k, d[v]))      # (plain int: compared with signed values)
    return '\n'.join(out) + '\n'

if __name__ == '__main__':
    if sys.argv[1] == 'header': sys.stdout.write(header(sys.argv[2], sys.argv[3:]))
    elif sys.argv[1] == 'versions': print(' '.join(VERSIONS))
