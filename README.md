# Alley Cat for Windows

A native Windows port of **Alley Cat** (Bill Williams / SynSoft, IBM, 1984), the MS-DOS
game, rewritten by hand in C from the original 8086 program. It plays like the original on
a 4.77 MHz IBM PC with CGA graphics and PC-speaker sound, in an SDL2 window, with keyboard
or a game controller.

**The game itself is not included.** You need your own `CAT.EXE` (55,067 bytes, dated
22 Aug 1984). On first start the program asks for it and remembers its location in
`alleycat.ini`.

## Status

| Phase | What | State |
|---|---|---|
| 1 | Static recompilation of the 8086 code to C, on a small IBM PC model | done (now the test reference) |
| 2 | Hand rewrite into C, routine by routine, each checked call by call against phase 1 (docs/REWRITE.md) | done: all 260 routines a PC runs, plus the main loop |
| 3 | Release built only from the hand-written C, shipping no game code or data | done: `alleycat_release`, `tools/make_release.py` |
| 4 | Turn the register-level C into plain C (named state, parameters, no CPU model) | next |

## How it works

`CAT.EXE` is an unpacked DOS program of hand-written 8086 assembly: one code segment
(25 KB), one data segment (29 KB of sprites, tables and variables), and no DOS calls. It
talks to the BIOS (INT 10h/11h/1Ah) and directly to the hardware: CGA 320x200 4-colour
video memory, the 8253 timer and port 61h for the speaker, and its own IRQ 1 keyboard
handler. It paces itself on the 18.2 Hz BIOS tick and the CGA vertical retrace.

- `tools/catmap.py` maps the code by recursive descent (8,482 instructions, 269 functions,
  98.4% of the code segment; the rest is in-code variables). The only indirect jump is an
  8-entry table, listed as a seed.
- `tools/catrecomp.py` turns every instruction into C on an exact emulated register file and
  stack, and charges approximate 8088 cycle costs so emulated time matches a 4.77 MHz PC. The
  decoder, recompiler and CPU core come from
  [LOHT-Recomp](https://github.com/Syncr0ndi/LOHT-Recomp) (NEC V30, a superset of the 8086).
- `src/pc/` is the machine: memory, a C BIOS, PIT + speaker synthesis, keyboard + 8259, CGA
  timing and colours. `src/pc/font8x8.c` is a font drawn for this project (the BIOS text
  font is IBM's).
- `src/app/main.c` is the SDL2 front end: 4:3 window, F11 full screen, F12 screenshot,
  audio queue, real-time pacing at 59.92 Hz frames.

- `src/game/` is the hand-written game: 30 modules (cat, dog, mice, windows, the seven
  rooms, sound, menus, main loop ...). At this stage each routine keeps the original's
  register interface, so it can be checked against the recompiled code.

The generated C is derived from the game, so it is written to the build folder and never
committed. The release program (`alleycat_release`) is built without it: only `src/game`,
the machine and a routine table made from the hand-written function names.

## Building (Windows, LLVM-MinGW clang)

```powershell
cmake -S . -B build/rel -G Ninja -DCMAKE_C_COMPILER=clang -DCAT_EXE="C:/path/to/CAT.EXE"
cmake --build build/rel
build/rel/alleycat_release.exe        # the game (hand-written C only)
python tools/make_release.py C:/path/to/CAT.EXE   # dist/AlleyCat-Windows.zip, scanned for game bytes
python tools/check.py C:/path/to/CAT.EXE --all-rooms   # compare every routine with the original
```

CMake fetches SDL2 2.30.9 (pinned by hash). `cat_test.exe` is a headless runner for
checks: scripted keys, PNG screenshots, WAV audio and video-memory hashes, e.g.

```powershell
build/rel/cat_test.exe C:/path/to/CAT.EXE --frames 600 --key 100:39 --shot 300:title.png --status 60
```

## Controls (the original game's)

Arrow keys move, Alt jumps/acts, Esc pauses, Ctrl-S sound on/off, Ctrl-R restart,
Ctrl-M menu. Added by this port: F11 full screen, F12 screenshot, game controller (as the
PC joystick; D-pad/A/Start also act as arrows/Alt/Esc).

## Licence

MIT for the code in this repository (see LICENSE). Alley Cat is © 1983-84 SynSoft / IBM;
none of it is in this repository.
