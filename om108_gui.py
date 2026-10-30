#!/usr/bin/env python3
"""Suzuki Omnichord OM-108 Custom Firmware Tool (by JohnnyW) - the window.  Open this file in Thonny and press Run (or: python3 om108_gui.py).

Works on macOS and Windows with a standard Python 3.9+ that has tkinter (Thonny's does) and NXP's SPSDK
(pip install spsdk; Thonny: Tools -> Manage packages -> spsdk). Keep it in the same folder as om108_patcher.py.

Your choices live in my_unit/settings.ini: loaded at every start, saved whenever you change something.
You can edit or back it up with any text editor (close the window first, or it will save over your edits).

Self-contained: everything it needs, reads and writes is in this folder: the tool's own files in assets/, everything
about your unit in my_unit/ (the stock firmware in backups/, copies in dumps/, logs, settings). Nothing can be installed
until the stock firmware of your unit is in my_unit/backups/om108_backup.bin (checked against the known stock firmware). If it is missing, the window offers to check
the unit, which reads it (read only) and keeps it if it is the stock firmware.

Version 0.23: Install Firmware installs the firmware with the choices in the window (MIDI, Sub Sounds, Bonus Features).
Hold F while clicking Restore Stock Firmware for the Full Factory Restore. Hover over a heading or a checkbox for an explanation."""
import queue, sys, os, threading, traceback, hashlib, configparser, shutil, time, re, subprocess, json
import importlib.util
from pathlib import Path

# NXP's SPSDK is required: without it the tool stops right here, like any Python script missing a package (v0.38, the
# user: clearer for a beginner than finding it out in the window's log). OM108_NO_SPSDK_CHECK=1 skips it (tests only).
if importlib.util.find_spec('spsdk') is None and not os.environ.get('OM108_NO_SPSDK_CHECK'):
    raise ModuleNotFoundError("No module named 'spsdk'. This tool needs NXP's SPSDK. Install it with:  pip install spsdk"
                              "   (Thonny: Tools > Manage packages > spsdk > Install), then run the tool again.", name='spsdk')
import tkinter as tk
from tkinter import ttk, messagebox

HERE = Path(__file__).resolve().parent                     # the tool's folder: om108_gui.py, README.md and
if (HERE / '.assets').is_dir() and not (HERE / 'assets').exists():   # (named .assets, hidden, before 2026-10-21 evening)
    try: os.replace(HERE / '.assets', HERE / 'assets')
    except OSError: pass
ASSETS = HERE / 'assets' if (HERE / 'assets').is_dir() else HERE     # assets/ = the tool's own files (engine, recipe, texts)
UNIT = HERE / 'my_unit' if ASSETS != HERE else HERE        # my_unit/ = everything about your unit (delete it before sharing)
sys.path.insert(0, str(ASSETS))
sys.dont_write_bytecode = True                             # no __pycache__ folders in the tool folder
import om108_patcher as OP                                  # noqa: E402
import tkinter.font as tkfont                               # noqa: E402

if sys.platform == 'win32':                     # Windows: draw at the screen's real resolution (not blurry / tiny on 4K)
    try:
        import ctypes
        try: ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception: ctypes.windll.user32.SetProcessDPIAware()
    except Exception: pass

# The window opens at the same share of the screen on every computer (measured on the author's 4K screen: the
# contents fill about 46.5 % of its height and 30 % of its width). Text and spacing are scaled to get there.
TARGET_H, TARGET_W = 0.465, 0.30
SCALE_MIN, SCALE_MAX = 0.75, 3.0               # never smaller than 3/4 size (readability), never silly big
SCALE_FIT = 0.62                               # ...except to fit a small screen (a 13" MacBook at 1280 x 832): down to this
LOG_ROWS, LOG_ROWS_MIN = 10, 5                 # the log box; on a small screen it gives up rows (to 5) before text gets smaller than SCALE_FIT
SCALE = 1.0
FIT = 1.0                                      # the scale the window opened with (fitted to the screen)
ZOOM = 1.0                                     # the user's zoom since then: the window resized = everything scaled (_zoom_to)
ZOOM_MIN, ZOOM_MAX = 0.4, 4.0
FONT_MIN = 6
def F(n):
    """A font size at the current scale."""
    return max(FONT_MIN, round(n * SCALE))
def Z(n, lo=1):
    """A size in pixels (padding, box sizes) at the current zoom."""
    return max(lo, round(n * ZOOM))

GUI_VERSION = '0.41'
SETTINGS_ACTIVE = True       # the firmware reads the settings block (Install uses the window's choices)
SETTINGS = UNIT / 'settings.ini'
STOCK = UNIT / 'backups' / 'om108_backup.bin'
SAFE = UNIT / 'work' / 'stock_safety_copy' / 'om108_backup.bin'     # a second copy of the stock firmware file (_guard_stock)
DUMPS = UNIT / 'dumps'
TITLE = 'Suzuki Omnichord OM-108 Custom Firmware Tool'

# ------------------------------------------------------------------------------------------------- the choices (= the Suggested Default: mod 2.86 behaviour)
PRESETS = ['P1', 'P2', 'P3', 'P4', 'P5']
MIDI_PARTS = ['Main', 'Chord', 'Bass', 'Sub', 'Drums']
CHANNELS = ['Off'] + [str(n) for n in range(1, 17)]
MIDI_DEFAULT = {'Main':  (8, [0, 0, 1, 1, 1]), 'Chord': (9, [1, 0, 1, 1, 1]), 'Bass': (7, [0, 0, 0, 1, 1]),
                'Sub':   (9, [0, 1, 0, 0, 0]), 'Drums': (1, [0, 0, 0, 0, 1])}      # mod 1 routing table 0x60039100
CLOCK_DEFAULT = [0, 0, 0, 1, 1]                                                     # 2.82: P4 / P5
VOICES = ['Omni 1', 'Omni 2', 'Harp', 'Celesta', 'A. Piano', 'Guitar', 'FM Piano', 'Organ', 'Vibes', 'Banjo']
SUBS = {1: 'Evolve', 2: 'Mellow Pad', 3: 'Sine Pad', 7: 'Church Bell', 11: 'Rezy', 15: 'Hand Pan', 17: 'Dial Up',
        18: 'Strings', 19: 'Strings 2', 20: '7th Doo', 21: 'Dah Guys'}   # the subs 2.85 / 2.86 use (13 = an exact copy of 3)
OLD_NAMES = {'pad': 3, 'pad (octave)': 3, 'vibes alt': 15, 'harp': 17, 'organ alt': 20, 'organ': 21}   # names in older settings.ini
SUB_NRM = [None, 3, 17, 3, 3, 3, 2, 21, 18, 18]                                     # mod2.c SUB_NRM / SUB_ALT (guitar 13 = 3)
SUB_ALT = [None, 2, 1, 7, 19, 2, 11, 20, 15, 2]
FLAGS = [('drum_map', 'midi', 'drum_note_remap'), ('electric_harp', 'sounds', 'electric_harp'),
         ('better_clap', 'sounds', 'better_clap'), ('modulation', 'features', 'modulation_modes'),
         ('extras', 'features', 'extras'), ('spoken_names', 'features', 'spoken_mode_names'),
         ('kb_transpose', 'features', 'keyboard_transpose')]

def defaults():
    return {'midi': {v: {'channel': c, 'presets': list(p)} for v, (c, p) in MIDI_DEFAULT.items()},
            'clock': list(CLOCK_DEFAULT),
            'subs': [None if n is None else {'sub': n, 'alt': a} for n, a in zip(SUB_NRM, SUB_ALT)],
            'drum_map': True, 'electric_harp': True, 'better_clap': True, 'modulation': True, 'extras': True, 'spoken_names': True,
            'kb_transpose': True}

# ------------------------------------------------------------------------------------------------- settings.ini
def key(name): return name.lower().replace('. ', '_').replace(' ', '_')
def yn(b): return 'yes' if b else 'no'
def plist(bits): return ' '.join(p for p, b in zip(PRESETS, bits) if b) or 'none'

