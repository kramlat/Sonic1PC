#include "EditorLive.h"

#include <string.h>

#include "EngineObject.h"
#include "GameInterface.h"

EditorSession editor_session;
bool editor_open;
bool editor_present;

static uint8_t live_objects[EDITOR_MAX_OBJECTS * 6 + 6];
static uint8_t live_rings[EDITOR_MAX_RINGS * 4 + 4];
static bool reload_pending;

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

void Editor_Hold(void) {
    if (!editor_present)
        return;
    player->pos.l.x.f.u = (int16_t)cursor_x;
    player->pos.l.y.f.u = (int16_t)cursor_y;
    player->xsp = player->ysp = player->inertia = 0;
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
}

void Editor_Close(void) {
    if (!editor_open)
        return;
    editor_open = false;
    editor_present = false;
    reload_pending = true;
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
