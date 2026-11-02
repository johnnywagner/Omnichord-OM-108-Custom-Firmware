"""Tester mode (python3 om108_gui.py --tester, or OM108_TESTER=1): the window works as usual, but on a SIMULATED unit.
Nothing is ever sent to USB: the patcher's sdphost / blhost commands go to SimTools, which keeps the unit's 16 MB flash
in a file. Everything lives in my_unit/tester/ (never the real my_unit/ files, which are only read to find stock images):

  my_unit/tester/unit_flash.bin   the simulated unit's chip (it keeps its state: install, then restore, ... as on a unit)
  my_unit/tester/my_unit/         the simulated user's folder (backups/, dumps/, logs/, settings.ini), as a user's my_unit/
  my_unit/tester/images/          stock firmware images picked for the simulator (copies of the owner's own files)
  my_unit/tester/sim.json         what the simulation started from (shown in the window's banner)

The stock firmware images come only from the owner's own files (their my_unit/backups/ and my_unit/dumps/, or a file
they pick): this tool never contains Suzuki's firmware."""
import os, json, time, shutil, hashlib
from pathlib import Path
import om108_patcher as OP

FLASH, USER, IMAGES, INFO = 'unit_flash.bin', 'my_unit', 'images', 'sim.json'
MARK = '[TESTER MODE: simulated unit, nothing is sent to USB]'
def vname(v): return f'1.0.{v[0]}.{v[1]}' if v else 'unknown'          # '29' -> 1.0.2.9


# ------------------------------------------------------------------------------------------------- the simulated unit
class SimTools:
    """Takes exactly the sdphost / blhost command lines the patcher sends (as Tools.run) and plays a unit in UPDATE mode,
    just switched on: boot ROM first, then the flashloader; flash only after configure-memory; no erase or write with the
    read-only FCB; erases <= 0x8000 in whole 4 KB sectors; writes only into erased flash; never the top 8 KB."""
    mods = None                          # (never 'in process': no Reconnect)
    def __init__(self, log, flash):
        self.log, self.path = log, Path(flash)
        self.mode = 'rom'; self.loaded = False; self.ram_fcb = self.fcb = None
        self.slow = float(os.environ.get('OM108_SIM_SPEED', '1'))     # a little time per command, so the progress shows
    def run(self, tool, args):
        ok, text = self._do(tool, list(args))
        self.log(f'(simulated) {tool} {" ".join(args)} -> {"ok" if ok else "FAILED"} {text if not ok else ""}')
        return ok, text
    def _wait(self, s):
        if self.slow: time.sleep(s * self.slow)
    def _do(self, tool, a):
        if len(a) < 3 or a[0] != '-u': return False, 'bad command'
        dev, cmd, a = a[1], a[2], a[3:]
        if tool == 'sdphost':
            if dev != OP.SDP_ID or self.mode != 'rom': return False, 'no device'
            if cmd == 'error-status': return True, ''
            if cmd == 'write-file':
                self.loaded = a[0] == '0x20209000' and OP.sha(Path(a[1]).read_bytes()) == OP.FLASHLOADER[1]
                return self.loaded, '' if self.loaded else 'not the flashloader'
            if cmd == 'jump-address':
                if self.loaded: self.mode = 'loader'
                return False, 'jump reports an error (normal)'
            return False, 'unknown command'
        if tool != 'blhost' or dev != OP.MB_ID or self.mode != 'loader': return False, 'no device'
        if cmd == 'get-property': return True, ''
        if cmd == 'write-memory' and len(a) == 2 and a[0] == '0x2000': self.ram_fcb = OP.sha(Path(a[1]).read_bytes()); return True, ''
        if cmd == 'configure-memory':
            if a != ['9', '0x2000'] or self.ram_fcb not in (OP.RW_FCB[1], OP.RO_FCB[1]): return False, 'bad FCB'
            self.fcb = self.ram_fcb; return True, ''
        if not self.fcb: return False, 'flash not configured'
        ad = int(a[0], 16) - OP.BASE
        with open(self.path, 'r+b') as f:
            if cmd == 'read-memory':
                n = int(a[1], 16)
                if not (0 <= ad and ad + n <= OP.SIZE): return False, 'out of range'
                f.seek(ad); Path(a[2]).write_bytes(f.read(n)); self._wait(n / 4e6); return True, ''
            if self.fcb != OP.RW_FCB[1]: return False, 'refused by the simulator: erase / write with the read-only FCB'
            if cmd == 'flash-erase-region':
                n = int(a[1], 16)
                if n > OP.ERASE_MAX or ad % OP.SECTOR or n % OP.SECTOR or ad < 0 or ad + n > OP.FW_END:
                    return False, 'refused by the simulator: bad or protected erase'
                f.seek(ad); f.write(b'\xff' * n); self._wait(0.005); return True, ''
            if cmd == 'write-memory':
                d = Path(a[1]).read_bytes()
                if ad < 0 or ad + len(d) > OP.FW_END: return False, 'refused by the simulator: protected area'
                f.seek(ad)
                if f.read(len(d)) != b'\xff' * len(d): return False, 'refused by the simulator: write into unerased flash'
                f.seek(ad); f.write(d); self._wait(len(d) / 1e7); return True, ''
        return False, 'unknown command'