def ini_text(d, win=None):
    L = ['# Suzuki Omnichord OM-108 Custom Firmware Tool settings. Edit with any text editor while the tool is closed.',
         '# yes / no for switches; channels 1-16 or off; presets as a list like: P3 P4 P5 (or none).',
         '# A value the patcher does not understand is replaced by its default (the log says which).', '', '[midi]']
    for v in MIDI_PARTS:
        c = d['midi'][v]['channel']
        L += [f'{key(v)}_channel = {"off" if c is None else c}', f'{key(v)}_presets = {plist(d["midi"][v]["presets"])}']
    L += [f'clock_presets = {plist(d["clock"])}', f'drum_note_remap = {yn(d["drum_map"])}', '', '[sounds]',
          '# sub sounds: ' + ', '.join(SUBS[k] for k in sorted(SUBS))]
    for v, s in enumerate(d['subs']):
        if s: L += [f'{key(VOICES[v])}_sub = {SUBS[s["sub"]]}', f'{key(VOICES[v])}_alt = {SUBS[s["alt"]]}']
    L += [f'electric_harp = {yn(d["electric_harp"])}', f'better_clap = {yn(d["better_clap"])}', '', '[bonus features]',
          f'modulation_modes = {yn(d["modulation"])}', f'extras = {yn(d["extras"])}', f'spoken_mode_names = {yn(d["spoken_names"])}',
          f'keyboard_transpose = {yn(d["kb_transpose"])}', '']
    if win: L += ['[window]', '# the window size you set (it keeps its proportions; delete this line for the size fitted to the screen)', f'size = {win[0]} x {win[1]}', '']
    return '\n'.join(L)

def ini_window(text):
    """The window size saved in settings.ini, or None."""
    try:
        cp = configparser.ConfigParser(); cp.read_string(text)
        w, h = (int(x) for x in cp.get('window', 'size').lower().replace('x', ' ').split())
        return (w, h) if w >= 100 and h >= 100 else None
    except Exception: return None

def ini_read(text, warn):
    d = defaults(); cp = configparser.ConfigParser(); cp.read_string(text)
    def get(sec, k, parse, cur):
        if not cp.has_option(sec, k): return cur
        raw = cp.get(sec, k).strip()
        try: return parse(raw)
        except Exception: warn(f'settings.ini: "{k} = {raw}" not understood, using the default'); return cur
    def p_yes(s):
        s = s.lower()
        if s in ('yes', 'y', 'true', 'on', '1'): return True
        if s in ('no', 'n', 'false', 'off', '0'): return False
        raise ValueError
    def p_ch(s):
        if s.lower() == 'off': return None
        n = int(s); assert 1 <= n <= 16; return n
    def p_pre(s):
        w = s.upper().replace(',', ' ').split()
        if w == ['NONE']: return [0] * 5
        assert all(x in PRESETS for x in w); return [int(p in w) for p in PRESETS]
    byname = dict(OLD_NAMES); byname.update({v.lower(): k for k, v in SUBS.items()})
    def p_sub(s): return byname[s.lower()]
    for v in MIDI_PARTS:
        d['midi'][v]['channel'] = get('midi', key(v) + '_channel', p_ch, d['midi'][v]['channel'])
        d['midi'][v]['presets'] = get('midi', key(v) + '_presets', p_pre, d['midi'][v]['presets'])
    d['clock'] = get('midi', 'clock_presets', p_pre, d['clock'])
    for v, s in enumerate(d['subs']):
        if s:
            s['sub'] = get('sounds', key(VOICES[v]) + '_sub', p_sub, s['sub'])
            s['alt'] = get('sounds', key(VOICES[v]) + '_alt', p_sub, s['alt'])
    for f, sec, k in FLAGS:
        if sec == 'features': d[f] = get('features', k, p_yes, d[f]); sec = 'bonus features'
        d[f] = get(sec, k, p_yes, d[f])
    return d

def is_stock_file(p):
    try:
        p = Path(p)
        if not p.is_file() or p.stat().st_size != OP.SIZE: return False
        with open(p, 'rb') as f: return hashlib.sha256(f.read(OP.FW_END)).hexdigest() in OP.KNOWN_STOCK
    except OSError: return False

# ------------------------------------------------------------------------------------------------- hover tips
TIPS_FILE = ASSETS / 'tooltips.txt'
DEFAULT_TIPS = [
    ('midi', 'Each preset (P1–P5) sends the parts ticked in its column, each on its own channel.\nTo choose a preset on the unit, hold [VOICE] button 1–5 while switching on.\nThe stock MIDI CC out messages (Volume & Release) have been removed to make room for the fifth preset, P5, so they are not configurable in this editor.'),
    ('channel', 'The MIDI channel this part is sent on, in every preset. Off: this part is never sent.'),
    ('presets', 'Tick a box to send that part (or the clock) in that preset.\nChoose the preset on the unit at power-on: hold [VOICE] button 1–5 while switching on.'),
    ('part_main', '**Main:** the strumplate notes.'),
    ('part_chord', '**Chord:** the chord notes.'),
    ('part_bass', '**Bass:** the bass notes.'),
    ('part_sub', '**Sub:** an identical copy of Main on its own channel.'),
    ('part_drums', "**Drums:** the rhythm's drum notes."),
    ('clock', "MIDI Clock, Start and Stop, so other gear can follow the rhythm's tempo. Sent only in the ticked presets."),
    ('drum_map', 'Drum notes are sent on a smaller, more logical note range matching the Teenage Engineering OP-XY, instead of the stock drum note range.'),
    ('sounds', 'The custom firmware adds an alternate sub sound to every voice except “omni 1”. After a voice has been selected, press the lit [VOICE] button again to toggle its alternate sub sound; press once more to go back. This is saved between power-offs.'),
    ('sub', 'The sub sound this voice starts with.'),
    ('alt', 'The sub sound you get by pressing the lit [VOICE] button again. This setting is saved.'),
    ('voice_omni_1', '“omni 1” is analog only: it has no customizable sub sound.'),
    ('voice_omni_2', '**omni 2:** its sub and alt sounds.'),
    ('voice_harp', '**harp:** its sub and alt sounds. Its main sound is set with Electric Harp (Bonus Features).'),
    ('voice_celesta', '**celeste:** its sub and alt sounds.'),
    ('voice_a_piano', '**A. piano:** its sub and alt sounds.'),
    ('voice_guitar', '**guitar:** its sub and alt sounds.'),
    ('voice_fm_piano', '**FM piano:** its sub and alt sounds.'),
    ('voice_organ', '**organ:** its sub and alt sounds.'),
    ('voice_vibes', '**vibes:** its sub and alt sounds.'),
    ('voice_banjo', '**banjo:** its sub and alt sounds.'),
    ('electric_harp', 'Replaces the main sound of the stock “harp” voice with a custom electric harp / clav sound.'),
    ('better_clap', 'Replaces the stock clap rhythm sound (also played by the Instant OFF plate in keyboard mode) with a "better" clap sound.'),
    ('header', '**Always On Mods**\n- **Saved Settings:** all your settings are saved between power-offs, except for Jazz Mode.\n- A factory reset (both [VOICE] and [PATTERN] left buttons (yellow) held while switching on) now sets the battery type to Ni-MH and also clears the saved settings.\n- The stock MIDI CC out messages (Volume & Release) have been removed, making room for a fifth MIDI preset, P5.\n\n**Stock Firmware Bug Fixes:** two small bugs involving LED states and chord data sent via MIDI out have been fixed (e.g. the held chord is now preserved on MIDI out when switching from Auto to Manual).'),
    ('modulation', "Changes what the Instant OFF plate does, using the 3 chord buttons at the bottom left of the chord button grid that are unused in keyboard mode.\n\nIn keyboard mode, the “G♭-7th” chord button sets the modulation mode, in one of 3 ways:\n**Tap:** cycles to the next mode, shown on the [VOICE] LEDs. The unit announces the selected mode (intended for beginners).\n**Hold + [↑]/[↓] buttons:** cycles through the modes silently, like setting Transpose or Master Tune.\n**Hold + [VOICE] button 1–5:** chooses one of the 5 modes directly and silently. This is my preferred method.\n\n**The 5 modes:**\n- **Instant Off:** the stock behavior.\n- **Vibrato Auto:** like the stock behavior, a finger on the plate turns on the vibrato. The plate now also detects partial coverage and adjusts speed and depth accordingly. Try rolling your finger off slowly after pressing.\n- **Vibrato Manual:** rest a finger on a corner of the Instant OFF plate and rock it in and out, like bending a guitar string.\n- **Tremolo Auto:** the same as Vibrato Auto, but for volume.\n- **Tremolo Manual:** the same as Vibrato Manual, but for volume.\n\n**Speed (Auto modes):** hold the “G♭-MINOR” chord button and press [↑]/[↓], −6 to +6 on the [VOICE] LEDs.\n**Depth:** hold the “D♭-MINOR” chord button and press [↑]/[↓], 0 to 12 on the [PATTERN] LEDs.\nThe [↓]/[↑] buttons are the “E♭-MAJOR”/“B♭-MAJOR” chord buttons, as for Transpose and Master Tune; pressing both together resets the setting.\n\n**Notes:**\n- In keyboard mode, hold a chromatic key and press the “G♭-MINOR” or “D♭-MINOR” chord button to play an Auto modulation mode on the played notes.\n- Both Vibrato modes send matching MIDI pitch bend out. Both Tremolo modes are local to the unit and are not sent over MIDI.\n- Due to limitations of the analog circuits, neither Tremolo mode works on “omni 1” or the main voice of “omni 2”.\n\nEach mode's settings are saved automatically and are unique to that mode."),
    ('extras', '**OM-27 mode:** hold the [PATTERN (slow rock/funk)] button on startup. The 6 [PATTERN] buttons (including the yellow one) then play the 6 OM-27 rhythm patterns.\n  - From left to right: Latin (yellow), Rock, Foxtrot, Slow Rock, Waltz, Swing.\n  - Hold any 2 [PATTERN] buttons and press the [KEYBOARD (ON)] button to lock the selection.\n  - Otherwise, this mode works just like the OM-27.\n\n**Alternate Bass:** hold the [PATTERN] left button (yellow) on startup for an alternate (plucked) bass sound. Hold the [VOICE] left button (yellow) for the stock bass sound.\n\n**Jazz Mode:** hold the [KEYBOARD (ON)] button on startup. Changes and adds chord fingerings, and voice leads intelligently when switching chords. This is the only setting NOT remembered between power cycles. Have fun!\n\n**3rd Manual/Auto State:** hold the [Chord Manual/Auto] button and press the [KEYBOARD (ON)] button at any time (both LEDs lit). This third state combines Manual chords with the Auto bass. Do the same again to go back.'),
    ('spoken_names', 'The unit announces the rhythm mode chosen at power-on ([PATTERN (swing/waltz)]: “84 mode”, [PATTERN (country/bossanova)]: “108 mode”, [PATTERN (slow rock/funk)]: “27 mode”) and the modulation mode selected by tapping the “G♭-7th” chord button. I recommend leaving this on.\n**Off:** the modes still change and the LEDs still show them; nothing is spoken.'),
    ('kb_transpose', 'In keyboard mode, the chromatic keyboard notes follow the Transpose setting, like the chords and the strumplate do (MIDI out too). If you change Transpose while holding a note, the new setting starts with the next note you play.\n**Off:** the stock behavior: keyboard notes ignore Transpose.'),
    ('install', 'Writes the custom firmware onto the unit, made from your stock firmware file and the settings chosen above.\n**Flashing Instructions:**\n1. Turn the Omnichord off, leaving the batteries in.\n2. Remove the battery cover and set the PLAY/UPDATE switch to UPDATE with a small screwdriver or similar tool.\n3. Connect the USB-B port to your computer and click this button to proceed.\n4. **IMPORTANT:** During all reads and writes, the red power button must be held down! If it is released during a transfer, you will need to start over.\n**Afterwards:** set the switch back to PLAY and test.'),
    ('restore', "Puts your unit's original firmware back, from your stock firmware file.\nConnect it the same way as for Install Firmware (PLAY/UPDATE switch on UPDATE, and hold the power button until DONE)."),
    ('check', 'Reads the whole unit (nothing is written), tells you what is on it and offers to save a copy in my_unit/dumps/.\nIf it has the stock firmware and the tool has none yet, this is automatically saved as the stock firmware file.\nConnect it the same way as for Install Firmware.'),
    ('defaults', "Sets every choice back to the Suggested Default: the recommended setup, with every feature on and JohnnyW's recommended MIDI and sub sound configuration."),
    ('stock', "The stock firmware file (my_unit/backups/om108_backup.bin): your unit's original firmware. Every custom firmware created by this tool is generated from it."),
]
TIPS = dict(DEFAULT_TIPS)

