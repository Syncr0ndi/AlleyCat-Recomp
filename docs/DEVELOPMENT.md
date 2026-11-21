# Development guide

## Requirements

- Windows 10 or 11
- Python 3
- CMake 3.20+ and Ninja
- [LLVM-MinGW](https://github.com/mstorsjo/llvm-mingw) (clang)
- Your own `CAT.EXE` (55,067 bytes, SHA-256 `4979c886…7bfdd`)

CMake downloads SDL2 2.30.9 itself, pinned by hash.

## Building

```powershell
cmake -S . -B build/rel -G Ninja -DCMAKE_C_COMPILER=clang -DCAT_EXE="C:/path/to/CAT.EXE"
cmake --build build/rel
```

| Target | What it is |
|---|---|
| `alleycat_release` | The game, built from the hand-written C only. This is what ships. |
| `alleycat` | The development build: hand-written and recompiled code together, so either can run. |
| `cat_test` | Headless test runner (scripted keys, PNG screenshots, WAV audio, the checker). |
| `cat_test_release` | The test runner built like the release, without the recompiled code. |

The recompiled C is generated from `CAT.EXE` into the build folder. It's derived from the game, so
it's never committed (`.gitignore` excludes `build/`, `dist/` and every `.exe`).

## Making a release

```powershell
python tools/make_release.py C:/path/to/CAT.EXE
```

This writes `dist/AlleyCat/` and `dist/AlleyCat-Windows.zip`:
- `alleycat.exe`
- `README.txt`
- `LICENSES/`, with this port's MIT licence and SDL2's zlib licence

It refuses to build the zip if the program contains any 32-byte run of `CAT.EXE`.

## Checking the hand-written code

```powershell
python tools/check.py C:/path/to/CAT.EXE --all-rooms
```

This runs many autoplayed games in check mode. For every call to a hand-written routine, the
recompiled original runs too, from the same state, and the two are compared:
- registers and flags, limited to the ones still used afterwards (`tools/liveness.py`);
- memory;
- port I/O and BIOS timer calls, recorded and replayed;
- interrupts, replayed at the same point.

The report lists calls checked and mismatches per routine. [REWRITE.md](REWRITE.md) describes the
method.

## Test runner

```powershell
build/rel/cat_test.exe C:/path/to/CAT.EXE --frames 600 --key 100:39 --shot 300:title.png --status 60
```

| Option | Meaning |
|---|---|
| `--frames N` | Stop after N frames. |
| `--key FRAME:SC[:LEN]` | Press XT scancode `SC` (hex) at `FRAME` for `LEN` frames. |
| `--shot FRAME:FILE.png` | Save a screenshot. |
| `--wav FILE.wav` | Record the speaker. |
| `--status N` | Print a video-memory hash every N frames. |
| `--autoplay SEED` | Random but plausible play, including menus, pauses and restarts. |
| `--rooms`, `--room N` | Jump into rooms (or always room N) for coverage. |
| `--joystick` | Play with the emulated joystick. |
| `--hand-mode gen\|on\|check` | Recompiled code only, hand-written code (default), or compare both. |
| `--coverage FILE` | Write which recompiled routines ran. |

## Source layout

| Path | Contents |
|---|---|
| `src/game/` | The hand-written game, in 30 modules (cat, dog, mice, windows, the rooms, sound, menus, main loop…), plus the check harness `hand.c`. |
| `src/pc/` | The IBM PC model: memory, a small BIOS in C, 8253 timer and speaker, keyboard and 8259, CGA timing and colours, game port. `font8x8.c` is a font drawn for this project. |
| `src/cpu/` | The 8086/V30 CPU core, used by the recompiled code and the checker. |
| `src/app/` | The SDL2 front end (`main.c`), the headless runner (`test_main.c`), the `CAT.EXE` check and the PNG writer. |
| `tools/catmap.py` | Maps the code by recursive descent: 269 routines, 8,482 instructions. |
| `tools/catrecomp.py` | The static recompiler: turns the map into C, and routes calls to the hand-written routines. |
| `tools/v30dec.py` | Instruction decoder. |
| `tools/check.py`, `liveness.py`, `coverage.py`, `hand_coverage.py`, `callgraph.py` | The checker and analysis tools. |
| `tools/hand_entries.py`, `make_release.py` | Release routine table and packaging. |
