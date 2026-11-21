# Alley Cat for Windows

A native Windows port of **Alley Cat** (Bill Williams / SynSoft, IBM, 1984), the MS-DOS
game. It runs the original game logic at the speed of a 4.77 MHz IBM PC, with CGA graphics,
PC-speaker sound and keyboard controls, in an SDL2 window.

**The game itself is not included.** You need your own `CAT.EXE` (55,067 bytes, dated
22 Aug 1984). On first start the program asks for it and remembers its location in
`alleycat.ini`.

## Status

| Phase | What | State |
|---|---|---|
| 1 | Static recompilation of the 8086 code to C, on a small IBM PC model | done: title, menus and gameplay run |
| 2 | Hand rewrite into readable C, routine by routine, each checked against phase 1 (see docs/REWRITE.md) | started: 7 of 269 routines |
| 3 | Release that ships no game code or data | planned |

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

The generated C is derived from the game, so it is written to the build folder and never
committed.

## Building (Windows, LLVM-MinGW clang)

```powershell
cmake -S . -B build/rel -G Ninja -DCMAKE_C_COMPILER=clang -DCAT_EXE="C:/path/to/CAT.EXE"
cmake --build build/rel
build/rel/alleycat.exe
```

CMake fetches SDL2 2.30.9 (pinned by hash). `cat_test.exe` is a headless runner for
checks: scripted keys, PNG screenshots, WAV audio and video-memory hashes, e.g.

```powershell
build/rel/cat_test.exe C:/path/to/CAT.EXE --frames 600 --key 100:39 --shot 300:title.png --status 60
```

## Controls (the original game's)

Arrow keys move, Alt jumps/acts, Esc pauses, Ctrl-S sound on/off, Ctrl-R restart,
Ctrl-M menu. F11 full screen and F12 screenshot are added by this port.

## Licence

MIT for the code in this repository (see LICENSE). Alley Cat is © 1983-84 SynSoft / IBM;
none of it is in this repository.
