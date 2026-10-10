# Changelog

Session notes capturing context, rationale, and outstanding work that isn't
already recorded in code comments or commit history — written so this
survives conversation summarization/compaction. Newest work first.

- **Wider tile entries, and freeing a bank purges what names it** (the user: memory is not the constraint, and freeing things in the table handles the stale issue; this also prepares for a Chaotix port, which will need a virtual frame buffer): the entry is four words now (`pattern`; `attr` with the flags and the 4 bit palette in its low byte and the bank, 8 bits, in its high byte; a 16 bit `generation`; one not used), `sprite_t` has the same attribute and generation, and there can be 254 banks (`TILEBANKS` 256; id 255, `TILEBANK_NONE`, is the id of an entry that shows nothing). Freeing a bank now goes through every table that holds entries (`TileBank_OnFree`; the viewports' planes, windows, scratch planes and sprite tables register) and changes the entries that name it to TILEBANK_NONE ones, so nothing is left pointing at it and it is not counted as a stale reference; the generation (65,535 frees to come round) stays as the check on an entry made from the old bank and put in a plane after the free. The two words the user asked for are the first two, as before: the loaders and the compat reading of the Genesis do not look at the other two. Tests: freed banks show nothing without a stale count, and a late entry still carries a generation that is caught; both games' tests pass and the frame hashes of the three runs are identical.

- **Tile entries are two words, and the VDP reads them compat or real** (the user: the first word the Genesis part without the flags and palette number, the second everything else; the compat loader reads like the Genesis and converts to the real form in memory; real versions of the palette, tile and sprite hooks beside the compat ones, compat for the Genesis remakes unless said otherwise): a name table entry (`tile_entry_t`) and a sprite's tile are a `pattern` word (16 bits: the Genesis' 11 are its low bits) and an `attr` word: its low byte has the flags and the palette (priority, x and y flip, the palette line in 4 bits where the Genesis has 2), its high byte the bank (4 bits) and the bank's generation (4 bits: a slot freed fifteen times since would match again; this is the price of the entry being 32 bits). The games' data is Genesis tile words, and the loaders (`Plane_Put`, the block drawing, `CopyTilemap`, the sprite builder) convert them as they write (`TileEntry_FromWord`: the palette group of a plane or object adds 4 lines a group; `TileEntry_Word` goes back); `Plane_PutEntry` and the sprite table take the real form as it is. The VDP reads each part through hooks (`VDP_SetReal(parts)`; `VDP_REAL_PALETTES`, `_PLANES`, `_SPRITES`, each its own): compat is the Genesis (the palette's low two bits, the pattern's low 11 bits, the main bank, no 8bpp, the bank and generation not looked at: all the games do by default) and real is the whole of it (16 lines, 16 bit patterns, banks with their generation caught when stale, 8bpp tiles). Sonic 2 switches the planes to real while a split screen has the second view's own bank (Hill Top's mountains) and back after. Tests (the tile bank ones, in real mode; three new: a Genesis word becoming the two words and back, the compat and real reading of one entry differing as they should (bank and line 5 against main bank and line 1), and each part being real alone), both games' tests pass, and the frame hashes of the three runs are identical to before.

