#!/usr/bin/env python3
"""OM-108 patcher: installs the OM-108 mod on a Suzuki Omnichord OM-108, and puts the original firmware back.

It never contains Suzuki's firmware: it first makes a backup of YOUR unit (read only), and every install is built from
that backup plus the mod's own bytes (om108_mod.patch). Keep the backup safe: it is what puts your unit back to stock.

  python3 om108_patcher.py backup            1. read the whole chip into backups/ (once per unit; read only)
  python3 om108_patcher.py install           2. install the mod
  python3 om108_patcher.py restore           put the original firmware back (from the backup)
  python3 om108_patcher.py check             read only: says what is on the unit
  python3 om108_patcher.py scan              read only: reads the whole unit, says what is on it, keeps the copy in
                                             work/last_read.bin (and saves it as the backup if it is stock and none exists)
  python3 om108_patcher.py import-backup F   use an existing full dump F (16 MB) as the backup

Before each: rear switch to UPDATE, USB cable in, then hold the power button until the patcher says DONE. If it stops
(button let go, cable out), just run the same command again: it carries on safely.

Needs Python 3.9+ and NXP's SPSDK (pip install spsdk), Mac / Windows / Linux, and NXP's flashloader file
ivt_flashloader_rt1020.bin in the same folder. Everything it reads and writes stays in the tool's folder: the tool's own
files in assets/ (this script, the recipe, the flashloader), the unit's files in my_unit/ (backups, dumps, work, logs)."""
import sys, os, json, struct, zlib, hashlib, time, argparse, io, contextlib, shutil, subprocess
from pathlib import Path

TOOL_VERSION = '1.1'
HERE = Path(__file__).resolve().parent
UNIT_DIR = HERE.parent / 'my_unit' if HERE.name in ('assets', '.assets') else HERE      # the unit's files (the tool folder's my_unit/)
SIZE, BOOT_END, FW_END, SECTOR, ERASE_MAX, BASE = 0x1000000, 0x2000, 0xFFE000, 0x1000, 0x8000, 0x60000000
SDP_ID, MB_ID = '0x1fc9:0x0130', '0x15a2:0x0073'
# the stock firmware this mod was built on: SHA-256 of 0x60000000-0x60FFDFFF (the part every unit shares; the top 8 KB
# holds each unit's own settings and is never written by this tool)
KNOWN_STOCK = {'ed36679655347814183e36f69bacba8f3a317b30074a7538df55d3bf46de157f': 'OM-108 stock firmware 1.0.2.9'}
# every factory version seen so far, by the same hash: its version digits (the stock test mode shows 1.0.2.9 / 1.0.3.0),
# which name the backup (om108_backup_v29.bin). Only KNOWN_STOCK can be installed on; 1.0.3.0 (issue #4) not yet.
STOCK_VERSIONS = {'ed36679655347814183e36f69bacba8f3a317b30074a7538df55d3bf46de157f': '29',
                  '97de517e44c4700190d314480203f535590220271f56afa74085028d0d9e016f': '30'}
