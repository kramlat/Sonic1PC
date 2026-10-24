#include "EditorLive.h"

#include <string.h>

#include "EngineObject.h"
#include "Backend/VDP.h"
#include "EnginePalette.h"
#include "GameInterface.h"
#include "LevelData.h"

EditorSession editor_session;
bool editor_open;
bool editor_present;
bool editor_playtest;

static uint8_t live_objects[EDITOR_MAX_OBJECTS * 6 + 6];
static uint8_t live_rings[EDITOR_MAX_RINGS * 4 + 4];
static bool reload_pending;
static uint16_t colour[4][16];
static bool colour_set[4][16];

static void WriteLive(void) {
    EditorLevel_WriteObjects(&editor_session.level, live_objects, sizeof(live_objects));
    EditorLevel_WriteRings(&editor_session.level, live_rings, sizeof(live_rings));
}

static int cursor_x, cursor_y;

void Editor_SetCursor(int x, int y) {
    cursor_x = x < 0 ? 0 : x;
    cursor_y = y < 0 ? 0 : y;
}

void Editor_GetCursor(int *x, int *y) {
    *x = cursor_x;
    *y = cursor_y;
}

#define EDITOR_PARK 0x7000 // (past the end of any level)

static void HoldColours(void);

void Editor_SetPlaytest(bool on) {
    if (on == editor_playtest || !editor_present)
        return;
    if (on) { // (the player comes to the cursor)
        player->pos.l.x.f.u = (int16_t)cursor_x;
        player->pos.l.y.f.u = (int16_t)cursor_y;
        player->xsp = player->ysp = player->inertia = 0;
    } else { // (back to editing: the cursor is where the player was left, and the player is parked again at the next frame)
        cursor_x = player->pos.l.x.f.u;
        cursor_y = player->pos.l.y.f.u;
    }
    editor_playtest = on;
    Game_EditorSetMode(!on);
}

void Editor_Hold(void) {
    if (!editor_present)
        return;
    HoldColours();
    if (editor_playtest)
        return; // (the player is playing)
    // While the level is edited the player is parked far outside it, so nothing that waits for him to come near (the signpost, the checkpoints, the path swappers, the badniks that go for him...) can be triggered;
    // the cursor is where the editor looks, and a playtest starts from it
    player->pos.l.x.f.u = EDITOR_PARK;
    player->pos.l.y.f.u = EDITOR_PARK;
    player->xsp = player->ysp = player->inertia = 0;
}

static int16_t saved_x, saved_y;
static bool standing_in;

bool Editor_Editing(void) {
    return editor_present && !editor_playtest;
}

void Editor_CameraBegin(void) {
    if (!Editor_Editing())
        return;
    saved_x = player->pos.l.x.f.u;
    saved_y = player->pos.l.y.f.u;
    player->pos.l.x.f.u = (int16_t)cursor_x;
    player->pos.l.y.f.u = (int16_t)cursor_y;
    standing_in = true;
}

void Editor_CameraEnd(void) {
    if (!standing_in)
        return;
    standing_in = false;
    player->pos.l.x.f.u = saved_x;
    player->pos.l.y.f.u = saved_y;
}

void Editor_Open(int zone, int act, const uint8_t *objects, const uint8_t *rings) {
    EditorLevel_Clear(&editor_session.level);
    EditorLevel_LoadObjects(&editor_session.level, objects);
    EditorLevel_LoadRings(&editor_session.level, rings);
    editor_session.level.dirty = false;
    editor_session.zone = zone;
    editor_session.act = act;
    editor_open = true;
    WriteLive();
    Game_EditorLamp(0); // (what the playtest leaves of the checkpoints is not the level's)
    Game_EditorSetMode(true); // (the level is edited, not played: a game mode of its own)
}

