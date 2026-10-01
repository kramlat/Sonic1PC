#include "DebugPeek.h"

#include <string.h>

// Variable watch table for the Qt backend's Variables viewer. First generated from the extern
// declarations in the headers below (scalars, 16.16 fixed-point values and fixed-size arrays), then
// kept as a plain hand-editable list: add a line (WATCH / WATCH_ARRAY) to expose a new variable.
// Names are shown as written, grouped by the header that declares them.

#include "Demo.h"
#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "SpecialStage.h"

#define KIND(x) _Generic((x), \
	uint8_t: VAR_U8, int8_t: VAR_S8, uint16_t: VAR_U16, int16_t: VAR_S16, \
	uint32_t: VAR_U32, int32_t: VAR_S32, _Bool: VAR_BOOL, dword_s: VAR_FIXED)
#define WATCH(group, x)       { #x, group, &(x), 1, sizeof(x), KIND(x) },
#define WATCH_ARRAY(group, x) { #x, group, &((x)[0]), (int)(sizeof(x) / sizeof((x)[0])), sizeof((x)[0]), KIND((x)[0]) },

static const struct {
	const char *name, *group;
	void *addr;
	int count;
	size_t elem_size;
	int kind;
} vars[] = {
	WATCH("Demo", btn_pushtime1)
	WATCH("Demo", btn_pushtime2)
	WATCH("Demo", cli_demo_record)
	WATCH("Demo", cli_demo_record_frames)
	WATCH("Game", CRAMPAL)
	WATCH("Game", VDP_PALETTE_DISPLAY)
	WATCH("Game", VRAMADDR)
	WATCH("Game", Z80_PEEK_DISPLAY)
	WATCH_ARRAY("Game", buffer0000)
	WATCH("Game", cli_force_demo)
	WATCH("Game", cli_special_stage)
	WATCH("Game", cli_start_level)
	WATCH("Game", cli_start_special)
	WATCH("Game", cli_start_x)
	WATCH("Game", cli_start_y)
	WATCH("Game", credits_cheat)
	WATCH("Game", credits_num)
	WATCH("Game", debug_cheat)
	WATCH("Game", debug_mode)
	WATCH("Game", demo)
	WATCH("Game", demo_length)
	WATCH("Game", gamemode)
	WATCH("Game", jpad1_hold1)
	WATCH("Game", jpad1_hold2)
	WATCH("Game", jpad1_hold_ext)
	WATCH("Game", jpad1_press1)
	WATCH("Game", jpad1_press2)
	WATCH("Game", jpad1_press_ext)
	WATCH("Game", jpad2_hold)
	WATCH("Game", jpad2_press)
	WATCH("Game", vbla_count)
	WATCH("Level", air)
	WATCH("Level", big_ring)
	WATCH("Level", big_ring_collected)
	WATCH("Level", boss_status)
	WATCH("Level", collision_path)
	WATCH("Level", continues)
	WATCH("Level", convey_rev)
	WATCH("Level", debug_item)
	WATCH("Level", debug_speed)
	WATCH("Level", debug_speed_timer)
	WATCH("Level", debug_subtype)
	WATCH("Level", debug_use)
	WATCH("Level", dle_routine)
	WATCH("Level", doupdatesinhblank)
	WATCH("Level", endact_bonus)
	WATCH("Level", f_lz1tunnel_open)
	WATCH("Level", f_slidemode)
	WATCH_ARRAY("Level", f_switch)
	WATCH("Level", f_wtunneldisallow)
	WATCH("Level", frame_count)
	WATCH("Level", gfx_big_ring)
	WATCH("Level", hblank_pal)
	WATCH("Level", invincibility)
	WATCH("Level", item_bonus)
	WATCH("Level", jump_only)
	WATCH("Level", last_lamp)
	WATCH("Level", last_special)
	WATCH("Level", level_id)
	WATCH_ARRAY("Level", level_map16)
	WATCH("Level", life_count)
	WATCH("Level", life_num)
	WATCH("Level", limit_btm1)
	WATCH("Level", limit_btm2)
	WATCH("Level", limit_btm_db)
	WATCH("Level", limit_left1)
	WATCH("Level", limit_left2)
	WATCH("Level", limit_left3)
	WATCH("Level", limit_right1)
	WATCH("Level", limit_right2)
	WATCH("Level", limit_top1)
	WATCH("Level", limit_top2)
	WATCH("Level", limit_top_db)
	WATCH("Level", lives)
	WATCH("Level", lock_ctrl)
	WATCH("Level", lock_multi)
	WATCH("Level", lock_screen)
	WATCH("Level", lz_deform)
	WATCH("Level", obj31_ypos)
	WATCH_ARRAY("Level", obj63)
	WATCH_ARRAY("Level", obj63_loaded)
	WATCH("Level", obj6B)
	WATCH_ARRAY("Level", objstate)
	WATCH("Level", objstate_left)
	WATCH("Level", objstate_right)
	WATCH("Level", opl_routine)
	WATCH("Level", opl_screen)
	WATCH("Level", pause_state)
	WATCH("Level", prev_lamp)
	WATCH("Level", restart)
	WATCH("Level", ring_bonus)
	WATCH("Level", ring_count)
	WATCH("Level", rings)
	WATCH("Level", score)
	WATCH("Level", score_count)
	WATCH("Level", score_life)
	WATCH("Level", shield)
	WATCH("Level", shoes)
	WATCH("Level", sonicend)
	WATCH("Level", sprite_anim_3buf)
	WATCH("Level", time_bonus)
	WATCH("Level", time_count)
	WATCH("Level", time_over)
	WATCH("Level", tunnel_allow)
	WATCH("Level", tunnel_mode)
	WATCH("Level", water)
	WATCH("Level", wtr_pos1)
	WATCH("Level", wtr_pos2)
	WATCH("Level", wtr_pos3)
	WATCH("Level", wtr_routine)
	WATCH("Level", wtr_state)
	WATCH("LevelCollision", angle_buffer0)
	WATCH("LevelCollision", angle_buffer1)
	WATCH("LevelDraw", scroll_block1_size)
	WATCH("LevelDraw", scroll_block2_size)
	WATCH("LevelDraw", scroll_block3_size)
	WATCH("LevelDraw", scroll_block4_size)
	WATCH("LevelScroll", bg1_scroll_flags)
	WATCH("LevelScroll", bg1_scroll_flags_dup)
	WATCH("LevelScroll", bg1_xblock)
	WATCH("LevelScroll", bg1_yblock)
	WATCH("LevelScroll", bg2_scroll_flags)
	WATCH("LevelScroll", bg2_scroll_flags_dup)
	WATCH("LevelScroll", bg2_scrpos_x)
	WATCH("LevelScroll", bg2_scrpos_x_dup)
	WATCH("LevelScroll", bg2_scrpos_y)
	WATCH("LevelScroll", bg2_scrpos_y_dup)
	WATCH("LevelScroll", bg2_xblock)
	WATCH("LevelScroll", bg2_yblock)
	WATCH("LevelScroll", bg3_scroll_flags)
	WATCH("LevelScroll", bg3_scroll_flags_dup)
	WATCH("LevelScroll", bg3_scrpos_x)
	WATCH("LevelScroll", bg3_scrpos_x_dup)
	WATCH("LevelScroll", bg3_scrpos_y)
	WATCH("LevelScroll", bg3_scrpos_y_dup)
	WATCH("LevelScroll", bg3_xblock)
	WATCH("LevelScroll", bg3_yblock)
	WATCH("LevelScroll", bg_scrpos_x)
	WATCH("LevelScroll", bg_scrpos_x_dup)
	WATCH("LevelScroll", bg_scrpos_y)
	WATCH("LevelScroll", bg_scrpos_y_dup)
	WATCH("LevelScroll", bgscrollvert)
	WATCH("LevelScroll", cam_x_delay)
	WATCH("LevelScroll", cam_y_delay)
	WATCH("LevelScroll", fg_scroll_flags)
	WATCH("LevelScroll", fg_scroll_flags_dup)
	WATCH("LevelScroll", fg_xblock)
	WATCH("LevelScroll", fg_yblock)
	WATCH("LevelScroll", look_shift)
	WATCH("LevelScroll", nobgscroll)
	WATCH("LevelScroll", scrpos_x)
	WATCH("LevelScroll", scrpos_x_dup)
	WATCH("LevelScroll", scrpos_y)
	WATCH("LevelScroll", scrpos_y_dup)
	WATCH("LevelScroll", scrshift_x)
	WATCH("LevelScroll", scrshift_y)
	WATCH("Palette", pal_chgspeed)
	WATCH("PaletteCycle", f_conveyrev)
	WATCH_ARRAY("PaletteCycle", pcyc_buffer)
	WATCH("PaletteCycle", pcyc_num)
	WATCH("PaletteCycle", pcyc_time)
	WATCH_ARRAY("SpecialStage", emerald_list)
	WATCH("SpecialStage", emeralds)
	WATCH("SpecialStage", palss_num)
	WATCH("SpecialStage", palss_time)
	WATCH("SpecialStage", ss_rotate)
};

