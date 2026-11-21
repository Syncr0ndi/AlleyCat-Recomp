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

All routines are hand-written (src/game, 30 modules), 260 checked call by call against the
recompiled code with 0 mismatches (`python tools/check.py CAT.EXE --all-rooms`: random play
seeds, every room pinned, joystick mode, the quit key; interrupts recorded and replayed).
Not checkable that way, and covered by play instead:

- 0000 and the room loops 0260-0459 (`mainloop.c`): they never return. All their callees
  are checked; in normal play no recompiled routine runs (`--coverage` stays empty) and the
  release runner executes 0 interpreted game instructions.
- 14FB, 1554: the PCjr keyboard handlers; the emulated machine is an IBM PC, so they never
  run and are not part of the release.

Harness notes: liveness-masked register/flag comparison (tools/liveness.py), port I/O and
INT 1Ah recorded / replayed, interrupts replayed by I/O position and routine entry/return
count, stalled hand runs abandoned, line coverage of the hand code (tools/hand_coverage.py).
Room-entry, room-finish and cage-drop pokes in `cat_test --rooms` give coverage autoplay
alone would not reach.

## Next: plain C

Turn the register-level routines into ordinary C: a `Game` struct with named fields over the
data segment, routines with parameters and return values instead of AX/CF, direct calls
instead of `CALL(0x....)`, and the BIOS / port accesses as calls into a small platform
layer. Then the CPU model is no longer needed for the game, only for the checker.