- **VDP memory split, stage 6: 16 palette lines in the tile format, and 8bpp tiles beside 4bpp** (the user: wider palettes in the tile format, stored as RGB32 with palette writes converting from the other two; tile storage for both 4bpp and 8bpp, with the allocator's flag in a table): all 16 lines of the colour RAM (it already held true colour, `VDP_WriteCRAM` converting the Genesis' words and `VDP_WriteCRAM_RGB` taking 24 bit) are drawn with now (`VDP_PALETTES_ACTIVE` 16). A name table entry has a `palette_group` and a sprite too (`sprite_t.palette_group`, `Object.palette_group`, `plane_t.palette_group` for the tile words a game writes): the line is the group times 4 plus the tile word's own two bits, so the games' data is unchanged and anything that wants lines 4 to 15 says which group. The water split's palettes stay the games' four lines (`VDP_WATER_COLOURS`), the other lines are the same under water. Tile banks hold 4bpp and 8bpp tiles: each `tilebank_t` has an allocation table of its own (`depth`, a byte for every 32 byte slot: 4bpp, the head of an 8bpp tile or its tail, 8bpp tiles being 64 bytes, two slots; `TileBank_SetDepth` marks them, `TileBank_IsDeep` asks, and the table goes with the bank when it is freed; the main bank's is a static array). The planes and sprites draw an 8bpp tile as a byte a pixel through the whole palette (the byte is a colour 0-255; 0 is clear; the palette bits do not apply), with the same priority rules; an 8bpp sprite steps two slots for each of its cells; the stacked split screen's pairs of patterns do not do 8bpp. `tile_entry_t` carries explicit reserved bytes (no padding left to chance: the scroll tests compare entries as bytes). Five new tests (palette group on a plane and on a sprite, the depth table, an 8bpp tile in a plane, an 8bpp sprite's cell step) and the palette test now says 16 lines are drawn. Both games' tests pass and the frame hashes of the three runs are identical to before.

- **VDP memory split, stage 5: the window plane** (the user: the window plane): each viewport has a window plane (its own name table, `screen1p.window` / `screen2p.window`) and a `window_region_t` (an edge from the left or right, an edge from the top or bottom, or both, in pixels of whole tiles: `Viewport_SetWindow`, `Viewport_ClearWindow`). Where the region is, the VDP draws the window plane in place of plane A: it does not scroll (its tiles stay where they are on the picture, from the top left of the viewport), and plane B shows through its clear pixels, not the plane A that is not drawn there (plane A's tiles at the region's edge run past it, so plane B's pixels under the window are kept and put back first). The region of two edges is the two parts together (an L), as the Genesis' window is. Nothing in either game uses it yet, so nothing changes in them (500 frame hashes of the Sonic 2 split screen, Emerald Hill and Green Hill identical again); seven tests (`test_vdpwindow.c`, both programs): no region, left, right and top edges, both together, not scrolling with plane B through its clear tiles, and clearing it. Still open from the plan: wider palettes in the tile format and a game using a bank of its own.

- **VDP memory split, stage 4: the sprites name their tiles' bank, and the sprite tables are the viewports' own** (the user: on to the next step; sprite tile references and the sprite table in `viewport_t`): a sprite table entry is a `sprite_t` now (the Genesis' four words, y, size and link, tile, x, and the bank and generation its patterns are in, as a name table entry names its tile), `Object` has a `bank` (NULL: the main bank; what its pieces name when they are drawn: `DrawPieces` and the multi-sprite draw), and the VDP draws a sprite's patterns from that bank (`TileBank_Get`; a bank that is gone draws nothing, a pattern past the bank's end is a blank tile). The tables are `screen1p.sprite_table` and `screen2p.sprite_table` (`VIEWPORT_SPRITES` and one for the list's end; `BUFFER_SPRITES` is the same number): `sprite_buffer` and `sprite_buffer_p2` are gone, `viewport_t.sprites` is the table the VDP reads (its own, another for the tests, or NULL for none), and the sprite pipeline, the special stage's own sprite writing and the debug viewer's sprite list work on them. Checked against the stage before by the VDP's frame hash hook (`SONIC_HASH`, `SONIC_FRAMES`): 500 frames of the Sonic 2 Hill Top split screen, Emerald Hill and Sonic 1's Green Hill are identical, and both games' tests pass, with two new ones (an object's sprites naming its bank; a sprite drawing from its bank, the main bank, and nothing from a freed one). Still to do from the plan: the window plane's drawing, the VDP's remaining Genesis-isms (the 4 palette lines of the tile format), and an object that uses a bank of its own in a game (nothing does yet).

- **VDP memory split, stage 3: name table entries name their tiles by bank** (the user: the name table refers to tile objects, with the lifetimes managed and use after free prevented): the engine has `TileBank.h`/`.c`. A `tilebank_t` is a bank of tiles (its art, a count, an id and a generation): bank 0 is the main bank, the VDP's tile space that the games load their art into and the sprites draw from (its generation is 0 for good); `TileBank_Create` / `TileBank_Free` make and free others (a table of 16), `TileBank_Write` puts art in. Freeing a bank changes its generation (never 0 again), and a `tile_entry_t` (pattern as 32 bits, attributes, bank, generation: priority, palette line and flips as the Genesis'; 8 bytes) that remembers the old generation no longer resolves: `TileBank_Pattern` returns NULL for a freed, never made, wrong-generation or too-high pattern, the plane draws nothing for the tile, and `tilebank_stale_count` counts it (`tilebank_stale_fatal` aborts at the first, for debugging). Planes hold these entries (`plane_t`: entries, count, and the `bank` that the tile words a game writes name their patterns in: the games' block and tile map data are unchanged, `Plane_Put` turns a word into an entry; `Plane_PutEntry`, `Plane_Word` and `Plane_At` are the rest) and the VDP's plane drawing fetches each tile through its bank (the stacked split screen's pairs of patterns too). The planes that the special stage keeps in numbers in the tile space (its seven background canvases, which the original puts among its tiles) are in a scratch plane memory of their own now (`Plane_UseScratchAt`: the same layout as the original's $0-$FFFF, not the tile space: its art can no longer be written over by them). And the Hill Top split screen's mountains of the second view are a bank of their own (32 tiles, made with the split and freed with it; the tile remap of stage 2 names the bank: `DrawTileRemap_Set(plane, first, count, bank)`), where stage 2 had them in tile space slots $540-$55F; no free slots to find any more. The SSRG screen's one write to $C704, which went into the tile space among the art, goes to the scratch plane memory too (it showed nothing before as far as I know: if the screen looks different, that was it). Eight tests (`test_tilebank.c`, in both programs): the main bank, an entry drawing from its own bank, freed banks drawing nothing, a slot made again not being the old bank, patterns past a bank's end, a bank of 12,288 tiles (past the 2,048 an entry of the Genesis could name), the table running out and being freed, and tile words keeping their flags. Both games' tests pass; the Hill Top split screen and single view look as before. Not yet: the sprites' tiles still draw from the main bank only, the sprite tables are still the games' own arrays, and the window plane is not drawn.

- **VDP memory split, stage 2: `plane_t` and `viewport_t`, and the planes by object, not by address** (the user: past emulation, the target is not a Genesis; each space its own C symbol, `screen1p` a `viewport_t`): the engine has `Viewport.h`/`.c`: `plane_t` (a name table: its entries and how much memory is behind them; no size of its own: the size in tiles is the viewport's `plane_width` and `plane_height`), `vsram_t`, and `viewport_t` (picture size, planes A and B and the window, its horizontal scroll table, its vsram, its sprite table, an optional palette, the horizontal interrupt's counter and enable), with `screen1p` and `screen2p` as globals that own the storage (six static arrays: each view's two name tables and scroll table; `vdp_vram` is the tile space and nothing else). The VDP draws from the two viewports (`VDP_SetSplitScreen(mode)` only picks the layout), and the register style calls of the Genesis are gone: `VDP_SetPlaneALocation` / `B` / `HScrollLocation` / `SpriteLocation` / `SetVScroll` / `SetHIntCounter` / `SetHIntEnable` / `SetPlaneSize` / `SetSpriteBuffer`, `VDPView`, `video_second_view`, the `VRAM_FG` ... `VRAM_HSCROLL` addresses and the address space of stage 1 (`VDP_Space`). In their place: `screen1p.vsram`, `.hint_counter`, `.sprites`, `Viewport_SetSize`, `Viewport_UseOwnPlanes`, `Viewport_UploadHScroll`, and for planes `Plane_Put` / `_Fill` / `_Clear` / `_Seek` + `_Write`, `Plane_UseTiles` (a plane kept among the tiles: the special stage's, as before). Every function that drew into a plane by address takes a `plane_t *` now (`DrawBlock` and the `DrawBlocks_*`, `DrawChunks`, `DrawBG_*`, `LevelPlane`, `CopyTilemap`; `MAP_PLANE(x, y)` is a byte offset in a plane), and the tile remap of the earlier entry works by plane. Converted: the engine's Video and LevelPlane, both games' LevelDraw, scrolling, level, title, Sega, ending, continue, special stage and SSRG screens, the split screen, and the tests (`test_vdpsplit`, `test_levelplane` use viewports and a plane of their own). The SSRG's one write to $C704, which went into the tile space (a name table address of the Genesis that was never moved), is kept as it was, through `Plane_UseTiles`. Both games' tests pass, and frames of the Sonic 2 split screen, its title and Green Hill look as before. Not yet looked at in play: the special stage, the ending, the SSRG screen and the level selects. Still to do from the plan: the entries by bank and index (stage 3) and the window's drawing; the sprite buffers are still the games' own arrays pointed at by the viewports.

- **VDP memory split, stage 1: the name tables and scroll tables are spaces of their own** (the user: implement the separate address spaces, each its own C symbol, with the main VRAM only for tiles): `vdp_vram` is the 64 KB tile space only now (`VRAM_SIZE` $10000: it was $20000 with the nametables above), and the first view's foreground and background name tables, the second view's, and the two views' horizontal scroll tables are six arrays of their own (`vdp_nametable_fg`, `_bg`, `_fg_p2`, `_bg_p2`, `vdp_hscroll_p1`, `_p2`, $2000 each), named by the addresses the games already use (`VRAM_FG` ... `VRAM_HSCROLL`, all above $10000; `VRAM_HSCROLL` moved from $FC00, inside the tile space, to $1A000) through `VDP_Space`, which gives the memory of an address and how much is left in its space. A write that would run past the end of a space is dropped and no longer lands in the next (the planes could be written over by tile art, and the other way round). `VDP_ClearVRAM` clears all of them. The picture is the same (a Hill Top split frame compared) and both games' tests pass. The special stage keeps its planes in the tile space, as before. Next, as the user's design says: `plane_t` (rows of entries, dimensions from the viewport's size plus a margin) and `viewport_t` (size, planes A and B and the window, its sprite table, its `vsram_t`, interrupt counters) with a `screen1p` and a second one, the games' plane drawing taking planes in place of addresses, and then the name table entries referring to their tiles by bank and index instead of 11 bits (banks freed whole, checked by generation). The Window plane is not emulated by the VDP at all yet (no registers, no drawing); it would be a plane_t of the viewport that stays empty until it is.

- **Hill Top's mountains in the split screen, a set of tiles for each view** (the user: the dynamic background does not draw in the split screen, in either view): the mountains are tiles made every frame from the camera ($500-$51F: six chunks chosen by the camera's step, and a strip of eight with each row shifted by its layer's scroll). The prototype makes them for one camera only and its two player scroll has no mountains, so a split screen drew none; and its layer scroll (`htz_layerdef`, with a drift that grows 4 a frame) was one set for the two views' deformation passes, which would have run the drift twice as fast. Now `htz_layerdef` and the chunk step are per view, `HTZBackground_Animate` makes one set of 32 tiles for the first view's camera at $A000 and, in a split screen, one for the second's at $A800 (tile $540; free in Hill Top's VRAM: checked by reading VRAM, free from $520 to $67F), and the second view's background plane has its tiles $500-$51F written as $540-$55F: a new engine facility, `DrawTileRemap_Set` / `_Clear` (`LevelDrawCore`), that moves a plane's tiles in a range to other patterns as they are written (set in `SplitScreen_LoadLevel` for Hill Top, cleared with the split). Seen: both views draw the mountains. The user's plan: split the CRAM, VRAM, planes and VSRAM into independent spaces for each purpose when it is needed or once Sonic & Knuckles is in, whichever is first (the tile remap is the bridge until then).

- **Split screen: the Hill Top quake belongs to the view whose camera starts it** (the user's test of the second player in the earthquake: Tails reaches the zone and nothing happens, and he is stuck): the quake's events followed the first camera only, so with Tails alone in a stretch they never started, and the ground's solid blocks (object 30) stayed at their layout height, which is what held Tails. Now the first camera's events run as before and the second camera's run in its own deformation pass (`HTZQuake_EventsP2`, with the second camera and its background swapped in): whichever starts a quake owns it (`quake_owner`) until it ends, the other camera's events leave it alone, and the ground (`htz_bg_y_offset`, which the blocks follow) is the one ground. The owner's view runs the shaking branch of the background; the other view runs its usual background and shakes with the ground (`HTZQuake_ShakeView`, for either view; it was the second's alone). Sprites follow per view (`SpritesFollow(view, ...)`), as the first's and second's. Tests: the second camera starting the quake (and the first's events leaving it be) and the first's being kept from the second's. Known: the blocks of act 2 that decide by the camera's height whether they exist (kinds 6 and 8) still look at the first camera when they are made. Not played yet.

- **The sprite x wrap: the real cause** (the user: the split screen drew the wide boxes right, and was running 8:5): `DrawPieces` (engine, `Sprites.c`) masked a sprite's x to the VDP's 9 bits under `#if (SCREEN_WIDTH <= 320)`, but `SCREEN_WIDTH` is the variable `screen_width`, not a constant: the preprocessor reads an unknown name as 0, so the test was always true and every picture, widescreen too, wrapped sprites at 512. A child sprite 496 or more pixels to the right of the camera (the far corner of a $100 wide box as it comes into a full width view) came in at the left edge; the split screen's views are half as wide, so their children never got that far, and the smaller boxes' never did either. It is a runtime test now (`if (SCREEN_WIDTH <= 320)`): a picture of 320 or less wraps as the hardware does, a wider one does not. The child distance check added before stays (it still matters at 320). Both games' tests pass; the ghost has not been looked at in play since.

- **No ghosts of the wide boxes' corners at the screen's sides** (the user: a ghost of a wide hurt box on the left and right as it enters the screen in one player mode, never for the smaller ones, and not in split screen): the "?" corners are child sprites, and a child a few hundred pixels from the view, such as the far corner of a $100 wide box as it comes in, was written to the VDP's sprite table with a position of 9 bits, so it came round the other side of the picture (a child $140 from the left edge showed up at the left, one $140 to the left of it at the right). Sonic 2's own multi-draw has no check (its children are close together). `DrawMultiSprite` (engine, `Sprites.c`) now skips a child more than 64 pixels outside the view (its width, or the picture's height; the vertical wraps like the original's). The smaller boxes never had it as their corners are never that far out, and the split screen's views are narrower, so their cull caught them. Both games' tests pass; not seen in the picture yet. Not a bug of the boxes themselves: the extra "boxes" seen beside the real ones may well have been these ghosts, and the earlier theory of a cancelled test box needs another look with this fixed.

- **"?" at the corners of the invisible boxes, with the debug cheat on** (the user: an icon in each corner of the bounding box of the invisible item, only with the debug cheat or in a debug build; Sonic 1's lava tag already does it with the Eggman icon): `DebugMarkers_Show` (`Object/DebugMarkers.c`) gives an object four child sprites, the "?" of the monitor art (tile 52, the icon of monitor subtype 8) inside each corner of its box (the icon's outer corner at the box's corner, 16 across). Used by the Hill Top lava boxes (31) and the Neo Green Hill leaves (2C), by the size their collision type touches ($16, $14, $15: $20, $40 or $80 by $20 half sizes), and by the earthquake's solid blocks (30) by their own; and the debug list's previews of 31 and 2C show the same corners. The earlier "?" for the objects with nothing to show (the placeholders) is the same mapping. Found by rendering the monitor's art as an image: its icons run (by mapping frame) static, Tails, Eggman, rings, shoes, shield, invincibility, "?", spring; so subtype 8 is the "?" (the final's versus monitor) and 9 the spring (the final's teleporter), the other way round from what the earlier entry and comments said (fixed). Path swappers already show as rows and columns of rings with the cheat on (and play the lamppost sound when they switch); the user's idea of the same for other objects that set and unset a state with no bounds (the pinball mode switch of Casino Night when it is built) is to be followed. Test: `DebugMarkers_TheCornersOfABoxAreMarkedOnlyWithTheCheat`.

- **Debug lists: a "?" for what has no art** (the user: the hurt blocks need their debug art, Sonic 2 uses a "?" icon, and use it for the list's artwork): the lava boxes of Hill Top (31) and the leaves of Neo Green Hill (2C), which the prototype draws with an empty mapping, and the placeholders for objects not built yet (3D, 73, 23, 2B), now show the "?" of the monitor art (the icon of the monitor that does nothing, tile 56 from the monitors' own) in the list (`Mappings_DebugUnknown`, three identical frames so each subtype's frame still picks one). The placeholders are still counted and not placeable. Confirmed in play by the user: the Hill Top fireball works, and the CPU Tails in the split screen works. Not seen in debug mode yet.

- **`true_two_player`: a second human, independent of the split screen** (the user: the way to get websocket multiplayer and local multiplayer both working later): `Game.h` has the variable (shared, `Game.c`; Sonic 1 never reads it), documented as a second human playing the second character on this machine (pad 2) or another (a websocket client), whatever the screen shows: not the split screen (`two_player_mode` / `camera_split`) and not the CPU Tails, any combination meant to work. Nothing is connected to it yet; the plan is that the second player's input (`jpad2_*`) comes from the local pad or the network by it and the CPU Tails stays away while it is set. The CPU Tails itself runs in the split screen now (the entry before this).

- **The CPU Tails runs in the split screen, as in the prototype** (the user remembered it: the Simon Wai prototype has that code active in split screen, and checked in its disassembly): `Obj02_Control` calls `TailsCPU_Control` with no check of the mode, so while pad 2 is idle Tails copies Sonic's pad from 16 frames back (`TailsCPU_Normal`), and a pad 2 that is held takes him over for 5 seconds. The port's `Tails_Control` returned at once in a split screen; it does not now. Needed for the planned split-screen campaign egg (pad 2 plays Tails, the CPU takes over when it is idle). No new test: the control is a static function of `Tails.c` that runs inside Tails' update.

- **Split screen: the level's events and the end of the level are per view** (the user's earlier note: the events set limits for both views): the level's events follow the first camera and set limits (left limit, bottom, `lock_screen`, a boss) for a level that a split screen has two views of. Emerald Hill's act 2 boss arena is left out of a split screen (the versus levels have no boss; it moved the first view's limits, the bottom limit of both and spawned a boss out of sight of the second). The end of the level (`SignpostArtLoad`: when the camera reaches the sign's stretch, `limit_left2 = end_x`) held the first view only already, but the second camera was never held by it and the art was loaded by the first view alone; `SplitScreen_ReachEnd` holds each view's camera on its own (the second's limit is `left_limit_p2`) and loads the sign's art once, whichever view gets there first. Test `Split_TheEndOfTheLevelHoldsOnlyTheViewThatReachedIt`. Hidden Palace's act 2 (the bottom limit by the first camera and Sonic's y) is not a versus level and stays as it is.

- **Split screen: the second view shakes with Hill Top's quake, and the second player's Start pauses** (the user): the second view ran the first's quake branch of the deformation with its own camera, which gave its lines a shake but not its foreground or its sprites, and left the first view's sprites following the second camera while a quake ran (its `sprite_cam` was overwritten by the second pass). The second pass now has a branch of its own (`Deform_HTZ` while `deforming_p2`): the usual background for its camera, then `HTZQuake_ShakeP2` shakes it by the same table and frame as the first (background Y, both scroll lines, the foreground Y that `SplitScreen_Scroll` adds `htz_p2_shake_y` to, and the second view's sprite layers, restored to its camera by `HTZQuake_Reset`), and the first view's state is no longer touched. The prototype's two player scroll never shakes: this is the user's wish, not the prototype's. The quake's events still follow the first camera. The loc_FBF4 release the notes listed as not ported was ported already (it is in `Obj_HTZQuakeBlock`). Pause: `PauseGame` (shared, `GM_Level.c`) takes Start from pad 2 as well when a game sets `pause_pad2` (Sonic 2 does in `main`; Sonic 1 never reads pad 2); the raw press is kept in `jpad2_press_raw` because Tails' pad copy overwrites `jpad2_press`. Test: `HTZQuake_TheSecondViewShakesWithTheGround`.

- **Debug mode: the prototype's lists for Sonic 2, and the HUD as in Sonic & Knuckles** (the user: get the debug stuff done; use the advanced debug code from before, and in both games make the HUD look normal until Sonic is replaced by an object, and normal again when he is back): Sonic 2 has its own `DebugList.c` (Sonic 1's is excepted in `paradoxmakefile.yml`) with the prototype's `Debug_*` tables, one for each zone slot (Emerald Hill 18 entries, Metropolis 28 for both its slots, Hill Top 25, Hidden Palace 8, Oil Ocean 16, Dust Hill 16, Casino Night 2, Chemical Plant 19, Neo Green Hill 17, and the ring and monitor for every other slot, as `Debug_Null`), the same objects, mappings, art, subtypes and frames in the same order; the unused entries the prototype leaves after some lists' ends are not in them. The preview machinery of Sonic.c is the one Sonic 1 uses: the ring's subtype previews as the formation it will make (built from `ring_pos`), and the monitor's as the box with its icon (Nick Arcade's numbering: 1 Sonic, 2 Tails, 4 rings, 5 shoes, 6 shield, 7 invincibility). Objects not built yet (3D, 73, 23, 2B) are placeholders that count but cannot be placed. The lava boxes and leaves (31, 2C) use an empty three frame mapping, as the prototype's is. Test: each list's length and that everything it places runs real code. The HUD (`HUD_Update` in both games) used the cheat flag (`debug_mode`) to show the camera and object positions in hex all the time; it now keys off `debug_use` (Sonic being an object): the HUD is normal until then, the positions show while he is an object, and on the way back the score, time and rings are redrawn at once (a stopped clock would otherwise have kept the hex digits). Not played through: debug mode needs a keypress that the headless frame grabs cannot make.

- **Metropolis: its art lists and objects** (the user: MTZ is indeed next; the debug lists once every zone's objects are in): the prototype's `Metropolis_Sprites_1` (teleporter $6780, wheels $6F00 and $7E00, lava cup $7F20, bolt and rope $7FA0, steam $80A0, spiked block $8280, spike $8380) and `_2` (button $8480, spikes $8680, diagonal spring $8780, up spring $8B80, side spring $8E00, the blocks $A000, lava bubble $A6C0, platform $A7E0, cog $ABE0): `PlcId_MTZ` / `PlcId_MTZ2`, twelve new arts and eleven mappings copied from the prototype. The layouts of both zone slots (`level_obj`: MTZ1 to MTZ3) are in and `ZoneId_MTZ` and `ZoneId_MTZ3` are in the object coverage test with nothing pending. New objects, in `MTZObjects.c`: 42 the steam vent (sinks and springs up every $80 frames, throws up at $600, lets out steam that hurts), 64 the pistons, 65 the sliding platform (eight modes, the cog that turns beside it), 66 the spring in a wall ($800 each way), 68 the block with a spiked arrow, 69 the screw nut (spin it up and down: a state for each character), 6D the harpoon; and in `MTZObjects2.c`: 67 the teleporter (lifts the character a few pixels, then carries him along one of sixteen paths at $10 pixels a frame along the longer axis, backwards with a negative subtype, keeping his speed with bit 4; a state for each character), 6C the platforms that go round a path of ten points (exactly: the shorter axis's speed is a fraction of 256 and the remainder of the division is where in the pixel it starts, as in the prototype; subtype bit 7 makes a ring of eight; they are gone for good once out of sight, as the prototype's are: its delete does not forget them), 6E the machinery (a square, by the oscillators; flip and mirror), 6F the parallelogram elevators (a very wide slab with a sloping top and bottom that waits to be stood on, then goes along a path of steps for ever), 70 the gears (eight, a step round the ring every 16 frames, solid, the size of their frame), 72 the conveyor belts (invisible: two pixels a frame to whoever is on the ground in the strip). The objects that Metropolis shares got its variants: 6A the boxes (waits until whoever stood on it steps off, then goes one step of its path of four and waits again), 6B the block (the slide platform's mappings outside Chemical Plant), 2D the barrier (its art and width), 71 the lava bubble (the Hidden Palace decor object had deleted it). New in the solid code: `Solid_CharacterDouble` (the prototype's DoubleSlopedSolid: a table gives both how high the top is and how thick the object is, for each two pixels; including its stand-on column for a flipped object, which adds half the width where the lookup adds all of it). `CharThrow.h` holds the tumble the springs and vents share (Oil Ocean uses it). The Dust Hill object's check is the zone's real id now (`ZoneId_MCZ`: DHZ's slot; the name in the earlier code did not exist). Tests (`test_mtzobjects.c`, 23: the teleporter forwards, backwards, long and refusing a busy character, the ring of platforms, a platform visiting every point exactly and carrying who stands on it, the machine's oscillators, flip and mirror, the elevator's mode, path and slope, the box that waits, the gears, the conveyor). Seen: Metropolis renders with the new objects in its first screen; not played through. Debug lists for every zone are next.

- **The render flags, swept in both games against the originals** (the user: if it is ORed, OR it in the code; sweep both games): the original sets an object's render flags in one of two ways in its init: `ori.b #sprite_cam_field,obRender(a0)` (the flips the layout gave it stay) or `move.b #sprite_cam_field,obRender(a0)` (they are cleared). The ports mostly set the level flag (`render.f.level_fg = true`, which is an OR without saying so) after a `render.b = 0` (an assignment: wrong for the ORs) or not clearing at all (wrong for the assignments). Every object whose original ORs now says so (`render.b |= SPRITE_CAM_FIELD`, a new engine constant: the original's `sprite_cam_field`): in Sonic 1 the path swapper, splash, flap door, harpoon, collapsing ledge and floor, scenery, small door, jaws, burrobot, spikes, spring, GHZ edge, giant ring and flash, fan, seesaw, bomb (it, the shrapnel and fuse), orbinaut, gargoyle, LZ conveyor and waterfall, spin platform, stomper, vanishing platform, flamethrower, electrocuter, spin conveyor, girder, invisible barrier, hidden bonus, boss spike ball and fragments (the Caterkiller keeps its `andi` then `ori`); in Sonic 2 the same of the prototype's: path swapper, splash, pipe tipper, floating platform, seesaw, lift, fireball, arrow shooter, booster, barrier, invisible block, scenery and HPZ decor, ledge, spikes, spring, diagonal spring, Buzzer, Snail, Batbot, boss, tube spring, and the Dust Hill and Oil Ocean ones. Those whose original assigns do (`render.b = SPRITE_CAM_FIELD`): in Sonic 1 the lava ball maker, swinging platform and its links, water surface, switch, ball hog and cannonball, prison capsule, waterfall, big spiked ball, elevator, circling platform, running disc, saw and the special stage entry; in Sonic 2 the CPZ elevator, block and tube cover, the HPZ emerald and waterfall, the Masher and Redz. Found by a static pass and an empirical one (every object's first run with both flips set, in the game's first zone; those that animate on their first run take their flips from their status either way and are left out). The Sonic 1 differences were mostly invisible (the objects that animate re-derive their flips from their status the next frame, and the ones that are not animated were not placed flipped), which is why the verified Sonic 1 never showed them; the Sonic 2 ones with flipped placements (spikes, springs, platforms, tubes) are where it mattered. Tests `test_renderflags.c` in both programs (24 + 64 objects in Sonic 1, 23 + 38 in Sonic 2: the first kind must keep both flips, the second clear them; checked to fail with the old code put back).

- **The collapsing platform (and the new objects that OR the level flag in) keep the facing of their layout** (the user: the collapsible ledge of Oil Ocean ignored which way it faced): the objects manager gives an object the placement's flip in `render.b`, and the prototype's init does `ori.b #4,1(a0)`, which keeps it; the port's inits for 1F, the stomper (2A), the boxes (6A), the spiked platform (76), the drawbridge (77) and the push spring (45) assigned `render.b = 0` and lost it. They only set the level flag now (the objects whose init is `move.b #4,1(a0)` in the prototype, the switch, chain, launcher, balls, spring ball and swing, still clear it, as it does). The platform's pieces take the facing (`FragmentatePlatform` copies the render flags), so a mirrored platform crumbles mirrored. Test `DHZ_TheCollapsingPlatformKeepsItsFacing...` for both zones.

- **Oil Ocean: its art lists and objects** (the user: OOZ next; the debug lists once every zone's objects are in): the prototype's `Oil_Ocean_Sprites_1` and `_2` (the rising platform $6000, spiked ball $6180, burner lid $6580, striped blocks $6640, the two oilfalls $66C0 and $68C0; the ball on a spring $6A80, transporter ball $6D00, collapsing platform $73A0, push spring $78A0, swing platform $7C60, button $8480, and the spikes and springs), `PlcId_OOZ` / `PlcId_OOZ2`, twelve new Nemesis arts copied from the prototype. The layout (`level_obj`) is in and `ZoneId_OOZ` is in the object coverage test with nothing pending: 07 the oil (`Obj_OilSurface`; the level makes it in slot $1E, as the prototype does where a water zone has its surface: a platform at $758 whose top sinks a pixel a frame under whoever stands on it, $30 deep, and which suffocates them at the bottom; it comes back up as slowly), 33 the launching platform (subtype 0 hops every $78 frames and settles; otherwise it waits for whoever stands within $10 of its middle, holds them, rises $7D pixels (exactly: the prototype's sum hits it on the 16th frame) and throws them up and away: $800 along, $1000 up, rolling), 43 the spiked balls (one, or a pair that turn round where they touch; limits by subtype 0, 6 and $C), 45 the push spring (a spring that presses nine steps while stood on and throws at $1000 or $A00; with subtype bit 4 a pusher that is pushed along $12 pixels and flings whoever pushed it back, by how far), 46 the ball on a spring (springs off when player 2 presses A or its switch is down, falls, and rolls along the floor; it spawns once: the prototype's remember mark), 48 the cannon (catches a character within $10, rolls it up, aims the barrel, fires it at $1000 in the direction of the subtype and lets it fly by itself until another cannon takes it; bit 7 lets it go free at once; Tails has a state of his own). Already shared: 1F (the collapsing platform), 47 (the switch), 15 (the swing: Oil Ocean's art now) and 19 (the elevator: art at $300). No events (the prototype's are empty), the palette cycle and background were done. Tests (`test_oozobjects.c`, 16). Seen: Oil Ocean renders, the oil is stood on and Tails sinks in it; the objects were not played through. Known differences from the prototype: the pusher's penetration is the character's x moved by the solid (the prototype's own d0 is the same unless the character is at the vertical edge of it, where it is not moved). Metropolis (about 16 kinds of object) is the last built zone left; then the debug lists.

- **Dust Hill's swinging platforms (object 15) are the prototype's own, a Newton's cradle** (the user: the three side by side should behave like one, "the other swings down and stops"): Dust Hill and Oil Ocean ran Nick Arcade's swing; they run the prototype's (the one Neo Green Hill had, `Obj_NGHZSwing`, now generalised: `Obj_SwingDispatch` picks it for those three zones): Oil Ocean's art (tile $3E3, mappings `OOZSwing`) and Dust Hill's (`DHZSwing`, `DHZSwingSpiked`, built from the prototype's inline mappings), and the subtype's bits 4-6: 1 swings between hanging and the left, 3 between the right and hanging, 2 hangs still, so three side by side ($18, $28, $38) alternate on the one oscillator like a Newton's cradle; 4 is the spiked platform (hurts, `col_type` $A7): it waits until Sonic is within $20 pixels, then swings between the left and hanging with a speed that grows by 8 a frame, pausing $3C frames at each end. With bit 7 it lets go when stood on at the bottom and, outside Neo Green Hill, falls to $720 and bobs there. Test `DHZ_TheCradle...`; seen in game. The final game has a wood block sound on the cradle that the prototype lacks (a memory note: for later). Also: the `SONIC_QT_PLAY` test hook takes a start x and y (decimal), and Dust Hill's spiked platform with its flashing 0x76 art question is open.

- **Dust Hill, stage 2: its objects** (the user: back to the incomplete zones): the prototype's Dust Hill layout is a small test area (19 objects of 9 kinds) and now runs in full (`level_obj` has the layout, `ZoneId_MCZ` is in the object coverage test with nothing pending). New objects: 1F the collapsing platform (`CollapsingPlatform.c`, shared with Oil Ocean: Dust Hill's 6 pieces and $20 half width, Oil Ocean's 7 and $40; flat top, breaks a moment after being stood on through `FragmentatePlatform` like the ledge, with the prototype's delays), and in `DHZObjects.c`: 2A the stomper (rises a pixel a frame for $60, drops 8 a frame), 47 the switch (the subtype's low nibble picks a byte of `f_switch`, bit 6 the bit 0 or 7; the sound when the byte was empty), 6A the boxes (a path of speed and time steps, `$18` makes two more with it at $40 right and left and below, a step ahead and a step behind; the prototype's first frame only sets the step up and its boxes that are not `$18` or made by one have no x to be forgotten by and so vanish: kept), 75 the spiked ball on a chain (circles its pivot at the subtype's high nibble x 8, through as many 16 pixel links as the low nibble; $F is a solid block; the flip bits give the starting angle; the chain is an object of its own that draws the links as child sprites, as the bridge's logs are), 76 the platform with spikes at its sides (slides $80 pixels away when a grounded character comes within reach; hurts whoever pushes at its sides: `Spikes_HurtCharacter` is the spikes' Touch_ChkHurt2, shared) and 77 the drawbridge (lowers at the start; the first time its switch is down it raises, once; flat it is a platform). Art lists done in the stage before. The prototype's mapping files are copied as they are (`res/Mappings/...`). Tests (`test_dhzobjects.c`, 17): breaking and falling of the platform in both zones, the stomper's cycle, the switch and its bit, the drawbridge, the boxes and their two followers, the chain's links, flip and turning, the solid block, the spiked platform's trigger and slide (and flipped). Seen: Dust Hill renders, chains, crates, boxes and bridge in sight; not played through. Oil Ocean shares 1F and 47 and lacks 33, 43, 45, 46 and 48; Metropolis lacks about 16.

- **Dust Hill, stage 1: art lists** (the user: back to the incomplete zones, DHZ first as it has the fewest objects, 7 kinds the port lacks: 1F, 2A, 47, 6A, 75, 76, 77; OOZ lacks 1F, 33, 43, 45, 46, 47, 48 and MTZ about 16): Dust Hill's lists are the prototype's `Dust_Hill_Sprites_1` (the crate $7A80, the collapsing platform $7E80, the pull switch $81C0, the vine pulley $83C0 and the zone's own art) and `_2` (the long horizontal spike $8580, spikes $8680, the drawbridge logs $8780, the lever spring $8800, the up spring $8B80, the side spring $8E00): `PlcId_DHZ` / `PlcId_DHZ2`, new ids that Sonic 1's `PLC.h` lets a game add (`PLC_IDS_EXTRA` and `src/PlcIdsExtra.h`, as palettes and sounds have their own hooks). Six new Nemesis arts copied from the prototype (`res/Art/DHZCrate`, `DHZCollapsePlat`, `DHZVineSwitch`, `DHZVinePulley`, `DHZGateLog`, `HorizSpike`). Already done and checked against the prototype's code: the zone's background deformation and palette cycle (named `ZoneId_MCZ` in the code: 0x0B), start positions and level sizes ($3C0 top in act 1, $60 in act 2). The zone's objects are next (the layout's `level_obj` stays null until they are in).

- **8bpp palette cycles** (the user's point: with 16 lines, an 8bpp pixel's byte is a direct index into the whole colour RAM, so cycling a range of it recolours an 8bpp tileset with no pixel upload): the handler reads colours from the colour RAM as it draws, so this works as it is; a test pins it (a tile of colours $C1 to $C4, line 12, drawn while the ramp rotates). One thing for when the planes and sprites get an 8bpp mode: the per-frame refresh (`VDP_RefreshPalette`) converts only the active lines (`ACTIVE_COLOURS`), so it must cover the lines the 8bpp pixels reach.

- **An 8bpp tile row handler in the VDP, not used yet** (the user's request, for later custom Sonic games on the engine): `VDP_DrawTileRow8` (`Backend/VDP.h`) draws one row of an 8 bits a pixel tile: 64 byte tiles (patterns numbered in 64 byte steps), a byte a pixel that is the colour RAM index over all 256 colours (0 transparent), x and y flips, and the 4bpp rows' priority rule (a pixel is not drawn if the mask has a bit of `and`; `or` goes into the mask of every opaque pixel). It reads true colour straight from the colour RAM (so it reaches all 16 lines). The planes and sprites do not call it: their tile format has no bit to say a pattern is 8bpp, which the later tile format will add, along with raising `VDP_PALETTES_ACTIVE`; the 4bpp paths are unchanged. Tests (`Sonic The Hedgehog/tests/test_vdp8bpp.c`, 5, in both programs): the rows of a tile, the flips, transparency and colour 255, the priority masks, and tiles 64 bytes apart.

- **The VDP's colour RAM has 16 palette lines** (the user's request: core engine feature, wanted long after Sonic 2, for much better levels in later Paradox Engine games): `VDP_PALETTES` (16) lines of storage (`COLOURS` is 256 now) and `VDP_PALETTES_ACTIVE` (4), the lines the picture is drawn with so far: a tile's or sprite's palette field is 2 bits, so lines 4 to 15 can be written (`VDP_WriteCRAM`, `VDP_WriteCRAM_RGB`, `VDP_FillCRAM`) and read (`VDP_PeekCRAM`, `VDP_PeekColour`) but not drawn with until a game widens the tile format and raises `VDP_PALETTES_ACTIVE`. Only the active lines are refreshed to screen colours each frame, so the cost is unchanged; the video setup now clears all 256. Both games render the same (a Hidden Palace frame compared pixel for pixel before and after). Test: the 16 lines each keep their own colour, the first four are untouched by writes to the rest, and the very last colour is writable.

- **The VDP's colour RAM is true colour** (the user's request, for the engine to carry games with more than the machine's 9 bits, Sonic Mania 2 among them; the games' own palettes stay 12 bit for now, VDP only): `vdp_cram` holds `0x00RRGGBB`; `VDP_WriteCRAM` / `VDP_FillCRAM` take the machine's words (`0000bbb0ggg0rrr0`) and upscale them as they go in (`VDP_Genesis2RGB`, by the DAC's own levels, 0, 52, 87, 116, 144, 172, 206, 255, which the VDP already used for output, so the picture of the two games is unchanged: checked on a Hidden Palace frame), and `VDP_WriteCRAM_RGB` takes true colour as it is. `VDP_PeekCRAM` gives the machine's word back (`VDP_RGB2Genesis`: the nearest level of each channel), and `VDP_PeekColour` the true colour. The palette loader's 24 bit conversion uses the same nearest-level rule now (it had rounded to even levels, which the DAC does not have): `Palette_FromRGB24(128,128,128)` is $0666, not $0888. The split-screen water views' dry and wet palettes (and the second view's) stay words, converted when drawn: true colour versions of those are for a game that needs them (the user). Tests: the upscale, all 512 colours round-tripping through true colour, and the colour RAM taking words, true colour and fills.

- **The palette loader reads three formats** (the user's request, to prepare for later): `PalettePointer` has a `format` (`EnginePalette.h`): `PAL_FORMAT_GENESIS` (the default, 0: the Mega Drive's big-endian words, `0000bbb0ggg0rrr0`), `PAL_FORMAT_RGB24` (three bytes a colour) and `PAL_FORMAT_0RGB32` (four bytes, big-endian `00rrggbb`, the top byte ignored). All four loaders (`PalLoad1..4_Water`) share one `LoadColours`; the 24 bit colours are brought down to the machine's 3 bits a channel by `Palette_FromRGB24` (each channel rounded to the nearest of 8 levels, so the machine's own levels round-trip exactly) because the palette lines and the fades are 12 bit for now: wider lines are the later step this prepares. The games' palette tables say `PAL_FORMAT_GENESIS` on every row (`palette` is a `const void *` now). Tests (`Sonic The Hedgehog/tests/test_palette.c`, 9, in both programs): the conversion and its rounding, the machine's levels round-tripping, each format loading, an entry with no format being a Genesis one, the three formats agreeing for one colour, and every loader taking them.

- **The engine has a null oscillator table** (the user's request): `oscillator_data` is declared in `Oscillator.h` and defined as a weak, all-zero `OscillatorData` in `NullServices.c` (with the other null services), so a game with no oscillators links and runs, and a game that defines its own (Sonic 1 and 2 do) wins at link. Checked: Sonic 2's tests still see the prototype's values, so its strong definition is the one in use.

- **The oscillators are in the engine** (the user's request): `ParadoxEngine/src/Oscillator.c/.h` has the `Oscillatory` state (and `oscillatory`, no longer defined by each game's Level.c), the `OscillatorData` table type (control word, start values, rate of change and amplitude each) and `Oscillator_Init` / `Oscillator_Step`, which take the table. The games keep `OscillateNumInit` / `OscillateNumDo` (`Oscillatory Routines.c`, two small wrappers) with the one rule that is theirs, that nothing moves while the player is dying (routine 6 and up), and each defines its `oscillator_data` (`Sonic1OscillatorData.c`, `Sonic2OscillatorData.c`). Level.h includes the engine's header, so the games' code reads `oscillatory` as before. Both test programs pass unchanged.

- **The oscillator routines are shared, only the tables differ** (the user's idea, after a diff showed the two files were identical but for the tables): `Oscillatory Routines.c` (Sonic 1's, reused by Sonic 2) reads `oscillator_data` (control word, start values, and rate of change / amplitude per oscillator: the `OscillatorData` struct in `Oscillatory Routines.h`) which each game defines: `Sonic1OscillatorData.c`, and `Sonic2OscillatorData.c` (the prototype's; `except:` drops Sonic 1's). Sonic 2's copy of the routines from the previous change is gone. Both test programs pass unchanged. The data is the natural thing to move into the engine with the routines if they ever go there (they use `player` and the `Oscillatory` type of Level.h, which are the game's today).

- **Oscillation and math tests, and Sonic 2's own oscillators** (the user's request): writing the tests against the prototype's `Osc_Data` / `Oscillate_Data2` found that Sonic 2 had been running Sonic 1's oscillator table, which differs from the prototype's in four entries' amplitudes (8 and 9: $38 here, $50 there; 14 and 15: $40, $10) and in the start values: entry 9 starts at $3848 / rate $EE (Sonic 1: $50F0 / $11E), entry 15 at $4000 / $FE (was $80 / 0) and the control word is $7D (entry 15 starts heading down; was $7C). Sonic 2 has its own `src/Oscillatory Routines.c` now (a copy with the prototype's values; the sources list in `paradoxmakefile.yml` names it and `except:` drops Sonic 1's). Whatever reads entries 8, 9, 14 or 15 moves as the prototype's does (the prototype's objects read them at +$20, +$24, +$38 and +$3C). Tests: `tests/test_oscillation.c` (6: the start values, entry 15 going down at once, a thousand frames against the prototype's routine written out, the heights each entry reaches, nothing moves while Sonic is dying (routine 6 and up), every entry turns round) and `Sonic The Hedgehog/tests/test_math.c` (16, in both games' test programs: the sine at the quarter turns and known values, cosine a quarter on, the first quarter rising, the table's odd values kept (the sine at $13 is $73 but at $93 it is -$75, and $6D/$ED: the original's own table, which the prototype has byte for byte; checked here against its `Sine_Data` and `AngleData` and they are identical), the circle's radius to 1%, `CalcAngle` at the origin, axes, diagonals, scale and round trip through the sine, and `RandomNumber` against the prototype's `PseudoRandomNumber` written out in 68000 steps for 200 draws plus the zero seed). Sonic 1 had two oscillator tests already (the flip on equal).

- **Scroll block tests** (`Sonic the Hedgehog 2/tests/test_scrollblock.c`, 14 tests; the user's request): the prototype's Scroll_Block1..4 (`BGScroll_Block1/2/3`) and the shared `UpdateBGScroll`: a line between 16 pixel blocks crossed raises the flag of the direction (the caller's bit for left, the next for right: D6 and D6 + 1), a move inside a block or no move raises nothing, one pixel steps cross once at the 16th, fractions accumulate, flags are ORed, the three blocks have their own position, tracker and flags, and `BGScroll_XY/Y/YRelative` use their rows of flag bits (0/1 and 2/3, 4/5, and 0/1 for y). Note the two trackers are opposite: Block1..3 (like the original's) toggle when the position's bit *is* the tracker's, `UpdateBGScroll` when it is not; a sweep test shows they raise the same flags on the same frames when started opposite, and the only difference is that the original's spurious first flag (tracker equal at the start) is not raised by `UpdateBGScroll`.

- **Camera, sign lock and split screen tests** (`Sonic the Hedgehog 2/tests/test_camera.c`, 19 tests; the user's request): the first camera (dead zone of 16 right of the following place, at most 16 a frame, falls back with Sonic, the left and right limits; vertically: stays, snaps within 4 pixels on the ground, capped by the top and bottom limits), the end sign's lock (without a split screen the one camera; the locked camera is held at the end of the level), and the split screen: two-player mode makes two views starting at Sonic, one-player none, the sign locks only the views it is in sight of (first only, second only, neither, both; the edge of the view to the pixel) and the second camera follows Tails past a lock on the first, and the second view's background deformation leaves the first's block trackers alone (checked to fail with the old code put back). Camera tests used the original's behaviour where it is odd: with `SCP_FIX_BUGS` off a Sonic far behind the camera moves it ahead (a 16 pixel step; the original's unsigned compare), so the limits test puts him 20 pixels behind instead.

- **Split screen: backgrounds and the end sign** (the user's reports: Hidden Palace's background was partly broken in the split screen, and the end sign moved both screens in a glitched way): (1) the second view's background deformation (`DeformLayersP2`) swapped the background positions but not the trackers that say which 16 pixel block each background was in (`bg1_xblock` .. `bg3_yblock`, what `UpdateBGScroll` compares to raise "new row/column" flags), so the two views corrupted each other's flags; each view has its own now. (2) The sign set `limit_left2 = limit_right2` for the one shared limit, so the second camera was dragged to the end of the level too: the second camera has its own left limit (`left_limit_p2`, the level's `limit_left1` until locked) and the sign locks only the views it is in sight of (`SplitScreen_LockCameras`). `SONIC_QT_PLAY` takes a fourth field, `split`, to play a demo in the split screen (for screenshots; HPZ was grabbed with `d.bin,8,0,split`). Not done: the dynamic level events (boss arenas, Hidden Palace act 2's and Hill Top's limits) still decide from the first player's camera and set limits for both views; the second view needs its own pass (the user: "DLEs need fixing to work better with split screen"). Not looked at: the sign locking on screen (checked to build and the tests pass only).

- **Wood Zone (WZ) checked against the prototype** (the user's request: it "has nothing"): and it does have almost nothing, as the port already had it: the prototype has layouts and ring layouts for both acts (no object file: `level_obj` stays null, which is right), start position ($60,$1AC), the plain $3FFF x $720 size, the palette ($0248.. cycle, ported), the eighth/quarter background, no water, no events, the Metropolis music. The one difference was the art lists: WZ used Hill Top's. The prototype's `Wood_Sprites_1` is only the zone's own art (nothing to queue: `plc1` 0) and `Wood_Sprites_2` the spikes and the diagonal and up springs at $8680/$8780/$8B80, now `PLC_SLZ2` (Emerald Hill has no second list, so the id was free). Tests for WZ's start, size and lists. Not looked at on screen (no way yet to start a given zone from the command line, and it has no objects, so it is the layout and the cycling palette only).

- **Compression tests for both games** (the user's request): one shared file, `Sonic The Hedgehog/tests/test_compression.c`, built into both `SonicTests` and `Sonic2Tests` (12 tests). Kosinski: hand-built streams (literals and the end token; a short match, whose 2-bit length is +1 and copies one more byte than it says) and round trips through our `KosEnc` for five kinds of input (zeros, ramp, noise, a short period, runs of both) at sizes around the 256 and 8192 match windows and 70000, the empty input, and that a zero page shrinks. Nemesis: a one-tile stream by hand (the palette byte, a run code, the data bits), two codes, the XOR mode (header bit 15: rows alternate with zero), and that the header's tile count sets the length exactly. Enigma: incremental and literal runs, the start tile added, a repeated inline value. The streams are worked out from the formats, not captured from the decoders; all pass at once.

- **Emerald Hill and Hidden Palace audited against the Simon Wai prototype** (the user's request): the Buzzer, Masher, Snail, platforms, waterfall, bridge, level events and every art list of both zones (Emerald Hill's shared art decoded and compared byte for byte, Hidden Palace's tile bases against `Hidden_Palace_Sprites_1/2`) match. Two fixes: Emerald Hill's lava-bubble fireball loads the prototype's own `Fireball.nem` at $79C0 (the Nick Arcade copy `res/Art/EHZFireball` is gone), and the spikes no longer check the character's flash timer before hurting. Hidden Palace's water splash stays Nick Arcade's (the prototype's reads art from the air-bubble list at $49C, which Hidden Palace's list does not load).

- **Object tests for both games**: Sonic 2 (`tests/test_cpzobjects.c`, `test_nghzobjects.c`, `test_htzobjects.c`, 15 tests): the CPZ barrier (rises 8 a frame to $40 while a character is in the strip on its open side, sinks after, ignores the far side; note the strip's left edge is an unsigned x, so a barrier is at least $200 from the level's left), the booster (speeds $1000 / $A00 by subtype, flipped goes left, ignores a character in the air), the staircase block of kind 5 (falls once stood on), the NGHZ arrow shooter (holds its fire while Sonic is near, shoots when he has been near and goes; the arrow flies $400 either way and hurts), the leaves' boxes (col by subtype, four leaves for a fast Sonic at his frame of the sixteen, none for a slow one), the HTZ lift (waits, slides for subtype x 8 frames, stops, leaves a pole, falls; flipped slides left), the valve barrier's and the rock's art and width by zone, and the platform's size field and solid kind. Sonic 1 (`tests/test_spring.c`, `test_spikes.c`, 9 tests): the spring's power by colour, direction by subtype, the up launch when Sonic has landed, the side spring's throw (and flipped), the spikes' size table, hurt on landing, no hurt when invincible, and that moving spikes move. (Including an object's own header in a test defines its art a second time at link: the tests repeat the scratch struct instead.) Many of Sonic 1's other objects still have no tests (the list: `Sonic The Hedgehog/src/Object/*.c` minus what `tests/` names).

- **Hill Top's fireball (object 20) and the quake blocks' last parts** (the user's request): the fireball is the prototype's lava bubble (`Obj_HTZFireball`, mappings `HTZFireball` and `HTZFireFlames`, animation `HTZFireball`, art the lava bubble at $416 and the fire at $39E): it animates, throws two hurting balls (col $8B) one each way at the speed its subtype gives ((subtype << 3) & $780, up and out) when the animation's timer is 5, waits (subtype's low nibble x 16 frames) and starts again; a ball that falls onto the floor burns there (the fire mapping, one animation) and spreads three steps ($E to the side each, 9 frames apart, a copy of itself). No layout places it (only the prototype's debug list has it, subtype $44), so it is spawned by hand in tests; seen as red balls flying out both ways. The quake blocks (object 30): whoever stands on one is let go when the level's own floor is at or under his feet (loc_FBF4's ChkFloorEdge2, `ObjFloorDist` on the character), and the hurt on the lava's top is for both kinds 4 and 6 (it was only 4). Tests for the fireball's speed, wait and the two balls.

- **Hill Top's background, the prototype's** (`HTZBackground.c`; the user's request): the usual branch of `Bg_Scroll_HTz` (loc_6108: the top $80 lines at an eighth of the camera, then bands, 16 layers' scroll in `TempArray_LayerDef` that drift 4 a frame) replaces Nick Arcade's, and the first part of the prototype's Hill Top animated art routine (loc_2244E, loc_22584) is ported: the mountains' tiles. Six chunks of 4 tiles (from `Data/htz/Backgnd.nem`, decompressed once for the level, picked by the camera's step, $30 of them: x/16 - x/8 - $10 mod $30, through the prototype's two tables) are copied to VRAM $A000 when the step changes, and a strip of 8 tiles at $A300 is made every frame from `Backgnd.dat`'s picture (16 rows of 4 pixel lines, each shifted by its own layer's scroll and the camera's eighth, the odd pixels from the picture's second copy at +$200). Hill Top's flowers are the same script as Emerald Hill's in the prototype (checked: the same frames, times and VRAM places), so those were right. Tests for the step, the chunk windows, the strip's rows (even and odd shifts) and the scroll. The title screen leaves its TM in VRAM at $A200, the place of two of the mountains' chunks: the prototype starts its step counter at 0, so a level that starts on step 0 shows that TM in the sky until the camera's step changes; here the counter starts at "none", the chunks are put over it when the level's data loads (`S2_PrimeLevelArt`, called from `LevelDataLoad`, before the title card, for the first frame of every zone's animated art too, without starting their counters so that the animation's phase stays the prototype's; the animated art routine only runs once the level loop does, so the first version showed the TM through the whole title card and fade, a second or two), and the TM never shows (the older builds, which never wrote those tiles, showed it all along). Seen: the layered mountains and haze behind the sky. The see-saw's ball (Obj14_Animate, in the prototype) now alternates its palette line every fourth frame (the flash) and faces Sonic; with this prototype's palette the two lines are yellow and green/orange where the final's are yellow and red.

- **Sonic 2 has unit tests** (`Sonic the Hedgehog 2/tests/`, target `Sonic2Tests` -> `bin/Sonic2Tests`, ctest `unit_tests_sonic2`; Sonic 1's `test.h` and harness, linked against Sonic2Core like Sonic 1's `SonicTests`; built whenever `BUILD_TESTS` is on, Showcase excepted): the prototype's level tables (start positions, level sizes, art lists per zone), object coverage (every id that the built zones EHZ, HTZ, HPZ, CPZ and NGHZ place must run something other than the null object; a `pending` table lists exceptions, and the test fails also when a pending id has been ported, so it is the M2 progress tracker for those zones, the unbuilt ones are not in it yet), `Solid_Character` (standing on a slope is at the table's height: the 16 pixel sink fails it, checked by putting the old bug back; flat top; walking off), Hill Top's earthquake (stretch entry and exit, the ground's rest of $79 calls, the quarter pixel steps, act 2's two starts), the water (which zones and acts have it, start heights, Sonic's underwater row for CPZ and NGHZ), the signpost's art rule (every act but EHZ's second) and Tails' demo in the Emerald Hill attract demo (the prototype's frame counts: the first record lasts its byte, the others byte + 1). All pass.

- **Tools > Countdown, in the Premier builds of both games** (the user's request): the countdown screen with the pie wipe (`GM_Countdown.c`, Sonic 1's) is reachable from the Qt window's Tools menu while debug tools are on: a dialog picks the song (the game's own countdown song or any of its music, listed from the sound table the SMPS Inspector uses) and how many seconds it counts; START skips it and the title screen follows. The engine's `Demo.h` has `Countdown_Available()` / `Countdown_Request(music, seconds)` (weak nulls in `NullServices.c`, so a game without one greys the item out), the game applies the request on the next VBlank that finds a leaveable screen (`Countdown_ServiceRequest`, as demo requests do), and `SONIC_QT_COUNTDOWN="music,seconds"` files one at frame 120 for tests. Sonic 2 has the screen now: a new `SCP_COUNTDOWN` define (set with `SCP_SPLASH` in Sonic 1; alone, from the SPLASH option, in Sonic 2, which has no SSRG splash yet) guards the countdown parts of `Game.h`/`Game.c`/the screen tables, `COUNTDOWN_DEFAULT_MUSIC` is overridable (Sonic 2: Emerald Hill's, id 2) and `GM_Countdown.c` is compiled into Sonic2Core in Premier/Showcase builds. Premier builds: `cmake -S . -B build-premier -DCMAKE_BUILD_TYPE=Premier -DBUILD_TESTS=OFF` (Sonic 1: `bin/Premier/Sonic`) and the same with `-B build-premier-sonic2 -DPARADOX_PROJECT="Sonic the Hedgehog 2"` (`bin/Premier/Sonic2`). Checked: both Premier builds run headless and the request switches both games to the countdown (a black frame under the overlay; the normal run shows SSRG / the title); the pie and the menu click themselves not looked at; the Sonic 1 unit tests pass.

- **Hill Top's earthquake, the prototype's** (`HTZQuake.c`; the user's request): the events (`DynResize_HTz`: act 1's three states and act 2's five, replacing Nick Arcade's boss arenas that the prototype does not have) move the background by diffs of their own while the camera is in a quake's stretch (act 1: x $1800-$1F00 from y $400; act 2: x $14C0-$1B00) and move the ground between two offsets ($E0-$140 in act 1, $0-$2C0 and $0-$300 in act 2) a quarter pixel every fourth frame, resting $78 frames at each end with the screen still before it turns; the screen shakes (the prototype's loc_5F60 table, on both scrolls and, here, on the sprites too: they follow a copy of the camera while the shaking branch runs) while the ground moves. The background scroll has the prototype's shaking branch (`Bg_Scroll_HTz`'s loc_6236: the diffs through the scroll blocks, one value for all the lines), the rest of it is still Nick Arcade's. Object 30 is the ground's solid pieces (invisible; kinds 0 and 2 solid, 4 the lava's top that hurts whoever stands on it, 6 and 8 by the camera's height, 8 with a sloped top; at the ground's offset; forgotten only when no quake's stretch is on). Traced: the quake runs (the offset goes $13B to $E0, rests, comes back; the screen shakes while it moves). Not done: the rest of the prototype's Hill Top background (its parallax mountains and the animated tiles that make them, and the flower script, which is the zone's own: the port still plays Emerald Hill's), and `ChkFloorEdge2`'s release of anyone left on a block that moves away.