#define VAR_COUNT ((int)(sizeof(vars) / sizeof(vars[0])))

int Peek_VarCount(void) {
	return VAR_COUNT;
}

void Peek_GetVar(int i, VarPeek *out) {
	out->name = vars[i].name;
	out->group = vars[i].group;
	out->count = vars[i].count;
	out->elem_size = (int)vars[i].elem_size;
	out->kind = vars[i].kind;
}

// Values come back as a sign-extended int64 (unsigned kinds zero-extended; a
// FIXED value is the raw 32-bit 16.16 word).
int64_t Peek_VarRead(int i, int element) {
	const uint8_t *p = (const uint8_t *)vars[i].addr + (size_t)element * vars[i].elem_size;
	switch (vars[i].kind) {
		case VAR_U8:    { uint8_t v;  memcpy(&v, p, sizeof(v)); return v; }
		case VAR_BOOL:  { uint8_t v;  memcpy(&v, p, sizeof(v)); return v != 0; }
		case VAR_S8:    { int8_t v;   memcpy(&v, p, sizeof(v)); return v; }
		case VAR_U16:   { uint16_t v; memcpy(&v, p, sizeof(v)); return v; }
		case VAR_S16:   { int16_t v;  memcpy(&v, p, sizeof(v)); return v; }
		case VAR_U32:   { uint32_t v; memcpy(&v, p, sizeof(v)); return v; }
		case VAR_S32:
		case VAR_FIXED: { int32_t v;  memcpy(&v, p, sizeof(v)); return v; }
	}
	return 0;
}

void Peek_VarWrite(int i, int element, int64_t value) {
	uint8_t *p = (uint8_t *)vars[i].addr + (size_t)element * vars[i].elem_size;
	switch (vars[i].kind) {
		case VAR_U8:   { uint8_t v = (uint8_t)value;   memcpy(p, &v, sizeof(v)); break; }
		case VAR_BOOL: { uint8_t v = value != 0;       memcpy(p, &v, sizeof(v)); break; }
		case VAR_S8:   { int8_t v = (int8_t)value;     memcpy(p, &v, sizeof(v)); break; }
		case VAR_U16:  { uint16_t v = (uint16_t)value; memcpy(p, &v, sizeof(v)); break; }
		case VAR_S16:  { int16_t v = (int16_t)value;   memcpy(p, &v, sizeof(v)); break; }
		case VAR_U32:  { uint32_t v = (uint32_t)value; memcpy(p, &v, sizeof(v)); break; }
		case VAR_S32:
		case VAR_FIXED: { int32_t v = (int32_t)value;  memcpy(p, &v, sizeof(v)); break; }
	}
}
