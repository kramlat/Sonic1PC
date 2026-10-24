#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

#include "EditorModel.h"

#ifdef __cplusplus
extern "C" {
#endif

// The built-in level editor's hold on the running game. The level the editor changes is a copy of the level's object and ring layouts (EditorModel.h) that the game's object and ring managers read in place of the
// layouts in its tables while the editor has the level; when something changes, the layouts are written again and the game starts its managers over (it clears the level's objects and loads them afresh).
//
// A game cooperates in two places, both in its object loading: it asks Editor_ObjectLayout / Editor_RingLayout for the layout it hands the managers (they return what it passed unless the editor has this level), and
// it asks Editor_TakeReload once a frame, starting its managers over when it says so.

typedef struct {
    int zone;
    int act;
    EditorLevel level;
} EditorSession;

extern EditorSession editor_session; // the level being edited (valid while editor_open)
extern bool editor_open;
extern bool editor_playtest; // the players are in the level again (the editor's cursor is where the first player was left): the level is played as edited
extern bool editor_present; // ... and its level is the one running (the editor's window keeps this up to date): only then do the players hold still and the layouts apply

// Takes the level a game is in: copies its layouts (either may be NULL) into the model and the live buffers, and starts using them
void Editor_Open(int zone, int act, const uint8_t *objects, const uint8_t *rings);
// Hands the game's own layouts back (the live ones are dropped); asks for a reload so the level is as the tables have it
void Editor_Close(void);
void Editor_Left(void);   // the level being edited is not the one running any more (another level, a game over...): the checkpoints the playtest left are put back
// The model has changed: writes it to the live buffers and asks the game to reload
void Editor_Apply(void);

// The editor's cursor: where it is looking in the level. While the editor has the level the players neither run nor show (the game says which objects they are: Game_EditorSkipObject), and the first player's place
// is the cursor's, so the camera follows it as it follows the player in play. Editor_Hold keeps that place up to date once a frame.
void Editor_SetCursor(int x, int y);
void Editor_GetCursor(int *x, int *y);
void Editor_Hold(void);
// The camera follows the cursor, not the player: around the game's camera scroll the cursor stands in for the first player (so nothing else sees it: the signpost, the checkpoints and the other things that wait for
// the player to come near are triggered by the player alone). Editor_Editing: the level is being edited, not played (the game's dynamic level events do not run then).
void Editor_CameraBegin(void);
void Editor_CameraEnd(void);
bool Editor_Editing(void);
void Editor_SetPlaytest(bool on); // on: the players run and show from the cursor; off: the cursor is where the first player is, and they hold still again

// What a game passes its managers: the live layout when the editor is open on this zone and act, else `original`
const uint8_t *Editor_ObjectLayout(int zone, int act, const uint8_t *original);
const uint8_t *Editor_RingLayout(int zone, int act, const uint8_t *original);

// True once after Editor_Apply / Editor_Close: the game clears the level's objects (Editor_ClearLevelObjects) and restarts its managers
bool Editor_TakeReload(void);
void Editor_ClearLevelObjects(void);

// ---- The terrain: the level's tiles, blocks, chunks, collision indices and palette, as they are loaded (the editor changes them in place and the level shows it) ----
// The game's level loaders say how much they loaded (the data is what was decompressed: its size is where the original ends), for the editor's lists and its saved files.
void Editor_NoteArt(uint8_t *art, size_t bytes);  // the level's tiles, 32 bytes each, in the game's own buffer (the level's art is also in the video memory from tile 0)
void Editor_NoteBlocks(size_t bytes);             // level_map16: 8 bytes a block (four tile words)
void Editor_NoteChunks(size_t bytes);             // level_map128: 128 bytes a chunk (8 x 8 words: block, flips, solidity)
void Editor_NoteSolids(int path, size_t bytes);   // coll_index[path]: a collision curve for each block

uint8_t *Editor_Art(size_t *bytes);
uint8_t *Editor_Blocks(size_t *bytes);
uint8_t *Editor_Chunks(size_t *bytes);
uint8_t *Editor_Solids(int path, size_t *bytes);
uint8_t *Editor_Layout(size_t *bytes);            // level_layout: 16 rows of 256 bytes, the foreground's 128 chunks and then the background's, each a chunk number (0: empty)
const uint8_t *Editor_HeightMap(void);            // 16 heights for each of the 256 collision curves, and the angle of each
const uint8_t *Editor_CurveAngles(void);

// After changing tile `tile` of the art, its picture in the video memory; after changing blocks or chunks, the level's planes drawn again from them
void Editor_CommitArt(int tile);
void Editor_RedrawLevel(void);

// The palette: a colour as the Mega Drive has it ($0BGR, 3 bits each, the low bit of each unused). A colour the editor sets is kept as set (the game's cycles and fades do not take it back) until the editor closes.
uint16_t Editor_GetColour(int line, int index);
void Editor_SetColour(int line, int index, uint16_t colour);
// A colour as the level's palette has it, for the saved file: the editor's own if it set one, else the palette as it was loaded (the target of fades), not what a palette cycle has put on the screen
uint16_t Editor_GetBaseColour(int line, int index);

#ifdef __cplusplus
}
#endif
