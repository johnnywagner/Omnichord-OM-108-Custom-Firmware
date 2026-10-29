# firmware_source: the code behind the custom firmware

**You don't need anything in this folder to use the tool.** It's here so anyone curious can see exactly what the custom
firmware does to their Omnichord.

## What this is, in plain words
The Omnichord has a small computer inside (an NXP i.MX RT1021 chip). The program that runs on it is the **firmware**.
Suzuki's original firmware runs the instrument: buttons, sounds, rhythms, MIDI.

This project doesn't replace that firmware. It **adds to it**: a small extra program is placed in unused space in the
instrument's memory, and a few places in Suzuki's original program are changed so that they briefly hand control to the
extra program. The extra program does its job (a vibrato, an OM-27 rhythm, a jazz chord, remembering your panel
settings...) and hands control back. Apart from that, some replaced sounds and a few fixes for stock bugs, the
instrument works exactly as Suzuki made it.

The files here are that extra program's **source code**: the human-readable text a programmer writes. A compiler turns it
into the machine code the chip runs.

## The files
| File | What it is |
|---|---|
| `mod2.c` | **The main code** (C language, about 2,500 lines). Every feature lives here, each in its own section with a heading comment (search for `----` to jump between them). A good place to start reading: the "settings block" section near the top, which is where the firmware reads the choices you make in the tool. |
| `entry.s` | **The doorways** (assembly language). A short table of addresses: each changed place in Suzuki's program jumps to one entry here, which leads into the right part of `mod2.c`. Also a few tiny pieces of code that have to be written in assembly. |
| `link.ld` | **Where it goes** (linker script, a few lines). Tells the build tools to place the code at memory address `0x6003A000`, the unused space it lives in. |
| `build.sh` | **How it's built** (a few commands). Runs the free clang / LLVM compiler to turn the two code files into the machine code. |
| `DEVELOPMENT.md` | **The starting point for making your own changes:** how the modification works, the safety rules, and where everything is. For programmers, and just as useful to hand to an AI coding assistant. |
| `notes/` | **The raw notes** from figuring out how the stock firmware works: addresses, what the parts of the original program do, the touch sensor, lessons learned. |
| `ghidra/` | **The scripts** used with Ghidra (a free tool) to turn the original firmware back into readable code. The result isn't included: it would be Suzuki's code. |

## How it gets from here to your instrument
1. These files are compiled into machine code (`build.sh`).
2. That machine code, the replacement sounds and the small changes to Suzuki's program were packed into one file:
   `assets/om108_mod.patch`. It holds only this project's own bytes, no Suzuki code.
3. When you click **Install Firmware**, the tool combines that file with **your own unit's original firmware** (read from
   your unit with Check/Dump Firmware), adds your settings, and writes the result to the instrument.

So the tool never compiles anything: steps 1 and 2 were done once, when the firmware was made.

## Can I change it and build my own?
Yes, with some programming help: start with `DEVELOPMENT.md` and the notes. If you're not a programmer, an AI coding
assistant such as Claude Code can read them for you: open this repo in it and describe the change you want. You'll need your unit's original firmware (the tool saves it when you
click Check/Dump Firmware on a stock unit) and some patience: this is real embedded programming. Never let anything
write to your instrument until you understand the plan and have a tested way back to stock.
