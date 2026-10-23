#pragma once

#include <stdint.h>
#include <stdbool.h>

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

// Takes the level a game is in: copies its layouts (either may be NULL) into the model and the live buffers, and starts using them
void Editor_Open(int zone, int act, const uint8_t *objects, const uint8_t *rings);
// Hands the game's own layouts back (the live ones are dropped); asks for a reload so the level is as the tables have it
void Editor_Close(void);
// The model has changed: writes it to the live buffers and asks the game to reload
void Editor_Apply(void);

// The editor's cursor: where it is looking in the level. While the editor has the level the players neither run nor show (the game says which objects they are: Game_EditorSkipObject), and the first player's place
// is the cursor's, so the camera follows it as it follows the player in play. Editor_Hold keeps that place up to date once a frame.
void Editor_SetCursor(int x, int y);
void Editor_GetCursor(int *x, int *y);
void Editor_Hold(void);

// What a game passes its managers: the live layout when the editor is open on this zone and act, else `original`
const uint8_t *Editor_ObjectLayout(int zone, int act, const uint8_t *original);
const uint8_t *Editor_RingLayout(int zone, int act, const uint8_t *original);

// True once after Editor_Apply / Editor_Close: the game clears the level's objects (Editor_ClearLevelObjects) and restarts its managers
bool Editor_TakeReload(void);
void Editor_ClearLevelObjects(void);

#ifdef __cplusplus
}
#endif