def factory(tdir):
    """The patcher's tools_factory for the simulated unit in tdir."""
    return lambda log: SimTools(log, Path(tdir) / FLASH)


# ------------------------------------------------------------------------------------------------- starting states
def find_stocks(real_unit, tdir):
    """{version: path} of the factory firmware images the owner already has (read only: nothing is renamed): the
    simulator's own copies first, then my_unit/backups/, the safety copy and my_unit/dumps/."""
    out = {}; real_unit = Path(real_unit)
    places = [Path(tdir) / IMAGES, real_unit / 'backups', real_unit / 'work' / 'stock_safety_copy', real_unit / 'dumps']
    for d in places:
        for p in sorted(d.glob('*.bin')) if d.is_dir() else []:
            try:
                if p.stat().st_size != OP.SIZE: continue
                with open(p, 'rb') as f: v = OP.STOCK_VERSIONS.get(OP.sha(f.read(OP.FW_END)))
            except OSError: continue
            if v and v not in out: out[v] = p
    return out

def keep_stock(tdir, path):
    """A picked file: its factory version (copied into tester/images/), or None if it is not a known factory firmware."""
    b = Path(path).read_bytes()
    v = OP.stock_version(b) if len(b) == OP.SIZE else None
    if v:
        d = Path(tdir) / IMAGES; d.mkdir(parents=True, exist_ok=True); tmp = d / 'stock.tmp'
        tmp.write_bytes(b); os.replace(tmp, d / f'stock_v{v}.bin')
    return v

def variant(recipes, v):
    """The recipe variant for factory version v (None if the recipe has none)."""
    h = next((k for k, x in OP.STOCK_VERSIONS.items() if x == v), None)
    return next((r for r in recipes if r[0]['stock_fw_sha256'] == h), None)

def unit_choices(recipes):
    """[(key, label)]: the states the simulated unit can start in. key = (version, kind, legacy name)."""
    out = []
    for v in ('29', '30'):
        out.append(((v, 'stock', ''), f'Factory firmware {vname(v)} (stock)'))
        rec = variant(recipes, v)
        for L in (rec[0]['legacy'] if rec else []):
            out.append(((v, 'legacy', L['name']), f'{vname(v)} with an earlier custom firmware: {L["name"]}'))
        out.append(((v, 'release', ''), f'{vname(v)} with this tool\'s custom firmware (the window\'s settings)'))
    out.append((('', 'unknown', ''), 'A factory firmware version this tool does not support (refused)'))
    return out

FOLDERS = [('new', 'New user: no my_unit/backups/ yet'),
           ('current', 'Existing user: my_unit/backups/om108_backup_v<version>.bin (tool 1.2 / 1.3 name)'),
           ('v11', 'Existing user from tool 1.1: my_unit/backups/om108_backup.bin (renamed on first use)'),
           ('refused', 'Tool 1.2 refused it: my_unit/backups/om108_backup_v<version>_not_supported_yet.bin')]

