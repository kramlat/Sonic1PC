#include "DebugPeek.h"

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "Palette.h"

// Read-only views of game state for the Qt debug viewers (see DebugPeek.h).

uint16_t Peek_GamePalette(int which, int pal, int index) {
	pal &= 3;
	index &= 0xF;
	switch (which) {
		case PEEK_PAL_DRY:        return dry_palette[pal][index];
		case PEEK_PAL_WET:        return wet_palette[pal][index];
		case PEEK_PAL_DRY_TARGET: return dry_palette_dup[pal][index];
		case PEEK_PAL_WET_TARGET: return wet_palette_dup[pal][index];
	}
	return 0;
}

int Peek_DebugToolsAvailable(void) {
#ifndef NDEBUG
	return 1; // debug builds: from the moment the window opens (debug_cheat itself is only set once the title screen runs)
#else
	return debug_cheat != 0; // other builds: after the C,C,C,C + Up,Down,Left,Right code at the title screen
#endif
}

int Peek_ObjectCount(void) {
	return OBJECTS;
}

void Peek_GetObject(int slot, ObjectPeek *out) {
	const Object *o = &objects[slot];
	out->type = o->type;
	out->routine = o->routine;
	out->routine_sec = o->routine_sec;
	out->frame = o->frame;
	out->anim = o->anim;
	out->render = o->render.b;
	out->status = o->status.b;
	out->subtype = o->scratch.u8[0];
	out->x = o->pos.l.x.f.u;
	out->y = o->pos.l.y.f.u;
	out->xsp = o->xsp;
	out->ysp = o->ysp;
}

const uint8_t *Peek_ObjectBytes(int slot, int *size) {
	*size = (int)sizeof(Object);
	return (const uint8_t *)&objects[slot];
}

void Peek_EditorState(EditorPeek *out) {
	out->in_level = (gamemode & 0x7F) == GameMode_Level || (gamemode & 0x7F) == GameMode_Demo; // (a demo is a level that plays itself)
	out->zone = LEVEL_ZONE(level_id);
	out->act = LEVEL_ACT(level_id);
	out->zone_count = ZoneId_Num;
	out->camera_x = scrpos_x.f.u;
	out->camera_y = scrpos_y.f.u;
	out->player_x = player->pos.l.x.f.u;
	out->player_y = player->pos.l.y.f.u;
	out->objects = out->zone < ZoneId_Num && out->act < 4 ? level_obj[out->zone][out->act] : NULL;
	out->rings = out->zone < ZoneId_Num && out->act < 4 ? level_ring[out->zone][out->act] : NULL;
}
