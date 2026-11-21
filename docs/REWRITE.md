# Phase 2: rewriting Alley Cat by hand, checked against the recompiled code

The recompiled build (phase 1) is the reference. Routines are rewritten by hand in C one at
a time, and each one is proved equal to its recompiled original on real game runs.

## Method

1. Pick a routine (or a small group) from `build/map/listing.asm` (`python tools/catmap.py
   CAT.EXE` writes it). Leaf routines first, then their callers.
2. Write `void hand_XXXX(V30 *c)` in `src/game/<module>.c`, where XXXX is the routine's
   offset in the code segment. Keep the original register interface for now: inputs and
   outputs in the 8086 registers (`AX`, `CL`, ... from `hand.h`), game variables through
   `G8(off)` / `g16(c, off)`, other memory through `m8/m16/w8/w16`. Do not do the final
   RET; it is done for you. Charge the approximate 8088 time with `V30_TICK(n)`. Call other
   routines with `cat_call(c, off)`.
3. Rebuild. The generator finds every `hand_XXXX` and routes calls of the original through
   `cat_hand_call()`. It refuses a routine that other code jumps into the middle of.
4. Check with the test runner:

   ```powershell
   build/rel/cat_test.exe CAT.EXE --hand-mode check --frames 3000 --key 100:39 --key 160:31 --key 240:25 --key 320:39 ...
   ```

   In check mode every call runs the recompiled routine and the hand-written one from the
   same state and compares registers, flags and memory (program image, data segment, CGA
   memory). The recompiled result is kept, so the run's timing stays identical to phase 1
   and interrupts are held during the comparison. Mismatches print the inputs and the
   differing registers / bytes; the summary lists calls checked per routine. A routine
   that is never called in the test run is not verified yet: extend the key script.
5. `--hand-mode on` (the default, also in the game) uses the hand-written versions;
   `--hand-mode gen` uses only the recompiled code.

Routines that do port I/O or BIOS calls cannot run twice in check mode without doubling
their side effects (sound, keyboard acknowledge). For those, compare with I/O recorded,
or check them through their callers once the rest is done.

## After all routines are hand-written

Once nothing calls generated code, move from the register interface to normal C: game
state in a struct with named fields, routines with parameters and return values, BIOS and
port access replaced by direct calls into the machine layer. The recompiler is then only a
test tool, and the shipped program contains no translated game code. The data segment
(sprites, tables) keeps being read from the player's CAT.EXE at start-up.

## Progress

| Area (code offset) | Routines | Done | Notes |
|---|---:|---:|---|
| 0000-0FFF | 20 | 0 | main loop and game-state dispatch (jump table at 0250), rooms |
| 1000-1FFF | 44 | 0 | machine setup (13AA-147F), keyboard IRQ handler (14B3), cat control (1E63) |
| 2000-2FFF | 55 | 7 | drawing primitives 2CB0-2DCA and random 2DFD done; collision 2E29 next |
| 3000-3FFF | 38 | 0 | scoring / bonus multiplier text (3A6C uses BIOS text) |
| 4000-4FFF | 44 | 0 | room logic |
| 5000-5FFF | 60 | 0 | speaker sound effects (5400-5B54), menus and joystick setup (5C60-5FE5) |
| 6000-6FFF | 8 | 0 | intro animation (6040), flying object (6106-62A6) |
| total | 269 | 7 | |

Done and verified (0 mismatches, flags included, over a 3000-frame scripted game):
2CB0 screen_address, 2CCC sprite_draw, 2D35 sprite_and, 2D70 rect_copy, 2D9D blit,
2DCA grab, 2DFD random.
