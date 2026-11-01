# Moving to the Sonic 3 & Knuckles engine (both games)

What `sonic3k.asm` (the Sonic 3 & Knuckles disassembly, `~/Projects/skdisasm`) does, what the port does today, and the order we move in. The aim is the one the
S3&K engine was built for: a smoother game (art streamed in a little a frame instead of stalling, tiles drawn as the camera moves, one object loop) and level data that is
sized to the level instead of padded. The same engine work applies to Sonic 1 and Sonic 2: everything below lives in `ParadoxEngine/` except the level conversions.

Line numbers are those of `sonic3k.asm` in the disassembly this was read from.

## 1. What S3&K does

### 1.1 Art: Kosinski Moduled archives and a decompression queue (line 2690 on)

* An archive is a word with the uncompressed size, then modules of $1000 bytes each, every one Kosinski-compressed on its own and padded to a 16-byte boundary. (A size
  of $A000 means $8000. A last module may be short.)
* `Queue_Kos_Module(a1 = archive, d2 = VRAM destination)` puts it on a queue of up to 4 archives (6 bytes each). Only the first is worked on.
* `Process_Kos_Module_Queue`, once a frame: if the decompression queue (`Kos_decomp_queue`, 4 entries) has room, it queues the next module of the first archive for
  `Queue_Kos`; when that module is decompressed (`Kos_decomp_queue_count` back to 0), its $1000 bytes go to VRAM by DMA (`Add_To_DMA_Queue`), the destination moves on by
  the module's size, and when the last module is done the archive leaves the queue and the next one starts (`Process_Kos_Module_Queue_Init`).
* `Process_Kos_Queue` (line 2862) does the actual decompression, and it can be interrupted by V-int (`Set_Kos_Bookmark` / `Restore_Kos_Bookmark`): decompression is
  spread over the frames that the main loop has to spare, never holding the game up.
* Level art (`Levels/*/KosinskiM Art`) loads this way, after the level has started, so a level begins at once and its tiles arrive over the first frames.

### 1.2 Level layout: a header with the level's size, then rows (line 38772 `Load_Level`, `sonic3k.constants.asm` line 292)

* `Level_layout_header`: four words: chunks per FG row, chunks per BG row, FG rows, BG rows (AIZ 1: $61, $42, $0D, $0B).
* `Level_layout_main`: $40 word-sized row pointers (a foreground and a background pointer for each of up to 32 line pairs), then the rows themselves, each only as wide as its
  plane is (a row is a run of chunk numbers, one byte each). The file is not compressed (`Levels/AIZ/Layout/1.bin` is 2124 bytes for 97 x 13 + 66 x 11 chunks).
* Masks make the size free: `Layout_row_index_mask`, `Camera_Y_pos_mask`, `Screen_Y_wrap_value` are set per level in `LevelSetup` (line 102216), so a level is any width and
  any height up to the 32 rows, and wraps where it should. The header is the level's size in chunks. What it does not hold is the camera's limits (`LevelSizes`, line 38121: x start, x end, y start, y end for each act, in code) or the start position (`Sonic_Start_Locations`): S3&K keeps those in tables, as Sonic 2 does.
* Chunks ($80 bytes: 8 x 8 words, block number in bits 0-9, X flip bit 10, Y flip bit 11, solidity above) and blocks (8 bytes: four tile words) are the Sonic 2 formats.

### 1.3 Drawing the planes: draw as you move (lines 102690-103290)

