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
  any height up to the 32 rows, and wraps where it should. The level size tables of the older games (sizes, start positions by act) are what this replaces.
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
| Objects | `Object` has a `type` id and `game_objects[type]` is called each frame; priority queues in `Sprites.c` | code pointer per slot, 8 priority queues |
| Rings | Sonic 2: groups of up to 8 (x, y, count, direction) in `S2Rings/*`; Sonic 1: `res/RingLayout/*` | one 4-byte entry per ring |

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
     `level_layout_size` says how big the level is. The size and start tables (`LevelSizes`, `StartLocArray`) have not moved into the data yet.
   * b. **Primary and secondary groups** of patterns, blocks and chunks in the level table (section 1.6), loaded the way `LoadLevelLoadBlock` does; Hill Top and Wing Fortress first.
   * c. **Plane drawing**: the plane buffer, `Draw_TileColumn` / `Draw_TileRow`, `DrawTilesAsYouMove`, reading the layout through its row pointers and masks (engine `LevelDrawCore.c`, `LevelPlane.c`),
     each checked against the old drawing (the planes' name tables identical after the same camera moves).
3. **The scrolling engine**: the per-zone routines (screen init / background init / screen event / background event), the block-table deformation (`Refresh_PlaneTileDeform`, `Draw_BG`),
   the camera code (`MoveCameraX/Y`, `DeformBgLayer`) in place of the games' `LevelScroll.c` `Deform_*`, ported zone by zone (Green Hill first in Sonic 1, Emerald Hill in Sonic 2), each checked against
   the old one frame by frame.
4. **Ring data format** (engine `RingsManager.c`, the editor's ring tools): the 4-byte entries and the status table; `Test_Ring_Collisions` and `Render_Rings` as the model; Sonic 2's `S2Rings/*`
   converted by expanding each group, and Sonic 1's `res/RingLayout/*` the same way. The editor keeps placing "a line of N rings" and writes N entries.
5. **Object handler** (engine `ObjectCore.c`, `ObjectsManager.c`, `Sprites.c`): a code pointer in each `Object` (filled from the layout's id through the game's table when the object is created),
   `Process_Sprites` as the one loop, `Draw_Sprite` / `Render_Sprites` with the 8 queues. Objects keep working as they are (the pointer starts as `game_objects[type]`); an object that changes its own
   routine can then do it the S3&K way, one object at a time, when it is touched anyway.
6. **The levels**: every zone of both games through the converters (Emerald Hill and Hill Top first: the one primary and two secondaries), the importers and the editor writing the new formats, the old
   readers removed, the size and start tables folded into the data where they belong.

## 4. Things to decide, and a warning

* **Space.** Measured with stage 2a: the 44 layouts of both games were 27,167 bytes as Kosinski, 79,107 as plain Sonic 3 & Knuckles files (the original does not compress its layouts) and are 27,982 as Kosinski compressed Sonic 3 & Knuckles files (3% more), which is how they are kept; the layout does not get smaller in this port, the art streams, but S3&K's ring list is bigger than Sonic 2's groups (a group of 8 rings is
  4 bytes there and 32 here), and an uncompressed layout is bigger than a Kosinski one when the level is mostly empty. The conversions will tell the real sizes; the doc is not
  promising a number until the first zone is converted.
* **Kosinski Moduled vs Nemesis.** Moduled art is Kosinski, which is bigger than Nemesis; its gain is the streaming, not the size.
* **Where the data lives.** The converted layouts replace `res/S2Layout/*` and Sonic 1's equivalents; the importer for Sonic 2's prototype levels (`tools/import_s2sw_levels.pl`) must write the
  new format, or be retired once everything is converted (it is already behind the hand-kept `Sonic2LevelData.c`).
* **The editor** needs each stage: a layout of any size (Plane tabs), rings one by one, the save formats. Its terrain tabs (tiles, blocks, chunks, solids, palette) do not change.
