ALLEY CAT for Windows
=====================

A native Windows version of Alley Cat (Bill Williams / SynSoft, IBM, 1984), rewritten
in C from the original program. It plays like the original on a 4.77 MHz IBM PC with a
CGA card: same graphics, PC-speaker sound, controls and speed.

THE GAME FILE IS NOT INCLUDED
You need your own copy of the original CAT.EXE (55,067 bytes, dated 22 August 1984).
On first start the program asks where it is and remembers it in alleycat.ini. You can
also put CAT.EXE next to alleycat.exe, or start: alleycat.exe C:\path\to\CAT.EXE
Your CAT.EXE is only read, never changed.

If Windows shows "Windows protected your PC", click More info, then Run anyway (the
program isn't code-signed).

CONTROLS (the original game's)
  Arrow keys          move / climb / jump
  Alt                 special action (lap milk, put down a brick, go through holes)
  Esc                 pause ("paws")
  Ctrl-S              sound on / off
  Ctrl-R              restart the game
  Ctrl-M              back to the menu
Added by this version:
  F11 or Alt+Enter    full screen
  F12                 screenshot (PNG next to the program)
  Game controller     works as the PC joystick (answer Y to the joystick question),
                      and its D-pad / A / Start also act as the arrows / Alt / Esc

CREDITS AND LICENCES
--------------------
"Alley Cat" (c) 1983-84 Bill Williams / SynSoft, published by IBM. This is an
unofficial, non-commercial fan project, not affiliated with or endorsed by the
copyright holders.

This port (alleycat.exe and its source code) is (c) 2026 Syncr0ndi and released under
the MIT licence (LICENSES\AlleyCat-port-LICENSE.txt). The licence covers only the
port's own code - not the game, its graphics, sound or program, which belong to their
owners and come from your own CAT.EXE.

Open-source components (licence texts in the LICENSES folder):
  * SDL2 2.30.9 - window, input and audio - zlib licence (LICENSES\SDL2-LICENSE.txt)

Source code: https://github.com/Syncr0ndi/AlleyCat-Recomp