def unit_image(stock, recipes, key, settings):
    """The 16 MB the simulated unit starts with (the unit's own boot area and top 8 KB: those of the stock image)."""
    v, kind, name = key
    if kind == 'stock': return stock
    if kind == 'unknown':
        # another factory version: the stock program with some code changed (as issue #4's revision); no mod anywhere
        b = bytearray(stock)
        for i in range(0x20000, 0x20200): b[i] ^= 0x5A
        assert OP.other_stock(b) and not OP.stock_version(b)
        return bytes(b)
    rec = variant(recipes, v)
    if rec is None: raise OP.Stop(f'this tool\'s recipe has no custom firmware for factory firmware {vname(v)}')
    if kind == 'release': return OP.build_image(stock, rec, settings)
    L = next(L for L in rec[0]['legacy'] if L['name'] == name)
    if L.get('code_only'):
        return dict(OP.code_variants(stock, rec, OP.build_image(stock, rec, OP.default_settings())))[name]
    return dict(OP.legacy_images(stock, rec))[name]

def start(tdir, real_unit, stocks, recipes, ukey, folder, settings, labels, both=False):
    """Set up a fresh simulation: the unit's flash and the simulated user's folder. both: the folder's
    work/stock_safety_copy/ also holds both factory versions (the window then shows Install Factory Version)."""
    tdir = Path(tdir); v = ukey[0] or ('29' if '29' in stocks else '30')
    if v not in stocks: raise OP.Stop(f'no factory firmware {vname(v)} image: pick one first')
    if ukey[1] == 'unknown' and folder in ('current', 'refused'):
        raise OP.Stop('a unit with an unsupported factory version has no backup name of its own: choose New user or tool 1.1')
    stock = Path(stocks[v]).read_bytes()
    img = unit_image(stock, recipes, ukey, settings)
    tdir.mkdir(parents=True, exist_ok=True); tmp = tdir / 'flash.tmp'; tmp.write_bytes(img); os.replace(tmp, tdir / FLASH)
    user = tdir / USER
    if user.exists(): shutil.rmtree(user)
    user.mkdir(parents=True)
    if folder != 'new':
        if (Path(real_unit) / 'settings.ini').exists(): shutil.copyfile(Path(real_unit) / 'settings.ini', user / 'settings.ini')
        b = user / 'backups'; b.mkdir()
        mine = img if ukey[1] == 'unknown' else stock          # the stock firmware this unit came with
        name = {'current': OP.backup_name(v), 'v11': 'om108_backup.bin', 'refused': f'om108_backup_v{v}_not_supported_yet.bin'}[folder]
        (b / name).write_bytes(mine)
        installed = {'stock': 'stock', 'unknown': 'stock', 'release': 'custom'}.get(ukey[1], ukey[2])
        if folder != 'refused': (b / 'om108_state.json').write_text(json.dumps({'backup_sha256': OP.sha(mine), 'installed': installed}, indent=1))
    if both:
        if not {'29', '30'} <= set(stocks): raise OP.Stop('both factory firmware images are needed for that: pick the missing one first')
        safe = user / 'work' / 'stock_safety_copy'; safe.mkdir(parents=True, exist_ok=True)
        for x in ('29', '30'): shutil.copyfile(stocks[x], safe / ('om108_backup.bin' if x == v else f'stock_v{x}.bin'))
        labels = (labels[0], labels[1] + ' + both factory versions in the safety copy folder')
    info = {'unit': labels[0], 'folder': labels[1], 'started': time.strftime('%Y-%m-%d %H:%M:%S')}
    (tdir / INFO).write_text(json.dumps(info, indent=1))
    return info

def current(tdir):
    """sim.json of the simulation in tdir (None if there is none)."""
    try:
        if (Path(tdir) / FLASH).stat().st_size == OP.SIZE: return json.loads((Path(tdir) / INFO).read_text())
    except (OSError, ValueError): pass
    return None