- **Hill Top, stage 1: the prototype's art lists and objects** (the user's request: HTZ is next after CNZ; its dynamic level events -- the earthquake, screen shake and background offset, ~300 lines in the prototype -- are stage 2, and object 30 (the quake's solid blocks, which read them) waits for it): Hill Top's lists are the prototype's `Hill_Top_Sprites_1/2` (`PLC_SBZ`/`PLC_SBZ2`: the fireball, rock, see-saw and its badnik, spikes, springs; the zip-line platform, lava bubble and valve barrier; the zones that are not built yet still share the first list as their placeholder), and the see-saw now sits at the prototype's tile ($3C6, was Nick Arcade's $3CE; its art and mappings are byte for byte the same). Objects: 2D is the valve barrier here too (default art $426, width 8; the Chemical Plant one has the stripes), 32 is the rock here (`Obj_TubeCover` picks by zone: art $3B2, width $18, six pieces), 2F the breakable floor (`Obj_HTZBreakFloor`: five heights by subtype, breaks row by row for a roller on the secondary path or, with subtype bit 7, any roller; the character's path is `Game_CollisionPath`, the solid bits the path swappers give him (an earlier version read the level's global `collision_path`, which nothing sets in Sonic 2, so the floors never broke unless subtype bit 7 was set: fixed, with tests)), 31 the lava boxes (`Obj_HTZLavaBox`: an invisible hurt box, col $96/$94/$95). The break helpers (`ObjectBreakToPieces`, `ObjectChainScore`) are shared in `CPZObjects.h`. The see-saw's ball is the Sol badnik: it sits at tile $3DE, over the art the first list loads at $7BC0 (`HTZSpiker`), not over the see-saw's own (this was edited on the wrong line the first time and so still drew the see-saw's tiles, as garbage; fixed, seen as the badnik's yellow face, and tested). The lift (16) is the prototype's own now: the port's only moved for subtype 0 (Nick Arcade's layout), but the prototype's lifts carry how long they slide in the subtype (times eight frames, a flipped one slides left), and then they stop, leave a broken pole (scenery, subtype 6) and fall away with whoever is on them (traced: 160 frames of sliding for subtype $14, then the fall). Seen: the valve barrier's art; not exercised: the floor's breaking, the rock, the lava boxes. Not placed in any layout, so not done: the fireball, lava bubble and see-saw badnik objects.

- **Casino Night checked against the prototype** (the user's request: it is the last time this build of the zone is seen before it starts to change): start positions ($60,$28F / $40,$2AF), the level size ($3FFF x $720, both acts), the background scroll (the camera's move shifted by 6 in x and 2 in y, never redrawn), the title card text and the zone slot's music all match. Two things did not: the zone's art lists were Hill Top's (seesaw and lift art over the turning cards' tiles), now the prototype's `Casino_Night_Sprites_1/2` (the turning cards at $7A00; the spikes, the diagonal spring and the straight springs) as `PLC_LZ`/`PLC_LZ2` (Sonic 1's Labyrinth lists, unused in Sonic 2) named by the zone's header and the importer. The prototype has no object layouts for it (there is no `level/objects/CNZ_*.bin`; its object table points the zone at an empty one), so there is nothing to port; the rings are there. Seen: both acts' scenery and the card blocks.

