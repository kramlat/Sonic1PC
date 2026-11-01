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

## Order (the user, 2026-10-31)

The special stage is saved for last in M4: it is the biggest payload.

## Done so far

* The title screen's menu, 1 PLAYER / 2 PLAYER VS (the pre-beta's object $0F; `Object/TitleMenu.c`, art `Art/TitleMenu` = `1p2pVs.nem` at tile $2F0, `asm/Mappings/TitleMenu.asm`): Up and Down turn it over, Start takes the choice as `two_player_mode`.
* The credits theme ($9E) and the effects $EA and $EB, from the final game (the pre-beta has them).
* Zone slots: Cyber City $09, Sky Chase $10.
* The title cards (object $34 of the pre-beta, `Object/TitleCard.c`, `tools/import_s2prebeta_titlecards.pl`): the pre-beta's names as they are (Emerald Hill's card says GREEN HILL: its first slot's name there), Wood, Sand Shower, Cyber City and Hidden Palace written from the same alphabet. The card brings its own art (`game_info.title_card_own_art`: no pattern load cue), loaded when the level's first lists are done; the zone's second list waits for the card to go (the water zones' bubbles are in the letters' tiles).

## Known pre-beta bugs and what the port does

* **Water levels' bubbles load over the title card's art** (the user, 2026-10-31: a known bug of the real pre-beta ROM): the card's letters are at tile $5DE on, where the water zones' countdown bubbles are loaded ($BD00, tile $5E8), so the card is garbled in the real ROM. The port does not keep it (the user: no problem, the port is not held to the hardware's limits): the card's art goes in after the level's first pattern lists, and the zone's second list waits for the card to go (`Object/TitleCard.c`, `Level.c`). Putting the original order back is the two lines marked there if it is ever wanted behind `SCP_FIX_BUGS` (the port's rule for original bugs that do not crash is to keep them unless that is on).

## The SEGA screen, in stages (the user, 2026-10-31)

The pre-beta's SEGA screen is the final game's (its art, `Sega_Screen`, and objects $B0 and $B1: Sonic running across the logo at twice his size, the letters pushed aside line by line, two palette sweeps, the voice at the end). `GM_Sega.c` (Sonic 2's own, replacing Sonic 1's) takes it in stages, the objects loaded directly (no id table):

1. **DONE** The logo where it ends (art, map, palette: `res/S2Sega`), the trademark hidden by object $B1 (`Object/SegaScreen.c`; the Japanese console, `SCP_JP`, keeps it), the pre-beta's timing (about 80 frames of the animation, the voice, 180 frames of hold, Start leaves).
2. Sonic running across the logo (object $B0: his art cut from `Sonic.dat`, tiles $2B6 to $301, doubled in RAM by `Sub_Sega_Intro`, at tile $88; frames 0-3 of `asm/Mappings/SegaSprites.asm` are made and unused).
3. The letters' lines pushed aside (35 offsets, the even lines on the way in and the odd ones on the way back) and the palette sweeps (the lists at $FB10 and $FB00).
4. **The last stage: the SEGA voice is replaced with the final game's. Ask the user for the sample rate when it is time for that one.**