# ------------------------------------------------------------------------------------------------- the window's chooser
def choose(root, tdir, real_unit, settings, keep_ok=True):
    """A dialog: pick the simulated unit's starting state. Returns sim.json's dict, or None (kept / cancelled)."""
    import tkinter as tk
    from tkinter import ttk, messagebox, filedialog
    try: recipes = OP.load_recipes(OP.HERE / OP.RECIPE)
    except OP.Stop as e: messagebox.showerror('Tester mode', f'The recipe could not be read: {e}', parent=root); recipes = []
    stocks = find_stocks(real_unit, tdir)
    w = tk.Toplevel(root); w.title('Tester mode: choose the simulated unit'); w.resizable(False, False)
    if root.winfo_viewable(): w.transient(root)          # (at start-up the window is still hidden)
    w.configure(bg=root.cget('bg')); ttk.Style(root).configure('TRadiobutton', background=root.cget('bg'))
    f = ttk.Frame(w, padding=16); f.pack(fill='both')
    ttk.Label(f, text='TESTER MODE: nothing is sent to USB. The buttons work on a simulated unit.', style='Head.TLabel').pack(anchor='w')
    ttk.Label(f, text='Stock firmware images (only your own files, copied into my_unit/tester/images/):').pack(anchor='w', pady=(10, 2))
    img_rows = {}
    for v in ('29', '30'):
        r = ttk.Frame(f); r.pack(fill='x')
        img_rows[v] = ttk.Label(r, text=''); img_rows[v].pack(side='left')
        def pick(v=v):
            p = filedialog.askopenfilename(parent=w, title=f'A full 16 MB dump of a unit with factory firmware {vname(v)}')
            if not p: return
            got = keep_stock(tdir, p)
            if got is None: messagebox.showwarning('Tester mode', f'{p}\n\nis not a factory firmware this tool knows.', parent=w)
            stocks.update(find_stocks(real_unit, tdir)); show()
        ttk.Button(r, text='Pick a file...', command=pick).pack(side='right')
    choices = unit_choices(recipes); uvar = tk.IntVar(value=0); fvar = tk.StringVar(value='current')
    ttk.Label(f, text='The simulated unit has:').pack(anchor='w', pady=(12, 2))
    ubtn = []
    for i, (k, lab) in enumerate(choices):
        b = ttk.Radiobutton(f, text=lab, variable=uvar, value=i); b.pack(anchor='w', padx=12); ubtn.append(b)
    ttk.Label(f, text='The simulated tool folder has:').pack(anchor='w', pady=(12, 2))
    for k, lab in FOLDERS: ttk.Radiobutton(f, text=lab, variable=fvar, value=k).pack(anchor='w', padx=12)
    bvar = tk.BooleanVar(value=False); ttk.Style(root).configure('TCheckbutton', background=root.cget('bg'))
    ttk.Checkbutton(f, text='Both factory versions in the safety copy folder (shows Install Factory Version)', variable=bvar).pack(anchor='w', padx=12, pady=(6, 0))
    def show():
        for v, lab in img_rows.items():
            lab.configure(text=f'  {vname(v)}: ' + (str(stocks[v]) if v in stocks else 'none found (pick one to simulate this version)'))
        for (k, _), b in zip(choices, ubtn):
            need = k[0] or ('29' if '29' in stocks else '30')
            ok = need in stocks and (k[1] in ('stock', 'unknown') or variant(recipes, k[0]) is not None)
            b.state(['!disabled'] if ok else ['disabled'])
    show()
    res = {'info': None}
    def go():
        k, lab = choices[uvar.get()]
        try:
            w.configure(cursor='watch'); w.update()
            res['info'] = start(tdir, real_unit, stocks, recipes, k, fvar.get(), settings,
                                (lab, dict(FOLDERS)[fvar.get()]), both=bvar.get())
        except OP.Stop as e: w.configure(cursor=''); messagebox.showwarning('Tester mode', str(e), parent=w); return
        w.destroy()
    bar = ttk.Frame(f); bar.pack(fill='x', pady=(14, 0))
    ttk.Button(bar, text='Start with this unit', command=go).pack(side='left')
    cur = current(tdir)
    if cur and keep_ok:
        ttk.Button(bar, text='Keep the current simulated unit', command=w.destroy).pack(side='left', padx=8)
        ttk.Label(f, text=f'Current: {cur["unit"]}; {cur["folder"]} (since {cur["started"]})', style='Note.TLabel').pack(anchor='w', pady=(8, 0))
    ttk.Button(bar, text='Cancel', command=w.destroy).pack(side='right')
    w.grab_set(); w.wait_window()
    return res['info']
