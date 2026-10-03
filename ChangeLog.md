# Changelog

Session notes capturing context, rationale, and outstanding work that isn't
already recorded in code comments or commit history — written so this
survives conversation summarization/compaction. Newest work first.

## Sonic 2 toward the Nick Arcade equivalent: Emerald Hill act 1 loads (2026-10-03)

**Status: EHZ1/2 load and Sonic stands on Nick Arcade's terrain (zone slot 3). Sonic 1 unchanged (42 scenarios + PCM identical). Not yet: EHZ background deformation/scrolling, objects, music, the rest of the zones.**

- **Goal: Emerald Hill with all its objects, then Nick Arcade's attract mode.** Ported so far, each Sonic 2 only (`Sonic the Hedgehog 2/src/Object/`), from the NA sources, with the assets imported by `tools/import_s2na_assets.pl` (art, mappings, animation scripts, and it lists them in the project file): bridge (`Bridge.c`: logs as child sprites of up to two objects, Sonic and Tails each give a log index; children are kept as slot indices), scenery (`Scenery.c`: bridge stakes etc.), basic platform (`Platform.c`: NA's object 18; both characters), EHZ waterfall (`EHZWaterfall.c`), Buzzer / Masher / Snail (`EHZBadniks.c`: parts find their parents by a checked slot index), corkscrew (`Spiral.c`: a character on it gets its height from a table and its flip angle set, which makes Sonic tumble along it) and the signpost (`Signpost.c`: NA's art/mappings/animation and sparkle places). `Solid.c` gained `Solid_PlatformLand`, `Solid_Platform` and `Solid_Ride`. The zone's art (bridge C6, waterfall AE, fireball E, Buzzer E6, Snail , Masher C, shield ) loads in Emerald Hill's PLC.
- **Attract mode is Nick Arcade's.** `src/Demo.c` (Sonic 2's copy) holds NA's recorded inputs (`res/Demo/NA*`, extracted from `Demo_CPZ/EHZ/HPZ/HTZ/S1GHZ/S1SS`) by zone slot, and the title screen cycles them in NA's order (`Demo_Levels`: Chemical Plant 1, Emerald Hill 1, Hidden Palace 1, Hill Top 1), 1800 frames each as before. NA's Emerald Hill demo is a two-player one (the second pad moves Tails); with no two-player mode yet only Sonic's pad is played. Checked: the Chemical Plant demo starts from the title.
- **Emerald Hill's boss and end of act.** `Object/EHZBoss.c`: Nick Arcade's objects 55 and 58 (with 57's code): the drill car (ship, cockpit, body, three wheels, spike, lift-off flame), hit counter, flash, defeat and drive-off, parts finding the ship by a checked slot index; its art (/C0/) in the boss PLC, which the zone's event now queues. When the boss is beaten Nick Arcade goes to the Sega screen, as here. `Level.c`: act 1 and 3 layouts etc. as NA's index (act 3 = act 1's layout with no objects or rings); the signpost-art loader skips act 2 as NA's does (Sonic 1's skipped act 3, and it cleared the art queue in the boss arena); `Object/TitleCard.c` (Sonic 2's copy: the end-of-act card) has NA's level order. Engine: `GameInfo.plc_capacity` (the art queue holds 16 in the original; Sonic 2 starts levels without a title card to drain it, so it asks for 32; Sonic 1 unchanged). Solid: the object-control flag (`lock_multi` bit 7) now switches the solid code off as NA does.
- **Engine: shared slide routine, per-object collision path, NA path swapper.** `Slide.c`/`Slide.h` (`Slide_Update(table, count)`, `SlideSurface {chunk, speed}`): Sonic 1's Labyrinth water slide code moved out of `LZWaterFeatures.c` into a table-driven routine (`f_slidemode` and the behaviour unchanged, baseline identical); Oil Ocean's slides will pass their own table. (Nick Arcade's `Sliding_flag`/`S1_LZWaterSlides` is just Sonic 1's own slide code left in its ROM with a broken table, never used there; the override flag and `Control_Locked` of NA are `lock_multi` and `lock_ctrl` here, which `Solid.c` now honours too.) `Game_CollisionPath(const void *obj)` (GameInterface.h, weak default = the global `collision_path`, so Sonic 1 is unchanged) now decides which collision path `FindFloor`/`FindWall` use per object; Sonic 2's `CollisionPath.c` gives Sonic and Tails each their own from their `top_solid_bit` ( = second path) and everything else the first. Sonic 2's `Object/PathSwapper.c` is Nick Arcade's object 03 (tracks Sonic and Tails separately, switches on leaving the strip, sets solid bits and sprite priority); the secondary collision indices were already imported. Smoke-tested running through Emerald Hill's first stretch; the swapper logic itself is not yet checked against a loop or corkscrew (those objects aren't ported).
- **Shield and stars (object 38).** Sonic 2's `ShieldInvincibility.c` + `res/Animation/ShieldInvincibility` (`Ani_Barrier`): the shield is 24 wide and uses tile $560 in Emerald Hill (the EHZ PLC loads it there), and the invincibility stars are Nick Arcade's unfinished ones: Sonic's own mapping frame with the stars' tiles, drawn at a trail position from a buffer that is never written (the filling routine `Unused_RecordPos` is never called), i.e. the level's top left corner, so nothing shows during invincibility but the music. **Checked against Nick Arcade and already identical to Sonic 1 (nothing to port):** the explosion (object 27: same art, mapping pieces, timing), animals (object 28: same code, zone table, speeds, PLCs and art), the ring and lost rings (objects 25/37, only the art tile differs: already overridden) and points (object 29).
- **Springs (Nick Arcade's object 41), with the prep they need.** Prep: `Scratch_Sonic` gained named `flip_angle`, `flips_remaining`, `flip_speed`, `top_solid_bit`, `lrb_solid_bit` (same size: they replace padding; Sonic 1 untouched). Sonic.c and Tails.c (Sonic 2 only) got the tumble (animation frames from flip angle: Sonic's from $9B, Tails' from $75; `JumpAngle` steps it; `ResetOnFloor` clears it). The solid bits are stored only: the port has no secondary collision path yet, so path swappers and spring subtype bits 2/3 do nothing visible. New `src/Solid.c` (`Solid_Character`): Nick Arcade's `SolidObject_Always_SingleCharacter` and `SlopedSolid_SingleCharacter` for either character (Sonic stands with status bit 3, pushes with 5; Tails 4 and 6), with `MvSonicOnPtfm`/`MvSonicOnSlope`/`RideObject_SetRide`; `Tails_ResetOnFloor` and `KillTails` are now public (`Object/Tails.h`). The spring: Sonic 2's `Object/Spring.c` (up, sideways, down, diagonal up/down, yellow, flips, x-speed cut, run-into check for sideways ones, Tails too), `asm/Mappings/Spring.asm`+`SpringYellow.asm` (generated from `Map_obj41`/`Map_obj41a`), `res/Mappings/SpringGHZ`, `res/Animation/Spring` (`Ani_obj41`), art `SpringUp`/`SpringSide`/`SpringDiag` at $45C/$470/$43C in every zone but Green Hill (which keeps Sonic 1's spring art at $4A8/$4B8: `ArtTile_Spring_Horizontal/Vertical` are now overridable in Constants.h). Tested in Emerald Hill: yellow up spring and a diagonal one launch Sonic correctly; sideways, down, Tails and the flip tumbles not yet exercised in play. NA's wall rebound is deliberately skipped (dropped next milestone). Sonic 1 hash/PCM baseline identical.
- Sonic 2 has its own `src/Main.c` (project-level `main:` in its yml): the ROM header title, and so the window titlebar, reads "SONIC THE HEDGEHOG 2". Sonic 1's Main.c is untouched.
- Title screen background: the two 32-cell parts fill the whole 64-cell plane, so with the widescreen margin `CopyTilemap` spilled each row into the next row's left cells (wrong hills, blue top-left patch). `GM_Title.c` now writes them with `CopyTilemapWrapped`. Confirmed fixed by the user.
- `Sonic the Hedgehog 2/tools/import_s2na_zone.pl ZONE ACT...` imports a zone from `~/Projects/s2na-disasm` into `res/` (layout FG+BG into the engine's interleaved blob, chunks, blocks, collision indices, objects
  to Sonic 2 entry format with ids remapped, rings, palette, cycle, Nemesis art). Needs `ParadoxEngine/tools/kosenc` (a CLI around the engine's `KosEnc`). Sonic 2 keeps Sonic 1's object ids (the enum is in S1's Object.h);
  Nick Arcade's own objects get ids from $8E (waterfall, Buzzer, Masher, Snail), still unwritten (null).
- Sonic 2 copies of the files holding per-zone data (`Sonic2LevelData.c`, `Sonic2PLC.c`, `Sonic2Palettes.c`, `PaletteCycle.c`) with slot 3 (Sonic 1's SLZ slot) replaced by Emerald Hill; `Sonic2Collision.c` = Nick Arcade's solids for every zone.
  `Level.c`: a header without art leaves it to its PLC (EHZ's Nemesis art loads at tile 0, as in Sonic 2).
- Title: "Sonic Team Presents" screen skipped; palette cycle is EHZ's; level select palette is the Simon Wai/Nick Arcade one. `GameInfo.no_title_card` (Sonic 2 true): the protos have no title cards.
- Title: Sonic and Tails no longer rise (the prototypes' Obj0E draws them in place; the rising code is unreachable there); the level select/debug font is the Simon Wai prototype's (`res/S2Art/Text`).
- **Shared-code fix (Sonic 1 too): foreground right-edge corruption.** `DrawBlocks_LR` drew 21 blocks a row at 320 wide where the original draws 22 (`(320+16+16)/16`), so the block at the right edge of the view stayed stale in every
  row drawn while the camera was off a block line (foreground only; mostly seen scrolling vertically). Test: `LevelPlane_ScrollingAtGameSpeedsLeavesNoHoles` (random level, random camera steps up to 16 px, scrolled plane vs a fresh draw).
  Frame hashes of the scrolling scenarios (boot, credits 2-7) changed accordingly; baseline refreshed, audio identical.
- **Levels in their slots** (`import_s2na_zone.pl` run for CPZ 1, HPZ 1, HTZ 1 2 3): Chemical Plant in the Marble slot (2), Emerald Hill in Spring-Yard-less slot 3 (Star Light's), Hidden Palace in the Spring Yard slot (4),
  Hill Top in the Scrap Brain slot (5; Nick Arcade's HTZ3 is where the Final Zone was). Blocks of Hill Top = Emerald Hill's with its own from block $130 (Nick Arcade's MainLevelLoadBlock); its chunks, collision and base art are Emerald Hill's, its
  own art loads from tile $1FC. Act 3s are copies of act 1 with no objects or rings (dropped in M2; GHZ is the exception, and goes in M2 itself); acts 2 of CPZ/HPZ have no objects (as in Nick Arcade).
  Dynamic level events stay Sonic 1's (Nick Arcade never changed them; they change in M2). Palette cycles of CPZ/HPZ/HTZ are not written yet (Sonic 1's must not run on those palettes: stubbed).
  Verified headless: all of CPZ1, HPZ1, HTZ1, HTZ2 render with their own art and palette (`--zone 2|4|5 --act n`; that start skips the title, so the HUD labels art, loaded by the title, is missing there).
- **Scrolling, background drawing, palette cycles** (Sonic 2 copies `LevelScroll.c`, `LevelDraw.c`, `PaletteCycle.c`): Nick Arcade's BgScrollSpeed per slot (CPZ quarter-height BG, HPZ half-height, EHZ/HTZ static start), and its Deform_CPZ / Deform_EHZ /
  Deform_HPZ (Deform_All bands) / Deform_HTZ written from `_inc/BgScrollSpeed & DeformBGLayer.asm` (the 16.16 gradients as plain accumulators; lines past the original's 222-224 repeat the last). Draw: `DrawBG_Top` gets Nick Arcade's bits 6/7 (Hidden Palace's
  background moves by them); the initial background draw is plain for slots 2-5 (the MZ/SBZ specials of Sonic 1 only run when flags that Nick Arcade's deformations never set are raised). Palette cycles: CPZ (3 cycles), HPZ (dry + underwater), HTZ (lava, with its delay table),
  EHZ's. The title has none (neither prototype's title loop calls a cycle; Nick Arcade's PalCycle_S1TitleScreen is unused), so `PCycle_Title` is empty. **Title**: `Deform_TitleScreen` (camera X +8 a frame to $1C00, foreground still, Emerald Hill's lines) runs each frame, so the title's background scrolls like Emerald Hill's.
  Seen headless: EHZ clouds and sea, CPZ buildings, HPZ cave, HTZ mountains/clouds; the title with its sea line. Not exercised yet: redraws while scrolling in HPZ (bits 6/7), CPZ/HPZ/HTZ cycles in motion.
  Note: the Sonic 1 frame-hash batch (`verify.sh`) is not deterministic under heavy CPU load (a different scenario or two diverge each time the machine is busy, e.g. while building); every flagged scenario matches the baseline when run alone.
- **Animated art and animated blocks** (`Sonic the Hedgehog 2/src/AnimatedArt.c`, from `_inc/Animated Stage Tiles.asm`): Nick Arcade's script-driven art animation (Dynamic_Normal: per script a timer, a frame list with a shared or per-frame duration, and the tiles copied over the
  level art at a fixed VRAM place): Emerald Hill's and Hill Top's five flower scripts, Hidden Palace's three glowing ball scripts; none for Green Hill (Nick Arcade removed Sonic 1's), Chemical Plant or the ending. LoadAnimatedBlocks (the zone's block patches written into the
  block table at level start, from `LoadTilesFromStart` before the tiles are drawn): Green Hill's set for Green Hill, Emerald Hill and Hill Top, then Chemical Plant's and Hidden Palace's. Art files: `res/S2Art/EHZFlower1-5`, `HPZGlowingBall`.
  Checked headless: Emerald Hill's flower tile in VRAM alternates between its frames with the script's durations. Not checked by eye: Hidden Palace's orb, the animated blocks' look.
  (Nick Arcade's `ShiftCPZBackground` is dummied out there and not ported.)
- **Sonic in Sonic 2** (art, mappings, animations and palette only; his code is Sonic 1's): Nick Arcade's art (`res/Art/Sonic`), mappings (`asm/Mappings/Sonic.asm`, assembled), DPLC (`res/Mappings/SonicDPLC`), animation scripts
  (`res/Animation/Sonic`, built from `_incObj/01 - Sonic.asm` by `tools/sonic_animation_s2na.pl`; animation 31, the port's spin dash, is Nick Arcade's spin dash script) and the "Sonic and Tails" palette. `Sonic_Animate` moved out of Sonic.c into
  `Object/SonicAnimate.c` (Sonic 1: a plain move, 42 scenarios + PCM identical); Sonic 2's copy has Nick Arcade's changes (frames go above $7F so a command is a byte >= $F0; walking and running share twelve-frame sets per angle and run at half the delay).
  ParadoxMake: a reuse `except:` entry `res/<name>` or `asm/<name>.asm` now also drops a reused resource or assembled file, so a game can give its own.
- **Object table follows Nick Arcade's ids** (`Sonic2Objects.c`): ids Nick Arcade removed are empty, ids where it has a different object than Sonic 1 are empty placeholders (Tails 02, Tails' tails 05, Spiral 06, ... EHZ boss 55-58) until ported, and ids where it kept the Sonic 1
  object keep the Sonic 1 code until each is replaced. Layout imports no longer remap ids. Sonic 1's own table is untouched.
- **Tails and his tails** (`Sonic the Hedgehog 2/src/Object/Tails.c`; objects 02 and 05 in Nick Arcade's ids): made as Sonic Team made him, by copying Sonic's object (Sonic.c) and editing it, so he has Sonic's quirks: his radii are 9x$F (ball 7x$E, five pixels up and down,
  one when he lands from a ball), control is pad 2 which a CPU stub fills with Sonic's own pad from 16 frames ago (a pad 2 in use keeps him for 300 frames), no water, no debug mode, no flight (his animation $E exists, unused), no dust. Hurting him drops SONIC's rings (the copy of the hurt code works
  on the shared ring count), but he does not die from enemies or with no rings, he just keeps getting hurt; he only dies by falling below the level, and comes back above Sonic (`Tails_GameOver`). His tails (object 05) copy his state and animate on their own table
  (the $FC command turns them with his velocity). Art, mappings, DPLC and animations are Nick Arcade's (`res/Art/Tails`, `res/Mappings/Tails`, `TailsDPLC`, `res/Animation/Tails`, `TailsTails`, built by `tools/tails_animation_s2na.pl`); art windows $7A0 and $7B0 written directly (no VBlank buffer).
  Spawned by the new weak hook `Game_LevelObjects()` (GameInterface.h; null in the engine; GM_Level calls it after the HUD): in every zone slot but Emerald Hill's, as Nick Arcade ("funny how they skipped Tails in EHZ for the Nick Arcade show": Tails loses Sonic's rings).
  Needed on the way: Sonic 1's splash/dust object (its dust art window is $7A0, Tails') is empty in Sonic 2 (Nick Arcade has none); `ArtTile_Ring` and `ArtTile_Lamppost` can be overridden (guarded in Constants.h) and are Nick Arcade's $6BC and $47C in Sonic 2.
  Checked headless: Tails stands beside Sonic in Chemical Plant and Hill Top (his tail drawn), none in Emerald Hill. NOT checked: his movement, spin dash, hurt, pit death/respawn (no input harness here).
- **Level art like Sonic 1** (decompressed at level start from the level header, not by the PLC): `ParadoxEngine/tools/nem2kos` turns Nick Arcade's Nemesis tilesets into the Kosinski level art (Hill Top's is Emerald Hill's with its own over it from tile $1FC); `import_s2na_zone.pl` runs it;
  the Sonic 2 level headers point at them and the zone PLC lists no longer hold the level art.
- **Sonic 1 restored, Sonic 2 has its own Sonic and Level code.** Sonic 1's `Sonic.c`, `Level.c` (and the rest of Sonic 1's Sonic) are exactly as before: the animation move into `SonicAnimate.c` and the Level.c art guard were taken back (regression: 42 scenarios + PCM identical). Sonic 2 has
  copies: `Object/Sonic.c` (with Nick Arcade's animation code) and `Level.c` (with Nick Arcade's dynamic level events). The only shared-code changes left are behaviour-free: the guarded `ArtTile_Ring`/`ArtTile_Lamppost` in Constants.h, the weak `Game_LevelObjects` hook, ParadoxMake's resource `except:`.
- **Dynamic level events, Nick Arcade's** (`Sonic the Hedgehog 2/src/Level.c`; from `DynResize_*` in s2.asm): Green Hill keeps Sonic 1's; Chemical Plant acts 1-2, Emerald Hill act 1 and Hidden Palace act 1 have none (Sonic 1's Marble/Spring Yard events are only
  "leftover from Sonic 1" there). **Fixes a bug in the previous build: Chemical Plant ran Marble's events, which dropped the camera's bottom limit to $1D0 so the screen scrolled by itself and killed Sonic.** Emerald Hill act 2 (boss arena: camera lock at $26E0, music), Hidden Palace act 2 (camera
  bottom by x and Sonic's y), Hill Top acts 1 and 2 (camera bottom by x, the arena objects $83/$82/lock) are ported; the bosses themselves are not (their objects are empty, so the arenas just lock the camera). Acts 3 (copies of act 1, dropped in M2) have no events.
- **Fixed: spin dash sound and the camera zipping ahead of Sonic while walking (Sonic 2 only).** Nick Arcade's Sonic has frames of up to 32 tiles (the run frames; that is why Tails' art window starts $20 tiles after Sonic's), but Sonic 1's `sgfx_buffer` holds 23 (`SONIC_DPLC_SIZE`
  $2E0): loading a big frame overran it into the neighbouring spin dash variables, which made Sonic "release" a spin dash he never started (sound, and the camera delay `cam_x_delay` set to $2000, so the camera froze and then sprinted) as soon as the camera started moving. `SONIC_DPLC_SIZE` can be overridden
  (guarded in Sonic.h) and is $400 in Sonic 2. Found with a scripted run (gdb injecting pad input at `Obj_Sonic`, a watchpoint on the camera delay, and a log of camera/Sonic/Tails positions). Tails now also has private camera variables (his copy of Sonic's code writes the shared camera delay and
  look shift, as Nick Arcade's does: with them Tails' own spin dash release 16 frames after Sonic's set the delay again). Also fixed on the way: generated resource headers did not always trigger recompiles (a clean rebuild of Sonic 2's objects is done whenever resources change).
- **Common objects ported so far (art, mappings, VRAM positions, code where Nick Arcade differs):** HUD (art, lives counter, digits; mapping with the swapped palettes: Nick Arcade's normal text is palette line 1 and flashing is line 0; no time over), rings, points, monitors (Sonic 2's `Monitor.c`:
  items renumbered, animation table from `Ani_obj26`), spikes (`Spikes.c`: upright only, palette 1), lamp post (`Checkpoint.c`: no twirling ball, flickers when hit), shield and invincibility art, Tails. VRAM: lamp post $47C, rings $6BC, points $4AC, shield $4BE, stars $4DE, spikes $434 (overridable constants in Constants.h).
  Not yet: Nick Arcade's springs, explosion, animals, lost rings, shield object code.
- **AddressSanitizer sweep** (`paradoxmake build -B <dir> -DSANITIZE=ON`, then every zone/act under gdb-injected walking, a spin dash script, the title, special stage and ending): found a second overflow in Sonic 2, the level art scratch (`level_art_scratch` 0x7000, but Emerald Hill's and Hill Top's tilesets
  are 912 tiles = 0x7200): 0x8000 in Sonic 2's own Level.c. Sonic 2 is clean after that; Sonic 1 is clean (18 zone/act runs + special stage + ending + title). (The Qt Multimedia SEGV at exit that every run of the hash hook ends with is the test harness, not the game.)
- Frame dump hook (`SONIC_DUMP`) had its channels shifted (blue always 255); fixed.
- Next: EHZ deformation + BG scrolling (level and title), EHZ objects, Tails, then CPZ/HPZ/HTZ. Later: Sega screen skipped after this milestone; replacement title cards in Gaslight font.

## Sonic 2 starts out of Sonic 1: ParadoxMake `reuse:` (2026-10-03)

**Status: Sonic2 builds and runs (own settings file); Sonic 1 unchanged (42 scenarios + PCM identical).**

- `paradoxmakefile.yml` gained `reuse: [{project, except: [...], main: true}]`: a game starts from another project's sources, include dirs, resources, assembled files and songs, minus the files it replaces; the reused project is only read,
  not built. Resources/songs are converted into the reusing project's own `src/Resource`. `songs:` (+ `song_dir:`) moved out of the Sonic 1 fragment into the yml (`pm_add_song` lives in the engine fragment).
- Build policy moved out of Sonic 1's fragment to where it belongs: build types/strip/sanitizer into ParadoxMake's prelude, the engine's warnings and endianness into the engine fragment.
- `Sonic the Hedgehog 2/`: reuses Sonic 1 except `Sonic1Assets.c` and `Sonic1Screens.c`; `Sonic2Assets.c` (identity: app id sonic2pc, own settings file; font and icon still Sonic 1's), `Sonic2Screens.c` (Sonic 1's screens for now),
  its own `paradoxmake.cmake`. Zone slots: GHZ, (empty), CPZ, EHZ, HPZ, HTZ (slot 3 plays SLZ music, as in Nick Arcade). Next: title with Sonic 2's emblem over an EHZ background, Sonic 2 solids, objects replaced one by one.
- Sonic 2 title (`Sonic the Hedgehog 2/src/GM_Title.c`, Sonic 1's with two changes): the level select cheat is on from the start in every build (hold A, press Start), and a plain Start begins in the
  Emerald Hill slot (zone slot 3), which holds Sonic 1's slot-3 level until EHZ exists, as in Nick Arcade. The emblem and background are still Sonic 1's.
- Level data split out of Sonic 1's `Level.c` into `Sonic1LevelData.c` (layouts, sizes, start positions, scroll block sizes, headers, collision indices, object and ring layouts; declared in `Level.h`), so a game with other levels replaces just that file. Output unchanged (42 scenarios + PCM identical).
- Sonic 2's title (`Sonic the Hedgehog 2/src/GM_Title.c`, Sonic 1's code) draws the Sonic 2 Simon Wai prototype's title (the same one Nick Arcade has; from AlexField442's disassembly, cloned to `~/Projects/s2sw-disasm`):
  wings/background art (Nemesis) at tile 0, the wings and emblem on plane A, the background in two 32-cell Enigma maps on plane B, the prototype's palette (`res/S2Title/`). No camera or scrolling (it is a picture, not a level);
  Sonic 1's "press start" objects remain; Sonic and Tails art (`S2Title/SonicTails`) is in the resources for when their objects come.

## Rings stage: ring layouts, RingsManager, SonLVL on the S2 format (2026-10-03)

**Status: done. 40 of the 42 frame-hash scenarios and every sound's PCM are identical; credits demos 4 and 5 drift (below) and their baseline was refreshed.**

- **Ring layouts** (`res/RingLayout/<level>`, Sonic 2's format): 4-byte entries sorted by X, ended by X = $FFFF: `u16 x, u16 y (bits 0-11) | rings-1 (12-14) | vertical (15)`, a straight line of up to 8 rings $18 apart.
  `tools/splitrings_s1.pl` (run once, kept like objlayout_s1_to_s2.pl) took the ring objects (id $25) out of the 20 level object layouts: Sonic 1's straight $18 lines became one entry each, every other
  formation (other spacings, diagonals, arcs) single rings, so every ring is where it was. The object layouts no longer carry rings (which also frees their respawn indexes).
- **RingsManager** (`ParadoxEngine/src/RingsManager.{h,c}`): the objects manager's twin on ring entries -- the same load window and side pointers, a status byte per entry (bit n: ring n collected; kept until the level restarts) in
  place of respawn marks, and a spawn callback instead of an object slot. Plain data (assignable; two cameras = two managers). Tests: `test_ringsmanager.c`.
- **Sonic 1**: `Object/Ring.c` provides the callback: a ring is still an object (drawn and touched like before, priority 2, `col_type` $47), made when its entry comes into range; collecting it sets its status bit.
  The old group path (`Obj_Ring` routine 0) stays for the debug placement, which still places a ring formation by subtype. `ObjPosLoad` runs `Rings_Init/Rings_Update` after the objects manager.
- **Why two demos drift**: rings used to claim object slots at different moments (a group made its rings while the object loop ran), so other objects landed in different slots. Slot order is behaviour in these games (when two solid objects
  touch Sonic the later one's flags win), and in the SBZ demos of credits pages 4 and 5 a pushed-against block flips Sonic's `pushing` flag one frame differently (from frame 622 / 1430): a 1-pixel difference, same outcome on screen.
  Rings that used to be dropped when the object slots were full no longer are.
- **SonLVL** (`SonLVL INI Files/SonLVL.ini`): `version=S2`, and every level has `rings=../res/RingLayout/<level>` beside its `objects=`. Untested here (no SonLVL on this machine); the S1 ring object definition stays for the sparkle.

## Housekeeping and ObjectsManager_2P (2026-10-03)

**Status: done. Frame hashes (42 scenarios) and PCM unchanged.**

- **ParadoxMake finds its helpers when installed**: `$PARADOXMAKE_DATA`, else `<prefix>/share/paradoxmake/cmake` beside the tool, else the source tree it was built from. `ParadoxMake/CMakeLists.txt` installs both.
- **Window identity is the game's** (`GameInfo`, GameInterface.h): `app_id`, `app_name`, `game_title`, `player_name`, `description`, `trademark`, `data_dir`, `url`, `bug_url`, `authors`, `credits`.
  The About box, handbook window/paths, menu entries, controls dialog, demo tool, settings folder and the icon theme name read them; the icon search now finds `<project>/packaging/icons` beside `bin/`.
- **Null services** (`ParadoxEngine/src/NullServices.c`): the debug services (Peek_*, Console_*, Demo_*, cli_*, frame_count) have weak null versions, so a game without them still links; a game's own definitions win.
  The game's data/identity tables stay required.
- **Sonic 2 skeleton** (`Sonic the Hedgehog 2/src`): `game_info`, a null-screen table, an object table; builds as `Sonic2Core`. No executable yet (that comes with its sound bank, palettes, PLCs, collision maps, Art_Text, icon).
- **ObjectsManager2P** (`ObjectsManager.{h,c}`): two `ObjectsManager` views (one per camera) over one layout and one set of marks. A view skips the entries its partner holds (between its side pointers, minus
  the entries the level start skipped behind its window), so an object both cameras have in range loads once -- also the ones that do not remember their state, which the marks alone would not cover. The partner pointers
  are set again on every Update, so an `ObjectsManager2P` stays assignable data. One-player use is untouched (partner NULL). Keeping an object alive while it is near *either* camera is the object's business
  (MarkObjGone against both views): not done yet, it comes with the first two-player object code.

## Four folders and ParadoxMake (2026-10-03)

**Status: done and verified (SonicTests pass; 42 frame-hash scenarios and every sound's PCM identical to before the move).**

- Layout: `ParadoxEngine/` (src, fmcore, libparadoxsmps, contrib, tools, res), `Sonic The Hedgehog/` (src, res, asm, tests, doc, packaging, SonLVL INI Files),
  `Sonic the Hedgehog 2/` (skeleton yml + the manual PDF), `ParadoxMake/` (the build tool). Each game/engine folder has a `paradoxmakefile.yml`.
- **ParadoxMake** is to Paradox Engine what qmake is to Qt: it manages and builds a Paradox project. `paradoxmake [generate|configure|build|install|test|clean|info] [-B dir]
  [-DNAME=VALUE] [--prefix dir] [folder]`. It reads the folder's yml, finds the projects it `depends:` on (sibling folders), and generates a CMake tree
  (`<build>/paradoxmake/CMakeLists.txt`) which it then configures and builds -- CMake keeps doing Qt moc / SDL2 / install / ctest for now. C++17 + yaml-cpp, built by the root
  `CMakeLists.txt` (which builds nothing else by hand: a custom target runs `paradoxmake build` into `<build>/projects`; `cmake --install` runs `paradoxmake install`).
- The yml says: project/version/license/type, `depends`, `include`, `sources`, `resources` (bin2h -> headers), `assemble` (clownassembler), `main`/`executable`/`core_library`
  (the whole-archive `link_game()` rule is generated). What is not declarative yet stays in each project's `paradoxmake.cmake` fragment (engine backends + SMPS tools; the game's
  compiler flags, handbook, song catalog, tests and tools). `ParadoxMake/cmake/{options,prelude}.cmake` are the shared policy (options, bin2h/clownassembler tools).
- Output paths are unchanged (`bin/`). Root-level options `-DSPLASH=ON -DBUILD_TESTS=OFF ...` are forwarded.

## Phase 2 of the split: screens, objects, identity (2026-10-03)

**Status: started. Frame hashes (42 scenarios) and PCM (every sound id) unchanged after each step.**

- **Screens** (`engine/Screen.{h,c}`): the engine owns `gamemode` and the screen loop (`Screens_Run`); the game lists its screens in `game_screens[]` (`src/Sonic1Screens.c`) with
  `game_boot_screen`. A screen the game does not have (NULL, `Screen_Null`, or past the table) is the null screen: the video is reset and the engine goes back to the boot screen -- so any
  screen can be left out or stubbed. `Game.c` keeps only Sonic 1's start-up (`EntryPoint`) and calls `Screens_Run()`.
- **Objects** (`engine/EngineObject.h`, `engine/ObjectCore.c`): the object slots (`objects[]`, `player`, `level_objects`, `objstate`), the `Object` type, slot allocation, `ExecuteObjects`, sprite
  building (`BuildSprites`, `AnimateSprite`, `DisplaySprite`), `ObjectDelete`, `SpeedToPos`, `ObjectFall`, `RememberState` and `Obj_Null` moved to the engine. The game supplies
  `game_objects[]` (`src/Sonic1Objects.c`); an id it does not list, or a NULL entry, runs as the null object. Sonic 1's `ObjId_*` ids and its object subroutines (bosses, platforms, solid
  objects, act results) stay in `src/Object.{h,c}`. The camera positions the sprite code reads live in `engine/Camera.{h,c}`.
- **Identity**: the settings file name and its heading come from the game (`game_info`, `GameInterface.h`), not the engine.
- **Terrain collision** (`engine/LevelData.{h,c}`, `engine/LevelCollision.{h,c}`): the layout, chunk, block and collision-index arrays, `collision_path`, the `META_*` flags and the collision code
  (FindFloor, FindWall, ...) are the engine's; the collision maps (`Collision_Angle/HeightMap/WidthMap`) are game data (`src/Sonic1Collision.c`). Unchanged output.
- **Objects manager, Sonic 2's** (`engine/ObjectsManager.{h,c}`): a port of S2's `ObjectsManager` (1-player) and `ChkLoadObj`, as plain data: an `ObjectsManager` struct (a game may assign, copy or reset it; a second one can follow a second
  camera) plus an `ObjectsManagerConfig` POD (`ahead`, `behind`: the load window, 0x280 / 0x80 for both Sonic 1 and 2 -- they turned out identical). The marks of remembered objects live in an array the game
  provides (Sonic 1: `objstate`). The plan (user, 2026-10-03): ONE object format for every game on the engine -- Sonic 2's layout, sprite mappings and drawing.
- **Sonic 1 runs on it now**: `Level.c`'s own loader (`ObjPosLoad`'s state machine, `ChkLoadObj`, the `opl_*`/`objstate_left/right` variables) is gone; `ObjPosLoad()` initialises/updates `objects_manager` and keeps Sonic 1's own
  resets (`obj63_loaded`, `f_lz1tunnel_open`). The 20 object layouts in `res/ObjectLayout` were converted to Sonic 2 format by `tools/objlayout_s1_to_s2.pl` (flip bits 14/15 -> 13/14, the remember flag from the id byte to
  bit 15 of the Y word; the `*PF*` conveyor platform lists are another format and were left alone). Tests: `test_objectsmanager.c`. Frame hashes (all demos: objects must appear on the same frames) unchanged.
  Note for SonLVL: `SonLVL INI Files` still says `version=S1`, whose object layout is the old format; the layouts are Sonic 2 format now.
- **PlaySoundLocal** (`engine/EngineObject.h`, Sonic 2's): an object only makes its sound effect if it is on screen; needed once there are two views (split screen). Test: `PlaySoundLocal_OnlyOnScreen`.
- **Sonic 2's drawing, in stages.** Stage 1 (done): the sprite pipeline (`DisplaySprite`'s priority queues, `BuildSpr_Normal`, the flip variants, `BuildSprites`) moved to `engine/Sprites.{h,c}`, and the camera layers it places
  sprites against are an assignable POD (`SpriteView sprite_view`, indexed by the render flags) instead of hard-coded pointers: Sonic 1's three layers by default, and a second view (split screen) is a second `SpriteView`.
  The three scroll layers stay because Sonic 1 uses the third (Sonic 2 has it, unused); Sonic 3 keeps scrolling as PODs instead of code-based layers and will replace them later -- the Sonic 2 drawing and physics are ported to the common core first (user, 2026-10-03). Sonic 2's own `BuildSprites` places both level render flags against the foreground camera. Unchanged output. Planned: 2) Sonic 2's remaining draw features (multi-sprite objects with child sprites, the render flag names, the Y wrap of the cull check -- the original
  has it, this port's does not, and "identical" wins until decided); 3) the split screen (`BuildSprites_2P`: two views, the 2P tile words, the VDP side); 4) the rings manager and ring drawing, with the rings split from the
  object layouts (and Sonic 1's ring objects converted to it).
- **Drawing stage 2 (done)**: the render flags carry Sonic 2's names (`level_fg`, `level_bg`, `explicit_height`, `static_mappings`, `multi_sprite`, `on_screen`); multi-sprite objects (`Object::children`, `child_count`: the main sprite's frame/width/height are the object's own `frame`, `width_pixels`, `y_rad`) draw as in S2's `BuildSprites_MultiDraw`; `SpriteView::wrap_y` switches the 11-bit Y wrap of the on-screen check (off for Sonic 1, on for Sonic 2). Tests: `test_sprites.c`. Unchanged output.
- **Drawing stage 3 (done): the split screen's sprites** (`engine/Sprites.c`): `sprite_split_screen` makes `BuildSprites` build two tables as Sonic 2's `BuildSprites_2P` does -- player 1's in `sprite_buffer` (led by the two
  masking sprites, link starting at 3) against `sprite_view` / the first camera, player 2's in the new `sprite_buffer_p2` against `sprite_view_p2` / `scrpos_x_p2, scrpos_y_p2` -- from the same queues. The pieces' 2-player tile
  words and half-height sizes (`SpriteSizes_2P`) are used, the views sit one under the other in double-height coordinates (tops 0x100 and 0x100 + the screen's height), `on_screen` means "in either view" (so `PlaySoundLocal`
  hears an object either player can see), and `Object_Adjust2PArtPointer` halves an object's base tile. The piece drawing is one function now (flips and 2P as parameters) instead of four copies. Tests: `test_sprites.c`.
  Unchanged 1-player output (frame hashes). Not there yet: the VDP's double-height (interlace 2) display and the per-half scroll/palette handling, the HUD and rings builders per player (stage 4 / the HUD), the 2-player
  objects manager (`ObjectsManager_2P`: it is a second `ObjectsManager` on the second camera), and Sonic 2's `Teleport_flag` quirk in the queue clearing.
- **The split screen's VDP** (`engine/Backend/VDP.{h,c}`, `SDL2/Render.c`): `VDP_SetSplitScreen(mode, VDPView *second)`. The first view is the VDP as set up; the second is a POD the game keeps (plane locations, scroll
  table, vscroll, sprite table, optional palette), read as the frame is drawn. `VDP_SPLIT_STACKED`: Sonic 2's 2-player look -- the double-height (interlace 2) display: a picture of twice the rows, 8x16 cells (a name table
  entry or sprite tile number names a pair of patterns at twice its number), sprite Y doubled (screen top at 256, the second view a screen lower), each view with its own planes/scroll/sprites/palette; the renderer's texture
  follows `VDP_OutputRows()` and is squeezed into the usual height. `VDP_SPLIT_SIDE`: for wide pictures (better use of the screen than stacking), each half the picture's width with normal 8x8 cells. One-view drawing is
  untouched (frame hashes). `VDP_DrawFrame`/`VDP_GetFrame` draw without presenting, for tests (`test_vdpsplit.c`). `sprite_teleport_flag` reserves Sonic 2's Teleport_flag (the split screen's queues are kept while it is set).
  Still to do for a playable split screen: the sprite pipeline's side-by-side form (half-width views, tops at 0x80, ordinary tile words), per-view widths in the cull checks, the second view's level drawing, the per-player
  HUD (laid out as the Sonic 2 manual's: score/time/rings top-left and a lives icon bottom-left in each half, an item box top-right of the top half), the 2-player objects manager and the rings.
- **Drawing stage 3b (done): the side-by-side sprite form** (`engine/Sprites.{h,c}`): `sprite_split_screen` is a `SpriteSplit` now (`NONE`, `STACKED` = Sonic 2's, `SIDE`). Side by side builds the same two tables
  with ordinary tile words and sizes, both views' tops at 128, no masking sprites, and the cull checks use each view's own width (the first half the picture's width rounded down, the second the rest; they match the VDP's
  `VDP_SPLIT_SIDE`); `Object_Adjust2PArtPointer` only halves tiles for the stacked layout. Tests in `test_sprites.c`.
  Order from here (user, 2026-10-03): the second view's level drawing; the per-player HUD is game-specific (art and mappings with the game); the 2-player objects manager when ready; then the next stage (rings).
- **The second view's level drawing** (`engine/LevelDrawCore.{h,c}`, `engine/LevelPlane.{h,c}`): the block drawing (`CalcVRAMPos`, `GetBlockData`, `DrawBlock`, `DrawBlocks_LR/TB`, `DrawChunks`) moved from `src/LevelDraw.c`
  to the engine, and `LevelPlane` is the plain-data foreground plane that follows a camera: its VRAM address, its camera, the 16-pixel line state and the redraw flags (`CameraMovedX/Y` flag the column/row a camera step
  needs, `Snapshot` copies camera and flags at the blank, `DrawPending` draws them, `DrawAll` draws the whole view). Sonic 1's own foreground now runs on it (`fg_plane`, replacing `fg_scroll_flags`, `_dup`, `fg_xblock`,
  `fg_yblock`): identical frames, which is the proof it works; a second `LevelPlane` on `VRAM_FG_P2` and the second camera (`scrpos_x_p2`) is the second view's foreground. Tests: `test_levelplane.c`. What the second
  view still needs from the game: its camera's follow-up (Sonic 2's ScrollHoriz/Vertical for player 2), its horizontal scroll table (the `VDPView`'s), and the HUD (game-specific art and mappings).
- **The second view's horizontal scroll** (`engine/Video.{h,c}`): `hscroll_buffer_p2` (a foreground and a background X per line, like `hscroll_buffer`), `Video_UploadHScrollP2` (writes it to `VRAM_HSCROLL_P2`, 0xF800), and a ready-made
  `video_second_view` (`VDPView`: plane A at `VRAM_FG_P2`, plane B shared, that scroll table, `sprite_buffer_p2`, the first view's palette); `level_plane_p2` is the second view's `LevelPlane` (idle until a split level
  sets it up). Test: `VDPSplit_SecondViewHasItsOwnHScrollTable`.
- **Delayed until the actual split (Sonic 2)** (user, 2026-10-03): the second camera's follow-up (player 2's scrolling) -- only placeholder variables exist (`scrshift_x_p2`, `scrshift_y_p2`, `look_shift_p2`,
  `cam_x_delay_p2`, `cam_y_delay_p2`; nothing reads them) -- and the per-player HUD (Sonic 1 uses its own HUD art and its second screen is unused; the manual-based layout begins with Sonic 2, art and mappings in that game).
  Next priority after that 2-player work: the 2-player objects manager, then the rings stage.
- **Order of the engine's games** (user, 2026-10-03): Sonic 1 -> Sonic 2 -> Sonic 3 -> Sonic & Knuckles -> Sonic CD's time travel handler -> Chaotix stuff.
- **Next**: drawing stages 3 (split screen) and 4 (rings), then the physics
  (`BuildSprites`) with the rings split from the object layouts (a rings manager) and the split-screen (2P) code; then the solid-object/platform routines and slope physics out of `Object/Sonic.c`.

## SonicCore split: ParadoxEngine + Sonic1Core (2026-10-02)

**Status: stage one done. `libParadoxEngine.a` (engine/) is what every Sonic game shares; `libSonic1Core.a` (src/) is Sonic 1 on top of it. Play and
graphics are unchanged: 42 headless scenarios (SEGA/title/attract boot, all 8 credits demos, every zone and act, special stages, ending, continue, every
resolution) give the same hash for every rendered frame before and after, and every sound id (byte and JSON engine) renders the same PCM.**

- **What moved to `engine/`**: the Mega Drive model (`Backend/`: VDP, YM2612/SN76489, joypad, SDL2 renderer/input, the whole Qt window and its tools), `fmcore/`,
  the codecs (Kosinski, Nemesis, Enigma), `MathUtil`, `Types.h`, `Video` (resolution, planes, water-split state), `PLC`/`Palette` (the loaders), `Sound` (the SMPS driver,
  JSON engine and the shared DAC drum bank), `DebugLog`. The DAC/PCM/PSG resources are generated into `engine/Resource/` (git-ignored like `src/Resource/`).
- **Mixed files were split, not moved whole**: `Palette`, `PLC`, `Sound` keep the mechanism in the engine and the Sonic 1 data in `src/Sonic1Palettes.c`, `Sonic1PLC.c`,
  `Sonic1Sound.c`. `Constants.h` and `Macros.h` likewise: `EngineConstants.h`/`EngineMacros.h` in the engine, the Sonic 1 art-tile map and `IS_OFFSCREEN`/`RES_REV` in `src/`.
  The Sonic 1 headers `Palette.h`, `PLC.h`, `Sound.h` still exist and include the engine halves, so game code did not change.
- **The engine never includes a Sonic 1 header** (its include path has no `src/`): the compiler enforces it. What it needs from a game is `engine/GameInterface.h`'s
  contract, resolved at link time: `palette_pointers`, `plcs`, `game_sound_bank` (a `SoundBank`: which ids are music/effects/commands, priorities, the 1-up jingle, the
  ring speaker pair...), `Art_Text`, `res_Icon` (in `src/Sonic1Assets.c`), `frame_count`, `cli_*`, and the `Peek_*`/`Console_*`/`Demo_*` services behind the Qt debug tools.
  Because the engine calls back into the game, `libParadoxEngine.so` is built with those symbols undefined and the executable resolves them (`link_game()` in
  CMakeLists links the game whole, after the engine).
- **Shared / static (same day)**: `ParadoxEngine` and `paradoxsmps` are shared libraries (`bin/<config>/libParadoxEngine.so`, `libparadoxsmps.so`, next to the
  executable via `$ORIGIN`; installed to `<prefix>/lib`) so each can carry its own licence -- the plan is MIT for the two Paradox libraries, whatever suits the rest.
  `Sonic1Core` stays a static library inside the executable. Re-verified identical (frame hashes and PCM) in the all-shared and the mixed form. ELF/Linux only.
- **FM algorithms 8-F are overridable defaults** (`fmcore/fm_voice.h`): 0-7 stay the chip's fixed routings; 8-15 ship as defaults (`FM_VOICE_ALGORITHM_DEFAULT`) that a game
  replaces with `FMVoice_SetCustomAlgorithm` -- the driver applies the game's `SoundBank::fm_algorithms` at `Sound_Init`. The chip, the editors and the carrier test (which
  operators the channel volume attenuates) all read the routing in force; the hard-coded `FM_SLOT_MASK` tables are gone (derived from the routing instead; identical for
  every shipped algorithm, and ParadoxComposer's copy had algorithms 9 and 12 swapped). Tests: `test_fmalgorithm.c`. Sound output unchanged.
- **REV00 is gone** (REV01 is the bug-fix build; the REV00 branches did not even compile): all `SCP_REV00`/`SCP_REV01` conditionals resolved to REV01, the `-DREV01`
  option, the `*REV00` resources and `asm/Mappings/RingREV00.asm` removed. Output unchanged.
- **Still Sonic 1 in the engine (next stages)**: the Qt window's identity strings (`Sonic1Settings.cfg`, About text, the handbook window); the object, level and game loop
  frameworks (`Object.c`, `Level*.c`, `Game.c`) are still in `src/` because they share one RAM map with Sonic 1's own state -- splitting them is the next job.
- **Before calling the engine MIT**: the codecs (Kosinski/Nemesis/Enigma) are deliberately the project's own C code (the decoders given by Clownacy, the Kosinski encoder written here) and NOT the KENS code, to keep
  the codebase clean of licence contamination: never pull KENS/KensSharp-derived code in. Still check where the VDP/Mega Drive layer comes from (the C port this grew from) and that `engine/Resource` (the DAC drums, the SEGA! clip, PSG envelopes: Sega-derived audio) is allowed in an MIT library or should move to the game libraries.

## The game is complete: special stages, ending, credits, continue, resolutions, controls, handbook, install (2026-10-01/02)

**Status: Sonic 1 is playable from the SEGA screen to the credits. Builds clean; `SonicTests` pass. Looked at on a real display: fullscreen,
window resizing, the Help window and the Credits music sync (all confirmed by the user). The rest (the controls dialog, the continue screen, the special stage debug mode, the install) was checked by headless
screenshots, a live key test on a real display, event traces and tests, but not yet played through by hand.**

- **Special stages** (`SpecialStage.c`, `Object/SpecialSonic.c`, `SpecialStageBG.c`, `GM_Special.c`): Sonic, every block type, the
  results screen, the background, the attract demos and a special stage debug mode with the REV00 ending debug list (`DebugList_EndingSS`).
  The original's out-of-bounds garbage reads (the zone number, "W" and 1UP blocks that turned out to be real) crash in C instead;
  that is the safer behaviour and was accepted on purpose.
- **Ending and credits** (`GM_Ending.c`, `Object/Ending.c`, `Object/EndingEggmobile.c`): the ending (good with six emeralds, bad
  without), the credits with the attract demos between pages, and the TRY AGAIN / END screens. The wrecked Eggmobile in the
  background (the cut "Unused - Eggman Ending" art the ending PLC still loads) is coded from scratch, as an optional bonus for either
  ending. CLI test hooks: `--ending N [--ship]`, `--credits N [--emeralds N]`.
- **Continue screen** (`GM_Continue.c`, `Object/ContinueScreen.c`, objects 80/81): the countdown, the mini Sonics, Sonic falling in and
  running off; Start restarts the act with 3 lives, the countdown running out goes back to the SEGA screen. Hook: `--continue N`.
- **Easter Eggs menu** (`GM_Title.c`): the sound test's 9E/9F cheat now opens a menu (credits, good/bad ending, wrecked ship on/off)
  instead of launching things; the Japanese credits unlock on the "Sonic Team presents" screen.
- **Resolutions** (`Video.{c,h}`, `Backend/VDP.*`, `Render.c`, `QtHost.cpp`): File > Resolution: original 320x224, scaled 16:9 (400x224),
  8:5 (360x224), 5:4 (320x256), 4:3 (320x240); fullscreen only changes the aspect, windowed resizes the window. The screen size is a runtime
  value now (`screen_width`/`screen_height`, `plane_height`: 64 rows when taller than 224); VRAM grew to 128 KB and the special stage
  planes moved above 64 KB. A widescreen lives counter sits in the upper right (HUD frames 4-8 in `asm/Mappings/HUD.asm`). Remembered
  in the settings file; `--resolution N` overrides it for a run.
- **Controls** (`Backend/Controls.h`, `Input.c`, `Qt/ControlsDialog.*`): Settings > Configure Sonic the Hedgehog; two keys per Mega Drive
  button plus a gamepad button for A/B/C/Start, saved under `controls:` in the settings file. The D-pad and left stick always move.
- **Handbook** (`doc/`): KDE DocBook (`index.docbook`, validated with `checkXML6`, builds with `meinproc6`), 60 screenshots, the badnik
  table with the Japanese names, the special stage blocks, the continues; `doc/handbook-html.xsl` + `xsltproc` build the HTML shown by the
  game's own Help window (`Qt/HelpWindow.*`) when KDE Help Center is not installed. F1 opens it.
- **KDE integration**: the standard KDE About dialog (optional KF6CoreAddons/XmlGui), `packaging/sonic1pc.desktop` and the icons in
  `packaging/icons/hicolor` (16/22 px: the 1UP icon; the rest: the title emblem with its TM mark).
- **Install**: `install(TARGETS Sonic ...)`; configure with `-DCMAKE_INSTALL_PREFIX=/usr` and `sudo cmake --install` puts the game in
  `/usr/bin` with the desktop entry, icons and both handbooks. Build as the normal user first so `sudo` only copies.
- **Sound**: nearly every sound effect fixed (the user confirmed all), the Credits song's sync (tick order DAC, FM1-6, PSG, tempo
  handling) confirmed by ear, the spin dash rev (`PrepareSpindashRev` etc., driver rules from the Sonic 2 driver), and the SMPS Inspector
  merged into the Tools menu with the full ParadoxSMPS drum kit; the game is paused while it is open (`Console_SetToolPause`), so the driver
  does not compete with itself. See the sound sections below for the engine.
- **Fixes**: the level timer no longer runs while paused (the original skips it); TIME flashes at 9:00 whether or not you have rings
  (the original's quirk, deliberately fixed in every build); GAME OVER plays its music; TIME OVER clears the stored lamppost time (REV01);
  **pressing Return (or any key that types a character) crashed the game inside SDL** because the Qt backend pushed synthetic SDL text
  events: those are gone, F11 is handled directly. The Qt key events are therefore no longer pushed into SDL's queue (see the Qt window
  section's note below).

Design notes kept elsewhere: the plan to pitch SEGA a Plasma-native Lite pack (Sonic 1 + 2, open) and Full pack (commercial) is in the
author's notes; if SEGA refuses, a code-only recomp that extracts assets from a ROM is the fallback (the icons and the screenshots are then
assets too).

## The Qt window is now THE game executable (plain-SDL2 window build dropped)

**Status: builds, all 7 ctest suites pass (the smoke runs boot the Qt game with `QT_QPA_PLATFORM=offscreen`); frames and viewers verified headless, not yet looked at on a real display.**

There is no `BACKEND` option any more: `bin/<config>/Sonic` is the Qt build. (A stale pre-Qt `bin/Release/Sonic` may still be lying around
until the next Release build.) The game is now a `QMainWindow` with a real `QMenuBar` (File: Fullscreen/Quit, Help: About) and the
frame is drawn as a texture on a `QOpenGLWidget` in the middle. Design: SDL stays for audio, gamepads and the
rendering itself -- SDL's *software* renderer draws the frame plus all the debug overlays into a surface
(`Render.c`, `Render_InitQt`), and `QtHost_Present` hands that surface to the widget, so none of the overlay code
changed. Keyboard comes from Qt (`QtHost_KeyState`, same SDL scancode indexing) and Qt key events no longer go
into SDL's queue (2026-10-02: F11 is handled directly -- see the top section). SDL gets the dummy video driver so it never touches the display.
Frame pacing is the existing own-clock path (no display vsync). Files: `src/Backend/Qt/QtHost.{h,cpp}`; `#ifdef
SCP_BACKEND_QT` in `Render.c`, `Input.c`, `System.c`.

**Debug viewers (Qt backend, View menu)** -- only shown while debug mode is active (`debug_cheat`: always in debug builds,
or after the C,C,C,C + Up/Down/Left/Right code). They replace the in-frame Tab/Alt overlays (which are disabled under Qt)
and read real data through plain-C accessors (`VDP_Peek*` in VDP.c, `src/DebugPeek.{h,c}`, `Backend/PeekData.h`):
*VDP Viewer* (scrolling VRAM tile sheet with palette/zoom/grid + tile inspector; live VDP CRAM, game water and dry palettes;
sprite table in link order with a preview), *Sound Viewer* (YM2612: key, algorithm + routing, feedback, pan, block/F-num/Hz,
TL1-4; SN76489 tones with period/Hz/attenuation dB, noise mode), *Object RAM* (all object slots + raw bytes of one). Dev
hooks: `SONIC_QT_OPEN=vdp,sound,objects` opens them at startup, `SONIC_QT_GRAB=<prefix>` saves PNGs of every window/tab.
Planned for this menu bar (user, 2026-10-01): a mod loader (.so/.dll with mods such as new characters or levels) that can add
entries to a Settings menu and override things like the level select and level order.

**Audio is Qt Multimedia now (SDL audio dropped).** `src/Backend/Qt/QtAudio.cpp`: two `QAudioSink`s in push mode, one for the music chip set and one
for the sound effects, fed once per frame (soft-clipped each; created on the first frame because `Audio_Init` runs before the QApplication exists;
no audio device = silent, not a crash). `Sound_Generate` was split into `Sound_GenerateMusic` / `Sound_GenerateSfx` (it is still exactly their sum,
for the tools). **Audio menu**: Music and Sound Effects each get a mute check box and a volume slider; *Channels* mutes FM1-6, PSG tone 1-3, PSG
noise and the DAC individually (`Sound_SetChannelMuted`, masks inside `YM2612_SetMuteMask` / `SN76489.mute_mask`; muted channels keep running so
unmuting resumes in step). SDL is now only used for gamepads (SDL_GameController, for Steam Input), the software renderer and timing.
Not persisted yet (settings reset each launch). Phonon was considered and rejected: it plays media files, it has no push-PCM path.

**Settings file** (`~/.local/share/SonicPC/Sonic1Settings.cfg`, YAML via yaml-cpp, `Backend/Qt/Settings.{h,cpp}`): music/SFX mute and volume,
the muted chip channels, and the video state (fullscreen, and the windowed size and position -- position is ignored on Wayland) plus the log file
and the demo recorder's last choices. Loaded before the window is built (so the menus show it), saved ~0.5 s after any change and on exit
(atomically, via a temp file). A missing or broken file means defaults; unknown channel names are ignored. YAML is for settings only -- demo files stay plain.

**Demo recording / playback** (Tools menu, same visibility as View: debug builds, or after the debug code). *Record Demo...* asks for a zone/act
(or special stage), an optional start position and frame limit, and a folder; the game restarts that level from whatever screen it is on and
records until *Stop Recording* (Start stays the pause button) or the frame limit; the game keeps running. Files are named
`Zone Z Act A[ xX yY] <stamp>.bin` and use the original recorder's format (see Demo.h). *Play Demo...* picks a file, reads the level from its name
(or asks), sets the attract-mode state and plays it, then returns to the Sega screen, like an attract demo. Special stage demos can't be played
back yet (the special stage has no demo loading). Game side: `Demo_RequestRecording/Playback` + `Demo_ServiceRequests` (called each VBlank); the
title and special stage loops leave when the mode is changed from outside. Dev hooks: `SONIC_QT_RECORD=zone,act,frames,file`, `SONIC_QT_PLAY=file,zone,act`.

**The separate `SonicDemoRecord` executable is gone** (target, `tests/demo_record_main.c`, `tests/timestamp.*`, the `DEMO_OUTPUT_DIR` build setting, and
the recorder-only paths in `Demo.c`: exit on stop, Start-to-stop). Recording is the Tools menu now. Not carried over: `--ai` / `--ai-pipe` (a bot or an
external process driving Sonic live while recording). `tests/ai_pipe.{c,h}` and the game's `cli_ai_control_hook` are still there, unused, to wire into the app.

**Logging** (View > Logging, debug mode only): Start/Stop Logging, Log File... (file used the next time logging starts) and Show Log.
`DEBUG_LOG("category", "fmt", ...)` (src/DebugLog.h) costs one flag test while off; entries go to an 8192-entry ring (Log window: filter,
auto-scroll, save) and, if chosen, to a file (flushed per line). Currently logged: game mode changes, level start, checkpoints, hurt/death,
every PlaySound/PlayMusic. Add more call sites freely. Dev hook: `SONIC_QT_LOG=<file>` starts logging as soon as debug mode is on.
Also: *Variables* viewer (src/DebugVars.c table, generated from the extern declarations; filter, group, change highlighting, double-click to
edit) -- add a `WATCH(...)`/`WATCH_ARRAY(...)` line to expose another variable.

Planned: wire the smoke test and demo recorder into this app (user, 2026-10-01), plus the mod loader below.

To add menu entries: `MainWindow::BuildMenus` in `QtHost.cpp` (no `&` mnemonics -- Alt is a game key). Fullscreen
(F11 or menu) hides the menu bar.

## SonLVL project files: all zones now on the recomp's resources, more definitions

**Status: done for every zone; checked with scripts, not opened in SonLVL itself.**

`SonLVL INI Files/` now reads the recomp's own files everywhere (`../res/Art`,
`Map16`, `Map128`, `Layout`, `ObjectLayout`, `Palette`, `Collision`,
`../asm/Mappings`) instead of the disasm's `../../artnem` etc. `SonLVL.ini` was
only converted up to SYZ; LZ, SLZ, SBZ, Final Zone and Ending are done now,
plus every definition file under LZ/SLZ/SBZ/END and their `.ini` lists. File
names were matched by content where possible (palettes, art), by name otherwise.
`startpos=` lines are dropped from the converted sections (no export path for
them yet); `buildscr`/`romfile` are disasm-specific and were left alone. Every
path in the folder resolves, and every object placed in any zone's layout has a
definition.

New or corrected definitions: LZ conveyor (`$63`: spawners draw their whole
platform group), SBZ platform conveyor (`$6F`, same), SBZ stomper/door/lift
(`$6B`, also registered for SBZ3), MZ chained stomper (`$31`) and lava geyser
maker (`$4C`) (both placed in MZ but had no definition), SBZ teleporter (`$72`,
draws each route), SLZ floating blocks (`$56`, shares the SYZ definition, whose
movement names were wrong and now follow `FBlock_TypeIndex`). The other 34
implemented objects without a definition are spawned at runtime only
(explosions, HUD, bosses, ...) and are not placed in layouts.

**Game bug found on the way: `res/Art/SBZFloor` was the wrong art.** It was a
copy of the 15-tile sliding-floor-trap art; the real collapsing floor art
(`SBZ Collapsing Floor.nem`) is 4 tiles, loaded at $3F5 in SBZ1 and $3F9 in
SBZ2. So SBZ's collapsing floors drew the wrong graphics and, in SBZ2, the extra
tiles spilled over the art after $3F9. Replaced with the real file, and
`test_plc.c` now pins it to 4 tiles.

## Sprite mappings converted to macros (asm/Mappings)

**Status: 131 of 143 files converted; every converted file verified byte-identical to its original.**

The mapping sources in `asm/Mappings/*.asm` (assembled into `res/Mappings` by
the build) now use macros instead of raw `dc.b` data: `mappingsTable`,
`mappingsTableEntry`, `spriteHeader` (piece count is automatic, from its
`<label>_End`) and `spritePiece x, y, w, h, tile, xflip, yflip, pal, pri`.
They live in `asm/Mappings/_MapMacros.asm`, which each file `include`s. The
macros are written in SN 68k (asm68k) syntax -- `\*` for the label on the
invocation line, `@` local labels -- because clownassembler mimics SN 68k and
rejects P128's `{INTLABEL}` version. Includes resolve relative to the working
directory, so the assemble rule in `CMakeLists.txt` now runs from `asm/` (and
rebuilds when the macro file changes).

`tools/mapconv/mapconv.pl` did the conversion and is the safety net: it only
keeps a file if the macro version assembles to EXACTLY the same bytes as the
original (dry run by default, `--apply` to write). Checked against the git HEAD
originals: 142 of 143 mapping files assemble identically; the only difference
is `LZBlocks.asm`, where the block frame's tile word was wrong (`$FFFA`, should
be `$FDFA`). 12 files with unusual layouts (Sonic's own mappings, DPLC data,
title/results screens, ...) were left as they were.

## Fullscreen toggle (F11)

**Status: done, verified headless (window flags, scale, cursor, pixel readback); not yet seen on a real display.**

F11 toggles borderless fullscreen (`SDL_WINDOW_FULLSCREEN_DESKTOP`, so the
desktop resolution is used as-is -- no mode change), F11 again returns to the
window. The frame is rendered at the windowed size in SDL *logical* pixels
(`SDL_RenderSetLogicalSize`), so SDL scales the game image -- and every
overlay (countdown pie, Z80 peek, console), which are laid out in those same
units -- up to fit the screen while keeping the aspect ratio; the rest is
black letterbox/pillarbox bars (`ClearLetterbox` blanks the whole target each
frame). In the normal window the scale is exactly 1:1, so nothing changes
there. Mouse cursor is hidden in fullscreen. Key handling is in
`Backend/SDL2/Input.c` (works in every build, ignores key repeat); the toggle
is `Render_ToggleFullscreen()` (declared in `Backend/VDP.h`, implemented in
`Backend/SDL2/Render.c`).

**The letterbox is a stopgap.** It exists only because the VDP still renders a
fixed 4:3-ish 320x224. The logical size is derived from `TEXTURE_WIDTH *
SCREEN_SCALE` (i.e. `SCREEN_WIDTH`, already parametrised via
`SCREEN_WIDEADD/2` in the camera code), so once the VDP is adapted for
widescreen the bars shrink on their own -- no fullscreen changes needed.

Non-integer scale factors use SDL's default nearest-neighbour filtering, so on
screens that aren't an integer multiple of the window size some pixel rows/
columns are slightly uneven; an integer-scale mode or linear filtering are
possible follow-ups.

## Sound playback driver (Sound.c) + PlaySound/PlayMusic API

**Status: real, working driver — PSG/DAC audible, FM wired but untested by ear; tempo bugs from this session found and fixed via SCHG wiki cross-referencing.**

Implements the actual byte-stream interpreter the earlier `smps2asmc`
pipeline only encoded data *for* — tempo governor, note/duration parsing,
most coordination flags ($E0-$F9), DAC/DPCM percussion + raw-PCM ("SEGA!"
clip) playback, and FM synthesis via a new `ymfm` C++ wrapper
(`Backend/YM2612.{h,cpp}`, `extern "C"`; pulled in `ymfm_opn.cpp` +
`ymfm_ssg.cpp` + `ymfm_adpcm.cpp` since they're one translation unit even
though `ym2612` itself only needs the OPN half). CMake now builds `C CXX`.

**High-level API** (`Sound.h`): `PlayMusic(id)`/`PlaySound(id)` are what
game code should call — routes to `QueueSound1`/`QueueSound2`, and
`PlaySound` dispatches the `$E0-$E4` range directly to `FadeOutMusic`/
`PlaySegaSound`/`SpeedUpMusic`/`SlowDownMusic`/`StopAllSound`. SFX carry a
priority (`sound_priorities[]`, mirrors the real `SoundPriorities` table)
so a lower-priority sound can't cut off one still playing. `Sound_Pause`/
`Sound_Resume` freeze `sound_music`'s tempo governor only — SFX keep
playing while paused, matching the real driver — wired into `PauseGame()`.

All `bgm_*`/`sfx_*` TODOs across `GM_Level.c`, `GM_Special.c`, `GM_SSRG.c`,
`Level.c`, and 9 `Object/*.c` files are now real calls instead of commented
pseudo-asm. Added `ResumeLevelMusic()` (`GM_Level.h`) as the shared
"return to this level's own zone track" helper — also fixed a real bug
along the way: the old ad-hoc `MusicPlaylist[level_id]` indexed by raw
`level_id` instead of `LEVEL_ZONE(level_id)`, silently wrong past the
first few levels. `ResumeLevelMusic` special-cases SBZ3 (`level_id==0x0103`,
stored under LZ's slot) and Final Zone (`0x0502`, under SBZ's slot) ahead
of the normal zone lookup — confirmed against title-card behavior, which
uses the same slot quirk. Also fixed a real, separate, unrelated bug the
user caught while discussing this: `GM_Level.c`'s underwater palette
ternary was inverted, loading green `PalId_LZWater` for the SBZ3 slot and
purple `PalId_SBZ3Water` everywhere else.

**Tempo/duration bugs found via the Sonic Community Hacking Guide wiki**
(the user pasted several SCHG "Music Hacking" pages mid-session) — these
were the actual cause of a "music isn't parsing correctly" report:

- The tempo governor was completely wrong. It modeled header bytes `$04`/
  `$05` as an accumulator-gate (only tick channels when a threshold
  crosses), so most frames nothing advanced at all. Real mechanism
  (`TempoWait`/`v_main_tempo_timeout`): every channel's duration countdown
  decrements by 1 **every frame unconditionally**; separately, a counter
  reset to `main_tempo` (header `$05`, music only) ticks down each frame,
  and on reaching 0 it resets and adds **+1 back to every channel** — a
  periodic "delay by one frame" correction, not a gate. Rewrote
  `TickChipSet` accordingly.
- `$04` ("dividing timing") is a **multiplier on a note's raw duration
  byte**, applied once when the byte is read (the multiplied value is what
  gets stored/reused, not re-multiplied on repeat notes) — wasn't applied
  at all before.
- SFX headers have **no `main_tempo` byte at all** — their single tempo
  byte (offset `$02`) is `duration_mult`, not `main_tempo` as first
  assumed. SFX now run with the periodic correction disabled
  (`main_tempo=0` sentinel) and only the multiplier scaling applied.
- `$E1` was writing into an unused `detune` field instead of `transpose`
  (the same "channel key displacement" the header pitch byte and `$E9`
  use) — `$E1` *sets* it, `$E9` *adds* to it.
- `$E5` (per-track duration-multiplier override) had a field
  (`tempo_divider`) but the multiply logic never actually read it — now
  every channel's duration multiply uses its own `tempo_divider` (seeded
  from the chip-set default at load, overridable via `$E5`), not the
  chip-set-wide value directly.
- `$E8` (note fill) had the relationship backwards: implemented as
  "duration minus a release-tail length," but it's actually the *total*
  number of frames the note is allowed to play, independent of the note's
  own duration — fixed, `0` now means "disabled" (no early cutoff).
- A bare duration byte (`$00-$7F`) encountered with no preceding note in
  that read (e.g. `81 01 04 03 02` — note `$81` replayed at durations
  `01`, `04`, `03`, `02`) means "keep the currently-held note, just change
  its duration," not "here's a new note." The interpreter treated *any*
  byte `<$E0` as a note trigger, so this case would've read a bare
  duration like `$04` as note `$04` — garbage pitch. Matches the
  `.noteloop`/`.gotnote`/`bpl .gotduration` structure in the real
  `PSGDoNext` disassembly. Fixed with an explicit `b < 0x80` branch ahead
  of the note-byte branch that updates duration/note-fill only, without
  touching pitch, key-on state, or retriggering the DAC/FM/PSG at all.
- `$EA`/`$EB` were no-op placeholders; they're real chip-set-wide setters
  (`main_tempo`/`duration_mult` respectively) issuable from inside any
  channel's byte stream — `$EB` also propagates to every channel's
  per-track `tempo_divider` copy.
- `$ED` ("ClearPush") was wrongly implemented as resetting the call/return
  stack — it's actually unrelated to the sound engine, an SFX-specific
  game-state flag (`sfx_Push`/pushable-block re-trigger prevention) that
  doesn't belong at this layer at all. Now a documented no-op.
- `$EE` needed no fix: its real-hardware complexity (restoring FM4's music
  instrument after a channel-stealing special SFX) is a workaround for a
  problem the dual-chip-set design doesn't have, since SFX never borrows
  channels from `sound_music` here in the first place.
- Verified (didn't need fixing): the `$F6`/`$F7`/`$F8` jump/loop/call
  pointer math (`target = word_offset + 1 + signed_value`) against SCHG's
  own worked examples — matched exactly, so that wasn't the bug. Per-track
  FM/PSG header field order (key-displacement, then volume-attenuation,
  then PSG's modulation-control byte and default-tone/envelope index) also
  checked out against SCHG's per-channel header tables.

**DAC sample IDs** corrected against a user-provided reference table:
`$81-$83` = Kick/Snare/Timpani (already had this), `$84-$86` = invalid on
real hardware (now explicitly excluded rather than silently falling
through), `$87` = the "SEGA!" PCM clip (now routes to `PlaySegaSound`),
`$88-$8B` = pitch-shifted Timpani variants (still not implemented — would
need per-trigger pitch, not just sample selection).

**PCM/"SEGA!" clip**: `res/PCM/sega` turned out to be a plain stripped-WAV
body (8-bit unsigned mono, no ADPCM decode step — confirmed both from the
real driver's `zPlaySEGAPCMLoop`, which just writes bytes straight to the
DAC register, and from a WAV header hex dump the user provided, which gave
an exact `SampleRate` field of 33598 Hz for a Sonic 3K-sourced version of
the clip vs. Sonic 1's own `pcmLoopCounter(16000)` — currently set to
16000 for the Sonic 1 sample; swap the constant if the source clip changes).

**FM voice format**: went through two real fixes. First, the operator
register write order — voice bytes are NOT laid out in logical op1-4
order; physical YM2612 slots are hardwired 1,3,2,4. Initially "fixed" by
remapping in the *decoder* (`FM_LoadVoice`), which turned out to be
patching the wrong side — the real bug was the *encoder*
(`smps2asmc/smps.c`'s `smpsVcTotalLevel`, which had `order[4]={3,2,1,0}`,
i.e. write order 4,3,2,1, when it should've been the Sonic-1-real 1,3,2,4).
Per explicit preference, ultimately settled on neither of those: the
encoder now writes voices in natural 1,2,3,4 order (the Sonic 2 format,
which "actually correctly follows the internal order of operators used by
the YM2612" per SCHG, vs. every other main-series game's real 1,3,2,4
layout) — `FM_LoadVoice` does the logical-op → physical-slot remap
(`{0,2,1,3}`) instead, isolating the hardware quirk to one clearly-commented
spot in the runtime rather than baking it into the intermediate format.

**Known gaps, still open**: FM pan (`$E0`) and per-channel volume changes
(`$E6`/`$EC`) don't touch FM operator TL yet — only PSG responds to
volume commands. `PlaySegaSound`'s pitch-shifted Timpani variants
(`$88-$8B`) aren't implemented. `$F0`/`$F1`/`$F4` (modulation/vibrato)
parameters are stored but never applied per-frame. None of this has been
verified by ear — only confirmed via build success, the existing automated
test suite, and cross-referencing SCHG's documentation and the real
driver's disassembly.

## Music/SFX pipeline (smps2asmc)

**Status: core pipeline done and verified; only 2 songs imported; playback driver not started.**

Built a from-scratch translator + runtime for Flamewing's SMPS2ASM format
(the human-readable macro language real Sonic 1 songs are written in),
specialized to `SonicDriverVer=1`/`SourceDriver=1` (the real Sonic 1 driver,
no cross-driver conversion needed).

- **`smps2asmc/smps2asmc.c`**: translates a `.asm` song/SFX file into a
  small C program that `#include`s `smps.h` and calls its functions in the
  same order/arguments as the original file's macro invocations. Only does
  *syntax* translation (macro names, `$XX`→`0xXX` literals, label lines →
  `SMPS_Label()` calls) — all real SMPS-format semantics live in smps.h/.c.
  This is viable specifically because SMPS2ASM song files are themselves
  just straight-line macro-call sequences with no AS-specific control flow
  (`switch`/`function`/`eval`/etc. — those only appear in the shared macro
  *package*, not in individual songs).
- **`smps2asmc/smps.h` + `smps.c`**: the C reimplementation of
  smps2asm.asm's macros for `SonicDriverVer=1`. Byte-length/encoding for
  every macro was hand-derived from the real macro source (pasted into
  chat by the user) by manually collapsing each macro's conditional logic
  to the `SonicDriverVer==SourceDriver==1` branch. Includes:
  - The full note-name→pitch-byte table (166 names), generated
    *mechanically* from the enum block (not hand-transcribed — a scripted
    parse of the `enum`/`nextenum`/alias sequence), specifically to rule
    out a transcription slip silently shifting every note past some point.
  - A single-pass byte buffer + deferred patch list for label references
    (forward refs are common — e.g. a song's header references its FM/PSG
    channel labels before they're defined later in the same file). Two
    patch kinds: `PATCH_SONG_RELATIVE` (`dc.w loc-songStart`, used by
    header pointers) and `PATCH_PC_RELATIVE` (`dc.w loc-*-1`, used by
    `smpsJump`/`smpsLoop`/`smpsCall`).
  - FM voice bank macros (`smpsVc*`), including the `SourceSMPS2ASM==0`
    interpretation of `smpsVcAmpMod` (`op<<5`, not `op<<7` — see that
    function's comment) and the `SonicDriverVer!=2` operator write order
    (4,3,2,1).
- **Verification**: not just "compiles" — cross-checked against
  hand-computed expected output. `SndA0 - Jump.asm` (SFX) matches
  byte-for-byte against manual derivation. `Mus81 - GHZ.asm` (full 627-line
  real song) verified via multiple independent pointer cross-checks: the
  Voice pointer, DAC pointer, and FM1 pointer all landed exactly where
  hand-computed, and a `smpsCall` PC-relative reference resolved exactly to
  its target's first note byte.
- **Two bugs found and fixed against the real GHZ file** (not caught by the
  synthetic test): `smpsVcUnusedBits` takes 1 *or* 5 args (d1r1-4 optional,
  default 0) — was hardcoded to require 5. Missing constants `nMaxPSG`
  (=`nA5`) and the DAC sample IDs (`dKick`, `dSnare`, etc.) from the
  "DAC Equates" section of smps2asm.asm.
- **CMake wiring**: `add_smps_song(asm_file, array_name)` function in the
  main CMakeLists chains three steps — translate (smps2asmc_tool) → build
  (per-song `songbuild_<name>` executable linking the generated `.c` +
  `smps.c`) → run (produces `Resource/Music/<name>.h`, same shape as every
  other resource). Two songs wired in as proof-of-concept:
  `Mus81 - GHZ.asm` → `Mus81_GHZ.h`, `SndA0 - Jump.asm` → `SndA0_Jump.h`.
- **Known gap**: unlike bin2h/clownassembler (built via `ExternalProject_Add`
  specifically so they're host-native even when Sonic itself is
  cross-compiled), `smps2asmc_tool` and the per-song `songbuild_*` targets
  are plain `add_executable()` — fine for this project's only configuration
  so far (native build) but would need the same isolation before trusting
  this pipeline under cross-compilation.
- **Not started**: importing the rest of the song/SFX catalog (only 2 of
  ~20+ tracks done). The actual **playback driver** — code that walks a
  song's byte stream frame-by-frame, updates `Sound.h`'s `sound_channels[]`
  state, and writes chip registers — does not exist yet. This is the
  critical missing piece before anything can actually be heard, and before
  `SonicSoundTest` (below) can do anything meaningful.

## Sound chip emulation

**Status: SN76489 (PSG) done and audible; YM2612 (FM) vendored but not wired up.**

- **`src/Backend/SN76489.h`/`.c`**: full SN76489 PSG emulator (3 tone + 1
  noise channel), standard latch/data write protocol, 2dB-step attenuation
  table, LFSR-based noise (white: tap bits 0+3; periodic: tap bit 0; resets
  to 0x8000 on every noise-control write, matching real hardware). Fixed
  point tick accumulator carries fractional cycles between
  `SN76489_Generate()` calls so successive per-frame calls don't drift or
  click at the boundary.
- **`ymfm` submodule** (root of repo, BSD-3-Clause, github.com/aaronsgiles/ymfm):
  vendored for YM2612 (FM) emulation. Chosen over e.g. Nuked-OPN2 because
  those are GPL and would put a copyleft license on this project. **Not
  wired up yet** — no C++/C interop wrapper exists, `Sound.h`'s
  `SoundChipSet` has no FM state, `Sound_Generate()` only calls the PSG
  path. This project is pure C99 today; wiring ymfm in will be this
  project's first C++ compilation unit, needing CMake changes to add a C++
  toolchain requirement plus an `extern "C"` bridge.
- **`src/Sound.h`/`.c`**: the driver-level channel/track state, deliberately
  designed around **two independent emulated chip sets** — one dedicated to
  music (`sound_music`), one to SFX (`sound_sfx`) — each with a full
  complement of channels, downmixed together in software right before
  handing samples to SDL. This is *impossible on real Genesis hardware*
  (only one physical YM2612 + one SN76489 exist, so music and SFX have to
  fight over the same channels — the entire reason the original driver's
  priority system and channel-stealing between music/SFX/special-SFX RAM
  banks exists at all), but free on a PC. User's explicit direction, not an
  invented simplification.
  - Channel counts are **deliberately over-provisioned** past real hardware
    limits, on purpose, to safely reproduce two RAM-reuse quirks the
    original relies on instead of letting them read/write out of bounds:
    - 7 "FM" slots instead of 6: on real hardware, a Sonic 1 song with no
      DAC part still has real DAC track data (just a self-looping
      `smpsJump`) — the RAM that occupies is what a 6th *music* FM
      channel's track data gets unpacked into instead, and that borrowed
      track plays on real YM2612 hardware FM channel 6 (via a 07/03
      register setup; the DAC hardware itself outputs nothing in this
      mode). Tracked as its own array slot rather than aliased onto the
      DAC channel's memory.
      - **Correction from user**: initially described as "6th FM channel
        works because DAC is a no-op" — actually the DAC track always has
        real track data (the self-loop), and the 6th-FM-channel trick
        plays on real FM6 hardware via 07/03, not some separate emulated
        path.
    - 4th PSG slot: same trick, deliberately extended. The SN76489 chip
      itself fully supports all 3 tone channels + noise running
      simultaneously (enabled by a 07/04 setup) — it's purely that the
      original driver never allocated RAM for a 4th concurrent PSG track,
      not a hardware limit. Doesn't exist as a constraint here since a PC
      has plenty of RAM to spare. Added specifically so SMS tracks (which
      do use tone+noise together) work for modders.
  - `sound_queue[3]` matches `v_soundqueue0-2` (`SOUND_QUEUE_NORMAL`/
    `SPECIAL`/`UNUSED`, i.e. `QueueSound1/2/3`).
- **`src/Backend/SDL2/Audio.h`/`.c`**: SDL audio device opened **passively**
  (no callback) — samples generated and pushed via `SDL_QueueAudio()` once
  per frame from `Audio_Update()`, matching the game's own frame-driven
  (60Hz VBlank) architecture instead of fighting it with a separate
  callback thread's timing. Hooked into `Backend/VDP.c`'s per-frame
  `Input_HandleEvents()` call site (same place, same pattern). Caps queued
  backlog at 4 frames and drops excess on a stall, rather than drifting out
  of sync. `SDL_INIT_AUDIO` added to `System_Init`'s flags.
- Quick smoke-tested: `./bin/Sonic` runs 3s with no audio-device errors;
  full `ctest` suite (7/7, includes headless `SDL_AUDIODRIVER=dummy`) still
  passes after every change in this section.

## Enigma decompression (EniDec)

**Status: done, verified byte-exact against known-good data.**

- **`src/Enigma.c`/`.h`**: line-for-line C translation of the disassembly's
  `EniDec` (used for 16x16 block mappings and similar tilemap-like assets).
  Deliberately *not* restructured/"cleaned up" — the bit-level tricks (the
  mid-word rotate when refilling with a bit deficit, the shared bitstream
  between the format-list reader and the per-tile render-flag bits) are
  easy to subtly break by simplifying control flow, so it mirrors the
  original's register/state usage closely.
- **Resource swap**: `res/Map16/{GHZ,LZ,MZ,SBZ,SLZ,SYZ}` were previously
  *pre-decompressed* binary blobs (a port-time simplification predating
  this session). Swapped back to the genuine compressed `.eni` originals
  from the disassembly, decompressed at runtime via `EniDec`, matching how
  Nemesis/Kosinski assets already worked. `LevelHeader.map16_size` (marked
  `//TEMP` in the struct) removed — no longer needed, since `EniDec` finds
  its own end. Both load sites in `Level.c` switched from `memcpy` to
  `EniDec(header->map16, level_map16, 0)`.
- **Verification**: wrote a standalone harness linking `Enigma.c` directly,
  ran it against all 6 zones' compressed data, diffed output byte-for-byte
  against the previously-known-correct decompressed data (pulled from git
  history) — exact match on all 6, and the source pointer lands exactly at
  each compressed file's true end (proving the trailing
  pointer-alignment/rewind logic is also correct, not just the main
  decode loop).

## Level select

**Status: done.**

Ported `LevSelTextLoad`/`LevelSelect`/`Tit_ChkLevSel` from the
disassembly into `GM_Title.c`. Reuses the `Art_Text` font asset that was
already sitting unused in the resource pipeline (also used by
`HUD_WriteHex` for digit rendering) — no new asset extraction needed. 21
rows (all zones/acts + Special Stage + a present-but-inert Sound Select,
since sound wasn't playable at the time this was written), Up/Down
navigation, A/B/C/Start confirm.

Gated behind the real cheat code in release builds (Up, Down, Left, Right,
then hold A + Start — via `TitleCheatStep()`, a small state machine that
also restarts cleanly on a partial-match retry rather than requiring a
hard reset). **Debug builds** (`#ifndef NDEBUG`) skip the D-pad sequence
entirely — just hold A + Start.

Also fixed `PlayLevel()`: it had a "hold A → Special Stage" shortcut that
predated level select's existence (a stand-in shortcut). Once A became
required just to *enter* level select, that check would misfire on every
level picked from the menu (A is always held by the time `PlayLevel()`
runs). Removed — now matches the disassembly exactly (always a normal
level; Special Stage is its own row, same as real hardware).

## Debug/object-placement mode

**Status: cheat entry + minimal DebugMode() core done. Full feature NOT done.**

- Cheat entry: `C,C,C,C,Up,Down,Left,Right` (this project's own entry
  gesture — the original instead counts total C presses during the level
  select code to layer this on top; not replicated since slow motion,
  which shares that mechanism, isn't implemented) sets `debug_cheat`.
  `GM_Level.c`/`GM_Special.c` already had dormant `debug_cheat`-gated code
  from before this session; this made it reachable.
- Debug builds: `debug_cheat` forced on, and the "hold A" requirement for
  activating `debug_mode` on level start is also dropped — any level start
  enables it, for faster iteration while testing.
- **`Object/Sonic.c`'s `DebugMode()`**: was a stub
  (`// DebugMode(); return;` — Sonic simply vanished when `debug_use`
  activated, since nothing drew anything in its place). Implemented a
  minimal core: on entry, temporarily unlocks level Y-boundaries and shows
  a ring sprite in place of Sonic (item 0 is always a ring in every real
  zone's debug item list, per the disassembly's `DebugList` — used as a
  placeholder since item cycling isn't implemented yet); B exits back to
  normal Sonic; D-pad free-flies at a fixed speed (not yet the real speed
  ramp-up from held input).
- **Not implemented**: cycling through the object list (A/C presses),
  spawning objects, the per-zone `DebugList` item tables (the disassembly
  has ~15-30 entries per zone, each referencing this port's existing
  object mapping/art constants — real data-entry work, not just logic),
  and the user's specific design ideas for this port: right-stick L/R to
  change subtype, Y button to traverse backwards, and conditional HUD
  switching (normal ring/score HUD when Sonic — i.e. no object attached —
  is on screen, debug object-ID HUD when an object is).

## Pause + frame-advance

**Status: done.**

`PauseGame()` (new, shared between `GM_Level.c` and `GM_Special.c` via
`GM_Level.h`) ported from the disassembly: pressing Start (with ≥1 life,
so a game-over screen can't be paused) blocks in a loop running VBlank
routine `0x10` (Paused) until Start is pressed again. Frame-advance
(`Pause_SlowMo` in the original): holding B or pressing C while paused
runs exactly one real frame then re-pauses without needing Start again
(returns from `PauseGame()` with `pause_state` left `true`, so the very
next call re-enters the paused loop immediately). The original only allows
this behind the slow-motion cheat (not implemented — see above); made
available unconditionally here instead of locking it behind a cheat that
doesn't exist.

`pause_state` existed already (defined, reset at level start, checked once
in `AnimateLevelGfx` to gate animation updates) but nothing had ever set it
— pause was entirely non-functional before this.

## Input / controls

**Status: done.**

- **Gamepad support** (`Backend/SDL2/Input.c`): `SDL_GameController` (not
  raw `SDL_Joystick`), specifically so Steam Input's virtual controller —
  which SDL sees as a standard Xbox-style pad regardless of physical
  hardware or the user's own Steam Input remapping — works automatically.
  Hotplug-aware (device add/remove events), since Steam Input's virtual
  device can appear or disappear after startup (e.g. overlay taking it
  over). Default mapping: D-pad + left stick → Genesis D-pad, X→A, A→B, B→C,
  Start/Options→Start (rebindable since 2026-10-02: see the top section).
- **Keyboard scheme reworked**: WASD (alongside arrows, no conflict) for
  movement, J/K/L for A/B/C — replacing the old A/S/D-for-jump scheme,
  which collided with using A/D for movement (likely why "jump always
  fails" was reported — a UX/keybinding mismatch, not a logic bug, though
  the fix generalized further per user request). Later user-edited (mid
  session, outside my changes) to B/N/M for A/B/C instead of J/K/L because
   it clashed with something else — respected as intentional, not reverted.
- **Real bug fixed**: `GM_Title.c` had `if (jpad1_press1 &= JPAD_START)`
  (assignment, not comparison) in two places on the title screen's
  Start-detection — a stray `&=` instead of `&`, corrupting the flags word
  as a side effect (harmless in that exact call site since the next
  frame's read overwrites it, but a real bug regardless).
- **VDP peek debug overlay** (VRAM/CRAM viewer): Tab or Select/Back toggles
  it — edge-detected (was previously un-debounced, flickering every frame
  while held). LB/RB/LT/RT pick CRAM palette 0-3, right stick (or O/L
  keys) pages through VRAM. **Compiled out entirely in release builds**
  (`#ifndef NDEBUG`) — a release build shouldn't expose a VRAM/CRAM dump to
  players.
- **`tests/joypad_test_main.c`** (`SonicJoypadTest`): standalone SDL2 tool,
  a row of 8 icons (4 direction triangles, A/B/C circles, Start square)
  that light from dim to full brightness while held. Reads input through
  the exact same `Joypad_GetState1()`/`Input_HandleEvents()` path the real
  game uses, so it's a live check of both the keyboard scheme and any
  connected gamepad. Not registered as a ctest (interactive tool).

## Background rendering fixes

**Status: done.**

- **MZ/SBZ backgrounds**: the port's `MZ_ScrollArray`/`SBZ_ScrollArray`
  tables were fabricated data that didn't match the real ROM's
  `BG_ScrollBlockMap_MZ`/`_SBZ` tables at all. Also missing: MZ's real
  `-512` row-index bias, the `+1` table-pointer offset both MZ and SBZ use,
  and the diagonal-scroll "column scan" path (`DrawBG_ColumnForBGIndex`)
  had its X/Y axes swapped and was unimplemented for SBZ entirely.
  Rewrote against the disassembly; tables padded with safe zero-bytes
  past their real length, since the real ROM's column scan can walk past
  the nominal table end into whatever adjacent ROM bytes follow (a
  hardware quirk not reproducible/desirable in a clean C port).
- **Universal fallback-path bug** (affected GHZ, MZ, SBZ alike): the
  "background row index 0" fallback drew via `DrawBlocks_LR_2` at a full
  `PLANE_WIDTH` (64 blocks = 1024px) instead of `PLANE_WIDTH/2` (32 blocks
  = 512px, the plane's real width) — double-wrapping the destination VRAM
  position and drawing over itself with the wrong tiles. This was the
  direct cause of a user-reported screenshot showing a wavy/wrong texture
  in MZ's sky. Fixed to use `DrawBlocks_LR_3` (REV01's VRAM-wrapping
  variant) at the correct width, matching the disassembly's
  `DrawBG_RowForBGIndex` `.bgXPos0` branch exactly — and matching every
  other "draw a full-width strip" call already in the same file.

## LZ water features

**Status: partial — surface sway/ripple done, water height scripting not done.**

`LZWaterFeatures()` was referenced (called from `GM_Level.c`'s main loop)
but never implemented — the call was commented out, which is why LZ's
water surface never animated. Implemented the surface-sway portion
(`src/LZWaterFeatures.c`, new file): a small continuous bob layered on top
of the water height via oscillator 0, updating `hbla_counter`/the VDP's
h-int line and `wtr_state` (whole-screen-underwater flag) every frame.

**Not implemented**: `LZWindTunnels`/`LZWaterSlides`/`LZDynamicWater` — the
per-act scripted water height changes, wind tunnels, and water slides.
`wtr_pos2` (actual, un-swayed water height) just stays wherever level init
left it (0) until those are ported.

## Known follow-ups

Done and no longer follow-ups: the sound playback driver, the YM2612/SN76489 emulation, the song/SFX catalog, debug mode, the sound tools
(the SMPS Inspector), and every object and game mode of Sonic 1.

1. **JSON-tree sound engine**: the byte driver matches the ROM and is the default; the JSON tree-walking engine (`TickChannelJSON`) is
   kept in step with it but is deliberately the later step. `mod_active` (smpsModOn/Off) parameters are not yet applied per frame.
2. **Sonic 2 and the rest of the pack**: Sonic 2 is next, then Sonic CD, Knuckles' Chaotix, Sonic 3, Knuckles in Sonic 1/2 and Sonic &
   Knuckles; the shared drum kit already holds Sonic 3 samples, so assets need per-source-game tags and per-pack manifests (a "Lite" pack
   would replace the Sonic 3 drums with silence in the same slots).
3. **If SEGA declines the pack**: a code-only recomp with a ROM asset extractor (via the s1disasm TwoEight project); not built.
4. **ParadoxComposer** is a development tool only; a `BUILD_COMPOSER` option (default OFF for pack builds) is the plan if it must not ship.
5. **Mod loader** (.so/.dll mods that add Settings menu entries, characters, levels): planned for the Qt menu bar, not started.
6. **Optional packaging**: building the KDE handbook with `kdoctools_create_handbook` (index cache) and a Doxygen target; renaming the
   `Sonic` binary (desktop `Exec`/`StartupWMClass` would change with it).
7. Minor items are batched into a weekly sweep rather than fixed mid-feature (stale TODO comments in `GM_Level.c`, `Sonic.c` (wind
   tunnels, now implemented) and `Monitor.c` are among the things to tidy).
