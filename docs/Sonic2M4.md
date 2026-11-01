# Sonic 2 milestone M4: the pre-beta (September 14, 1992)

The reference is Neto's disassembly of the pre-beta, https://github.com/NyperYuhgard/Sonic-2-September-14-1992-Pre-Beta-prototype (ROM by drx, hidden-palace.org); kept outside this repository at `~/Projects/s2prebeta-disasm` (`S2140992.asm`, `vars.asm`, `Data/<zone>/`, `Data/Objects/Obj_0x*.asm`, `Data/Sounds/*.snd`). M3 (the alpha) is tagged `S2A`; M4 follows it. Nothing of M4 is built yet.

## Do not port

* **Hidden Palace (HPZ) data.** Sonic Team is in the middle of removing Hidden Palace in this build, and we do not want that state (the user, 2026-10-31). Nothing of the pre-beta's Hidden Palace is imported in M4: not its layout (the header of `S2140992.asm` says it is a copy of Oil Ocean act 1's), not its art, objects, music assignment, demo or events. Hidden Palace stays in this port exactly as M2 and M3 left it (zone slot $08, with the versus page and the music plan in the notes): the port only adds, moves and replaces zones, it never removes them, so the pre-beta's removal is not followed.

## Notes from the pre-beta's own header

* The title card object refers to Sonic 1's oval and Final Zone mappings.
* The Rhinobot's art was edited to use another palette.
