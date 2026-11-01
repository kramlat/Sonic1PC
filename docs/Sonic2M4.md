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

* Emerald Hill, the pre-beta's Green Hill (`tools/import_s2prebeta_ghz.pl`, from its `Data/GHz`): its layouts (as the Sonic 3 & Knuckles layout files), objects, rings (one entry for each ring, sorted), chunks, collision (one interleaved file), blocks (five differ, in the secondary group: blocks $1EB-$1EF) and tiles (two more than the alpha's, $390-$391, also secondary: the shared primary group with Hill Top is unchanged, which the script checks), and the second act's start ($0060, the alpha's was $0040). Its art lists, palette, animated tiles and badnik art are the alpha's byte for byte; the level sizes and the background's lines are too. Its boss arena's left edge is $28F0 (the pre-beta writes $2940 and then $28F0: the alpha's was $2880). Not compared: the code of its objects against the alpha's.

* **The prison capsule is not an M4 item** (the user, 2026-11-01: that note was mixed up with M5, the censor build). It stays out of this milestone: the pre-beta only has Sonic 1's capsule code left in its object table (nothing places it, nothing loads its art), and our table's $3E is Sonic 1's `Obj_PrisonCapsule` as before. To be looked at again with M5.

* **Tails** (the user, 2026-11-01): the pre-beta's AI is ported (`Object/Tails.c`: Init, Spawning, Flying, Normal, Panic). **Flight added 2026-11-01** from the Sonic Retro guide "Add Tails' flying and swimming to Sonic 2" (Painto; the user's screenshots; the final game never had it): a second jump press in a jump makes him fly (8 seconds, flaps with a press, weak gravity), he hits enemies above him with his tails, he carries Sonic (Sonic hangs from him: `sonic_carried`, `Sonic_CarryCollision`; Sonic lets go with a jump, left or right with it), and the optional single player assist: Sonic's jump and Up call the AI Tails to fly and carry him (`CPU_ASSIST`, then `CPU_CARRY` with Sonic's pad driving him: left and right, Up to flap). The assist's AI (forcing the jump and the flaps) is ours: the guide only says the state is entered. Animations $21-$29 for Tails, $20 (Hang2) for Sonic and $B, $C for his tails were added to the tables (`tools/tails_flight_animation.pl`). **Flight art in 2026-11-01** (the user's own edit in the staging folder: `tools/import_tails_flight_art.pl`, then `tools/tails_flight_animation.pl --art`, `TAILS_FLIGHT_ART 1`): Tails now has 155 frames, $5E and $5F the spinning tails alone and $8B-$9A the 16 new ones; the guide has 17 ($8B-$9B): the user's sheet has six flying sprites without tails where the guide's has seven, so Tired is $8E, $8F, CarryTired the one $90, Swim $91-$95, SwimCarry $96, $97 and SwimTired $98-$9A. **Sounds in 2026-11-01**: Sonic 3 & Knuckles' Grab, Flying and Flying (Tired) ($4A, $BA, $BB of skdisasm) are effects $EC, $ED and $EE (`res/SMPS/converted/sfx`; priorities $70, $60, $60), played by `Tails_PlayFlightSound`: not listened to yet. Deferred on purpose (the user, 2026-11-01): the Oil Ocean fan fix and the Wing Fortress ones (propellers, wind tunnel, ending) are done and tested when those zones are (OOZ, and WFZ = Sky Fortress); the single player assist is tested after the sounds are in. Flying animations: passed (the user). Not done from the guide: the boss options (Tails drops Sonic when he hits a boss: the guide's first option). The rest of the common objects (results card, monitors, shield, invincibility stars, spindash dust) are noted in the ChangeLog as differing and not yet ported; Sonic's object and the HUD object need nothing.

* Chemical Plant, the pre-beta's (`tools/import_s2prebeta_zone.pl CPZ`, from its `Data/CPz`): its tiles (867, the alpha's 858: the art differs throughout), blocks, chunks, collision, layouts, objects and rings; the level size (act 2's right edge $2A80, was $2880) and the start (x $60 in both acts, was $30). Same as the alpha, checked: the palette, the water's palette, the three palette cycles, the animated background art, every object's art, the water level and its rise. Objects changed: the speed booster (it looks at how fast a character goes the way it faces: not at the size of his speed), the worm (two more splashes), the tube mouths (a Tails who is flying in is not taken), the tube spring (its weaker throw is $A80), the door (8 wide, and its strip a box: $20 above and below, not for a character an object holds), and the lever spring and the diagonal ones (they take a spin dash charge). Not yet: the Grabber (object $A7: the alpha's was an empty stub; the pre-beta's is the whole badnik with its parts $A8-$AA), the boss (object $5D, with its events: the arena at $2A20, the boss's art list $28 and music) and the Lander ($A5, $A6: the Spiny of the alpha, renamed: the same code).

## Known pre-beta bugs and what the port does

* **Water levels' bubbles load over the title card's art** (the user, 2026-10-31: a known bug of the real pre-beta ROM): the card's letters are at tile $5DE on, where the water zones' countdown bubbles are loaded ($BD00, tile $5E8), so the card is garbled in the real ROM. The port does not keep it (the user: no problem, the port is not held to the hardware's limits): the card's art goes in after the level's first pattern lists, and the zone's second list waits for the card to go (`Object/TitleCard.c`, `Level.c`). Putting the original order back is the two lines marked there if it is ever wanted behind `SCP_FIX_BUGS` (the port's rule for original bugs that do not crash is to keep them unless that is on).

## The SEGA screen, in stages (the user, 2026-10-31)

The pre-beta's SEGA screen is the final game's (its art, `Sega_Screen`, and objects $B0 and $B1: Sonic running across the logo at twice his size, the letters pushed aside line by line, two palette sweeps, the voice at the end). `GM_Sega.c` (Sonic 2's own, replacing Sonic 1's) takes it in stages, the objects loaded directly (no id table):

1. **DONE** The logo where it ends (art, map, palette: `res/S2Sega`), the trademark hidden by object $B1 (`Object/SegaScreen.c`; the Japanese console, `SCP_JP`, keeps it), the pre-beta's timing (about 80 frames of the animation, the voice, 180 frames of hold, Start leaves).
2. **DONE** Sonic running across the logo (object $B0: his art cut from `Sonic.dat`, tiles $2B6 to $301, doubled in RAM by `Sub_Sega_Intro`, at tile $88; frames 0-3 of `asm/Mappings/SegaSprites.asm`; it runs left, is out of sight for a sweep, runs right, then the voice).
3. **DONE** The blinds and the palette sweeps: plane A strips of the block graphic ($80-$87) on every second line (even lines right to left, then odd lines left to right, 35 lines from line 78 with the pre-beta's ragged offsets), and the two seven-step palette sweeps of $B0 (four frames a step). The screen's planes are 128 tiles wide while it runs (`SEGA_PLANE_WIDTH`), as the pre-beta's are; the others' 64 come back on leaving.
4. **DONE** The SEGA voice is the final game's (`res/PCM/SegaFinal`, 24948 bytes at 16500 Hz, from the s2disasm): the sound bank's optional `sega_pcm`, `sega_pcm_size` and `sega_pcm_hz` (the engine keeps Sonic 1's 16000 Hz clip when a game gives none).

