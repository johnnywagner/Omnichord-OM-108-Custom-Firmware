# Suzuki Omnichord OM-108 Custom Firmware Tool

*by JohnnyW*

A small desktop tool (macOS and Windows) that builds and installs a custom firmware for the **Suzuki Omnichord OM-108**,
made from **your own unit's original firmware**, with the features and sounds you choose. It can always put the original
firmware back.

The full guide to the new features on the instrument is the one-page **[User Manual](OM108_CFW_User_Manual.pdf)**
(`OM108_CFW_User_Manual.pdf`). Inside the tool, hover over anything in the window for a full explanation.

> **Disclaimer:** this is an unofficial hobby project, not affiliated with or endorsed by Suzuki or NXP. It is provided
> as is, without any warranty. Modifying your instrument's firmware is at your own risk. It was built and tested on one
> OM-108; units running firmware the tool doesn't recognise are refused, never written.

## Quick start
**Setup (once):**
1. Install Python 3.9 or newer with tkinter. For single purpose tools like this, I recommend something like
   [Thonny](https://thonny.org) for beginners (python.org's installer and Homebrew's `python-tk` also work).
2. Install NXP's SPSDK: `pip install spsdk` in a Command Prompt / Terminal, or in Thonny: Tools > Manage packages >
   spsdk. The tool won't start without it.
3. A USB cable (USB-B on the Omnichord's end). No drivers are needed: the unit's update mode is a standard USB device.

**First use:**
1. Download this repo and open `om108_gui.py` in Thonny (or run `python3 om108_gui.py`).
2. Your unit must be on its original (stock) firmware. Turn the Omnichord off, leaving the batteries in. Remove the
   battery cover and set the **PLAY/UPDATE** switch to **UPDATE** with a small screwdriver or similar tool, connect USB
   and click **Check/Dump Firmware**. This only reads the unit, and saves its stock firmware in
   `my_unit/backups/om108_backup.bin`.
3. **Keep a copy of that file somewhere safe:** it is your way back to stock.
4. Choose your settings and click **Install Firmware**. **During all reads and writes, the red power button must be held
   down** until the log says DONE; if it is released during a transfer, just start over.
5. Set the switch back to **PLAY** and test.

**Restore Stock Firmware** puts the original back the same way. Every write is read back and checked; if an install is
interrupted, just run it again.

## What the custom firmware adds
Names below follow the panel labels, as in the User Manual: buttons in [BRACKETS], chord buttons in “quotes”.

### Startup modes (hold while switching on)
- **OM-27 mode:** hold the [PATTERN (slow rock/funk)] button. The 6 [PATTERN] buttons (including the yellow one) now
  play the 6 OM-27 rhythm patterns, left to right: Latin (yellow), Rock, Foxtrot, Slow Rock, Waltz, Swing. Hold any 2
  [PATTERN] buttons and press the [KEYBOARD (ON)] button to lock the selection.
- **Alternate Bass:** hold the [PATTERN] left button (yellow) for an alternate (plucked) bass sound; hold the [VOICE]
  left button (yellow) for the stock bass sound.
- **Jazz Mode:** hold the [KEYBOARD (ON)] button. Changes and adds chord fingerings (see the table below), and
  voice-leads intelligently when switching chords. This setting is not remembered between power cycles.
- **MIDI preset:** hold [VOICE] button 1–5 to choose P1–P5.

### Alternate sub sounds
After a voice has been selected, press the lit [VOICE] button again to toggle its alternate sub sound. “omni 1” has no
alternate sub sound due to technical limitations. Both sounds are configurable in the tool; the defaults are:

| Voice | omni 1 | omni 2 | harp | celeste | A. piano | guitar | FM piano | organ | vibes | banjo |
|---|---|---|---|---|---|---|---|---|---|---|
| **Sub** | – | Sine Pad | Dial Up | Sine Pad | Sine Pad | Sine Pad | Mellow Pad | Dah Guys | Strings | Strings |
| **Alt** | – | Mellow Pad | Evolve | Church Bell | Strings 2 | Mellow Pad | Rezy | 7th Doo | Hand Pan | Mellow Pad |

### Modulation Modes (Instant OFF plate)
In keyboard mode, three chord buttons that are otherwise unused set what the Instant OFF plate does:
- **“G♭-7th”** sets the modulation mode.
  - **Tap:** cycle to the next mode, displayed on the [VOICE] LEDs. The device will announce the selected mode.
  - **Hold + [↑]/[↓] buttons:** cycle through the modes silently.
  - **Hold + [VOICE] button 1–5:** choose one of the 5 modulation modes directly & silently.
- **“G♭-MINOR”:** hold to adjust the speed of the Auto modes with the [↑]/[↓] buttons, −6 to +6 on the [VOICE] LEDs.
- **“D♭-MINOR”:** hold to adjust the depth of the current mode, 0 to 12 on the [PATTERN] LEDs.

The [↓]/[↑] buttons are the “E♭-MAJOR”/“B♭-MAJOR” chord buttons, the same as for adjusting Transpose and Master Tune.
Reminder: pressing both together resets the parameter.

The 5 modes:
- **Instant Off:** stock behavior.
- **Vibrato Auto:** like the stock behavior, a finger on the plate enables the vibrato, but the plate now detects
  partial coverage and adjusts speed and depth accordingly. Try rolling your finger off slowly after pressing to hear
  the effect.
- **Vibrato Manual:** rest a finger on a corner of the Instant OFF plate and rock it in and out, like bending a guitar
  string.
- **Tremolo Auto:** the same as Vibrato Auto but for volume.
- **Tremolo Manual:** the same as Vibrato Manual but for volume.

**Modulation notes:** both Vibrato modes send matching MIDI pitch bend out. Both Tremolo modes are local to the unit and
are not sent over MIDI. Due to limitations of the analog circuits, neither Tremolo mode works on “omni 1” or the main
voice of “omni 2”.

**Keyboard Mode Modulation:** in keyboard mode, hold a chromatic key and press the “G♭-MINOR” or “D♭-MINOR” chord button
to play an Auto modulation mode on the played notes.

### Third Manual/Auto state
Hold the [Chord Manual/Auto] button and press the [KEYBOARD (ON)] button at any time to enter a third Manual/Auto state
combining Manual chords with the Auto bass (both LEDs lit).

### Other changes
- **Saved Settings:** all your settings are now saved between power-offs, except for the new Jazz Mode.
- **Factory reset** (both [VOICE] and [PATTERN] left buttons (yellow) held while switching on) now sets the battery type
  to Ni-MH, and also clears the saved settings.
- **MIDI:** the stock MIDI CC out messages (Volume & Release) are removed, making room for a fifth MIDI preset, P5.
- **Bug fixes:** two small bugs in the stock firmware, involving LED states and chord data sent via MIDI out, have been
  fixed.
- **Sounds:** the “harp” voice's main sound has been replaced with an electric harp sound (**Electric Harp**), and the
  clap rhythm sound has been replaced (**Better Clap**).
- **Spoken Mode Names:** the unit announces the rhythm mode chosen at power-on ([PATTERN (swing/waltz)]: “84 mode”,
  [PATTERN (country/bossanova)]: “108 mode”, [PATTERN (slow rock/funk)]: “27 mode”) and also the Modulation Mode
  selected via tapping.

### MIDI Presets reference (default)
Choose at power-on: hold [VOICE] button 1–5. Each part is sent on its own channel. In the tool you can pick each part's
channel and the presets it is sent in, plus MIDI Clock / Start / Stop per preset.

| Part | Channel | P1 | P2 | P3 | P4 | P5 |
|---|:-:|:-:|:-:|:-:|:-:|:-:|
| Main (strumplate) | 8 | | | ● | ● | ● |
| Chord | 9 | ● | | ● | ● | ● |
| Bass | 7 | | | | ● | ● |
| Sub (copy of Main) | 9 | | ● | | | |
| Drums \* | 1 | | | | | ● |
| Clock, Start / Stop | – | | | | ● | ● |

\* **Drum MIDI Remap:** drum notes are sent on a smaller note range matching the Teenage Engineering OP-XY.

### Jazz Mode reference
Shown in C; every root works the same. ★ = new combination. Unlike the stock mode, Jazz Mode voice-leads intelligently
when switching chords.

| Chord buttons | Plays |
|---|---|
| “C-MAJOR” | C6 |
| “C-MINOR” | Cm9 |
| “C-7th” | C13 |
| “C-MAJOR” + “C-7th” | Cmaj9 |
| “C-MINOR” + “C-7th” | Cm11 |
| “C-MAJOR” + “C-MINOR” | Cdim7 |
| “C-MAJOR” + “C-MINOR” + “C-7th” | C9♯5 |
| “C-MAJOR” + “F-7th” | C9sus4 |
| “C-MAJOR” + “F-MINOR” | C6/9 |
| ★ “C-MINOR” + “F-MAJOR” | Cm6/9 |
| ★ “C-MINOR” + “F-7th” | Cm7♭5 (half-diminished) |
| ★ “C-7th” + “F-MAJOR” | C13♭9 |
| ★ “C-MINOR” + “G-7th” | Cm(maj9) |

## What you choose in the tool
- **MIDI:** the five presets (P1–P5): each part's channel, the presets it is sent in, MIDI Clock / Start / Stop per
  preset, and the Drum MIDI Remap.
- **Sub Sounds:** the sub sound and the alternate sub sound for every voice.
- **Bonus Features:** Modulation Modes, Extras (OM-27 mode, Alternate Bass, Jazz Mode, 3rd Manual/Auto State), Spoken
  Mode Names, Electric Harp, Better Clap.
- **Always included:** Saved Settings and the stock firmware bug fixes.
- **Suggested Default** sets every choice back to the recommended setup, with every feature on.

## The folder
| | |
|---|---|
| `om108_gui.py` | the tool (start this) |
| `OM108_CFW_User_Manual.pdf` | the one-page User Manual for the new features |
| `assets/` | the tool's own files (don't delete): the install engine, the firmware recipe (`om108_mod.patch`: only the modification's own bytes, no Suzuki code), NXP's flashloader, the hover texts (`tooltips.txt`) and popup texts (`popups.txt`) |
| `my_unit/` | made on first use: everything about **your** unit: its stock firmware, copies read from it, logs, your settings. **Never share it**; delete it before giving the tool to someone else. |
| `firmware_source/` | the source code of the modification, plus the development notes for making your own changes (see its README) |

## License, credits and third-party files
- **Tool and firmware modification:** JohnnyW. This project's own code and documentation are released under the
  [MIT License](LICENSE). The third-party files below are not covered by it.
- **NXP flashloader** (`assets/ivt_flashloader_rt1020.bin`): NXP's i.MX RT1020 flashloader, Copyright NXP. It is
  **not** part of this project and not covered by any license this project uses. It is included unmodified, for
  convenience only: the tool loads it into the unit's memory (RAM) during an install or restore, and it is gone when the
  unit is switched off. NXP publishes the flashloader's source code under the BSD-3-Clause license
  ([mcuxsdk-middleware-bootloader](https://github.com/nxp-mcuxpresso/mcuxsdk-middleware-bootloader)). If you are NXP and
  would like this file removed, please open an issue and it will be taken down.
- **Flash configuration blocks** (`assets/omnichord_rw_fcb.bin`, `assets/omnichord_readonly_fcb.bin`): 512-byte flash
  settings read from the instrument, the write version adjusted for this tool.
- **NXP SPSDK** (`pip install spsdk`) does the USB communication. It is installed separately and not included here.
- **Suzuki and Omnichord** are trademarks of their respective owners, used here only to say which instrument this tool
  is for. This project contains no Suzuki firmware: the tool works from your own unit's firmware, read from your unit.
