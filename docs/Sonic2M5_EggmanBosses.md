# Beta 4's two Eggman bosses still to port: Wing Fortress ($C5) and the Death Egg final boss ($C7)

Status 2026-11-21. Every other Beta 4 boss is done (see `Sonic2M5.md`). These two are not diffs of what the port has: the port's `$C5` (`SFZBoss.c`, 280 lines) is the pre-beta's unused stub, and the port's `$C7` (`DEZFinalBoss.c`, 247 lines) is the pre-beta's, which Beta 4 rewrote (1164 normalised lines differ of 1465). Both are ports of the Beta 4 objects from scratch, then the level events, art lists and the end of the act.

## Helpers of the Beta 4 source (Sonic2B4.asm) in the port's terms
* `loc_32970` = Load_Sub_Obj: `a2` points at `{word offset, byte object id, byte subtype}`; it finds a slot (`S1SingleObjectLoad`: after the current one), stores its address (word) at `offset(a0)`, sets its id and `$28` (subtype), `$2C(a1)` = parent address, and copies x/y. The port's `SFZBoss.c` has the same idea (`LoadSub`, addresses as `$B000 + slot * $40`).
* `loc_3A892` DisplaySprite, `loc_3A898` DeleteObject, `loc_3A8A4` MarkObjGone, `loc_3A8AA` Delete_A1_Object (zeroes the object in a1), `loc_3A8B6` S1SingleObjectLoad, `loc_3A8BC` AnimateSprite, `loc_3A8EC` Boss_Defeated (the explosions), `loc_3A8F2` the solid box for both characters (`loc_15C6A`), `loc_3A8F8` SolidObject, `loc_3A904` SpeedToPos, `loc_3A8FE` ObjectFall, `loc_32866` the orientation to the nearest character (d0 = left/right, d2 = dx, d3 = dy of the one used).
* `EnemySettings` (`$24` += 2): the object's table entry is by its subtype as a byte offset into `loc_327B8` (a word table of pointers to `{long mappings, word art, byte render-or, byte priority, byte width, byte collision}`); `EnemySettings_02` takes a1 directly.
* Both bosses use Beta 4's pattern: a layout-placed object with a subtype; its routine (`$24`) = subtype minus a base ($90 for `$C5`, set by the init routine), its sub-state in `$25`; the parts are the same object id with other subtypes.

## `$C5` Wing Fortress (obj_0xC5.asm, 803 lines)
* Placed once in the layout: `$C5` (id `$45` with the flip bits), **subtype $92** at ($2B80,$450). Subtype -> routine: $92 body (routine 2, states `loc_38382..`), $94 solid platform (4), $96 (6), $98 (8), $9A (`$A`), $9C (`$C`), $9E (`$E`: Robotnik_Running mappings), $A0 (`$10`: mappings `loc_38DA0`, art word `$A46D`), $A2 (`$12`: follows the body's Y + $26).
* Spawn table (`loc_38B44..B60`): `{offset,id,subtype}`: $2A:$94 (twice, at x -$88 and +$88, y +$60), $3E:$98, $3C:$9A, $3C:$9C, $3A:$96, $3E:$9E, $38:$A0, $3E:$A2.
* The body: camera lower limits `$FFFFEECE`/`$FFFFEEC6` = $442 (and $720 at the end); waits for a character within $20 of its X; hits 8 (`$21`); animation script `loc_38B96`; flashes with the shared hit routine `loc_38AD0` (flash $20 frames, `$AC`, defeated: `$64` points, `$EF` frames, routine $1C).
* Mappings `loc_38BBC` (19 frames), `loc_38DA0` (second table), animations `loc_38B96` (4 scripts), art words `$379` (Spikeball_Laser at VRAM $6F20), `$A46D`; PLC `$3E` = Spikeball_Laser $6F20, Robotnik (`sfzRobotnik.nem`) $A300, Robotnik_Faces (`rbtkface.nem`) $A000, Robotnik_Body (`rbtkbody.nem`) $AC80, Boss_Explosions; PLC `$3F` = Tornado $A000, Tornado_Rocket $AC20, Clouds $A9E0 (after the boss).
* The boss switch `$C2` already holds the camera at $2880; the events' boss part and the PLC loads (`SFz_Boss_Sprs` in `ArtLoadCues`) are still to do, then the capsule/Tornado ending (the object `$B2`'s later routines: the camera flies, scripted Sonic, level $E00).

## `$C7` Death Egg final boss (obj_0xC7.asm, 1465 lines)
* Routine table of 17 (`loc_393C0`: 0 init via `EnemySettings_02` with `loc_3A3E0`, 1 the body with seven sub-states `loc_39412..`, 2..16 parts), body spawns ten parts from the table `loc_3A3AC..3A3D0`, `$38(a0)` the ship's address, hits 8 (`$21`), the animation scripts `loc_3A132` and the like. The rest of the structure is not read yet.

## Order I would take (each stage committed and pushed with tests)
1. Wing Fortress: import the mappings/animations/art (`tools/import_s2b4_sfz.pl`), PLCs `$3E`/`$3F`; the boss object; the events; the capsule; then the Tornado's ending.
2. Death Egg: read `$C7` fully into this note, then the object and its parts, the events, the end (the ending sequence is the port's own milestone).
