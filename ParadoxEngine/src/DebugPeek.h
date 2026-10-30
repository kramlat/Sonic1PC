#pragma once

// Plain-C read-only views of game state (palettes, object slots) for the Qt
// backend's debug viewers, which can't include the game's own headers.

#include <stdint.h>

#include <stdbool.h>

#include "Backend/PeekData.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	PEEK_PAL_DRY,         // dry_palette: what the dry part of the screen shows (live)
	PEEK_PAL_WET,         // wet_palette: the underwater palette (live)
	PEEK_PAL_DRY_TARGET,  // dry_palette_dup: the fade target for the dry palette
	PEEK_PAL_WET_TARGET,  // wet_palette_dup: the fade target for the water palette
};

// Raw 9-bit CRAM word of the game's own palette copies. pal 0-3, index 0-15.
uint16_t Peek_GamePalette(int which, int pal, int index);

// Nonzero while the game's debug mode is available (a debug build, or the debug code entered):
// the Qt backend shows its debug tools menu only then.
int Peek_DebugToolsAvailable(void);

// Object slots 0..Peek_ObjectCount()-1 (reserved + level objects), and the raw
// bytes of one slot's Object struct (size returned through *size).
int Peek_ObjectCount(void);
void Peek_GetObject(int slot, ObjectPeek *out);
const uint8_t *Peek_ObjectBytes(int slot, int *size);

// Watchable variables (the table lives in DebugVars.c). element is 0 for scalars. Writing sets
// the live game variable -- fine between frames, which is when the viewers run.
int Peek_VarCount(void);
void Peek_GetVar(int i, VarPeek *out);
int64_t Peek_VarRead(int i, int element);
void Peek_VarWrite(int i, int element, int64_t value);

// The level editor's view of the game (EditorLive.h has the editing itself). `in_level` is nonzero while a level is running; zone and act are the game's own numbers (the zone slot and the act, from 0); the
// camera is the picture's top left corner in the level; `objects` and `rings` are the level's layouts in the game's tables (NULL for none).
typedef struct {
	int in_level;
	int zone, act;
	int zone_count;
	int camera_x, camera_y;
	int player_x, player_y; // where the first player is (the editor's cursor starts there)
	const uint8_t *objects;
	const uint8_t *rings;
} EditorPeek;
void Peek_EditorState(EditorPeek *out);

// The name of an object id for the editor's list (NULL: the game does not name it), and the names of the resource files a level's layouts are kept in: the folder of each kind ("S2Objects") and the
// level's name in it ("SSZ1"), so that a saved file can be copied back over the one in the game's res/ folder. NULL when the game does not say; the editor then names its files by zone, act and kind.
const char *Peek_ObjectName(int id);
const char *Peek_LevelObjectsFolder(void);
const char *Peek_LevelRingsFolder(void);
const char *Peek_LevelName(int zone, int act);
// The folders the terrain files are kept in (0 tiles, 1 blocks, 2 chunks, 3 collision indices, 4 palette: "S2Art", "S2Map16", "S2Map128", "S2Collision", "S2Palette") and a zone's name in them ("SSZ"; the collision
// indices add the path, "SSZ1" and "SSZ2"). NULL when the game does not say; the editor then names its files by zone and kind.
const char *Peek_TerrainFolder(int kind);
const char *Peek_LevelLayoutFolder(void); // the folder of a level's layout file ("S2Layout"), named in it by Peek_LevelName
const char *Peek_ZoneCode(int zone);
// A zone whose tiles and blocks are another zone's with its own supplement over them (Hill Top over Emerald Hill): where the supplement starts, in tiles and in blocks, or false. The level editor lets the supplement be edited apart from the base.
bool Peek_LevelSupplement(int zone, int *tile_start, int *block_start);
// The zone's own name for the editor's title ("Sand Shower Zone"), or NULL
const char *Peek_ZoneName(int zone);

#ifdef __cplusplus
}
#endif
