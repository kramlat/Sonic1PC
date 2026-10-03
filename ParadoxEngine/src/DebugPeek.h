#pragma once

// Plain-C read-only views of game state (palettes, object slots) for the Qt
// backend's debug viewers, which can't include the game's own headers.

#include <stdint.h>

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

#ifdef __cplusplus
}
#endif
