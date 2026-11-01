# Sonic 2 milestone M4: the pre-beta (September 14, 1992)

The reference is Neto's disassembly of the pre-beta, https://github.com/NyperYuhgard/Sonic-2-September-14-1992-Pre-Beta-prototype (ROM by drx, hidden-palace.org); kept outside this repository at `~/Projects/s2prebeta-disasm` (`S2140992.asm`, `vars.asm`, `Data/<zone>/`, `Data/Objects/Obj_0x*.asm`, `Data/Sounds/*.snd`). M3 (the alpha) is tagged `S2A`; M4 follows it. Nothing of M4 is built yet.

## Do not port

* **Hidden Palace (HPZ) data.** Sonic Team is in the middle of removing Hidden Palace in this build, and we do not want that state (the user, 2026-10-31). Nothing of the pre-beta's Hidden Palace is imported in M4: not its layout (the header of `S2140992.asm` says it is a copy of Oil Ocean act 1's), not its art, objects, music assignment, demo or events. Hidden Palace stays in this port exactly as M2 and M3 left it (zone slot $08, with the versus page and the music plan in the notes): the port only adds, moves and replaces zones, it never removes them, so the pre-beta's removal is not followed.

* **Wood Zone (WZ) data**, the same (the user, 2026-10-31: "same with WZ"). Nothing of the pre-beta's Wood Zone is imported in M4 (its `Data/Wz`, layout, art, objects, music assignment, demo `Demo_Wood`, events). Wood Zone stays in this port as M2 and M3 left it (zone slot $02), for the same reason: zones are only added, moved or replaced here, never removed.

## Notes from the pre-beta's own header

* The title card object refers to Sonic 1's oval and Final Zone mappings.
* The Rhinobot's art was edited to use another palette.

## From Sonic Retro (the user's screenshots, 2026-10-31)

* The pre-beta is dated September 14, 1992 (EPROMs found by drx, released November 9, 2019, a day before the alpha). The prerelease order on that site: Nick Arcade (1992-05), Simon Wai (1992-0X), Alpha (08-21), **Pre-Beta (09-14)**, CENSOR (09-1X), Beta 4 (09-18), **Beta 5 (09-21)**, Beta 6 (09-22), Beta 7 (09-24), Beta 8 (09-24): so M4 = pre-beta, M5 = censor, M6 = beta 4 (s2b4), and s2b5 is beta 5.
* "To the untrained eye this version could be passed off as the final game, with every level being accounted for and all but the final three (Sky Chase Zone, Wing Fortress Zone (known here as Sky Fortress Zone) and Death Egg Zone) being completable. However, the Special Stages and 2-player mode have yet to be implemented properly, and there are a number of minor differences and layout changes, particularly towards the end of the game. It also lacks much of the polish seen in prototypes from here on out, such as title and score cards, introductions and endings."
* So the three zones that are not completable (Sky Chase, Wing Fortress/Sky Fortress, Death Egg) are not zones to play in M4; the title and score cards come in later builds (the title card in full around s2b5).

## Zone slots (2026-10-31)

Cyber City moved from slot $0E to $09, Sky Chase has slot $10 (no data yet), the slot $0E is empty, and a slot $11 exists for Sonic 1's zone names that the shared Sonic 1 code tests for (ZONE_SLOTS is 18). Hidden Palace ($08) and Wood ($02) keep their slots and stay as they are (see above).