- **Every act's end signpost loads its art, not just the first acts'** (`SignpostArtLoad`, `Level.c`): Nick Arcade skipped every second act (its act 2 was the boss's), and the port did too, so Chemical Plant's and Neo Green Hill's second acts (and Hill Top's) never loaded the signpost's art. The prototype's `End_Level_Art_Load` only skips Emerald Hill's act 2 (`green_hill_zone_act_2`, the boss's), now the same. Seen: the Eggman board in CPZ 2.

- **The water splash is back, and Chemical Plant and Neo Green Hill have the prototype's own** (`Object/WaterObjects.c`): the third water surface that I moved to object slot 27 for the wide pictures is the slot that Sonic 1's level start gives the splash and the spin dash dust (`objects[0x1B]`), so it had overwritten them in every Sonic 2 level with water -- no splash, no spin dash dust there; it is slot 22 now. The prototype's splash (`Obj_0x08_Water_Splash`) shows three frames (4 ticks each) at the water's height, from the art that the air bubbles' list holds (`airbubls.nem` at $A000, tiles from $49C on: loaded by CPZ's and NGHZ's second lists, `res/Art/AirBubbles`), with its own mappings (`WaterSplashSW`) and animation; Hidden Palace keeps Nick Arcade's. Traced (the object runs through its frames on entering the water), not seen in a screenshot. Sonic 1's splash is the final Sonic 2 one and takes over before the pre-beta build. Neo Green Hill act 1's water is past the level's kill plane until a build after M2, so Sonic dies there at once (the user's note): the water exists, it is just not reachable alive.

