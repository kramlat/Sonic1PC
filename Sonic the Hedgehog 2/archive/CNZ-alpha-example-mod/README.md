# Casino Night, as it was before the pre-beta's (archived 2026-11-01)

The tiles, blocks and chunks of Casino Night that the port had up to M3 (the alpha / Simon Wai data), kept for an example mod of the modding engine: a "classic Casino Night" over the pre-beta's. The import
of the pre-beta's Casino Night (`tools/import_s2prebeta_zone.pl CNZ`) overwrites `res/S2Art/CNZ`, `res/S2Map16/CNZ` and `res/S2Map128/CNZ` in place.

The three the user named are `res/S2Art/CNZ` (tiles), `res/S2Map16/CNZ` (blocks) and `res/S2Map128/CNZ` (chunks). The collision, layouts, objects, rings and palette are copied too, because the importer
replaces them as well and a mod needs them to match the blocks. Same folder layout as the game's `res/`; nothing here is built into the game.