FLASHLOADER = ('ivt_flashloader_rt1020.bin', 'bce4ccff076ca397dab2ffa72f260fcc8c7784a71ee532038cbdb4d2e4f0024a')
RW_FCB = ('omnichord_rw_fcb.bin', '770ac368d4076745a9303772bc783ac2fdead605e8764ec9e1325d7c33fdd8ce')      # writes
RO_FCB = ('omnichord_readonly_fcb.bin', 'fc3652b343f399998369198c2d70c53067044f211323af603a68b43e611e79b1')  # backup / check
RECIPE = 'om108_mod.patch'      # made by the dev kit's make_recipe.py from the release (its name is inside)
def ident_sectors(regs):
    """Sectors the mod never changes, read before any write to prove the unit holds the backup's firmware: every
    untouched sector of the stock program (0x60002000-0x60038FFF, where another firmware version would differ) plus 16
    spread over the rest of the chip."""
    rset = {s for a, n in regs for s in range(a, a + n, SECTOR)}
    code = [s for s in range(BOOT_END, 0x39000, SECTOR) if s not in rset]
    rest = [s for s in range(0x39000, FW_END, SECTOR) if s not in rset]
    return code + rest[::max(1, len(rest) // 16)][:16]
sha = lambda b: hashlib.sha256(b).hexdigest()
REPORT = 'https://github.com/johnnywagner/Omnichord-OM-108-Custom-Firmware/issues'
OTHER_STOCK = 'a factory firmware version this tool does not support yet'
def other_stock(d):
    """An unknown firmware with no sign of a mod (the area every mod writes, 0x60039000-0x60047FFF, is blank, as on a
    stock unit): most likely a factory firmware version other than KNOWN_STOCK. The mod patches fixed addresses, so it is
    never installed on one (issue #4: a revision whose code moved by up to 0x1E4 bytes). The top of the chip, where the
    mod puts samples (0x60DA0000-), must be blank too, so a cut install or restore is not taken for one."""
    return sha(d[:FW_END]) not in KNOWN_STOCK and d[0x39000:0x48000] in (b'\xff' * 0xF000, b'\x00' * 0xF000) \
        and d[0xDA0000:FW_END] == b'\xff' * (FW_END - 0xDA0000)
def stock_version(b): return STOCK_VERSIONS.get(sha(b[:FW_END]))
def backup_name(v): return f'om108_backup_v{v}.bin'
def find_backup(bdir):
    """The stock firmware file in bdir (None if there is none): om108_backup_v<version>.bin of a supported version. A
    backup made before the names carried the version (om108_backup.bin) is renamed once it checks as stock; any other
    file of that name keeps it (and is reported as not stock)."""
    bdir = Path(bdir)
    for v in sorted(STOCK_VERSIONS[h] for h in KNOWN_STOCK):
        if (bdir / backup_name(v)).exists(): return bdir / backup_name(v)
    old = bdir / 'om108_backup.bin'
    if not old.exists(): return None
    try: b = old.read_bytes()
    except OSError: return old
    if len(b) == SIZE and sha(b[:FW_END]) in KNOWN_STOCK:
        new = bdir / backup_name(stock_version(b)); os.replace(old, new); return new
    return old
BLANK = {}
def blank_sha(n):
    if n not in BLANK: BLANK[n] = sha(b'\xff' * n)
    return BLANK[n]

class Reconnect(Exception):
    """1.1: the flashloader was started but this process cannot see it (seen on a Mac with Python 3.15: its USB device list
    never refreshed, while a new process found the flashloader at once). Raised before anything is read or written;
    main() runs the same command again in a fresh process."""

class Stop(Exception):
    """A safe stop: the message says what to do. Nothing unsafe has happened."""

# ------------------------------------------------------------------------------------------------ files and the recipe
def find_file(name, want_sha):
    for d in (HERE,):                    # self-contained: only the patcher's own folder
        p = d / name
        if p.exists():
            if sha(p.read_bytes()) != want_sha:
                raise Stop(f'{p} is not the expected file (its checksum differs). Use the original file.')
            return p
    raise Stop(f'{name} not found in the tool\'s files ({HERE}). Copy the whole tool folder again (with its assets folder).')

def load_recipe(path):
    raw = zlib.decompress(Path(path).read_bytes())
    if raw[:8] != b'OM108RCP': raise Stop(f'{path} is not a patch file')
    jl = struct.unpack_from('<I', raw, 8)[0]; hdr = json.loads(raw[12:12 + jl]); o = 12 + jl
    if hdr.get('format') != 2: raise Stop(f'{path} is for another version of this tool')
    def take(n_runs):
        nonlocal o
        runs = [struct.unpack_from('<II', raw, o + 8 * i) for i in range(n_runs)]; o += 8 * n_runs
        return runs
    runs = take(hdr['runs']); data = raw[o:o + hdr['data_len']]; o += hdr['data_len']
    if len(data) != hdr['data_len'] or sha(data) != hdr['data_sha256']: raise Stop(f'{path} is damaged')
    for L in hdr['legacy']:                  # earlier releases this tool recognises (only their own bytes)
        L['_runs'] = take(L['runs']); L['_data'] = raw[o:o + L['data_len']]; o += L['data_len']
        if sha(L['_data']) != L['data_sha256']: raise Stop(f'{path} is damaged')
    if o != len(raw): raise Stop(f'{path} is damaged')
    return hdr, runs, data

def put_runs(img, runs, data):
    o = 0
    for a, n in runs:
        assert BOOT_END <= a and a + n <= FW_END
        img[a:a + n] = data[o:o + n]; o += n

# ------------------------------------------------------------------------------------------------- the settings
# The window's choices (settings.ini) as a dict: midi {part: {channel: 1-16 or None (Off), presets: [P1..P5 0/1]}},
# clock [P1..P5], subs [None (Omni 1) or {sub, alt}] per voice (the sound numbers in SUB_NAMES), and the switches.
PARTS = ['Main', 'Chord', 'Bass', 'Sub', 'Drums']
PART_CH = {'Main': 0, 'Chord': 1, 'Bass': 2, 'Sub': 3, 'Drums': 9}      # mod 1's internal channels
SUB_NAMES = {1: 'Evolve', 2: 'Mellow Pad', 3: 'Sine Pad', 7: 'Church Bell', 11: 'Rezy', 15: 'Hand Pan', 17: 'Dial Up',
             18: 'Strings', 19: 'Strings 2', 20: '7th Doo', 21: 'Dah Guys'}
SWITCHES = ['modulation', 'extras', 'spoken_names', 'drum_map', 'electric_harp', 'better_clap']   # settings block bits 0-5
def default_settings():
    return {'midi': {'Main': {'channel': 8, 'presets': [0, 0, 1, 1, 1]}, 'Chord': {'channel': 9, 'presets': [1, 0, 1, 1, 1]},
                     'Bass': {'channel': 7, 'presets': [0, 0, 0, 1, 1]}, 'Sub': {'channel': 9, 'presets': [0, 1, 0, 0, 0]},
                     'Drums': {'channel': 1, 'presets': [0, 0, 0, 0, 1]}},
            'clock': [0, 0, 0, 1, 1],
            'subs': [None] + [{'sub': n, 'alt': a} for n, a in zip([3, 17, 3, 3, 3, 2, 21, 18, 18], [2, 1, 7, 19, 2, 11, 20, 15, 2])],
            **{k: True for k in SWITCHES}}
def norm_settings(d):
    """A clean copy of d, checked (Stop if anything is out of range)."""
    try:
        out = {'midi': {}, 'clock': [int(bool(x)) for x in d['clock']], 'subs': [None]}
        for p in PARTS:
            c = d['midi'][p]['channel']
            if c is not None and not (isinstance(c, int) and 1 <= c <= 16): raise ValueError(p)
            out['midi'][p] = {'channel': c, 'presets': [int(bool(x)) for x in d['midi'][p]['presets']]}
            if len(out['midi'][p]['presets']) != 5: raise ValueError(p)
        if len(out['clock']) != 5 or len(d['subs']) != 10: raise ValueError('clock / subs')
        for s in d['subs'][1:]:
            if s['sub'] not in SUB_NAMES or s['alt'] not in SUB_NAMES: raise ValueError('sub')
            out['subs'].append({'sub': int(s['sub']), 'alt': int(s['alt'])})
        for k in SWITCHES: out[k] = bool(d[k])
        return out
    except (KeyError, TypeError, ValueError, AttributeError) as e:
        raise Stop(f'the settings could not be read ({e}); Suggested Defaults sets them all back')
GUITAR = 5
def fw_sub(v, n): return 13 if v == GUITAR and n == 3 else n     # the guitar plays Sine Pad from its own copy (instrument 13)
CFG_OFF, CFG_MAGIC = 0x47000, 0x31434D4F                      # the settings block (mod2.c CFG); its sector holds nothing else
def settings_block(d):
    b = bytearray(64)
    struct.pack_into('<IB', b, 0, CFG_MAGIC, 1)
    b[5] = sum(1 << i for i, k in enumerate(SWITCHES) if d[k])
    b[6] = sum(1 << i for i, x in enumerate(d['clock']) if x)
    for v in range(1, 10): b[8 + v] = fw_sub(v, d['subs'][v]['sub']); b[18 + v] = fw_sub(v, d['subs'][v]['alt'])
    for i, p in enumerate(PARTS):
        b[28 + i] = d['midi'][p]['channel'] or 0; b[33 + i] = sum(1 << j for j, x in enumerate(d['midi'][p]['presets']) if x)
    struct.pack_into('<I', b, 60, CFG_MAGIC ^ 0xFFFFFFFF)
    return bytes(b)
def settings_from_block(b):
    """The settings a block on a unit holds, or None (no block / not one of ours)."""
    if len(b) < 64 or struct.unpack_from('<I', b, 0)[0] != CFG_MAGIC or b[4] != 1 or struct.unpack_from('<I', b, 60)[0] != CFG_MAGIC ^ 0xFFFFFFFF:
        return None
    un = lambda v, n: 3 if v == GUITAR and n == 13 else n
    try:
        d = {'midi': {p: {'channel': b[28 + i] or None, 'presets': [(b[33 + i] >> j) & 1 for j in range(5)]} for i, p in enumerate(PARTS)},
             'clock': [(b[6] >> j) & 1 for j in range(5)],
             'subs': [None] + [{'sub': un(v, b[8 + v]), 'alt': un(v, b[18 + v])} for v in range(1, 10)],
             **{k: bool((b[5] >> i) & 1) for i, k in enumerate(SWITCHES)}}
        d = norm_settings(d)
        return d if settings_block(d) == bytes(b[:64]) else None
    except Stop: return None
# where the settings go (flash offsets; the dev kit's firmware tables)
ROUTE, DRUMS = 0x39100, 0x39180           # mod 1: route[preset][internal channel] = MIDI channel - 1 (0xFF = not sent); drum note map
VT_FLASH = 0x381E8                         # the voice table (copied to RAM at power-on): slot * 0x40 + layer * 16
SUB_OFF = [0, 0x11, 0x11, 0x11, 0x11, 0x21, 0x21, 0x11, 0x11, 0x11]   # each voice's sub instrument byte (= mod2.c)
BOOST_L = [0, 2, 2, 2, 2, 1, 0, 2, 2, 2]                               # Evolve's boost layer per voice (0 = none: FM Piano)
def boost_vol(sub_vol): return (sub_vol * 52 + 63) // 127                # = mod2.c boost_set
def build_image(backup, recipe, d):
    """The firmware for settings d: the Suggested Default (the recipe) with the choices applied. Only the owner's
    backup supplies stock bytes (the stock clap / harp, the stock voice-table bytes)."""
    hdr, runs, data = recipe
    if sha(backup[:FW_END]) != hdr['stock_fw_sha256']: raise Stop('the backup is not the firmware this mod was made for')
    d = norm_settings(d)
    img = bytearray(backup); put_runs(img, runs, data)
    if sha(img[:FW_END]) != hdr['target_fw_sha256']: raise Stop('internal error: the result does not match the mod')
    if d == default_settings(): return bytes(img)
    for p in range(5):                                            # MIDI routing
        for part, ch in PART_CH.items():
            m = d['midi'][part]; img[ROUTE + 16 * p + ch] = (m['channel'] - 1) if m['channel'] and m['presets'][p] else 0xFF
    if not d['drum_map']: img[DRUMS:DRUMS + 128] = bytes(range(128))           # the stock drum notes
    for v in range(1, 10):                                        # subs, and Evolve's boost layer
        n, a = fw_sub(v, d['subs'][v]['sub']), fw_sub(v, d['subs'][v]['alt'])
        for slot in ([v] if v == 1 else [v, v + 12]):
            base = VT_FLASH + slot * 0x40; img[base + SUB_OFF[v]] = n
            if BOOST_L[v]:
                L = base + BOOST_L[v] * 16
                if 1 in (n, a):
                    lay = bytearray(img[base + SUB_OFF[v] - 1:base + SUB_OFF[v] + 15]); lay[1] = 1
                    lay[13] = boost_vol(lay[13]) if n == 1 else 0
                    img[L:L + 16] = lay
                else: img[L:L + 16] = backup[L:L + 16]
    if not d['better_clap']: a, b = hdr['clap']; img[a:b] = backup[a:b]
    if not d['electric_harp']:
        H = hdr['harp_stock']
        for src, n, dst in H['moves']: img[dst:dst + n] = backup[src:src + n]
        for adr, val in H['words']: struct.pack_into('<I', img, adr, val)
    img[CFG_OFF:CFG_OFF + 64] = settings_block(d)
    return bytes(img)
def legacy_images(backup, recipe):
    """[(name, image)] for the earlier releases this tool knows (the Suggested Default image with their own bytes)."""
    hdr, runs, data = recipe; out = []
    base = bytearray(build_image(backup, recipe, default_settings()))
    for L in hdr['legacy']:
        if L.get('code_only'): continue
        img = bytearray(base)
        for a, n in L['stock']: img[a:a + n] = backup[a:a + n]
        put_runs(img, L['_runs'], L['_data'])
        if sha(img[:FW_END]) != L['target_fw_sha256']: raise Stop('internal error: an earlier release does not match')
        out.append((L['name'], bytes(img)))
    return out

def code_variants(backup, recipe, img):
    """The same settings firmware as img with the code of each earlier settings build this tool knows (a unit installed
    with an earlier version of this tool): [(name, image)]."""
    out = []
    for L in recipe[0]['legacy']:
        if not L.get('code_only'): continue
        v = bytearray(img)
        for a, n in L['stock']: v[a:a + n] = backup[a:a + n]
        put_runs(v, L['_runs'], L['_data']); out.append((L['name'], bytes(v)))
    return out

def regions_for(recipe):
    """Every flash region any choice of settings (or a release this tool knows) can touch, in install order (proven on
    the unit): sample data first, then our code area with the entry table 0x6003A000 last, then the stock code sectors,
    then the settings block, which says what is installed, last of all. Each region is at most 0x8000 bytes and erased in
    one go, so wherever a run is cut, each region is old, blank or new. The same list for every settings choice."""
    secs = sorted(recipe[0]['sectors'])
    regs = []
    for s in secs:
        if s >= 0xB0000 and regs and regs[-1][1] == s and regs[-1][1] - regs[-1][0] < ERASE_MAX: regs[-1][1] = s + SECTOR
        else: regs.append([s, s + SECTOR])
    def key(r):
        a = r[0]
        if a == CFG_OFF: return (3, a)
        if a >= 0x48000: return (0, a)
        if 0x39000 <= a < 0x48000: return (1, 1 if a == 0x3A000 else 0, a)
        return (2, a)
    regs.sort(key=key)
    return [(a, b - a) for a, b in regs]
def restore_order(regs):
    """Restore: the install order reversed, but the settings block still last (it says what the rest is until the end)."""
    r = [x for x in regs[::-1] if x[0] != CFG_OFF]
    return r + [x for x in regs if x[0] == CFG_OFF]

# --------------------------------------------------------------------------------------------- talking to the unit
class Tools:
    """Runs NXP's sdphost / blhost commands (the same commands the hand-made packages used on this unit), in this
    Python process when SPSDK imports, else as separate programs."""
    def __init__(self, log):
        self.log = log; self.mods = None
        try:
            from spsdk.apps import blhost, sdphost
            self.mods = {'blhost': blhost.main, 'sdphost': sdphost.main}
        except Exception as e:
            self.log(f'(SPSDK not importable here: {e}; using the blhost / sdphost programs)')
            for t in ('blhost', 'sdphost'):
                if not self._exe(t): raise Stop('NXP SPSDK is not installed. Install it with:  pip install spsdk')
    def _exe(self, t):
        p = shutil.which(t)
        if not p:
            for d in (Path(sys.executable).parent, Path(sys.executable).parent / 'Scripts'):
                for n in (t, t + '.exe'):
                    if (d / n).exists(): return str(d / n)
        return p
    def run(self, tool, args):
        t0 = time.time()
        if self.mods:
            out = io.StringIO(); ok = True
            try:
                with contextlib.redirect_stdout(out), contextlib.redirect_stderr(out):
                    r = self.mods[tool](list(args), standalone_mode=False)
                ok = not r
            except SystemExit as e: ok = not e.code
            except Exception as e: ok = False; out.write(f'{type(e).__name__}: {e}')
            text = out.getvalue()
        else:
            p = subprocess.run([self._exe(tool)] + list(args), capture_output=True, text=True)
            ok = p.returncode == 0; text = p.stdout + p.stderr
        self.log(f'{tool} {" ".join(args)} -> {"ok" if ok else "FAILED"} ({time.time() - t0:.2f} s) {text.strip()[-200:] if not ok else ""}')
        return ok, text

class Unit:
    """All flash access goes through here. Hard rules, checked before anything is sent: never the boot area
    (0x60000000-0x60001FFF), never the records / settings (0x60FFE000-0x60FFFFFF), erases of at most 0x8000 in whole
    4 KB sectors (the flashloader silently skips 64 KB block erases)."""
    def __init__(self, tools, work, log):
        self.t, self.work, self.log = tools, work, log; work.mkdir(parents=True, exist_ok=True)
    def _bl(self, *a):
        return self.t.run('blhost', ['-u', MB_ID] + list(a))
    def connect(self, fcb):
        loader = find_file(*FLASHLOADER); fcbp = find_file(*fcb)
        ok, _ = self.t.run('sdphost', ['-u', SDP_ID, 'error-status'])
        if ok:
            ok, _ = self.t.run('sdphost', ['-u', SDP_ID, 'write-file', '0x20209000', str(loader)])
            if not ok: raise Stop('could not load the flashloader into the unit. Power it off, check UPDATE + USB, try again.')
            self.t.run('sdphost', ['-u', SDP_ID, 'jump-address', '0x20209000'])     # it often reports an error: normal
        for i in range(20):
            ok, _ = self._bl('get-property', '1')
            if ok: break
            if (i >= 3 and getattr(self.t, 'mods', None) and not os.environ.get('OM108_FRESH')):
                raise Reconnect()               # in-process USB list may be stale: a fresh process will see the flashloader
            time.sleep(1)
        else:
            raise Stop('the unit was not found. Rear switch on UPDATE, USB cable in, hold the power button, then run again.')
        if not (self._bl('write-memory', '0x2000', str(fcbp))[0] and self._bl('configure-memory', '9', '0x2000')[0]):
            raise Stop('could not set up the flash chip. Power off, then run again.')
    def read(self, off, n):
        out = bytearray()
        while n:
            k = min(n, 0x40000); f = self.work / 'read.bin'
            if f.exists(): f.unlink()
            ok, _ = self._bl('read-memory', '0x%08X' % (BASE + off), '0x%X' % k, str(f))
            d = f.read_bytes() if ok and f.exists() else b''
            if len(d) != k: raise Stop('reading the unit failed (button let go or cable?). Run the same command again.')
            out += d; off += k; n -= k; self._work(k)
        return bytes(out)
    allow_boot = False                   # only the Full Factory Restore (restore --full) may write the start-up area
    work_cb = None                       # progress: called with the work done (bytes read; erased x 0.5; written)
    def _work(self, n):
        if self.work_cb: self.work_cb(n)
    def _guard(self, off, n):
        lo = 0 if self.allow_boot else BOOT_END
        if not (lo <= off and off + n <= FW_END and n > 0): raise AssertionError('refused: protected area 0x%X+0x%X' % (off, n))
    def erase(self, off, n):
        self._guard(off, n)
        if n > ERASE_MAX or off % SECTOR or n % SECTOR: raise AssertionError('refused: bad erase 0x%X+0x%X' % (off, n))
        if not self._bl('flash-erase-region', '0x%08X' % (BASE + off), '0x%X' % n, '9')[0]:
            raise Stop('an erase failed (button let go or cable?). Run the same command again.')
        self._work(n // 2)
    def write(self, off, data):
        self._guard(off, len(data)); f = self.work / 'write.bin'; f.write_bytes(data)
        if not self._bl('write-memory', '0x%08X' % (BASE + off), str(f), '9')[0]:
            raise Stop('a write failed (button let go or cable?). Run the same command again.')
        self._work(len(data))

def read_regions(u, regs):
    """Read every region, joining neighbours into reads of up to 256 KB (fewer USB commands)."""
    have = {}; spans = []
    for a, n in sorted(regs):
        if spans and spans[-1][1] == a and spans[-1][1] - spans[-1][0] < 0x40000: spans[-1][1] = a + n
        else: spans.append([a, a + n])
    for a, b in spans:
        d = u.read(a, b - a)
        for ra, rn in regs:
            if a <= ra < b: have[ra] = d[ra - a:ra - a + rn]
    return have

# ---------------------------------------------------------------------------------------------------- the commands
class Patcher:
    def __init__(self, folder, tools_factory=None, out=print):
        self.dir = Path(folder); self.dir.mkdir(parents=True, exist_ok=True); self.bdir = self.dir / 'backups'; self.out = out
        self.backup_file = find_backup(self.bdir) or self.bdir / backup_name('29'); self.state_file = self.bdir / 'om108_state.json'
        (self.dir / 'logs').mkdir(parents=True, exist_ok=True)
        self.logf = open(self.dir / 'logs' / time.strftime('patcher_%Y%m%d.log'), 'a')
        try: from importlib.metadata import version as _v; sv = _v('spsdk')
        except Exception: sv = 'not installed'
        self.log(f'--- om108 patcher {TOOL_VERSION}, {time.strftime("%Y-%m-%d %H:%M:%S")}, {sys.platform}, python {sys.version.split()[0]}, spsdk {sv}'
                 + (' (fresh process after loading the flashloader)' if os.environ.get('OM108_FRESH') else ''))
        self.tools_factory = tools_factory or (lambda log: Tools(log))
        self.t0 = time.time()
    def log(self, s): self.logf.write(s + '\n'); self.logf.flush()
    def say(self, s): self.out(s); self.log('SAY ' + s)
    def elapsed(self): t = int(time.time() - self.t0); return f'{t // 60}:{t % 60:02d}'
    prog_out = None                      # the window: a line '@@PROGRESS <percent>' each time the whole percent goes up
    def plan(self, total):
        """Progress: the work still to come is `total` (bytes read; erased x 0.5; written); never goes backwards."""
        self.p_total = getattr(self, 'p_done', 0) + max(1, total); self.p_done = getattr(self, 'p_done', 0); self._did(0)
    def _did(self, n):
        self.p_done = getattr(self, 'p_done', 0) + n
        if self.prog_out and getattr(self, 'p_total', 0):
            pct = min(99, int(100 * self.p_done / self.p_total))
            if pct > getattr(self, 'p_shown', -1): self.p_shown = pct; self.prog_out(pct)
    def unit(self, fcb):
        u = Unit(self.tools_factory(self.log), self.dir / 'work', self.log); u.work_cb = self._did; u.connect(fcb); return u
    def state(self):
        return json.loads(self.state_file.read_text()) if self.state_file.exists() else {}
    def save_state(self, st):
        tmp = self.state_file.with_suffix('.tmp'); tmp.write_text(json.dumps(st, indent=1)); os.replace(tmp, self.state_file)
    def load_backup(self):
        if not self.backup_file.exists(): raise Stop('no backup yet: make one first (backup, or use an existing dump of this unit).')
        b = self.backup_file.read_bytes()
        if len(b) != SIZE or sha(b[:FW_END]) not in KNOWN_STOCK: raise Stop(f'{self.backup_file} is damaged or not a stock backup')
        return b
    def recipe(self):
        p = HERE / RECIPE
        if not p.exists(): raise Stop(f'{RECIPE} not found next to the patcher')
        return load_recipe(p)          # checked by its own hashes

    # -- backup: read only
    def cmd_backup(self):
        if self.backup_file.exists(): self.say(f'A backup already exists ({self.backup_file}); nothing to do.'); return
        self.bdir.mkdir(exist_ok=True); part = self.bdir / 'om108_backup.partial'
        have = part.stat().st_size if part.exists() else 0
        have -= have % 0x40000
        self.say('Connecting (read only: nothing is written)...')
        u = self.unit(RO_FCB); self.plan(SIZE - have)
        with open(part, 'r+b' if part.exists() else 'wb') as f:
            f.truncate(have); f.seek(have)
            for off in range(have, SIZE, 0x40000):
                f.write(u.read(off, 0x40000)); f.flush()
                self.say(f'  backup {100 * (off + 0x40000) // SIZE:3d} %   ({self.elapsed()})')
        b = part.read_bytes(); h = sha(b[:FW_END])
        if h not in KNOWN_STOCK:
            v = stock_version(b)
            bad = self.bdir / (f'om108_backup_v{v}_not_supported_yet.bin' if v else 'unknown_unit_dump.bin'); os.replace(part, bad)
            try:
                rec = self.recipe()
                mod = h == rec[0]['target_fw_sha256'] or h in {L['target_fw_sha256'] for L in rec[0]['legacy']} or settings_from_block(b[CFG_OFF:CFG_OFF + 64]) is not None
            except Stop: mod = False
            if mod: raise Stop('this unit already has the mod installed, so it can\'t be backed up as stock. If you have its '
                               'original backup, use:  python3 om108_patcher.py import-backup <file>')
            if other_stock(b):
                raise Stop(f'this unit has {OTHER_STOCK}: it is not the one this patcher knows, which the custom firmware '
                           f'is made for (saved as {bad}). Nothing was changed. Please don\'t install; report it at {REPORT} '
                           'so support can be added.')
            raise Stop(f'this unit\'s firmware is not the one this patcher knows (saved as {bad}). Nothing was changed. '
                       'Please don\'t install; send the log in logs/ to the mod\'s author.')
        self.backup_file = self.bdir / backup_name(stock_version(b)); os.replace(part, self.backup_file)
        st = self.state(); st.update(backup_sha256=sha(b), installed='stock'); self.save_state(st)
        self.say(f'DONE - backup saved: {self.backup_file} ({KNOWN_STOCK[h]}, checked). Keep this file safe. ({self.elapsed()})')

    def cmd_import(self, path):
        b = Path(path).read_bytes()
        if len(b) != SIZE: raise Stop(f'{path} is not a full 16 MB dump')
        if other_stock(b): raise Stop(f'{path} is not the stock firmware this patcher knows: it looks like {OTHER_STOCK}')
        if sha(b[:FW_END]) not in KNOWN_STOCK: raise Stop(f'{path} is not the stock firmware this patcher knows')
        if self.backup_file.exists():
            if self.backup_file.read_bytes()[:FW_END] == b[:FW_END]: self.say('That backup is already in place.'); return
            raise Stop(f'a different backup already exists ({self.backup_file}); move it away first')
        self.backup_file = self.bdir / backup_name(stock_version(b))
        self.bdir.mkdir(exist_ok=True); tmp = self.backup_file.with_suffix('.tmp'); tmp.write_bytes(b); os.replace(tmp, self.backup_file)
        st = self.state(); st.update(backup_sha256=sha(b)); self.save_state(st)
        self.say(f'DONE - backup imported: {self.backup_file}')

    # -- install / restore: the same safe engine, in opposite directions
    def known_images(self, u, backup, rec):
        """What the unit may hold besides stock: the releases this tool knows, the custom firmware its settings block
        names (read from the unit), and the one a cut-off install was writing (the state file)."""
        out = [img for _, img in legacy_images(backup, rec)]
        d = settings_from_block(u.read(CFG_OFF, 64))
        if d:
            img = build_image(backup, rec, d); out.append(img); out += [v for _, v in code_variants(backup, rec, img)]
        p = self.state().get('pending')
        if p:
            try: img = build_image(backup, rec, p); out.append(img); out += [v for _, v in code_variants(backup, rec, img)]
            except Stop: pass
        return out
    def _flash(self, u, target, accepted, regs, label, pending=None):
        st = self.state(); jr = st.get('journal')
        backup = accepted[0]
        # 1. read-only checks, before anything is written
        if u.read(0, BOOT_END) != backup[:BOOT_END]: raise Stop('this is not the unit the backup was made from (boot area differs). Nothing was changed.')
        ident = ident_sectors(regs)
        self.plan(SECTOR * len(ident) + 64 + sum(n for a, n in regs) * 3.5)       # (every region checked; each may need writing)
        have = read_regions(u, [(s, SECTOR) for s in ident])
        if any(have[s] != backup[s:s + SECTOR] for s in ident):
            raise Stop('this unit\'s firmware differs from the backup. Nothing was changed.')
        self.say(f'Checking the unit ({len(regs)} regions)...')
        have = read_regions(u, regs); todo = []
        for a, n in regs:
            h = sha(have[a])
            if h == sha(target[a:a + n]): continue
            ok = h == blank_sha(n) or any(h == sha(x[a:a + n]) for x in accepted)
            if not ok and jr and jr.get('addr') == a and jr.get('len') == n: ok = True   # the region a cut-off run was writing
            if not ok: raise Stop('0x%08X holds content this patcher did not write. Nothing was changed.' % (BASE + a))
            todo.append((a, n))
        self.say(f'Unit checked: {len(regs) - len(todo)} of {len(regs)} regions already done, {len(todo)} to write. ({self.elapsed()})')
        self.plan(sum(n for a, n in todo) * 2.5)                                  # erase + write + read back
        # 2. write, region by region: erase, write, read back, compare
        if todo: st['pending'] = pending; self.save_state(st)
        for i, (a, n) in enumerate(todo):
            st['journal'] = dict(addr=a, len=n, to=label); self.save_state(st)
            u.erase(a, n); u.write(a, target[a:a + n])
            if u.read(a, n) != target[a:a + n]: raise Stop('a write did not read back correctly. Run the same command again.')
            st['journal'] = None; self.save_state(st)
            if i % 8 == 7 or i == len(todo) - 1: self.say(f'  {i + 1}/{len(todo)} regions written   ({self.elapsed()})')
        st['installed'] = label; st['journal'] = None; st['pending'] = None; self.save_state(st)

    def read_settings(self, path):
        if not path: return default_settings()
        try: return norm_settings(json.loads(Path(path).read_text(encoding='utf-8')))
        except (OSError, ValueError) as e: raise Stop(f'the settings file {path} could not be read ({e})')

    def cmd_install(self, settings=None):
        backup = self.load_backup(); rec = self.recipe(); d = self.read_settings(settings)
        target = build_image(backup, rec, d); regs = regions_for(rec)
        self.say('Installing ' + ('the Suggested Default firmware' if d == default_settings() else 'a custom firmware with your settings') + '.')
        self.say('Connecting...'); u = self.unit(RW_FCB)
        self._flash(u, target, [backup] + self.known_images(u, backup, rec), regs, 'custom' if d != default_settings() else rec[0]['name'], pending=d)
        self.say(f'DONE - FIRMWARE INSTALLED AND VERIFIED. You can let go of the power button. ({self.elapsed()})')

    def cmd_restore(self):
        backup = self.load_backup(); rec = self.recipe(); regs = restore_order(regions_for(rec))
        self.say('Connecting...'); u = self.unit(RW_FCB)
        self._flash(u, backup, [backup] + self.known_images(u, backup, rec), regs, 'stock')
        self.say(f'DONE - ORIGINAL FIRMWARE PUT BACK AND VERIFIED. You can let go of the power button. ({self.elapsed()})')

    def cmd_restore_full(self):
        """Full Factory Restore (the window: hold F while clicking Restore Stock Firmware). Reads the whole unit and
        writes the backup over every 4 KB sector that differs, whatever it holds, the start-up area (0x60000000-
        0x60001FFF) included and written last. Never the top 8 KB (this unit's own settings): a factory reset clears
        them. Each region is erased, written, read back and compared; if it is cut off, the same command carries on."""
        backup = self.load_backup()
        self.say('FULL FACTORY RESTORE: every part of the firmware that differs from your stock firmware file is rewritten.')
        self.say('Connecting...'); u = self.unit(RW_FCB); u.allow_boot = True
        self.say('Reading the whole unit (about 40 s)...')
        self.plan(FW_END * 2 + 0x400000)                                         # read, (some) writes, read again
        d = u.read(0, FW_END)
        secs = [s for s in range(0, FW_END, SECTOR) if d[s:s + SECTOR] != backup[s:s + SECTOR]]
        regs = []
        for s in secs:
            if s >= BOOT_END and regs and regs[-1][1] == s and regs[-1][0] >= BOOT_END and regs[-1][1] - regs[-1][0] < ERASE_MAX: regs[-1][1] = s + SECTOR
            else: regs.append([s, s + SECTOR])
        regs = [r for r in regs if r[0] >= BOOT_END] + [r for r in regs if r[0] < BOOT_END]     # the start-up area last
        self.say(f'{len(secs)} sectors differ ({len(regs)} regions to write). ({self.elapsed()})')
        self.plan(len(secs) * SECTOR * 2.5 + FW_END)
        st = self.state()
        for i, (a, b) in enumerate(regs):
            n = b - a; st['journal'] = dict(addr=a, len=n, to='stock (full)'); self.save_state(st)
            u.erase(a, n); u.write(a, backup[a:b])
            if u.read(a, n) != backup[a:b]: raise Stop('a write did not read back correctly. Run the same command again.')
            if i % 8 == 7 or i == len(regs) - 1: self.say(f'  {i + 1}/{len(regs)} regions written   ({self.elapsed()})')
        if u.read(0, FW_END) != backup[:FW_END]: raise Stop('the unit does not read back as the stock firmware. Run the same command again.')
        st['installed'] = 'stock'; st['journal'] = None; st['pending'] = None; self.save_state(st)
        self.say(f'DONE - FULL FACTORY RESTORE COMPLETE AND VERIFIED. You can let go of the power button. ({self.elapsed()})')

    def what_is(self, d, backup, rec):
        """(text, kind) for a whole-unit copy d."""
        h = sha(d[:FW_END])
        if backup is not None and d[:BOOT_END] != backup[:BOOT_END]: return 'not the unit the stock firmware file was made from', 'other'
        if h in KNOWN_STOCK: return 'the original (stock) firmware', 'stock'
        if rec is not None and h == rec[0]['target_fw_sha256']: return 'the Suggested Default firmware', 'mod'
        if rec is not None and h in {L['target_fw_sha256'] for L in rec[0]['legacy']}:
            return 'an earlier version of the Suggested Default firmware (Install brings it up to date)', 'legacy'
        if other_stock(d): return OTHER_STOCK + ' (nothing was changed; please don\'t install)', 'unsupported'
        if rec is None or backup is None: return 'a User Generated Custom Firmware', 'unknown'
        s = settings_from_block(d[CFG_OFF:CFG_OFF + 64])
        if s and h == sha(build_image(backup, rec, s)[:FW_END]): return 'a User Generated Custom Firmware (made with this tool)', 'custom'
        if s and any(h == sha(v[:FW_END]) for _, v in code_variants(backup, rec, build_image(backup, rec, s))):
            return ('an earlier version of the Suggested Default firmware (Install brings it up to date)' if s == default_settings() else
                    'a User Generated Custom Firmware made with an earlier version of this tool (Install brings it up to date)'), 'legacy'
        regs = regions_for(rec)
        known = [backup] + [img for _, img in legacy_images(backup, rec)] + ([build_image(backup, rec, s)] if s else [])
        if s: known += [v for _, v in code_variants(backup, rec, known[-1])]
        p = self.state().get('pending')
        if p:
            try: known.append(build_image(backup, rec, p))
            except Stop: pass
        if all(any(d[a:a + n] == x[a:a + n] for x in known) or d[a:a + n] == b'\xff' * n for a, n in regs) \
           and all(d[s:s + SECTOR] == backup[s:s + SECTOR] for s in range(BOOT_END, FW_END, SECTOR) if not any(a <= s < a + n for a, n in regs)):
            return 'a partly installed firmware: run Install Firmware or Restore Stock Firmware', 'partial'
        return 'a User Generated Custom Firmware', 'unknown'

    def cmd_scan(self):
        """Read the whole unit (read only) and say what is on it. The copy is kept in work/last_read.bin. If it is the
        stock firmware and there is no backup yet, it becomes the backup."""
        self.say('Connecting (read only: nothing is written)...')
        u = self.unit(RO_FCB); (self.dir / 'work').mkdir(exist_ok=True); self.plan(SIZE)
        part = self.dir / 'work' / 'last_read.partial'
        with open(part, 'wb') as f:
            for off in range(0, SIZE, 0x40000):
                f.write(u.read(off, 0x40000)); f.flush()
                if (off // 0x40000) % 8 == 7: self.say(f'  read {100 * (off + 0x40000) // SIZE:3d} %   ({self.elapsed()})')
        last = self.dir / 'work' / 'last_read.bin'; os.replace(part, last)
        d = last.read_bytes(); backup = self.backup_file.read_bytes() if self.backup_file.exists() else None
        try: rec = self.recipe()
        except Stop: rec = None
        r, kind = self.what_is(d, backup, rec)
        if kind == 'stock' and backup is None:
            self.backup_file = self.bdir / backup_name(stock_version(d))
            self.bdir.mkdir(exist_ok=True); tmp = self.backup_file.with_suffix('.tmp'); tmp.write_bytes(d); os.replace(tmp, self.backup_file)
            st = self.state(); st.update(backup_sha256=sha(d), installed='stock'); self.save_state(st)
            r += f'; saved as the stock firmware file ({self.bdir.name}/{self.backup_file.name})'
        self.say(f'RESULT: the unit has {r}. ({self.elapsed()})')
        res = {'kind': kind, 'text': r, 'file': str(last), 'version': stock_version(d)}   # (a factory version, else None)
        s = settings_from_block(d[CFG_OFF:CFG_OFF + 64]) if kind == 'custom' else None
        if s: res['settings'] = s
        return res

    def cmd_check(self):
        backup = self.load_backup(); rec = self.recipe(); regs = regions_for(rec)
        self.say('Connecting (read only: nothing is written)...')
        u = self.unit(RO_FCB); self.plan(BOOT_END + sum(n for a, n in regs))
        if u.read(0, BOOT_END) != backup[:BOOT_END]: self.say('RESULT: not the unit the backup was made from.'); return 'other'
        have = read_regions(u, regs); d = bytearray(backup)
        for a, n in regs: d[a:a + n] = have[a]
        r, kind = self.what_is(bytes(d), backup, rec)       # (the sectors no setting touches are taken as stock here: check is quick)
        self.say(f'RESULT: the unit has {r}. ({self.elapsed()})'); return r

def main(argv=None, tools_factory=None, folder=None, out=print, result=None):
    ap = argparse.ArgumentParser(description='OM-108 mod patcher', epilog=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('command', choices=['backup', 'install', 'restore', 'check', 'scan', 'import-backup'])
    ap.add_argument('file', nargs='?')
    ap.add_argument('--settings', help='install: a settings file (JSON, written by the window); none = the Suggested Default')
    ap.add_argument('--full', action='store_true', help='restore: Full Factory Restore (every differing sector, the start-up area too)')
    ap.add_argument('--result-json', action='store_true', help=argparse.SUPPRESS)   # for the window: last line = @@RESULT {json}
    A = ap.parse_args(argv)
    p = Patcher(folder or UNIT_DIR, tools_factory, out)
    if A.result_json: p.prog_out = lambda pct: print('@@PROGRESS %d' % pct, flush=True)
    try:
        if A.command == 'import-backup':
            if not A.file: raise Stop('say which file:  python3 om108_patcher.py import-backup <file>')
            p.cmd_import(A.file)
        else:
            if A.command == 'install': v = p.cmd_install(A.settings)
            elif A.command == 'restore': v = p.cmd_restore_full() if A.full else p.cmd_restore()
            else: v = getattr(p, 'cmd_' + A.command)()
            if result is not None: result['value'] = v
            if A.result_json and v is not None: print('@@RESULT ' + json.dumps(v), flush=True)
        return 0
    except Reconnect:
        p.log('(the flashloader is running but this process does not see it: running the same command in a fresh process)')
        time.sleep(1)
        env = dict(os.environ, OM108_FRESH='1')
        return subprocess.run([sys.executable, '-u', str(Path(__file__).resolve())] + list(sys.argv[1:] if argv is None else argv),
                              env=env, creationflags=0x08000000 if sys.platform == 'win32' else 0).returncode
    except Stop as e:
        p.say(f'STOPPED: {e}'); return 1
    except KeyboardInterrupt:
        p.say('STOPPED (interrupted). Run the same command again to carry on.'); return 1

if __name__ == '__main__':
    sys.exit(main())