- **Neo Green Hill (zone slot $0F, `ZoneId_ARZ`) is the starting level, with water, art lists, objects and the attract demos** : a plain Start on the title begins in Neo Green Hill act 1 (the prototype's title sets `Current_ZoneAndAct` to `neo_green_hill_zone_act_1`; the level select is still A + Start). Its two art lists (`Neo_Green_Hill_Sprites_1/2`: water surface, leaves, arrow shooter, splash; spikes, lever spring, straight springs) are `PLC_GHZ`/`PLC_GHZ2` in `Sonic2PLC.c` (Sonic 1's Green Hill lists, unused in Sonic 2), named by the zone's header (importer `%plc`/`%plc2`); both acts have water ($410 and $510, `PalPtr_NGHZ_U` = `PalId_NGHZWater`, all four lines, Sonic's first included, nothing moves it). Objects: 22 arrow shooter and 2C leaves (`Object/NGHZObjects.c`; the leaves' touch box is marked by the touch response (col $D4-$D6) with bit 0 Sonic, bit 1 Tails in `col_property`), and 18 now has the prototype's size field (three bits) and its solid-block kind (subtype bit 7, y radius $28 here, $30 elsewhere) with Neo Green Hill's own mapping. Attract demos: the prototype's four (CPZ, EHZ, HPZ, HTZ) are Nick Arcade's byte for byte, so only the Emerald Hill one changed: it is the two player demo (split screen, `two_player_mode` set) with Tails playing the prototype's `Demo_Tails_Ghz` on pad 2 (`res/Demo/SWEHZTails`, `Demo.c`). Also Obj0C (the prototype's small floating platform, `Obj_FloatingPlatform`; no level uses it). Also 15, the swinging platform (`Obj_NGHZSwing`: a pendulum on the first oscillator whose chain is an object of its own drawn as sub-sprites; with subtype bit 7 it lets go when stood on at the end of its swing and becomes a raft that falls to the water, floats and drifts right until a wall; the prototype's modes 0-3 only, the spiked-ball mode 4 is not used here), and the zone's object layouts, which were not wired up before (`level_obj`; the importer's `%objects_built` has NGHZ now). **Sonic's underwater row**: the prototype's CPZ and NGHZ underwater palettes cover all four lines and the first is Sonic's (and Tails'), which is the one loaded live (`PalLoad3_Water`) -- only the whole table (`PalLoad4_Water`, the fade reference) was loaded, so Sonic and Tails were black underwater in CPZ 2 (now `PalId_CPZWaterSonic` / `PalId_NGHZWaterSonic`, for Super Sonic's later). Not done: the water splash's art in use. Checked only by screenshots (chains and platforms swing, the level and its objects load); act 2's background is the prototype's own formula (height less $E0, halved) and its sky.

- **Chemical Plant, step 5 (barrier, tube cover, rotating and sliding platforms, invisible block, droplet chain)** (`Object/CPZObjects.c`, mappings `CPZBarrier`, `TubeCover`, `CPZInvisibleBlock`, `CPZWorm`, `CPZSlider` from the prototype's `obj2D`, `obj32_CPZ`, `obj74`, `obj1D`, `obj7A`; 78 reuses the staircase block's mapping): 2D rises while a character is within $200 of it on its open side; 32 breaks into four pieces (the object is the first piece, `BreakToPieces`) when a rolling character stands on it, bounces him at -$300 and scores like a badnik (`item_bonus`); 78 is four blocks hanging from the first and sinking or rising in a staircase once stood on (kinds 0, 4) or bumped from below (2, 6) -- this is the ascending staircase of the rings in CPZ 2 near x 3472; 7A slides one or two platforms along a row, turning when they meet; 74 is solid only while on screen and shows in debug mode; 1D is the droplet chain (hurts, col $8B; kinds with the high nibble set hop sideways at the top). Parents/partners are found by slot index (the prototype stores long pointers at $3C). Checked by screenshots (droplets, the four blocks rising under Sonic, the cover); not exercised: the cover's breaking, the sliders, the barrier. **The Grabber (spider badnik) is not in this prototype** (no object, art or mapping), so it is a later addition from the final game's behaviour: it grabs the character, who can shake free with fast left/right mashing before it explodes.
- **Sloped solids no longer swallow a standing character** (`Solid_Character`, `Solid.c`): a character standing on a sloped solid (lever spring, diagonal spring) was placed at the object's y minus the slope height *plus* the table's first entry, i.e. sunk by twice that entry (16 pixels on a lever spring) and could overlap the spring. The original's `MvSonicOnSlope` subtracts it; now he stands on the table's height and bounces off the spring (checked: land at y 941, bounce ysp -1168 on the first CPZ 2 lever spring).
- **Each game has its own levels in the demo recorder** (Tools > Record Demo / Play Demo): the Qt host had Sonic 1's seven zones built in; the zone and act pickers (and the status bar's level name) now come from the game's own list, `game_zone_list` in `GameInterface.h` (`GameZone { id, name, acts, act_names }`; the id is the zone as the game's level ids number it and is what the demo file names carry): Sonic 1 defines its seven (Labyrinth's fourth act, the Final Zone, the two endings, as before) in `Sonic1Assets.c`, Sonic 2 the eleven built zone slots (Emerald Hill, Wood, Metropolis, Metropolis' third act, Hill Top, Hidden Palace, Oil Ocean, Dust Hill, Casino Night, Chemical Plant, Neo Green Hill) in `Sonic2Assets.c`; a game with none gets one zone of three acts (weak null in `NullServices.c`). Add each zone here as it is built.
- **Chemical Plant, step 4 (elevator, speed booster, pipe tipper)** (`Object/CPZObjects.c`, mappings `CPZElevator`, `Booster`, `PipeTipper` from the prototype's `obj19`, `obj1B` and `obj0B`; the art is the zone's own, loaded by its first list: elevator at tile $3A0, booster $39C, the tipper's pipe $3B0): object 19 is the prototype's elevator, not Nick Arcade's platform (those two stay in `CPZPlatforms.c`, unused): five sizes, sixteen kinds by the subtype (slides along x or y with the oscillators, waits to be stood on and then rises and slows to a stop, bobs, circles one way or the other), a platform for both characters; object 1B gives a grounded character in its box a speed of $1000 (or $A00 for subtype bit 1) in its direction, faces him, stops him turning for 15 frames and plays the spring sound, its frame flickers with the frame counter; object 0B is a section of pipe that is a platform until it tips (it turns every so often by the subtype and the frame counter, tips over in five frames and back) and drops whoever is on it (both characters: the prototype only lets Sonic go). All three leave and come back as the camera moves (`MarkObjGone`'s mark clearing). Not tested in motion: the three of them were seen in place in Chemical Plant act 1; I did not manage to run Sonic over a booster or ride the elevator headless.