def load_texts(path, defaults, store, head):
    """path: [name] on a line, then the text (several lines allowed). Missing names are added with their default text."""
    have = {}
    if path.exists():
        cur = None
        for line in path.read_text(encoding='utf-8').splitlines():
            m = re.fullmatch(r'\s*\[([a-z0-9_]+)\]\s*', line)
            if m: cur = m.group(1); have[cur] = []
            elif cur and not line.startswith('#'): have[cur].append(line)
        have = {k: '\n'.join(v).strip() for k, v in have.items()}
    store.update({k: v for k, v in have.items() if v})
    missing = [(k, v) for k, v in defaults if k not in have]
    if missing:
        lead = '' if path.exists() else head
        with open(path, 'a', encoding='utf-8') as f:
            f.write(lead + ''.join(f'\n[{k}]\n{v}\n' for k, v in missing))
def load_tips():
    load_texts(TIPS_FILE, DEFAULT_TIPS, TIPS, '# Hover tips of the Suzuki Omnichord OM-108 Custom Firmware Tool. Edit the text under each [name];\n'
               '# keep the [name] lines. Lines starting with # are ignored. Read when the tool starts.\n'
               '# **Text between two pairs of stars** is shown in bold.\n')

# ------------------------------------------------------------------------------------------------- pop-up texts
# popups.txt in assets/: the same format as tooltips.txt. {words in braces} are filled in by the tool (keep them).
POPUPS_FILE = ASSETS / 'popups.txt'
DEFAULT_POPUPS = [
    ('stock_bad', '{file}\n\nis not the known stock OM-108 firmware (its checksum differs). Move it out of the my_unit/backups folder, then start the tool again.'),
    ('stock_missing', 'The stock firmware file is missing:\n\n{file}\n\nNo changes can be made until the stock firmware has been dumped from your device or copied to the backups folder. \n\nWould you like to dump the current firmware from your Omnichord now? This is a read-only process and if your unit has the stock firmware it saves it to the backups folder. It takes a few minutes and only needs to be done once.'),
    ('ready', "Power off your OM-108. Remove the battery door and put the PLAY/UPDATE switch in the UPDATE position. Connect the OM-108 to your PC using a USB-B cable.\n\nPress OK, then hold the Omnichord's power button until the log says DONE or RESULT. If it stops, just do the same again."),
    ('stopped', 'The requested process unexpectedly stopped. Please check the log for more information.'),
    ('check_settings', 'Your Omnichord OM-108 currently has {result}.\n\nShow its settings in the window? (Your current choices are replaced; settings.ini is saved with them.)'),
    ('check_done', 'Your Omnichord OM-108 currently has {result}.\n\nYou can let go of the power button.\n\nSave a copy of the firmware that is on the unit now (in the dumps folder)?'),
    ('install_done', 'Done. You can let go of the power button and set the PLAY/UPDATE switch back to PLAY. Check the hover tooltips on each setting for a detailed explanation on how to use them. I hope you find this custom firmware useful.'),
    ('restore_ask', 'Reinstall the stock firmware?'),
    ('full_warning', "WARNING: Full Factory Restore.\n\nThis rewrites every part of the unit's firmware that differs from your stock firmware file, whatever is on it now (also firmware this tool does not recognize), and the start-up area too. Only your unit's own saved settings at the very top are left alone (a factory reset clears them).\n\nUse it ONLY if a normal Restore Stock Firmware fails. If it stops part way, the unit may not start normally until you run it again: the UPDATE switch always works, so just do the same again. \n\nRun the Full Factory Restore?"),
    ('factory_done', 'Your Factory Firmware backup has been installed. Let go of the power button and set the PLAY/UPDATE switch back to PLAY.'),
    ('stock_restored', 'The stock firmware file was missing (deleted or moved), so the tool put it back from its safety copy:\n\n{file}'),
    ('stock_bad_restore', '{file}\n\nis not the stock OM-108 firmware (its checksum differs).\n\nThe tool has a safety copy of your stock firmware file. Put it back now? (The file above is kept, renamed to {renamed}, in the backups folder.)'),
]
POPUPS = dict(DEFAULT_POPUPS)
class _Keep(dict):
    def __missing__(self, k): return '{' + k + '}'
def P(name, **kw):
    """A pop-up text from popups.txt, with its {fields} filled in (**bold** marks dropped: pop-ups show plain text)."""
    return POPUPS.get(name, dict(DEFAULT_POPUPS).get(name, name)).replace('**', '').format_map(_Keep(kw))
def load_popups():
    load_texts(POPUPS_FILE, DEFAULT_POPUPS, POPUPS, '# Pop-up texts of the Suzuki Omnichord OM-108 Custom Firmware Tool. Edit the text under each [name];\n'
               '# keep the [name] lines and the {words in braces} (the tool fills them in). Lines starting with # are ignored.\n'
               '# Read when the tool starts. Blank lines inside a text are kept.\n')