* `DrawTilesAsYouMove` / `DrawBGAsYouMove` compare the camera with where the plane was last drawn (`Camera_X_pos_rounded`, `Camera_Y_pos_rounded`) and, when it has moved into a
  new column or row of 16 pixels, `Draw_TileColumn` / `Draw_TileRow` build the new column or row (`Setup_TileColumnDraw`, `Setup_TileRowDraw`: block by block from
  `Get_LevelChunkColumn`) as VDP writes in `Plane_buffer`; `VInt_DrawLevel` sends them. Nothing redraws a whole plane while the game runs (`Refresh_PlaneFull` is for a level's start).
* Each zone has four routines (the arrays `LevelSetupArray` and `LevelEventArray`, line 102290): `<Z>_ScreenInit` and `<Z>_BackgroundInit` draw the planes when the level starts,
  `<Z>_ScreenEvent` (foreground) and `<Z>_BackgroundEvent` (background) run every frame (`ScreenEvents`, line 102264) and do the scrolling, the deformation and the drawing.
* The background's deformation is data: block tables (`Refresh_PlaneTileDeform`, `Draw_BG`, `Get_DeformDrawPosVert`) say how tall each parallax block is and where it is
  scrolled to, and the tile columns of each block are drawn from that block's own position. That is the improved scroll block system: more bands, and bands that scroll
  vertically too, without a table of every line.

### 1.4 Objects: one loop, a code pointer per object, priority queues (line 35995 on)

* An object slot ($4A bytes) starts with a long: the address of its code (0 = free). `Process_Sprites` walks `Object_RAM` calling each one (`sub_1AAFC`); an object's routine
  changes by changing that pointer. There is no id table at run time: the object layout's ids are turned into code addresses once, at load (`Sprite_Listing3` / `Sprite_ListingK`).
* `Draw_Sprite` puts the object into a priority queue (`Sprite_table_input`: 8 queues, each a count then up to $7E object addresses); `Render_Sprites` (line 36348) turns the queues
  into the sprite table in order, culling by position as it goes. `Delete_Current_Sprite` clears the slot.
* The object layout (`Load_Sprites`, line 37423) is read as before (6 bytes an entry, sorted by X, a window that follows the camera with a respawn table).

### 1.5 Rings: one entry per ring (`Load_Rings`, line 18244)

* The ring layout is a list of 4-byte entries, x then y, sorted by x; each ring has a 2-byte status in `Ring_status_table`. Rings are not objects and they are not groups: a line of
  rings is that many entries. `Test_Ring_Collisions` (line 18470) and `Render_Rings` (line 18643) work through the window of entries near the camera; collected rings are kept in
  `Ring_consumption_table` for their sparkle.

### 1.6 Primary and secondary groups (`LevelLoadBlock`, line 199524; `LoadLevelLoadBlock`, line 9727; `LoadLevelLoadBlock2`, line 38699)

* A level's patterns (the 8 x 8 tiles), blocks (16 x 16) and chunks (128 x 128) are each in two groups, a primary and a secondary. The table has one entry for each act: two sprite
  art lists (PLC), the palette, then the primary and secondary archives of the patterns (Kosinski Moduled), of the blocks and of the chunks (`levartptrs`, line 177).
* Zones share their primary groups and give each act its own secondary (Hydrocity: `HCZ_8x8_Primary_KosM` for both acts, `HCZ1_8x8_Secondary_KosM` and `HCZ2_8x8_Secondary_KosM`). When a
  level uses one group for both (Carnival Night), the two pointers are the same and the second load is skipped.
* The secondary goes straight after the primary: the primary's size (the first word of its archive) is the secondary's destination in the tile memory, and the secondary blocks and
  chunks are decompressed into the same table right after the primary's (`Kos_Decomp` goes on where it stopped).
* This is how the act 2 transition works: only the secondary is loaded again (with the next act's art list and palette), which is how a level changes under the player's feet without a
  reload. That transition is what stays unused until the real Sonic 3 is ported. The groups themselves are used by every level from the start: each level is loaded as a primary and a
  secondary, so that nothing is converted twice when the transition is added.
* Hill Top in the port (Emerald Hill's tiles and blocks with its own written over them from tile $1FC and block $130; the editor's supplement modes) is the same idea: the base is
  the primary group and the supplement the secondary. Converted to this, Emerald Hill is its primary plus its own secondary, and Hill Top the same primary plus a different one. Wing Fortress gets a real secondary too (the user); what its primary is to be settled when it is ported, and the loader does not care which zone's primary a level names.

## 2. What the port has today

| Part | Port | S3&K |
|---|---|---|
| Level art | Nemesis/Kosinski, decompressed whole at level start | Kosinski Moduled, queued and streamed, in a primary and a secondary group |
| Blocks, chunks | one table each (`S2Map16`, `S2Map128`); Hill Top's are joined files with its own written over Emerald Hill's | a primary and a secondary group of each, the secondary straight after the primary |
| Layout | `S2Layout/*`: 16 rows x 128 chunks, FG and BG interleaved, Kosinski; sizes in tables (`LevelSizes`, `StartLocArray`, `BGScrollBlockSizes`) | header with the size, rows as wide as the level, row pointers |
| Planes | `LevelDrawCore.c` / `LevelPlane.c`: planes redrawn from the camera with the game's own tables per zone (`Deform_*` in `LevelScroll.c`) | draw-as-you-move into a plane buffer, per-zone init/event routines, block-table deformation |
| Objects | `Sst` (the S3&K status table) with a code pointer per slot (`sst_object_codes[id]` by id), run by `Sst_Process`; 8 priority queues drawn by `SstRenderQueue` in `Sprites.c` | code pointer per slot, 8 priority queues |
| Rings | one 4-byte entry per ring (`res/RingLayout/*`, `res/S2Rings/*`), kept by the ring handler `Rings.c`: not objects (DONE, stage 5) | one 4-byte entry per ring |

Sonic 1 and Sonic 2 share the engine (both already use the Sonic 2 layout conventions: 16 rows of a $100-byte stride, 128-pixel chunks, `res/Layout` and `res/RingLayout`
in Sonic 1, `S2Layout` and `S2Rings` in Sonic 2), so every stage below is done once in `ParadoxEngine/` and each game then converts its own data. Sonic 1's ring and object
layout files are looked at again at stage 3 (what format `res/RingLayout` is in decides how its conversion goes).

## 3. The order

The numbering is the user's (2026-10-30): stage 1 is the dependencies, stage 2 the drawing engine, stage 3 the scrolling engine. Each stage can be built, tested and pushed alone; the
game plays as before after each. Tests compare the new path with the old one (the layout read back, the plane drawn, the sprites produced) before the old one is dropped.

1. **Kosinski Moduled support** (DONE: `QueueKosinskiModule()` and its queue, `kosm`, the tests; no level uses it yet). The queue takes the secondary archive with the primary's size as its
   destination, as `LoadLevelLoadBlock` does. Putting one level per game through it (Hill Top, Green Hill) waits for a decision about the editor's own copy of the art.
2. **The drawing engine**:
   * a. **Level layout format** (DONE): the Sonic 3 & Knuckles layout file (`LevelLayout.c/.h`), Kosinski compressed; all 44 layouts of both games converted by `tools/layout2s3` (each checked to
     read back to the old buffer byte for byte); both games' loaders, the editor's save and the importer write and read it. In memory it is still the fixed buffer (zero where the file has no chunk),
     `level_layout_size` says how big the level is, in chunks, as the original's header does. The camera limits and start positions (`LevelSizes`, `StartLocArray`) stay tables in code, as they are in S3&K.
   * a2. **The layout is allocated at the size its file says** (DONE): two planes of exactly that size (`LevelLayoutPlane`), no fixed buffer; the chunk accessors wrap as the old buffer did until the per-level masks of stage 3. The user's note: the start positions and the dynamic events may have to be adjusted where an optimisation changes a level.
   * b. **Primary and secondary groups** (Emerald Hill and Hill Top DONE 2026-10-31 for patterns and blocks, plain Kosinski; chunks are one group; Wing Fortress waits for its port) of patterns, blocks and chunks in the level table (section 1.6), loaded the way `LoadLevelLoadBlock` does; Hill Top and Wing Fortress first.
   * b2. **The level load block** (DONE 2026-10-31 as the table and its loader: `level_load_block[zone][act]` in both games, every act naming one group twice so far; real secondaries and Moduled patterns are still to come) (the user, 2026-10-31: "the MLLB needs changing", read as `LevelLoadBlock`): each game's `LevelHeader` (`level_header[zone]`: one PLC, art, a second PLC, map16, palette, map128, per zone) becomes S3&K's table of one entry for each act (`levartptrs`): two PLCs, the palette, then primary and secondary for the patterns, the blocks and the chunks. Zones share their primaries, an act names its own secondaries, and `LoadLevelLoadBlock` / `LoadLevelLoadBlock2` read it. The user's copy of the table (pasted 2026-10-31) is the model: the same 9 fields in the same order, one entry for each zone and act in zone order, an act that has one group names it twice (CNZ, ALZ, BPZ...: the second load is skipped), and the unused entries point at Angel Island's. The port keeps one entry for each zone and act it has. In C it is an array of arrays, `level_load_block[ZoneId_Num][4]` (zone, then act: the same shape as `level_layouts`), because S3&K and later games have an entry for each act and not for each zone; an act the game does not have is left empty.
   * c. **Plane drawing** (DONE, in the shape the games needed: `LevelPlane.c` (a plane that follows a camera: flags, snapshot, `LevelPlane_DrawPending`) and `ScrollBands` read the layout through `LevelLayout_Chunk`, which applies the level's masks; the S3&K routines below are what they are modelled on): the plane buffer, `Draw_TileColumn` / `Draw_TileRow`, `DrawTilesAsYouMove`, reading the layout through its row pointers and masks (engine `LevelDrawCore.c`, `LevelPlane.c`),
     each checked against the old drawing (the planes' name tables identical after the same camera moves).
3. **The scrolling engine** (the user's stage 3). The games keep their own camera (how a game's camera moves is part of how it feels: Sonic 1's 16-pixel limit, Sonic 2's look-ahead); S3&K's
   `MoveCameraX` / `MoveCameraY` (line 38378 on: the $90 to $A0 window, $18 pixels a frame, the position history for the flying and the panic) are added to the engine as an option for the Sonic 3 port. What is
   converted is what the zones do each frame.
   * a. **The frame and the wrap masks** (DONE): `ScrollEngine.c/.h`: the four routines of a zone and act (screen init, background init, screen event, background event) in a table, `ScrollEngine_Init` /
     `_Events`, and the level's wrap masks (`level_layout_col_mask`, `_row_mask`, `level_camera_y_mask`, `level_screen_y_wrap`: the engine's old 128 across and 16 down unless a level sets its own),
     which `LevelLayout_Chunk` reads. Both games register their zones' old deformation routines as background events, so nothing moves yet.
   * b. **Deformation as data** (DONE: `ScrollBands_BandAt`, `ScrollEngine_ApplyDeformation`, `ScrollBands_Init/DrawAll/Draw`; the old scroll flags are removed with the zones in d): the block tables of S3&K (`Refresh_PlaneTileDeform`, `Draw_BG`, `Get_DeformDrawPosVert`: each band's height and the position it scrolls to) as engine primitives that fill the
     scroll lines and draw the background's columns band by band, replacing the old scroll flags (`bg1_scroll_flags`...) and `BGScroll_Block*`. A layer is a table of band heights (bit 15: a position for every line) and a list of positions; `ApplyDeformation` fills the scroll lines from them (the foreground or the background deformed), and `ScrollBands` keeps the plane drawn as the bands move: a band that crosses a 16-pixel line draws a column for its own rows, a layer that crosses one in Y draws a row with each block at its own band's X. Tested against drawing the view again from scratch after 600 random moves, a jump included.
   * c. **First zones through it** (DONE): Green Hill in Sonic 1 and Emerald Hill in Sonic 2 written as engine zones (init, events, deformation tables), each checked against the old routine for the same camera
     moves: the scroll lines and the planes' name tables must come out the same.
   * d. **The other zones** (DONE: every zone of both games, the scroll flags removed), one at a time the same way (Sonic 1: Labyrinth, Marble, Star Light, Spring Yard, Scrap Brain; Sonic 2: Hill Top with its quake, Hidden Palace, Chemical Plant, Wood, Oil
     Ocean, Dust Hill, Casino Night, Neo Green Hill, Sand Shower, Cyber City, Metropolis), then the split screen's second view; the old `Deform_*`, `BGScroll_*` and the scroll flags are removed.
   * (The solids, the user's note 2026-10-31: the three shared uncompressed parts, `HeightMap`, `WidthMap` and `Angle`, stay as they are, as S3&K keeps them; only the level-specific part, each level's collision index, is converted.)
   * e. **Per-level masks and camera limits** (DONE as the helper: `ScrollEngine_SetLevelMasksForLayout`, `_LevelLoops`; every level keeps the defaults until one is optimised): each level sets its masks (from its layout's size) instead of the defaults; where a level was optimised, its start position and its dynamic events are adjusted (the
     user's note).
4. **Ring data format and the ring handler** (DONE 2026-10-31: engine `Rings.c/.h`, `tools/rings2s3.pl`, the editor's ring tools): the 4-byte entries and the status table; `Test_Ring_Collisions` and `Render_Rings` as the model; Sonic 2's `S2Rings/*`
   converted by expanding each group, and Sonic 1's `res/RingLayout/*` the same way. The editor keeps placing "a line of N rings" and writes N entries.
   **Status:** all 46 layouts are converted (`rings2s3.pl`; the importers write the new format); `Rings.c` is `Test_Ring_Collisions`, `Render_Rings` and the consumption list, with the box test in the original's word arithmetic and the
   rings drawn before the queues. One difference from the original, on purpose: it finds the rings near a camera or a player when asked (binary search of the sorted list) and does not keep `Ring_start_addr_ROM`/`_end_addr_ROM` moving, so
   the split screen's second view and a player away from the camera work with no second set of pointers. Left out: the Lightning Shield attraction (neither game has it), `Ring_status_table_2` (the vertical level of zone $14) and `Perfect_rings`.
5. **Object handler** (the user's stage 4, after the scrolling engine): the object format of Sonic 3 & Knuckles with its own status table, an SST of its own beside the old `Object` one (the games' objects keep
   **Status (2026-10-31):** stage 4 is done for both games: every object, the characters and the screens' objects (title, continue, results, ending, the SSRG screen) are on the status table, and the old `Object` path is gone (4e): `Object`/`objects[]`, the old loop of `ExecuteObjects`, the old half of `BuildSprites`, `ObjectsManager`, the `game_objects[]` tables and the explosion markers, the old `Solid.c`/`SolidObject`/`BossMove`/`BossDefeated`/`Object.c` helpers (`SolidSst_*` is `Solid_*` again, `DebugMarkersSst_Show` is `DebugMarkers_Show`). The loader has a second window for the other player's camera (`SstLoad_UpdateSecond`, as S3&K keeps a second set of pointers for Player_2), which Sonic 2's split screen uses.
   working until each moves over). The S3&K object is $4A bytes (`object_size`) with the code address first (0: a free slot), `render_flags` ($4), the routine ($5), the height and width in pixels ($6, $7), the
   priority as a word in units of $80 ($8), the art tile word ($A), the mappings pointer ($C), the position ($10 and $14, words with a sub-pixel word after), the speeds ($18, $1A), the radii ($1E, $1F), the
   animation ($20, $21, $23, $24), the mapping frame ($22), the angle ($26), the collision flags and property ($28, $29), the status bytes ($2A, $2B, $37), the subtype ($2C), the secondary routine ($3C) and the
   respawn address ($48), the rest each object's own (`objoff_*`). In C it keeps the names and the meanings and not the byte offsets (the code is a function pointer). The slots are Object_RAM's: Player_1,
   Player_2, the reserved one, 90 dynamic and 17 fixed ones of the level (110).
   * a. **The table and the handler** (DONE: `Sst.c/.h`): the struct, `Sst_Process` (Process_Sprites, with the frozen-level case), the 8 draw queues of 63 (`Sst_Draw`: Draw_Sprite, a full queue passes the object
     on), `Sst_Delete`, `Sst_Allocate`/`_AllocateAfter` as the original searches (it starts at the second dynamic slot and runs one slot past the last: kept), `MoveSprite`/`MoveSprite2` and the
     reverse-gravity versions, `Animate_Sprite` and `Animate_SpriteIrregularDelay` with all their codes. Tests: `test_sst.c`.
   * b. **Rendering** (DONE except DPLC, the HUD and rings): `SstRenderQueue` (Sprites.c): `Render_Sprites` over the queues into the sprite table the old objects use (the S3&K mappings, 6-byte pieces; the cull tests
     with `Screen_Y_wrap_value`; static mappings; multi-draw objects with their children, each piece culled singly; the on-screen flag). A queue's new objects come after its old ones. Still to come: DPLC
     (`Perform_DPLC`) for the players, the sprite mask, and the HUD drawn first (the rings are drawn first too: stage 5, `Rings_Render`); the existing `BuildSprites` stays for the old objects until the last has moved.
   * c. **Loading** (DONE: `SstLoad.c/.h`, `test_sstload.c`): `Load_Sprites` with the respawn table (a byte per entry, bit 7), the X window and the Y window (a row pass; bit 15 loads whatever the Y; the wrapping
     versions), the held free slot, `Seek_Object_Manager`, `Clear_SpriteRingMem`'s object half, `Sprite_OnScreen_Test` / `_Test2` / `Delete_Sprite_If_Not_In_Range`. The layout files already are the 6-byte
     format, so nothing is converted in bulk: the id's routine comes from a table in each game's `Level.c` (empty until objects move over); the old `ObjectsManager` skips ids the table has. Bit 15 is the old
     remember and the new ignore-Y: step d clears it for the entries of an id as it moves. The old manager is removed in step e. Later with the objects: the collision-response variant of the on-screen
     test, the split-screen second loader.
   * d. **Moving the objects**: Sonic 1 has 235 object files (about 22,700 lines) and Sonic 2 99 (about 20,600): each is rewritten for the `Sst` (field names, `Sst_*` calls, the routine tables as code pointers)
     and its tests move with it, a group at a time (players first, then the objects every zone shares, then each zone's), with the old and new handlers both running every frame until the last one is over. Not
     mechanical: the new `x_pos` is a word with a sub-pixel word, the radii are two fields, the status bits differ from the old ones, and the sprite pieces are drawn from different mappings.
     *Method, proven on Sonic 1's Pylon and Waterfall trigger* (see the ChangeLog entry): the object's `.c` takes `Sst *self`; its mappings file includes `asm/Mappings/_MapMacrosS3.asm` (6-byte pieces; the
     `res/Mappings` files are generated and ignored by git, the `.asm` is the source); its row goes into `sst_object_codes[]` (`Sonic1Objects.c`) and NULL into `game_objects[]`;
     `tools/objlayout_clear_remember.pl` clears bit 15 of its id in `res/ObjectLayout`; a test loads it from a layout and runs it. Left for the players' step: the debug list (its previews draw the old
     mappings format), the editor's live placement, and everything that touches `player` or the solid/touch helpers.
     *Status of d (Sonic 1)*: 79 object ids run on the status table (everything of the level machinery, the badniks that do not need the player's animation, the platforms, hazards, fireballs, explosions, points,
     bumpers); 42 object files remain: the player (`Sonic.c`, `SpecialSonic.c`) and what follows it (`ShieldInvincibility`, `Splash`, `AirBubbles`, `DrownCount`, `WaterSurface`, `InvisibleBarrier`), the bosses and their
     parts, the screens' objects (title, ending, credits, results, continue, special stage), `Ring` (stage 5), `Monitor`, `Signpost`, `Checkpoint`, `Animal`, `Seesaw` (shares its ball with the Star Light boss),
     `Button`/`Scrap Eggman`, `PathSwapper`, `GHZTunnel`, `GiantRing`, `HiddenBonus`. Sonic 2: its shared objects moved with Sonic 1's; its own 90-odd object files (two characters, `Solid.c`, `Tails`) are all still to come,
     with the characters, which move after Sonic 1's player. The tools (`objtosst.pl`, `sstregister.pl`) and the lessons in the ChangeLog make a file a matter of minutes; the player and the bosses are not mechanical.
   * e. **The old handler goes** (DONE 2026-10-31): `Object`, the old loop of `ExecuteObjects`, the old half of `BuildSprites` (which now builds every view from the status table's queues), `ObjectsManager` removed.
6. **The levels** (the solid indices are done, 2026-10-31: `LevelSolids.c/.h`, one file for both paths; the primary/secondary split of the other zones, Kosinski Moduled art streaming and the editor/importer writing groups wait for the move toward Sonic 3, the user's call): every zone of both games through the converters (Emerald Hill and Hill Top first: the one primary and two secondaries), the importers and the editor writing the new formats, the old
   readers removed (the camera limits and start positions stay in their tables, as in S3&K, unless the user wants them taken from the data).

## 4. Things to decide, and a warning

* **Space.** Measured with stage 2a: the 44 layouts of both games were 27,167 bytes as Kosinski, 79,107 as plain Sonic 3 & Knuckles files (the original does not compress its layouts) and are 27,982 as Kosinski compressed Sonic 3 & Knuckles files (3% more), which is how they are kept; the layout does not get smaller in this port, the art streams, but S3&K's ring list is bigger than Sonic 2's groups (a group of 8 rings is
  4 bytes there and 32 here), and an uncompressed layout is bigger than a Kosinski one when the level is mostly empty. The conversions will tell the real sizes; the doc is not
  promising a number until the first zone is converted.
* **Kosinski Moduled vs Nemesis.** Moduled art is Kosinski, which is bigger than Nemesis; its gain is the streaming, not the size.
* **Where the data lives.** The converted layouts replace `res/S2Layout/*` and Sonic 1's equivalents; the importer for Sonic 2's prototype levels (`tools/import_s2sw_levels.pl`) must write the
  new format, or be retired once everything is converted (it is already behind the hand-kept `Sonic2LevelData.c`).
* **The editor** needs each stage: a layout of any size (Plane tabs), rings one by one, the save formats. Its terrain tabs (tiles, blocks, chunks, solids, palette) do not change.