- **Chemical Plant's staircase** (object 6B, `Obj_CPZBlock` in `Object/CPZObjects.c`, mapping `CPZBlock` = `obj6B.bin`, the stair block art at tile $418): the prototype's moving block, a solid block (SolidObject: box $B wider than the block, both characters) of a wide kind (width $20, $C high) or a square one by the subtype's bit 4; by its low four bits it stays, slides along x or y with the oscillators, waits to be stood on and then falls (stopping for good once past the camera's bottom), or goes round a square of side $10/$30/$50/$70 (kinds 8-11: the oscillator of its size drives it, its status bits 0-1 say which side of the square it is on, and it turns a corner when the oscillator turns). Chemical Plant's staircases are four blocks of kinds 8 and 9 at the same place: they rise as a diagonal stair, fold into a column and turn. Seen working in act 1 (x 5568).

- **Player 2's dead zone, and which gamepad is whose** (Configure > Gamepad): the second player's left stick has a dead zone and a box/ring shape of its own (`Controls_DeadzoneLeft2` / `Controls_DeadzoneRing2`, a worn stick drifts on its own; a live stick view and readout for it beside the first player's; `Controls_PollAxes` takes the player), and each player picks his gamepad from the connected ones, or leaves it automatic (the first not taken, the first player's first): `Controls_PadDeviceCount/Name`, `Controls_PadAssigned`, `Controls_ChoosePad` (a choice is the pad's GUID and which of the pads with that GUID, kept in the settings as `pad_guid1/2` and `pad_nth1/2`; a chosen pad that is not connected leaves its player without one until it is), `Controls_ReassignPads` on every hot-plug (the old "first pad, then the second" logic is gone); Restore Defaults puts both back to automatic. The selectors and the second stick only show in a game with a two-player mode; Sonic 1's tab is as it was. Not run (headless): the dialog.

- **Tubes: a character no longer freezes at the end of a tube** (both acts): the tube objects (1E) remember their state in the prototype's layouts (id $9E, so the objects manager marks each as loaded), and the tube object deleted itself off-screen without clearing that mark (the prototype's `MarkObjGone3` clears it): a tube that had scrolled away never came back, so an exit path that ended at its mouth left the character held (object control $81) and motionless there. Now the mark is cleared as it goes (the lever spring and the tube spring use `RememberState` too, the same as `MarkObjGone`). Checked with every tube mouth of both acts, entered from both sides: every chain now ends at a tube with a pipe spring (act 1: x 2576, 4240, 7824, 9232; act 2: 1296, 2320, 2832, 4112) and the character is let go there; one maze (act 1 around x 5120) keeps a character that is not steering going round its tubes, as its exit table sends him back into it. The tube's sounds are the prototype's: the spin sound ($BE) when he is taken in and when he turns on to an exit path, the spin dash release sound ($BC) when he leaves.

- **Player 2's controls** (Settings > Configure Sonic the Hedgehog 2): a game with a two-player mode (`GameInfo::split_screen`) gets a second set of bindings. `Controls_Key2` / `Controls_Pad2` (`Backend/Controls.h`, `Controls_KeySlot` / `Controls_PadSlot` to reach either player's by number), defaults I J K L for the D-pad, U O P for A B C and Right Shift for Start (the second player's gamepad buttons are the first's defaults); the SDL backend reads them in `Input_GetState2` (before it was a stub that returned 0) through the same `ReadPlayer` as the first player's, the first connected gamepad is the first player's and the second the second's (hot-plug: if the first is unplugged the second becomes the first); the settings file keeps them under `controls: player2:`; the dialog has "Player 1" and "Player 2" tabs (Sonic 1, with one player, keeps its one "Buttons" tab), a key drives one button only across both players. Tails reads pad 2 as before (a pad 2 that is used keeps him for 300 frames; the split screen and the online versus use it all the time). Not run (headless): the dialog.

- **Split screen in the demo recorder (Sonic 2 and later)**: `GameInfo::split_screen` says a game has the two-player split screen (Sonic 2 sets it); the recorder's and the player's zone pickers then show a "Split screen" box. `DemoRecordRequest` and `DemoPlayRequest` carry it (`split_screen`), Sonic 2's `Demo.c` sets `two_player_mode` from it (so the level starts split, Tails the second player on pad 2, who is idle in a demo: only the first player's pad is recorded), a recording made that way is named "Zone N Act M Split ..." and Play Demo reads the tag back; the title's attract demos clear `two_player_mode`. Sonic 1 has no such box. The dialogs themselves were not run (headless), the build and Sonic 1's tests pass.

- **Chemical Plant: second art list and the lever spring's art**: the level headers' second pattern load list (`plc2`) was 0 for every zone, so Chemical Plant's second list (spikes, worms' droplets, the lever spring, the straight springs) never loaded; the importer gives Chemical Plant `PlcId_MZ2` now. The list no longer holds Sonic 1's diagonal spring art, whose tiles ($43C-$45B) sat on the lever spring's ($440): the lever springs now draw as the red and white ramp the prototype's mapping and art make.