def tip(widget, key):
    """A small note that appears while the mouse is over the widget. Returns the widget."""
    state = {'win': None, 'job': None}
    def show():
        state['job'] = None
        if state['win'] or not widget.winfo_viewable(): return
        w = tk.Toplevel(widget); w.wm_overrideredirect(True)
        text = TIPS.get(key, key); fam = tkfont.nametofont('TkDefaultFont', root=widget).cget('family')
        plain = text.replace('**', '')
        box = tk.Text(w, bg=ACCENT, fg=INK, relief='solid', bd=1, padx=8, pady=5, wrap='word', highlightthickness=0,
                      cursor='arrow', font=(fam, F(10)), width=max(12, min(95, max(len(l) for l in plain.splitlines() or ['']))), height=1)
        box.tag_configure('b', font=(fam, F(10), 'bold'))
        for part in re.split(r'(\*\*.+?\*\*)', text):            # **bold** in tooltips.txt
            if part.startswith('**') and part.endswith('**') and len(part) > 4: box.insert('end', part[2:-2], 'b')
            else: box.insert('end', part)
        box.pack(); w.update_idletasks()
        n = box.count('1.0', 'end', 'update', 'displaylines'); n = n[-1] if isinstance(n, tuple) else n
        box.configure(height=max(1, n or 1), state='disabled')
        w.update_idletasks()                     # below the item, or above it if there is no room below; inside the screen
        x = min(widget.winfo_rootx() + 12, w.winfo_screenwidth() - w.winfo_reqwidth() - 8)
        y = widget.winfo_rooty() + widget.winfo_height() + 6
        if y + w.winfo_reqheight() > w.winfo_screenheight() - 40: y = max(8, widget.winfo_rooty() - w.winfo_reqheight() - 6)
        w.wm_geometry(f'+{max(8, x)}+{y}')
        state['win'] = w
    def enter(e): state['job'] = widget.after(350, show)
    def leave(e):
        if state['job']: widget.after_cancel(state['job']); state['job'] = None
        if state['win']: state['win'].destroy(); state['win'] = None
    widget.bind('<Enter>', enter, add='+'); widget.bind('<Leave>', leave, add='+'); widget.bind('<ButtonPress>', leave, add='+')
    return widget

# ------------------------------------------------------------------------------------------------- the window
BG, INK, MUTED, ACCENT, OK, BAD = '#a8dff7', '#000a66', '#000a66', '#D4FEFF', '#16602d', '#8a1c1c'
IDLE = '#7fb0d6'                                   # the progress bar's dimmed outline when nothing is running

def migrate_layout():
    """Tool folders from before 2026-10-21 kept the unit's files next to the program: move them into my_unit/."""
    if UNIT == HERE: return
    UNIT.mkdir(exist_ok=True)
    for n in ('backups', 'dumps', 'work', 'logs', 'settings.ini'):
        if (HERE / n).exists() and not (UNIT / n).exists():
            try: os.replace(HERE / n, UNIT / n)
            except OSError: pass