void Editor_Close(void) {
    if (!editor_open)
        return;
    if (Editor_Editing()) { // (the player is where the cursor is when the level is played again)
        player->pos.l.x.f.u = (int16_t)cursor_x;
        player->pos.l.y.f.u = (int16_t)cursor_y;
    }
    editor_open = false;
    editor_present = false;
    editor_playtest = false;
    Game_EditorSetMode(false); // (the level is played again, from the game's own data)
    memset(colour_set, 0, sizeof(colour_set)); // (what the editor set is the editor's: the level's own palette comes back)
    reload_pending = true;
}

void Editor_Left(void) {
    editor_playtest = false;
    Game_EditorLamp(1);
}

void Editor_Apply(void) {
    if (!editor_open)
        return;
    WriteLive();
    reload_pending = true;
}

const uint8_t *Editor_ObjectLayout(int zone, int act, const uint8_t *original) {
    return editor_open && zone == editor_session.zone && act == editor_session.act ? live_objects : original;
}

const uint8_t *Editor_RingLayout(int zone, int act, const uint8_t *original) {
    return editor_open && zone == editor_session.zone && act == editor_session.act ? live_rings : original;
}

bool Editor_TakeReload(void) {
    const bool pending = reload_pending;
    reload_pending = false;
    return pending;
}

void Editor_ClearLevelObjects(void) {
    memset(level_objects, 0, LEVEL_OBJECTS * sizeof(Object));
}

// ---- The terrain ----
static uint8_t *art_buffer;
static size_t art_bytes, block_bytes, chunk_bytes, solid_bytes[2];

void Editor_NoteArt(uint8_t *art, size_t bytes) { art_buffer = art; art_bytes = bytes; }
void Editor_NoteBlocks(size_t bytes) { block_bytes = bytes; }
void Editor_NoteChunks(size_t bytes) { chunk_bytes = bytes; }
void Editor_NoteSolids(int path, size_t bytes) { solid_bytes[path & 1] = bytes; }

uint8_t *Editor_Art(size_t *bytes) { *bytes = art_bytes; return art_buffer; }
uint8_t *Editor_Blocks(size_t *bytes) { *bytes = block_bytes; return level_map16; }
uint8_t *Editor_Chunks(size_t *bytes) { *bytes = chunk_bytes; return level_map128; }
uint8_t *Editor_Solids(int path, size_t *bytes) { *bytes = solid_bytes[path & 1]; return coll_index[path & 1]; }
uint8_t *Editor_Layout(size_t *bytes) { *bytes = sizeof(level_layout); return level_layout; }
const uint8_t *Editor_HeightMap(void) { return Collision_HeightMap; }
const uint8_t *Editor_CurveAngles(void) { return Collision_Angle; }

void Editor_CommitArt(int tile) {
    if (art_buffer == NULL || tile < 0 || (size_t)(tile + 1) * 32 > art_bytes)
        return;
    VDP_SeekVRAM((size_t)tile * 32);
    VDP_WriteVRAM(art_buffer + (size_t)tile * 32, 32);
}

void Editor_RedrawLevel(void) {
    Game_EditorRedraw();
}

uint16_t Editor_GetColour(int line, int index) {
    line &= 3;
    index &= 15;
    return colour_set[line][index] ? colour[line][index] : dry_palette[line][index];
}

uint16_t Editor_GetBaseColour(int line, int index) {
    line &= 3;
    index &= 15;
    return colour_set[line][index] ? colour[line][index] : dry_palette_dup[line][index];
}

void Editor_SetColour(int line, int index, uint16_t value) {
    line &= 3;
    index &= 15;
    colour[line][index] = value & 0x0EEE;
    colour_set[line][index] = true;
    dry_palette[line][index] = colour[line][index];
}

// (the colours the editor has set, put back every frame over what a palette cycle or a fade writes)
static void HoldColours(void) {
    for (int line = 0; line < 4; line++)
        for (int i = 0; i < 16; i++)
            if (colour_set[line][i])
                dry_palette[line][i] = colour[line][i];
}