- **Chemical Plant, step 3 (the tube network)**: object 1E (`Object/CPZTubes.c`, the paths in `Object/TubeData.h`, generated from the disassembly: 12 entry paths relative to the tube, 15 exit paths in absolute points, and the two subtype tables) and object 7B (the spring at a tube's mouth). Object 1E: an invisible object at a tube's mouth; a character within $A0/$100/$120 pixels along x (by the subtype) and 0-$7F below it is taken, set rolling at speed $800 and carried point to point (each leg's speed and its frame count worked out from the longer axis, as `loc_16A80`), with his object control set (bit 0 his own movement off, bit 7 his collisions off); at the end of an entry path the subtype picks an exit path to follow (forward or, for a negative number, backwards) to the next tube, which takes him in turn, or lets him go (his y wraps to $7FF and he is out of reach of the tube until he leaves it). Both characters have their own state. For that the prototype's per-character object control byte is new (`Object/CharControl.h`: scratch $2E of the character; Sonic's `lock_multi` still counts for him): Sonic's, Tails' and the solid objects' tests read it. Object 7B: a spring in the mouth of a tube, throws the character on it (power $1000, or $A00 for the weaker subtype, tumble and path bits as the springs'), opens (animation 2 for Sonic, 3 for Tails) when one is in the box above it, and is not solid while open. Checked in act 1: Sonic is taken by the first tube, goes round its loop, is let go upwards and is taken again when he falls back; a spring tube launches him.

- **Signpost corruption fixed** (it showed only once the signpost's art was loaded, as Sonic reached the end of the level): the signpost's pattern load cue loaded three things, as Sonic 1's does: the signpost, the hidden bonus and the giant ring flash. The prototype's list (`End_Level_Sprites`) only loads the first ("Not all sprites are loaded in to VRam": its count is one entry), because the flash's place, $8C40 (tile $462), is inside the signpost's own 82 tiles from $8680: the flash overwrote the lower part of the signpost. `PLC_Signpost` now loads the signpost alone.

- **Chemical Plant, step 2 (springs)**: object 41 (the springs) is the prototype's own already (its art tiles $45C, $470 and $43C are the ones here, its power $1000 / $A00, the tumble and path bits and the five kinds all match). Object 40 is new (`Object/DiagSpring.c`, mapping `res/Mappings/DiagSpring` = `obj40.bin`, art `LeverSpring` at tile $440): the prototype's diagonal springs, a different object from 41's diagonals: a sloped solid (the height of the top for each 2 pixels of its 80, two tables for the open and the compressed frame) that, once a character stands on its springy half (past $10 pixels from the middle towards the low end), compresses (animation 1) and throws him when it is open again; the throw is $FC00 less up to 4 times 256 depending on how far along the slope he is (`loc_1A4E6`), the x speed is changed only if he is going faster than $400, facing and tumble as the other springs (subtype bit 0 tumbles, bit 1 the weaker, bits 2-3 the path). Both characters are handled. Checked in Chemical Plant act 2 (five of them, the first at x 3484): Sonic lands on it, is thrown up and comes down on it again.

- **Narrow layouts repeat (the real cause of Chemical Plant's half-empty background)**: the prototype's layout files are as narrow as they need to be (Chemical Plant's background is 6 chunks by 7: 44 bytes), and its loader (`Interleave_Level_Layout`) repeats each row across the 0x80 entries of the RAM layout, as many whole times as it fits (128 / width). The importer copied each row once and left zeros after it, so the background ended at a hard edge and the rest was empty (dark red). `tools/import_s2sw_levels.pl` now repeats the rows for foreground and background alike and every zone's layout was re-imported (the other zones' backgrounds looked the same; the Sonic 2 level data source did not change).

- **Chemical Plant's background, column strip fixed**: `Draw_CPZ`'s strips of columns (a background moving sideways) started one row late: the prototype reads its row table (`loc_718E`) from one entry before the view's row because the strip starts a row above the screen; `CPZ_ScrollArray` now has that leading entry and the strip reads from `idx - 1`. The second background's Y starts as the first's (as the deformation keeps it). Checked by running through act 1: the flags of both backgrounds arrive as the prototype's (bit 5 every 16 pixels of the second background, bit 3 for the first).

- **Chemical Plant, step 1 (art, water, events)**: the zone's pattern load cues are the prototype's own (`Chemical_Plant_Sprites_1` and `_2`; addresses are bytes in the disassembly, tiles here): metal structure $6E60, construction stripes $7280, speed booster $7380, elevator $7400, animated bits $7600, tube spring cover $7C00, water surface $8000, stair block $8300, metal block $8600; then the droplet (worm) $8780 and the lever spring (the art of object 40, the prototype's diagonal springs, tile $440) $8800; art files copied into `res/Art` (`CPZMetalStructure`, `CPZStripes`, `CPZBooster`, `CPZElevator`, `CPZAnimBits`, `CPZTubeSpring`, `CPZStairBlock`, `CPZMetalBlock`, `CPZDroplet`, `LeverSpring`). The two Nick Arcade platform objects (0C, 19) are empty until the prototype's own are ported (their art no longer loads: tile $400 is the water surface's). Water: only the second act has it (`Level_HasWater`: Hidden Palace, and Chemical Plant act 2); start height $710 (the prototype's `WaterHeight`); `DynamicWater_CPZ2`: once the camera is past x $1DE0 the water's target goes to $510; the prototype's CPZ underwater palette (`S2Palette/CPZWater`, all four lines, `PalId_CPZWater`). Chemical Plant has no dynamic level events (`DynResize_CPz` is an `rts`). Still to do in Chemical Plant: objects (40 and 41 springs, 7B and 1E tubes, 1B booster, 19 elevator, 0B, 2D, 32, 6B, 78, 7A, 74, 1D), its attract demo, its debug object list. Neo Green Hill also has water in the prototype (both acts, start $410/$510); not done yet (its art list needs the water surface).

- **Animated blocks, the prototype's own** (`Map16Delta_*` in its disassembly; I had first missed them, searching for Nick Arcade's name, and removed the patch: the pulsing balls of Emerald Hill's wall vanished): at level start a few blocks past the 500 in the zone's block file are written into the block table (the chunks refer to them), from tables generated straight from the disassembly: Emerald Hill and Hill Top ($1788, 60 words, different from Nick Arcade's), Metropolis ($1730), Hidden Palace ($1710), Oil Ocean ($17A0), Casino Night ($1760), Chemical Plant ($17F8, 4 words), Neo Green Hill ($17C0). The two-player halving variant is left out (the split screen draws ordinary tiles). Checked in Emerald Hill: the dithered pulsing balls are back.

- **Emerald Hill's pulsing ball** (the animated background tile the prototype adds, art `S2Art/EHZFlower5`, also used by the final Sonic 2): its frame times were Nick Arcade's fixed 2 ticks; the prototype's script gives each of its 6 frames its own time ($17, 9, $B, $17, $B, 9), now used. The four flower scripts and all five art files match the prototype's bytes (checked), and Hill Top shares the list as in the prototype.

- **Level select stands on the title screen** (as the prototype's does): the title's own planes and sprites stay (wings, emblem, Sonic and Tails), plane B is cleared (the backdrop stays the title's blue), and the text is drawn on plane A over the art (cells of spaces are left as the title drew them), in white and yellow (colour 15 of lines 0 and 2) of the prototype's level select palette, which now colours the whole picture (the sepia tones of the prototype's menu: Sonic and Tails brown, the wings tan). The menu's font is at tile $700 now: Sonic 1's $680 is among the title's wing tiles and overwrote them (white blocks).

- **Water in a split screen, one water line per view** (the Mega Drive cannot: it has one H interrupt for the whole picture; here each view is its own screen): the engine's `VDP_SetSplitWater(dry, wet, line1, line2)` draws each view's rows after its own line with the wet palette (views read the game's live `dry_palette`/`wet_palette`; the H interrupt is skipped while it is set), `SplitScreen_Scroll` sets the lines from `wtr_pos1` against each camera once both have moved, `LZWaterFeatures` leaves the H interrupt dormant in a split, and the second view has its own three water surface objects (`which` 3-5 in `Obj_NAWaterSurface`, along the second camera). The second view's foreground lines now come from the zone's deformation run for its camera (so Hidden Palace's ripple is right in both). Found on the way: the extra water surface used Sonic 1's slot 28, which is Tails' in Sonic 2 (it overwrote Tails in Hidden Palace): it has slot 27 now. Stacked and side by side both work; the wet row starts after the surface row, as the H interrupt's did.

- **Per-zone code of the Simon Wai prototype for every zone built so far** (WZ, MTZ, OOZ, DHZ/MCZ, CNZ, NGHZ/ARZ added; EHZ, HTZ, HPZ were done; CPZ finished): background starts (`BgScrollSpeed`: InitCam values per zone) and deformation (`LevelScroll.c`: plain backgrounds for WZ/MTZ (x<<5,y<<6), OOZ (x<<5,y<<5) and CNZ (x<<6,y<<2, flags cleared: it never redraws); DHZ's 24 mirrored bands at nine tenths-of-camera speeds with its absolute Y (flags 6/7); NGHZ's 12 bands; `Deform_Bands` is the loop at loc_6526/loc_6A36), CPZ's real two-background scroll (rows down to row 18 follow the first background, those below a second at four times the speed, Emerald Hill's wave on row 18, the flags of both merged into the third's and drawn by `Draw_CPZ`/`CPZ_ScrollArray` in `LevelDraw.c`, replacing the Sonic 1 Marble Zone drawing it used), palette cycles (`PaletteCycle.c`: WZ, MTZ (three cycles), OOZ, DHZ, CNZ (three), NGHZ), animated art (`AnimatedArt.c`: MTZ six scripts, OOZ five, NGHZ four, CPZ one; the art copied from the disassembly into `res/S2Art`). Animated blocks: see the entry on the prototype's own (an earlier version of this entry wrongly said it has no such step). The split-screen second view runs the same deformation for its own camera (`DeformLayersP2`), so every zone above works split. Split screen now allows water levels too (the sea's palette line follows the first view only). CPZ objects still missing (SW ids): 0B pipe tipper, 1B speed booster, 1D worms, 1E tube attributes, 2D one-way barrier, 32 breakable obstacle, 40 diagonal springs, 6B platform, 74 invisible block, 78 rotating platforms, 7A horizontal platform, 7B spring tubes, and a check that 19 (elevator) matches.

- **Split screen (level picked with B in the level select)**: two ordinary views of the level, not the hardware's double-height interlace (a Gemini suggestion the user chose): the engine's VDP draws the stacked layout as two whole pictures, each box-averaged to half the height (`VDP_OutputRows()` is the picture's height; Sprites build ordinary tables, `Object_Adjust2PArtPointer` does nothing), and a picture 8:5 or wider is side by side with half-width views. Game side (`SplitScreen.c`, Sonic 2 only): Sonic is view 1, Tails is player 2 on pad 2 (`Tails_Control` stops copying Sonic's pad); the second camera follows Tails with Sonic's rules (vertical simplified, no look up/down), the second foreground is a `LevelPlane` on `VRAM_FG_P2`, and the second view has its own background plane (`VRAM_BG_P2`) scrolled by the zone's deformation run a second time with the second camera and its own background state (`DeformLayersP2` in `LevelScroll.c`), so each view's background follows only that view's player. Objects load around both cameras (`ObjectsManager2P`, `IS_OFFSCREEN` checks both through `camera_split`). The second view's scroll table moved to `VRAM_HSCROLL_P2` = $18000: at $F800 it overwrote the HUD's lives art. The lives counter goes to the lower left of each view (`hud_lives_lower_left`). Hidden Palace (water) is always played alone for now. Known limits: rings only load around Sonic, one HUD (drawn in both views), no second camera wrap for vertically wrapping levels, the deformation's own animation steps twice per frame (Emerald Hill's wave runs separately for each view). Water idea (not built): the water line is a per-view scanline, so the VDP could swap to the wet palette at a line of each view, with a surface object per camera. Stacked-mode tests (`test_vdpsplit.c`, `test_sprites.c`) rewritten for the new design.

- **Objects brought up to the Simon Wai prototype** (after its levels): Tails is in Emerald Hill again (Nick Arcade left him out); the results screen and title card are the prototype's (`TitleCard.c`: the level order of its `word_BF9A`, which goes on to Emerald Hill's first act where an entry is 0 instead of the Sega screen, no Sonic 1 Scrap Brain 2 cutscene branch; the card is placed by the zone's row of its table and named by the mapping frame of the zone's number, so zones past Sonic 1's six get no name, and the ACT frame is not changed to the act: the prototype does not); the signpost (its art and mappings, art loaded over the spikes' place at tile $434, sparkles of tile $7B2 as the prototype does); mappings from the prototype for the Emerald Hill platform (one for every zone, the tiles of the level art), waterfall, Buzzer (and its shot now starts $18 below and $D beside it), Hill Top's lift and seesaw ball; spikes (all eight kinds: up to 64 wide or tall, sideways and upside down, solid and hurting for Tails too, new `Tails_Hurt`); scenery with the prototype's table of looks (Hill Top's lift poles use frames 3-4; subtypes 7-8 show level art) and its object 71 for Hidden Palace's bridge stakes and glowing orbs. The lamp post and bridge code are the same as the prototype's (the bridge wriggles only in Hidden Palace, zone $08). Not yet compared: the Emerald Hill boss (its art differs), springs, monitors, path swapper and spiral.

- **Zone numbers fixed after the renumbering** (found when the bridge logs drew as wedges: the bridge and the platform chose their art by the old slot numbers 3, 4 and 5): `Bridge.c`, `Platform.c`, `HPZObjects.c` and `ShieldInvincibility.c` use the zone names; the animals of a level (`TitleCard.c`) and the special stage's demo (`Demo.c`: index 7 was Hill Top's) no longer index by the old numbers; the title screen's demo list no longer takes Hill Top's level id ($700) for the special stage. The level select now clears the title screen's emblem and sprites when it opens. The HUD's art, mappings and code are the same in the Simon Wai prototype as in Nick Arcade (compared byte for byte), so there was nothing to bring over.

- **The Simon Wai prototype's levels are in** (`tools/import_s2sw_levels.pl`, which reads the disassembly's own SonLVL project, `SonLVL INI Files/SonLVL.ini`, and writes `res/S2*` and the whole of `src/Sonic2LevelData.c`): art (Nemesis to the game's Kosinski tilesets, Hill Top's over Emerald Hill's from tile $1FC), 16x16 blocks (raw to Kosinski; Hill Top's from block $130), 128x128 chunks (Kosinski already, Sonic 2's final format: copied), layouts (stored apart, FG and BG, in the ROM, put side by side as the game has them in RAM, one 0x100 row: FG first, BG from +0x80), collision indices and the shared collision arrays and angles (these differ from Nick Arcade's), rings, object layouts (Sonic 1's entry format, converted to the engine's), palettes, level sizes (from `s2b.asm`) and start positions. Zones, by slot: EHZ 00 (the prototype calls it Green Hill; it is Emerald Hill), WZ 02, MTZ 04 (acts 1-2) and 05 (act 3), HTZ 07, HPZ 08, OOZ 0A, DHZ 0B, CNZ 0C, CPZ 0D, NGHZ 0F; all load and render. New: `PaletteIds.h` (Sonic 1's `Palette.h` includes it when `PALETTE_IDS_CUSTOM` is set). Only EHZ, HTZ, HPZ and CPZ have their objects put in the levels (the object table is Nick Arcade's ids, which match the prototype's for 01-1C and the ones used there; the ids the prototype uses differently, 1F 22 23 2A 2B 2C 3D 40 42 44 48, are empty now); the other zones start with none. Not done yet, per zone: the background deformation, palette cycles, animated art, dynamic level events, sprite art lists (PLCs) and the objects of the zones that are not built. Sonic 2's attract-mode demos are still Nick Arcade's (the prototype has none of its own that are used, every slot has Emerald Hill's if it has no demo).

- **Sonic 2's level select is the Simon Wai prototype's** (`GM_Title.c`: `LevelSelect`, its controls and text load): 27 lines in its own text (Green Hill, Wood, Metropolis (3 stages), Hill Top, Hidden Palace, Oil Ocean, Dust Hill, Casino Night, Chemical Plant, Genocide City, Neo Green Hill, Death Egg, Special Stage, Sound Select), the order of its `LevelSelect_Order` in the zone slots, its music (`mus_LevelSel`) playing while it is open, Up/Down with the 11-frame repeat, and the sound test of all ids $80-$FF (Left/Right step, A adds $10 without playing, B/C/Start play; the ids are mapped onto this port's: music $81-$9F, effects $A0-$E1, the rest silent). A line whose zone has no header in the level tables yet (all but Green Hill = Emerald Hill, Hill Top, Hidden Palace and Chemical Plant) does nothing when picked, as an unused entry of the prototype's order. B is the button that starts a level in the split screen: `two_player_mode` (new `SplitScreen.c/.h`) is set from it and `SplitScreen_LoadLevel` is called at the start of the level (from `Game_LevelObjects`), where the prototype's level loader sets the split screen up; two-player play itself is not built, so the level is a normal one for now. The old Easter Eggs menu (credits, endings) went with Sonic 1's sound test. Access is still hold A and press Start on the title screen (the prototype wants its Up, Down x4, Up code first).

- **Sonic 2 has the Simon Wai prototype's sound (music and effects).** Its own sound bank (`Sonic the Hedgehog 2/src/Sonic2Sound.c`, ids in `SoundIds.h`, which Sonic 1's `Sound.h` includes when `SOUND_IDS_CUSTOM` is set): music ids 1-$1F are the prototype's $81-$9F, effects $A0-$E1 follow (Helicopter at both $DE and $DF as it has them); Sonic 1's sound names that shared code uses point at what plays for them in the prototype. Level music is by zone slot as the prototype's `MusicList` has it (`Level_Music`, a new hook: Sonic 1's `GM_Level.c` calls it, its own table moved into Sonic 1's `Level.c`, behaviour unchanged). Priorities and the speed-shoes tempo list are the prototype's. The 26 songs and effects $D0-$E0 are its SMPS driver 2 sources run through `asm2json` (the compiler gained the Sonic 2 drum names: Scratch, Clap, HiTom, VLowClap, MidTom, LowTom, FloorTom, HiClap, MidClap, LowClap, in the engine's extended DAC scheme); its effects $A0-$CF exist in its disassembly only as bytes, so `tools/sw_sfx2asm.pl` turns them back into SMPS2ASM source (checked: its voice for $A1 equals Nick Arcade's). Everything compiles and each zone runs; nothing has been listened to yet (the prototype's DAC drums, driver-2 tempo and the cross-channel jumps of $B4/$B9 and a few songs, which the converter flags as "needs manual review", are the things to check by ear). Zone music was silent for Hill Top, Hidden Palace and Chemical Plant between the slot renumbering and this.

- **17 zone slots for Sonic 2 (prep for M2, the Simon Wai prototype).** `ZoneId_Num` in Sonic 1's `Level.h` can be set by a game with `ZONE_SLOTS` (Sonic 1 leaves it unset: unchanged); Sonic 2 sets 17 in `paradoxmake.cmake`, so every per-zone table is sized for ids 0-$10 (the Simon Wai prototype's; entries not yet filled are zero). Done after that: the zones now sit in the prototype's slots (new `Sonic the Hedgehog 2/src/ZoneIds.h`, included by Sonic 1's `Level.h` when `ZONE_SLOTS` is set; names provisional): EHZ 00, HTZ 07, HPZ 08, CPZ 0D (were 3, 5, 4, 2); Green Hill, Labyrinth and the ending slot are gone from Sonic 2, and Sonic 1's zone names that shared code still tests are aliases of empty slots so those checks never match. Every Sonic 2 per-zone table names its slots (`[ZoneId_EHZ] = ...`). `GM_Special.c` names the special-stage demo by `ZoneId_SS` (same value in Sonic 1). Sonic 1's test suite passes; the four zones render as before. Test with `--zone 0/13/8/7`. Level-select labels still to do: EHZ "GREEN HILL", MCZ (0B) "DUST HILL", lev03 "GENOCIDE CITY", lev10 "DEATH EGG", ARZ (0F) "NEO GREEN HILL". Nick Arcade is finished as a source; the prototype's disassembly (`~/Projects/s2sw-disasm`) is next.
- **Hill Top (zone slot 5) objects: seesaw and lift.** `Object/HTZObjects.c`: NA's seesaw (14: the seesaw and its ball are the same id; the ball finds its seesaw by a checked slot index, is thrown when the seesaw tips under it, and launches whoever stands on the end it lands on, Sonic or Tails) and the lift (16: slides down the slope when stood on). Art (`HTZSeesaw` $3CE, `HTZLift` $3E6) and mappings imported with the tool and added to the Hill Top PLC. Checked in-game (Sonic dropped on the seesaw is thrown; the lift carries him). Hill Top's other objects (path swapper, platform, scenery, monitor) were already done; NA has no lava objects in Hill Top. Also this round: Sonic frames $80+ (hurt, death, tumble) now load the right art; the turn skid that NA never fires is removed; the level-results tally blip is every 4th frame as in NA.

## Sonic 2 toward the Nick Arcade equivalent: Emerald Hill act 1 loads (2026-10-03)

**Status: EHZ1/2 load and Sonic stands on Nick Arcade's terrain (zone slot 3). Sonic 1 unchanged (42 scenarios + PCM identical). Not yet: EHZ background deformation/scrolling, objects, music, the rest of the zones.**

- **Goal: Emerald Hill with all its objects, then Nick Arcade's attract mode.** Ported so far, each Sonic 2 only (`Sonic the Hedgehog 2/src/Object/`), from the NA sources, with the assets imported by `tools/import_s2na_assets.pl` (art, mappings, animation scripts, and it lists them in the project file): bridge (`Bridge.c`: logs as child sprites of up to two objects, Sonic and Tails each give a log index; children are kept as slot indices), scenery (`Scenery.c`: bridge stakes etc.), basic platform (`Platform.c`: NA's object 18; both characters), EHZ waterfall (`EHZWaterfall.c`), Buzzer / Masher / Snail (`EHZBadniks.c`: parts find their parents by a checked slot index), corkscrew (`Spiral.c`: a character on it gets its height from a table and its flip angle set, which makes Sonic tumble along it) and the signpost (`Signpost.c`: NA's art/mappings/animation and sparkle places). `Solid.c` gained `Solid_PlatformLand`, `Solid_Platform` and `Solid_Ride`. The zone's art (bridge C6, waterfall AE, fireball E, Buzzer E6, Snail , Masher C, shield ) loads in Emerald Hill's PLC.
- **Turn-around fixed (Sonic and Tails).** Nick Arcade's skid on turning (animation $0D, the Stop frames $81-$88, with the skid sound) can never fire: `Sonic_TurnLeft/Right` compare the masked angle (always 0 there) with $400 where they mean the speed. The port had Sonic 1's working skid, so turning showed wrong frames; it is removed from `Sonic.c` and `Tails.c`, and turning only brakes (the animation stays as it was).
- **Frames $80 and up fixed (hurt, death, tumble).** `Sonic_LoadGfx` doubled the frame number in a `uint8_t`, so every frame from $80 read the wrong DPLC entry (garbage for hurt $8D, death $9A and the corkscrew tumble $9B+). It now indexes with a `size_t`. (The earlier 16-bit tile-offset fix was a different bug.)
- **Audit of Sonic (and Tails) against Nick Arcade.** Compared byte for byte: Sonic's and Tails' mappings (asm and binary), DPLC tables, art and palette are identical to the disassembly's files; Sonic's and Tails' animation scripts are identical (the 32nd entry of each table is the port's own alias of the spin dash, the script NA has as animation 9); every frame's mapping needs exactly the tiles its DPLC loads (only frame $0B loads 12 more than it uses). Found and fixed in `Sonic.c`: the DPLC loader worked out a tile's byte offset in 16 bits, but Nick Arcade's Sonic art has 2214 tiles, so frames $9D-$A6 (the tumble frames the corkscrew shows) read the wrong art; Tails' loader was already right. Touch response: Nick Arcade lets a charging spin dash (animation 9, here 31) hurt enemies as a roll does; the port's check now has it (the monitor check, roll only, was already as NA's). `tools/render_frames.pl` renders frames from the raw data for such checks. Not matched on purpose: the spin dash itself (kept as the final game's), the wall recoil animations (data present, code skipped).
- **Hidden Palace's objects and water.** Objects (Sonic 2 only): emerald (12), waterfall (13: streams that follow the water's height), collapsing ledge (1A: Green Hill's and Hidden Palace's, sloped platform for both characters, via the shared `Solid_SlopedPlatform`), BBat (4C) and Redz (4F); the bridge (11) and scenery (1C) gain their Hidden Palace art. Water: Nick Arcade runs Sonic 1's Labyrinth water code in Hidden Palace (zone 4, `Water_flag`), whose first act lets Tails' pad (the second one) move the water up and down. Shared Sonic 1 code now asks `Level_HasWater()`, `Level_WaterStartHeight()`, `Level_LoadWaterPalettes()` and `Level_MakeWaterSurfaces()` (Sonic 1's answers are its old code, hash baseline identical); Sonic 2's Level.c answers for Hidden Palace (heights $600 etc., NA's underwater palettes), `LZWaterFeatures.c` (Sonic 2's own) does WaterEffects/DynamicWaterHeight, `Object/WaterObjects.c` has NA's surface (04) and splash (08). **The underwater ripple of Sonic 1's Labyrinth is kept as `Water_Ripple()` (an offset added to whatever the zone's scrolling set) and runs in Hidden Palace; it is meant to stay in every zone with water up to Sonic & Knuckles' code, Oil Ocean and Sandopolis excepted** (Nick Arcade itself has no ripple here). The art queue and palettes follow Nick Arcade's.
- **Chemical Plant's objects.** Act 1 uses only four ids: path swapper, monitor (both done), and two platforms, now ported in `Object/CPZPlatforms.c`: object 19 (slides, rises, falls or shuttles by subtype, hides until a button; both characters) and object 0C (the floating platform that bobs and shakes at the top of its swing). Nick Arcade's object 0C asks for art at tile $418 but its PLC only loads the platform art at $400-$410, so it shows what happens to be there: kept as it is. The tipping floor (0B) is in no act's object list and the boss at the end of act 3 jumps to address 0 in Nick Arcade (crashes), so neither is ported. The zone's PLC loads `CPZ - Floating Platform` and the dust art as Nick Arcade's does.
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