class App(tk.Tk):
    def __init__(self):
        migrate_layout()
        super().__init__()
        self.title(TITLE); self.configure(bg=BG)
        self.q = queue.Queue(); self.busy = False; self.gated = []; self.always = []; self.vars = {}; self._save_job = None
        load_tips(); load_popups(); self._style(); self._build()
        d = defaults()
        if SETTINGS.exists():
            try: d = ini_read(SETTINGS.read_text(encoding='utf-8'), self.say); self._win_saved = ini_window(SETTINGS.read_text(encoding='utf-8'))
            except Exception as e: self.say(f'settings.ini could not be read ({e}); using the defaults.')
        self.load_dict(d); self._save()
        for v in self._all_vars(): v.trace_add('write', lambda *a: self._save_soon())
        self.say(f'{TITLE} {GUI_VERSION} (engine {OP.TOOL_VERSION}), Python {sys.version.split()[0]}, {sys.platform}. Settings: {SETTINGS.name}')
        self.f_down = False; self._f_job = None          # F held (Restore Stock Firmware -> Full Factory Restore)
        for ev in ('<KeyPress-f>', '<KeyPress-F>'): self.bind_all(ev, self._f_press)
        for ev in ('<KeyRelease-f>', '<KeyRelease-F>'): self.bind_all(ev, self._f_release)
        self._seen = None; self._guard_stock(repeat=False)
        self._fit(); self.after(100, self._poll); self.after(300, self._startup); self.after(3000, self._guard_stock)

    def _style(self):
        s = ttk.Style(self)
        if 'clam' in s.theme_names(): s.theme_use('clam')
        s.configure('.', background=BG, foreground=INK)
        s.configure('TFrame', background=BG); s.configure('TLabel', background=BG)
        s.configure('Head.TLabel', font=('TkDefaultFont', F(14), 'bold'))
        s.configure('Voice.TLabel', font=('TkDefaultFont', F(10), 'bold'))
        s.configure('Col.TLabel', foreground=MUTED, font=('TkDefaultFont', F(9), 'bold'))
        s.configure('Note.TLabel', foreground=MUTED, font=('TkDefaultFont', F(9), 'italic'))
        s.configure('Good.TLabel', foreground=OK, font=('TkDefaultFont', F(10), 'bold'))
        s.configure('Bad.TLabel', foreground=BAD, font=('TkDefaultFont', F(10), 'bold'))
        s.configure('Title.TLabel', font=('TkDefaultFont', F(17), 'bold')); s.configure('By.TLabel', foreground=MUTED, font=('TkDefaultFont', F(10)))
        s.configure('TCheckbutton', background=BG, indicatorbackground=ACCENT, indicatorforeground=INK, indicatorsize=Z(max(10, round(10 * FIT)), 6))
        s.map('TCheckbutton', background=[('active', BG), ('disabled', BG)], indicatorbackground=[('disabled', '#b9dcef'), ('active', ACCENT)])
        s.configure('TSeparator', background=INK); s.configure('Light.TSeparator', background='#6f9fd8')
        s.configure('TButton', background=ACCENT, foreground=INK, padding=(Z(10), Z(5)), bordercolor=INK)
        s.map('TButton', background=[('disabled', '#b9dcef'), ('active', '#ffffff')], foreground=[('disabled', '#5b6fa8')])
        df = tkfont.nametofont('TkDefaultFont', root=self)       # same size as the other buttons, just bold
        s.configure('Go.TButton', padding=(Z(10), Z(5)), font=(df.cget('family'), df.cget('size'), 'bold'))
        s.configure('TCombobox', fieldbackground='white', background=ACCENT, arrowcolor=INK, selectbackground='white', selectforeground=INK, foreground=INK, bordercolor=INK, arrowsize=Z(max(12, round(12 * FIT)), 7))
        s.map('TCombobox', fieldbackground=[('readonly', 'white'), ('disabled', '#e4eef4')], background=[('disabled', '#b9dcef')],
              selectbackground=[('readonly', 'white')], selectforeground=[('readonly', INK)])
        s.configure('Horizontal.TProgressbar', background=ACCENT, troughcolor=BG, bordercolor=INK, lightcolor=ACCENT, darkcolor=ACCENT)
        s.configure('Idle.Horizontal.TProgressbar', background=BG, troughcolor=BG, bordercolor=IDLE, lightcolor=BG, darkcolor=BG)   # idle: an empty, dimmed outline
        # the percent drawn inside the bar, centred (a text element on top of the bar; '' = nothing shown)
        for st in ('Horizontal.TProgressbar', 'Idle.Horizontal.TProgressbar'):
            s.layout(st, [('Horizontal.Progressbar.trough', {'sticky': 'nswe', 'children': [('Horizontal.Progressbar.pbar', {'side': 'left', 'sticky': 'ns'})]}),
                          ('Horizontal.Progressbar.label', {'sticky': ''})])
            s.configure(st, foreground=INK, font=(tkfont.nametofont('TkDefaultFont', root=self).cget('family'), F(10), 'bold'), text=getattr(self, '_pct_text', ''))

    # -------------------------------------------------------------------------- layout
    def _build(self):
        bottom = ttk.Frame(self, padding=(28, 6, 28, 22)); bottom.pack(side='bottom', fill='x')
        bar = ttk.Frame(bottom); bar.pack(fill='x')
        for c in range(4): bar.columnconfigure(c, weight=1, uniform='buttons')
        tip(self._gate(ttk.Button(bar, text='Install Firmware', style='Go.TButton', command=self.do_install)), 'install').grid(row=0, column=0, padx=(0, 6), sticky='nsew')
        tip(self._gate(ttk.Button(bar, text='Restore Stock Firmware', command=self.do_restore)), 'restore').grid(row=0, column=1, padx=(6, 6), sticky='nsew')
        b = tip(ttk.Button(bar, text='Check/Dump Firmware', command=self.do_check), 'check'); b.grid(row=0, column=2, padx=(6, 6), sticky='nsew'); self.always.append(b)
        tip(self._gate(ttk.Button(bar, text='Suggested Defaults', command=self.do_defaults)), 'defaults').grid(row=0, column=3, padx=(6, 0), sticky='nsew')
        self.stock_lbl = tip(ttk.Label(bottom, text=''), 'stock'); self.stock_lbl.pack(pady=(6, 0))       # centred in the window
        pf = ttk.Frame(bottom); pf.pack(fill='x', pady=(8, 4))
        # the bar across the full width; while a job runs, its percent is shown in the middle of the bar (_pct)
        self.prog = ttk.Progressbar(pf, mode='determinate', value=0, maximum=100, style='Idle.Horizontal.TProgressbar'); self.prog.pack(fill='x', expand=True, ipady=3)
        self.log = tk.Text(bottom, height=LOG_ROWS, bg='white', fg=INK, relief='flat', bd=0, highlightthickness=1, highlightbackground=INK, highlightcolor=INK, wrap='word', padx=8, pady=4,
                           font=('Menlo' if sys.platform == 'darwin' else 'Consolas', F(10)))
        self.log.pack(fill='x'); self.log.configure(state='disabled')
        # the page (a scroll bar appears only if the window is made smaller than the page)
        wrap = ttk.Frame(self); wrap.pack(fill='both', expand=True)
        self.cv = cv = tk.Canvas(wrap, bg=BG, highlightthickness=0); self.vs = ttk.Scrollbar(wrap, command=cv.yview)
        cv.configure(yscrollcommand=self.vs.set); cv.pack(side='left', fill='both', expand=True)
        self.page = page = ttk.Frame(cv, padding=(28, 20, 28, 0)); win = cv.create_window(0, 0, window=page, anchor='nw')
        def conf(e=None):
            cv.configure(scrollregion=cv.bbox('all')); cv.itemconfigure(win, width=cv.winfo_width())
            if getattr(self, '_zjob', None) or getattr(self, '_zooming', False): return   # mid-resize: no scroll bar flashing
            need = page.winfo_reqheight() > cv.winfo_height() + 2
            if need and not self.vs.winfo_ismapped(): self.vs.pack(side='right', fill='y', before=cv)
            elif not need and self.vs.winfo_ismapped(): self.vs.pack_forget(); cv.yview_moveto(0)
        page.bind('<Configure>', conf); cv.bind('<Configure>', conf); self._conf = conf
        def wheel(e):
            if not self.vs.winfo_ismapped(): return
            d = -1 if e.num == 4 else 1 if e.num == 5 else (-e.delta if sys.platform == 'darwin' else -e.delta // 120)
            cv.yview_scroll(int(d), 'units')
        for ev in ('<MouseWheel>', '<Button-4>', '<Button-5>'): self.bind_all(ev, wheel)
        head = ttk.Frame(page); head.pack(fill='x'); self.head = head
        self.title_lbl = tip(ttk.Label(head, text=TITLE, style='Title.TLabel'), 'header'); self.title_lbl.pack(side='left')
        ttk.Label(head, text='by JohnnyW', style='By.TLabel').pack(side='left', padx=10, pady=(6, 0))
        self._sep(page); self._sec_midi(page); self._sep(page); self._sec_sounds(page); self._sep(page); self._sec_bonus(page)

    def _sep(self, page): ttk.Separator(page).pack(fill='x', pady=7)
    def _gate(self, w): self.gated.append(w); return w

    def _sec_midi(self, page):
        top = ttk.Frame(page); top.pack(fill='x')
        tip(ttk.Label(top, text='MIDI', style='Head.TLabel'), 'midi').pack(anchor='w', pady=(0, 6))
        f = ttk.Frame(top); f.pack(anchor='w')                           # the grid, on the left
        for j, h in enumerate(['', 'Channel'] + PRESETS):
            tip(ttk.Label(f, text=h, style='Col.TLabel'), 'channel' if h == 'Channel' else 'presets').grid(row=1, column=j, padx=(0, 28) if j == 0 else 16)
        self.vars['midi'] = {}
        for i, v in enumerate(MIDI_PARTS):
            tip(ttk.Label(f, text=v), 'part_' + v.lower()).grid(row=2 + i, column=0, sticky='w', padx=(0, 28), pady=3)
            ch = tk.StringVar(); self._gate(ttk.Combobox(f, textvariable=ch, values=CHANNELS, width=4, state='readonly')).grid(row=2 + i, column=1, padx=16)
            pv = [tk.IntVar() for _ in PRESETS]
            for j, x in enumerate(pv): self._gate(ttk.Checkbutton(f, variable=x)).grid(row=2 + i, column=2 + j, padx=16)
            self.vars['midi'][v] = (ch, pv)
        ttk.Separator(f, style='Light.TSeparator').grid(row=7, column=0, columnspan=7, sticky='ew', pady=(8, 3))
        tip(ttk.Label(f, text='Clock, Start / Stop'), 'clock').grid(row=8, column=0, columnspan=2, sticky='w', pady=(3, 0))
        self.vars['clock'] = [tk.IntVar() for _ in PRESETS]
        for j, x in enumerate(self.vars['clock']): self._gate(ttk.Checkbutton(f, variable=x)).grid(row=8, column=2 + j, padx=16, pady=(3, 0))

    def _sec_sounds(self, page):
        f = ttk.Frame(page); f.pack(fill='x')
        tip(ttk.Label(f, text='Sub Sounds', style='Head.TLabel'), 'sounds').grid(row=0, column=0, columnspan=6, sticky='w', pady=(0, 6))
        names = [SUBS[k] for k in sorted(SUBS)]
        self.vars['subs'] = [None] * len(VOICES)
        for row in range(2):                                   # the two voice sets, as on the panel
            for col in range(5):
                v = row * 5 + col
                c = ttk.Frame(f); c.grid(row=1 + row, column=col, sticky='nw', padx=(0, 10), pady=(0, 10))
                tip(ttk.Label(c, text=VOICES[v], style='Voice.TLabel'), 'voice_' + key(VOICES[v])).pack(anchor='w')
                if SUB_NRM[v] is None: ttk.Label(c, text='N/A, Analog Only', style='Col.TLabel').pack(anchor='w', pady=(4, 0)); continue
                a, b = tk.StringVar(), tk.StringVar()
                for var, lab in ((a, 'Sub'), (b, 'Alt')):
                    r = ttk.Frame(c); r.pack(anchor='w', pady=1)
                    tip(ttk.Label(r, text=lab, style='Col.TLabel', width=3), lab.lower()).pack(side='left')
                    self._gate(ttk.Combobox(r, textvariable=var, values=names, width=11, height=len(names), state='readonly')).pack(side='left')
                self.vars['subs'][v] = (a, b)

    def _sec_bonus(self, page):
        f = ttk.Frame(page); f.pack(fill='x')
        ttk.Label(f, text='Bonus Features', style='Head.TLabel').grid(row=0, column=0, columnspan=4, sticky='w', pady=(0, 6))
        rows = [('modulation', 'Modulation Modes'), ('extras', 'Extras'), ('spoken_names', 'Spoken Mode Names'),
                ('drum_map', 'Drum MIDI Remap'), ('electric_harp', 'Electric Harp'), ('better_clap', 'Better Clap'),
                ('kb_transpose', 'Keyboard Transpose')]
        for i, (k, t) in enumerate(rows):
            self.vars[k] = tk.IntVar()
            tip(self._gate(ttk.Checkbutton(f, text=t, variable=self.vars[k])), k).grid(row=1 + i // 4, column=i % 4, sticky='w', padx=(0, 32), pady=(0, 10))

    def _natural(self):
        self.update_idletasks()
        bottom = self.winfo_children()[0]
        return (max(self.page.winfo_reqwidth(), bottom.winfo_reqwidth()), self.page.winfo_reqheight() + bottom.winfo_reqheight() + 4)

    def _apply_scale(self, sc):
        global SCALE
        SCALE = sc
        if not hasattr(self, '_font0'):
            self._font0 = {}
            for n in tkfont.names(self):
                try: self._font0[n] = tkfont.nametofont(n, root=self).cget('size')
                except Exception: pass
        for n, s0 in self._font0.items():
            new = round(s0 * sc) if s0 > 0 else -max(FONT_MIN, round(-s0 * sc))
            if s0 > 0: new = max(FONT_MIN, new)
            tkfont.nametofont(n, root=self).configure(size=new)
        self._style(); self.log.configure(font=('Menlo' if sys.platform == 'darwin' else 'Consolas', F(10)))

    def _usable(self):
        """(width, height) a window's contents can really have on this screen: without the menu bar, the Dock / taskbar
        and the window's own title bar. macOS: the screen's visible frame (asked from the system); Windows: the work area;
        otherwise (or if asking fails) the screen less a fixed allowance."""
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        try:
            if sys.platform == 'darwin':
                js = "ObjC.import('AppKit'); var f = $.NSScreen.mainScreen.visibleFrame; f.size.width + ' ' + f.size.height"
                out = subprocess.run(['osascript', '-l', 'JavaScript', '-e', js], capture_output=True, text=True, timeout=5).stdout.split()
                vw, vh = float(out[0]), float(out[1])
                if 300 < vh <= sh and 300 < vw <= sw: return int(vw) - 16, int(vh) - 34          # title bar ~28 + a margin
            elif sys.platform == 'win32':
                import ctypes, ctypes.wintypes
                r = ctypes.wintypes.RECT(); ctypes.windll.user32.SystemParametersInfoW(0x30, 0, ctypes.byref(r), 0)
                vw, vh = r.right - r.left, r.bottom - r.top
                if 300 < vh <= sh and 300 < vw <= sw: return vw - 24, vh - 48
        except Exception: pass
        return sw - 40, sh - (140 if sys.platform == 'darwin' else 70)

    def _fit(self):
        """Scale the contents to the same share of the screen as on the author's 4K screen, then lock the size.
        On a small screen (a 13" MacBook, 1280 x 832) the window must still show everything without a scroll bar: the text
        gets smaller (down to SCALE_FIT), then the log box gives up rows (down to LOG_ROWS_MIN). Only if even that does not
        fit does the window stay resizable with a scroll bar. Checked again once the window is on screen (_refit)."""
        sw, sh = self.winfo_screenwidth(), self.winfo_screenheight()
        uw, uh = self._usable(); self._uw, self._uh = uw, uh
        sc = 1.0
        for _ in range(3):                                   # text scales in whole steps: refine a few times
            w, h = self._natural()
            want = min(TARGET_H * sh / h, TARGET_W * sw / w)
            new = min(SCALE_MAX, max(SCALE_MIN, sc * want))
            if abs(new - sc) < 0.02: break
            sc = new; self._apply_scale(sc)
        saved = getattr(self, '_win_saved', None); self._win_saved = None
        self._shrink_to(uw, uh, sc)
        self._zmin()                                         # the smallest window the contents fit in (no scroll bar)
        if saved:                                            # the size the user set last time (within the screen)
            self._win_saved = saved; W, H = min(saved[0], uw), min(saved[1], uh)
            W, H = max(W, self.minsize()[0]), max(H, self.minsize()[1])
            self._set_geom(W, H); self.update_idletasks(); self._fit_in(W, H)
        self._fitted = True; self.after(400, self._refit)
        self._zjob = None; self.bind('<Configure>', self._on_resize, add='+')

    # -------------------------------------------------------------------------- zoom (the window resized)
    def _on_resize(self, e):
        if e.widget is not self or getattr(self, '_zooming', False): return
        size = (self.winfo_width(), self.winfo_height())
        if size == getattr(self, '_geom_set', None) or size[0] < 50: return
        if self._zjob: self.after_cancel(self._zjob)
        self._zjob = self.after(150, self._zoom_when_let_go)

    _cg_button = None                                            # macOS: CoreGraphics' CGEventSourceButtonState (loaded once)

    @classmethod
    def _mouse_down(cls):
        """A mouse button is still held (the window edge being dragged): Windows and macOS. Elsewhere, or if the system
        can't be asked: False (then the zoom simply follows 150 ms after the last size change, as in v0.37)."""
        try:
            import ctypes
            if sys.platform == 'win32':
                gk = ctypes.windll.user32.GetAsyncKeyState
                return bool((gk(0x01) | gk(0x02)) & 0x8000)      # physical left / right button
            if sys.platform == 'darwin':
                if cls._cg_button is None:
                    cg = ctypes.cdll.LoadLibrary('/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics')
                    f = cg.CGEventSourceButtonState; f.argtypes = [ctypes.c_int32, ctypes.c_uint32]; f.restype = ctypes.c_bool
                    cls._cg_button = f
                return bool(cls._cg_button(0, 0) or cls._cg_button(0, 1))   # combined session state; left / right
        except Exception:
            if sys.platform == 'darwin': cls._cg_button = lambda *a: False   # not available: don't try again
        return False

    def _zoom_when_let_go(self):
        """v0.38 (Windows) / v0.39 (macOS too): zoom once the window edge is let go, not mid-drag. If a button seems
        held for over 10 s (a stuck reading), zoom anyway."""
        t0 = getattr(self, '_held_since', None) or time.time(); self._held_since = t0
        if self._mouse_down() and time.time() - t0 < 10: self._zjob = self.after(100, self._zoom_when_let_go); return
        self._held_since = None; self._zoom_to()

    def _freeze(self, on):
        """Windows only: stop painting the window's contents while they are re-laid out (on=True), then paint them once
        (on=False). Without it Windows shows every trial size of the zoom (laggy, jumpy). macOS / Linux: nothing to do."""
        if sys.platform != 'win32': return
        try:
            import ctypes
            u = ctypes.windll.user32; hwnd = self.winfo_id()        # the contents (the frame and title bar keep painting)
            u.SendMessageW(hwnd, 0x000B, 0 if on else 1, 0)         # WM_SETREDRAW
            if not on: u.RedrawWindow(hwnd, None, None, 0x0001 | 0x0004 | 0x0080 | 0x0100 | 0x0400)  # INVALIDATE|ERASE|ALLCHILDREN|UPDATENOW|FRAME
        except Exception: pass

    def _zoom(self, z):
        """Everything at z x the size the window opened with: text, spacing, boxes, buttons (the log keeps its rows)."""
        global ZOOM
        ZOOM = z; vf = getattr(self, '_vsp_fit', None)
        if vf is None: vf = self._vsp_fit = getattr(self, '_vsp', 1.0)
        self._apply_scale(FIT * z); self._apply_vsp(vf * z, z)

    def _zoom_to(self):
        """The user resized the window (any shape): everything zoomed as large as fits in it. The size is remembered."""
        self._zjob = None; self._zooming = True
        self._freeze(True)
        try:
            W, H = self.winfo_width(), self.winfo_height(); self._geom_set = (W, H)
            self._fit_in(W, H); self._win_saved = (W, H); self._save_soon()
        finally:
            self._freeze(False)
            def done():
                self._zooming = False
                try: self._conf()                                    # the scroll bar check skipped during the resize
                except Exception: pass
            self.after(80, done)

    def _fit_in(self, W, H):
        """The largest zoom at which the contents fit in W x H. v0.38: the size is roughly proportional to the zoom, so
        aim straight at it and correct (usually 1-3 re-layouts; v0.37 halved 11 times, which Windows painted step by
        step). Text grows in whole steps, so 'fits within 2 %' is good enough."""
        z = ZOOM; w, h = self._natural()
        good = bad = None                                            # largest zoom seen to fit / smallest seen not to
        prev = None                                                  # the previous measurement (z, w, h)
        for _ in range(8):
            r = min(W / w, H / h); fit = w <= W and h <= H
            if fit:
                good = z if good is None else max(good, z)
                if r < 1.02 or z >= ZOOM_MAX: break
            else:
                bad = z if bad is None else min(bad, z)
                if z <= ZOOM_MIN: break
            if good is not None and bad is not None and bad / good < 1.015: break
            nz = z * r                                               # first guess: size proportional to the zoom
            if prev and abs(prev[0] - z) > 1e-6:                     # then a straight line through the last two sizes
                aim = []                                             # (small text stops shrinking: not proportional)
                for d0, d1, T in ((prev[1], w, W), (prev[2], h, H)):
                    k = (d1 - d0) / (z - prev[0])
                    if k > 1e-6: aim.append(z + (T - d1) / k)
                if aim: nz = min(aim)
            nz = min(ZOOM_MAX, max(ZOOM_MIN, nz * 0.997))            # a hair under
            if good is not None and bad is not None and not good < nz < bad: nz = (good + bad) / 2
            if abs(nz - z) < 1e-4: break
            prev = (z, w, h); z = nz; self._zoom(z); w, h = self._natural()
        z = good if good is not None else ZOOM_MIN
        if abs(z - ZOOM) > 1e-9: self._zoom(z)

    def _zmin(self):
        """The smallest window zoom at which the contents (text at its smallest) still fit: no scroll bar from resizing."""
        if getattr(self, '_zmin_v', None) is None:
            z0 = ZOOM; self._zoom(ZOOM_MIN); w, h = self._natural(); self._zoom(z0)
            self._zmin_v = 1.0
            self.minsize(round(w * 1.03), round(h * 1.03))
        return self._zmin_v

    def _usable_now(self):
        return getattr(self, '_uw', self.winfo_screenwidth() - 40), getattr(self, '_uh', self.winfo_screenheight() - 70)

    def _set_geom(self, w, h):
        self._geom_set = (w, h); self.geometry(f'{w}x{h}')

    def _apply_vsp(self, f, fx=None):
        """The spacing between rows and sections at f x its built size vertically, fx x horizontally (1 = as built)."""
        if fx is None: fx = getattr(self, '_hsp', 1.0)
        if not hasattr(self, '_vsp0'):
            self._vsp0 = {}
            todo = [self]
            while todo:
                w = todo.pop(); todo += w.winfo_children()
                mgr = w.winfo_manager(); o = {}
                if mgr in ('pack', 'grid'):
                    info = w.pack_info() if mgr == 'pack' else w.grid_info()
                    for k in ('pady', 'ipady', 'padx', 'ipadx'):
                        v = info.get(k)
                        if v not in (None, '', 0, '0'): o[k] = tuple(int(x) for x in str(v).split()) if not isinstance(v, (tuple, list)) else tuple(int(x) for x in v)
                if isinstance(w, ttk.Frame) and str(w.cget('padding')):
                    o['padding'] = tuple(int(float(str(x))) for x in w.cget('padding'))
                if o: self._vsp0[w] = (mgr, o)
        for w, (mgr, o) in self._vsp0.items():
            kw = {}
            for k in ('pady', 'ipady', 'padx', 'ipadx'):
                if k in o: kw[k] = tuple(round(x * (f if k.endswith('y') else fx)) for x in o[k])
            if kw: (w.pack_configure if mgr == 'pack' else w.grid_configure)(**kw)
            if 'padding' in o:
                pd = list(o['padding']) + [None] * (4 - len(o['padding']))
                l, t = pd[0], pd[1] if pd[1] is not None else pd[0]
                r = pd[2] if pd[2] is not None else l; b = pd[3] if pd[3] is not None else t
                w.configure(padding=(round(l * fx), round(t * f), round(r * fx), round(b * f)))
        self._vsp = f; self._hsp = fx

    def _shrink_to(self, uw, uh, sc):
        """Make the contents fit uw x uh: tighter spacing between rows first (down to half), then fewer log rows, then
        smaller text (down to SCALE_FIT); then set the window size."""
        f = getattr(self, '_vsp', 1.0)
        while f > 0.5 and self._natural()[1] > uh:
            f = round(f - 0.1, 2); self._apply_vsp(f)
        rows = int(self.log.cget('height'))
        while rows > LOG_ROWS_MIN and self._natural()[1] > uh:
            rows -= 1; self.log.configure(height=rows)
        for _ in range(6):
            w, h = self._natural()
            if w <= uw and h <= uh: break
            new = max(SCALE_FIT, sc * min(uw / w, uh / h) * 0.99)
            if new >= sc - 0.005: break
            sc = new; self._apply_scale(sc)
        w, h = self._natural()
        global FIT
        FIT = sc; self._style()
        self._base = (w, h); self._zmin_v = None           # the contents' own size at zoom 1: their proportions
        self.resizable(True, True); self.minsize(200, 150); self._set_geom(min(w, uw), min(h, uh))
        return sc

    def _refit(self):
        """Once on screen: if the system made the window smaller than asked (the Dock, the menu bar), the page would get
        a scroll bar: fit the contents to the height the window really got."""
        try:
            self.update_idletasks()
            w, h = self._natural(); gh = self.winfo_height()
            if self.vs.winfo_ismapped() and gh < h - 2 and gh > 200:
                self._uh = gh
                if ZOOM == 1.0 and not getattr(self, '_win_saved', None):
                    self._shrink_to(min(self._uw, self.winfo_width() if self.winfo_width() > 200 else self._uw), gh, SCALE)
                else: self._geom_set = (self.winfo_width(), gh); self._fit_in(*self._geom_set)
        except Exception: pass

    # -------------------------------------------------------------------------- settings
    def _all_vars(self):
        out = [x for ch, pv in self.vars['midi'].values() for x in [ch] + pv] + self.vars['clock']
        out += [x for s in self.vars['subs'] if s for x in s] + [self.vars[f] for f, _, _ in FLAGS]
        return out

    def to_dict(self):
        name2n = {v: k for k, v in SUBS.items()}
        d = {'midi': {}, 'clock': [x.get() for x in self.vars['clock']], 'subs': []}
        for f, _, _ in FLAGS: d[f] = bool(self.vars[f].get())
        for v, (ch, pv) in self.vars['midi'].items():
            d['midi'][v] = {'channel': None if ch.get() == 'Off' else int(ch.get()), 'presets': [x.get() for x in pv]}
        for s in self.vars['subs']: d['subs'].append(None if s is None else {'sub': name2n[s[0].get()], 'alt': name2n[s[1].get()]})
        return d

    def load_dict(self, d):
        for v, (ch, pv) in self.vars['midi'].items():
            c = d['midi'][v]['channel']; ch.set('Off' if c is None else str(c))
            for x, b in zip(pv, d['midi'][v]['presets']): x.set(int(bool(b)))
        for x, b in zip(self.vars['clock'], d['clock']): x.set(int(bool(b)))
        for s, e in zip(self.vars['subs'], d['subs']):
            if s and e: s[0].set(SUBS[e['sub']]); s[1].set(SUBS[e['alt']])
        for f, _, _ in FLAGS: self.vars[f].set(int(bool(d[f])))

    def _save_soon(self):
        if self._save_job: self.after_cancel(self._save_job)
        self._save_job = self.after(400, self._save)

    def _save(self):
        self._save_job = None
        try:
            t = ini_text(self.to_dict(), getattr(self, '_win_saved', None))
            if not SETTINGS.exists() or SETTINGS.read_text(encoding='utf-8') != t: SETTINGS.write_text(t, encoding='utf-8')
        except Exception as e: self.say(f'settings.ini could not be saved: {e}')

    # -------------------------------------------------------------------------- the stock firmware
    def _refresh(self):
        ok = is_stock_file(STOCK)
        self.stock_lbl.configure(text='Stock Firmware: Found' if ok else 'Stock Firmware: Missing', style='Good.TLabel' if ok else 'Bad.TLabel')
        for w in self.gated: w.state(['!disabled'] if ok and not self.busy else ['disabled'])
        for w in self.always: w.state(['disabled'] if self.busy else ['!disabled'])
        return ok

    def _guard_stock(self, repeat=True):
        """Every 3 s: the stock firmware file is kept safe. While it is there (and checked), a second copy is kept in
        work/stock_safety_copy/; if the file goes missing (deleted, moved), it is put back from that copy at once."""
        try:
            sig = lambda p: (p.stat().st_size, p.stat().st_mtime_ns) if p.exists() else None
            now = (sig(STOCK), sig(SAFE))
            if now != self._seen:
                if now[0] and is_stock_file(STOCK):
                    if not SAFE.exists() or SAFE.stat().st_size != STOCK.stat().st_size or not is_stock_file(SAFE) \
                       or hashlib.sha256(SAFE.read_bytes()).digest() != hashlib.sha256(STOCK.read_bytes()).digest():
                        SAFE.parent.mkdir(parents=True, exist_ok=True); tmp = SAFE.with_suffix('.tmp')
                        shutil.copyfile(STOCK, tmp); os.replace(tmp, SAFE); self.say(f'(A safety copy of the stock firmware file is kept in {SAFE.parent.parent.name}/{SAFE.parent.name}.)')
                elif now[0] and repeat and self._seen is not None and is_stock_file(SAFE) and not is_stock_file(STOCK):
                    self._seen = (sig(STOCK), sig(SAFE)); self._offer_safe(); now = None   # (asked once per change)
                elif not now[0] and is_stock_file(SAFE):
                    STOCK.parent.mkdir(parents=True, exist_ok=True); tmp = STOCK.with_suffix('.tmp')
                    shutil.copyfile(SAFE, tmp); os.replace(tmp, STOCK)
                    self.say(f'The stock firmware file was missing: put back from the safety copy ({STOCK.parent.name}/{STOCK.name}).')
                    if repeat: messagebox.showinfo('Stock Firmware', P('stock_restored', file=STOCK))
                if now is not None: self._seen = (sig(STOCK), sig(SAFE))
                if not self.busy: self._refresh()
        except OSError as e: self.say(f'(Could not check the stock firmware file: {e})')
        if repeat: self.after(3000, self._guard_stock)

    def _offer_safe(self):
        """The stock firmware file is not stock: offer the safety copy (if there is a good one). True if it was put back."""
        if not is_stock_file(SAFE): return False
        renamed = time.strftime('om108_backup_not_stock_%Y-%m-%d_%H%M%S.bin')
        if not messagebox.askyesno('Stock Firmware', P('stock_bad_restore', file=STOCK, renamed=renamed)): return True
        try:
            os.replace(STOCK, STOCK.parent / renamed); tmp = STOCK.with_suffix('.tmp')
            shutil.copyfile(SAFE, tmp); os.replace(tmp, STOCK)
            self.say(f'The stock firmware file was put back from the safety copy (the other file: {STOCK.parent.name}/{renamed}).')
        except OSError as e: self.say(f'Could not put the safety copy back: {e}')
        self._seen = None; self._refresh(); return True

    def _startup(self):
        if self._refresh(): self.say(f'Stock firmware: {STOCK.parent.name}/{STOCK.name} (checked).'); return
        if STOCK.exists():
            if not self._offer_safe(): messagebox.showwarning('Stock Firmware', P('stock_bad', file=STOCK))
            return
        if messagebox.askyesno('Stock Firmware Missing', P('stock_missing', file=STOCK)):
            self.do_check()

    # -------------------------------------------------------------------------- jobs
    def say(self, s):
        self.log.configure(state='normal'); self.log.insert('end', s + '\n'); self.log.see('end'); self.log.configure(state='disabled')

    def _poll(self):
        try:
            while True:
                kind, val = self.q.get_nowait()
                if kind == 'say': self.say(val)
                elif kind == 'prog': self._progress(val)
                elif kind == 'done': self._finish(*val)
        except queue.Empty: pass
        self.after(100, self._poll)

    def run_job(self, label, args, after=None):
        if self.busy: return
        self.busy = True; self._refresh(); self.prog.stop(); self.prog.configure(mode='indeterminate'); self.prog['value'] = 0; self._pct(''); self.prog.configure(style='Horizontal.TProgressbar'); self.prog.start(12); self.say(f'--- {label}')
        def work():
            # Every job runs in its own helper process (the same Python, om108_patcher.py): NXP's USB library must not be
            # used twice from one program on macOS, and a crash in it can then never close this window.
            rc, value = 1, None
            try:
                flags = 0x08000000 if sys.platform == 'win32' else 0         # Windows: no console window
                proc = subprocess.Popen([sys.executable, '-u', str(ASSETS / 'om108_patcher.py')] + list(args) + ['--result-json'],
                                        cwd=str(ASSETS), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                        encoding='utf-8', errors='replace', creationflags=flags)
                cont = False
                for line in proc.stdout:
                    line = line.rstrip('\n')
                    if line.startswith('@@PROGRESS '):
                        try: self.q.put(('prog', int(line[11:])))
                        except ValueError: pass
                        continue
                    if line.startswith('@@RESULT '):
                        try: value = json.loads(line[9:])
                        except ValueError: pass
                    elif cont and line[:1] in (' ', '\t'): self._quiet(line); cont = False   # the source line Python prints under its warning
                    elif re.match(r'\s*(\S*warning\b|.*\b\w*Warning:)', line, re.I):          # library notices: the log file only
                        self._quiet(line); cont = bool(re.match(r'.*:\d+: \w*Warning:', line))
                    elif line.strip(): cont = False; self.q.put(('say', line))
                rc = proc.wait()
                if rc not in (0, 1):
                    self.q.put(('say', f'STOPPED: the flashing helper ended unexpectedly (code {rc}). Nothing unsafe happened: '
                                       'switch the Omnichord off and on (UPDATE), then try again.'))
                    rc = 1
            except Exception:
                self.q.put(('say', 'STOPPED: unexpected error:\n' + traceback.format_exc(limit=3)))
            self.q.put(('done', (rc, label, after, value)))
        threading.Thread(target=work, daemon=True).start()

    def _quiet(self, line):
        """A WARNING line from NXP's SPSDK or Python itself (not from this tool): kept in logs/messages_<date>.log, not shown."""
        try:
            (UNIT / 'logs').mkdir(parents=True, exist_ok=True)
            with open(UNIT / 'logs' / time.strftime('messages_%Y%m%d.log'), 'a', encoding='utf-8') as f: f.write(time.strftime('%H:%M:%S ') + line + '\n')
        except OSError: pass

    def _progress(self, pct):
        """The helper's percent complete: the bar fills (it moves to and fro until the first figure arrives)."""
        if str(self.prog.cget('mode')) != 'determinate': self.prog.stop(); self.prog.configure(mode='determinate')
        self.prog['value'] = pct; self._pct(f'{pct} %')

    def _finish(self, rc, label, after, value):
        self.busy = False; self.prog.stop(); self._refresh()
        if rc == 0 and str(self.prog.cget('mode')) == 'determinate': self.prog['value'] = 100; self._pct('100 %')
        self.after(1500 if rc == 0 else 0, self._prog_reset)
        if rc == 0 and after: after(value)
        if rc != 0: messagebox.showwarning(label, P('stopped'))

    def _pct(self, t):
        self._pct_text = t; ttk.Style(self).configure('Horizontal.TProgressbar', text=t); ttk.Style(self).configure('Idle.Horizontal.TProgressbar', text='')

    def _prog_reset(self):
        if self.busy: return
        self.prog.stop(); self.prog.configure(mode='determinate', style='Idle.Horizontal.TProgressbar'); self.prog['value'] = 0; self._pct('')   # no moving block, no text when idle

    def _ready(self, what, first=''):
        return messagebox.askokcancel(what, first + P('ready'))

    def do_check(self):
        if self._ready('Check/Dump Firmware'): self.run_job('check / dump firmware (read only)', ['scan'], after=self._checked)

    def _checked(self, v):
        if not v: return
        if v.get('settings') and OP.norm_settings(v['settings']) != OP.norm_settings(self.to_dict()):
            if messagebox.askyesno('Check/Dump Firmware', P('check_settings', result=v['text'])):
                d = OP.norm_settings(v['settings']); d['subs'] = [None if s is None else {'sub': s['sub'], 'alt': s['alt']} for s in d['subs']]
                self.load_dict(d); self.say('The settings of the firmware on the unit are now shown.')
        if messagebox.askyesno('Check/Dump Firmware', P('check_done', result=v['text'])):
            DUMPS.mkdir(exist_ok=True); dst = DUMPS / time.strftime('om108_dump_%Y-%m-%d_%H%M%S.bin')
            shutil.copyfile(v['file'], dst); self.say(f'Saved: {DUMPS.name}/{dst.name}')

    def do_defaults(self):
        self.load_dict(defaults()); self.say('All settings set to the Suggested Default.')

    def _f_press(self, e=None):
        self.f_down = True
        if self._f_job: self.after_cancel(self._f_job); self._f_job = None
    def _f_release(self, e=None):                        # key repeat sends release + press pairs: clear only after a pause
        if self._f_job: self.after_cancel(self._f_job)
        self._f_job = self.after(150, lambda: setattr(self, 'f_down', False))

    def do_install(self):
        d = self.to_dict()
        try: OP.norm_settings(d)
        except OP.Stop as e: messagebox.showwarning('Install Firmware', str(e)); return
        work = UNIT / 'work'; work.mkdir(parents=True, exist_ok=True); sf = work / 'install_settings.json'
        sf.write_text(json.dumps(d, indent=1), encoding='utf-8')
        if self._ready('Install Firmware'):
            self.run_job('install', ['install', '--settings', str(sf)], after=lambda v: messagebox.showinfo('Firmware Installed', P('install_done')))
    def do_restore(self):
        if self.f_down:
            self.f_down = False
            if messagebox.askyesno('Full Factory Restore', P('full_warning'), icon='warning', default='no') \
               and self._ready('Full Factory Restore'):
                self.run_job('full factory restore', ['restore', '--full'], after=lambda v: messagebox.showinfo('Full Factory Restore', P('factory_done')))
            return
        if messagebox.askyesno('Restore Stock Firmware', P('restore_ask')) and self._ready('Restore Stock Firmware'):
            self.run_job('restore', ['restore'], after=lambda v: messagebox.showinfo('Restore Stock Firmware', P('factory_done')))

if __name__ == '__main__':
    App().mainloop()
