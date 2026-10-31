#include "DebugPeek.h"

#include "Demo.h"
#include "Game.h"
#include "LevelDraw.h"
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
	return SST_COUNT;
}

// The object id a slot runs (the table's id whose routine is the slot's; 0 for a free slot)
static uint8_t SlotId(const Sst *o) {
	if (o->code == NULL)
		return 0;
	for (int id = 1; id < 0x100; id++)
		if (sst_object_codes[id] == o->code)
			return (uint8_t)id;
	return 0;
}

void Peek_GetObject(int slot, ObjectPeek *out) {
	const Sst *o = &sst[slot];
	out->type = SlotId(o);
	out->routine = o->routine;
	out->routine_sec = o->routine_secondary;
	out->frame = o->mapping_frame;
	out->anim = o->anim;
	out->render = o->render_flags;
	out->status = o->status;
	out->subtype = o->subtype;
	out->x = o->x_pos.f.u;
	out->y = o->y_pos.f.u;
	out->xsp = o->x_vel;
	out->ysp = o->y_vel;
}

const uint8_t *Peek_ObjectBytes(int slot, int *size) {
	*size = (int)sizeof(Sst);
	return (const uint8_t *)&sst[slot];
}

void Peek_EditorState(EditorPeek *out) {
	out->in_level = (gamemode & 0x7F) == GameMode_Level || (gamemode & 0x7F) == GameMode_Demo || (gamemode & 0x7F) == GameMode_Editor; // (a demo is a level that plays itself)
	out->zone = LEVEL_ZONE(level_id);
	out->act = LEVEL_ACT(level_id);
	out->zone_count = ZoneId_Num;
	out->camera_x = scrpos_x.f.u;
	out->camera_y = scrpos_y.f.u;
	out->player_x = player->x_pos.f.u;
	out->player_y = player->y_pos.f.u;
	out->objects = out->zone < ZoneId_Num && out->act < 4 ? level_obj[out->zone][out->act] : NULL;
	out->rings = out->zone < ZoneId_Num && out->act < 4 ? level_ring[out->zone][out->act] : NULL;
}

#ifndef ZONE_SLOTS // (Sonic 2 has its own: Tails is a player too)
// While the level editor has the level the player and the HUD do not run or show (the editor draws its own)
bool Game_EditorSkipObject(const void *obj) {
	return obj == (const void *)player;
}
#endif

// The level editor changed blocks or chunks: both planes are drawn from the tables again
void Game_EditorRedraw(void) {
	LoadTilesFromStart();
}

// The level editor's mode and the game's own: `editing` is the level screen without play (GameMode_Editor: no demo, no pause, no fade out, no deaths), else the level is played as it is (GameMode_Level). A demo that was running
// is over once the editor has the level (the player is not driven by its data any more).
// The checkpoint state the level had when the editor took it: a playtest that touches a starpost must not leave it for the next level (a demo would start at the last edited level's checkpoint)
static uint8_t saved_last_lamp, saved_prev_lamp;

void Game_EditorLamp(int action) {
	if (action == 0) {
		saved_last_lamp = last_lamp;
		saved_prev_lamp = prev_lamp;
	} else {
		last_lamp = saved_last_lamp;
		prev_lamp = saved_prev_lamp;
	}
}

void Game_EditorSetMode(bool editing) {
	const int mode = gamemode & 0x7F;
	if (mode != GameMode_Level && mode != GameMode_Demo && mode != GameMode_Editor)
		return;
	Game_EditorLamp(1); // (a playtest's checkpoints do not stay)
	if (editing) {
		demo = 0;
		cli_demo_override = NULL;
		gamemode = (gamemode & 0x80) | GameMode_Editor;
	} else {
		gamemode = (gamemode & 0x80) | GameMode_Level;
	}
}
