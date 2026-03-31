#include "LevelScroll.h"

#include "Video.h"
#include "Level.h"
#include "LevelDraw.h"
#include "Game.h"

#include "Object/Sonic.h"

//Level scroll state
uint8_t nobgscroll, bgscrollvert;

uint16_t fg_scroll_flags,     bg1_scroll_flags,     bg2_scroll_flags,     bg3_scroll_flags;
uint16_t fg_scroll_flags_dup, bg1_scroll_flags_dup, bg2_scroll_flags_dup, bg3_scroll_flags_dup;

dword_s scrpos_x,     scrpos_y,     bg_scrpos_x,     bg_scrpos_y,     bg2_scrpos_x,     bg2_scrpos_y,     bg3_scrpos_x,     bg3_scrpos_y;
dword_s scrpos_x_dup, scrpos_y_dup, bg_scrpos_x_dup, bg_scrpos_y_dup, bg2_scrpos_x_dup, bg2_scrpos_y_dup, bg3_scrpos_x_dup, bg3_scrpos_y_dup;

int16_t scrshift_x, scrshift_y;

uint8_t fg_xblock, bg1_xblock, bg2_xblock, bg3_xblock;
uint8_t fg_yblock, bg1_yblock, bg2_yblock, bg3_yblock;

int16_t look_shift;

static ALIGNED4 uint8_t bgscroll_buffer[0x200];

const int8_t Drown_WobbleData[] = {
#ifdef SCP_REV00
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
	2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
	2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -3, -3, -3, -3, -3,
	-3, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,
	-4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3,
	-3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1
#else
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
	2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
	2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -3, -3, -3, -3, -3,
	-3, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,
	-4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3,
	-3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1,
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
	2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2,
	2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,
	0, -1, -1, -1, -1, -1, -2, -2, -2, -2, -2, -3, -3, -3, -3, -3,
	-3, -3, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4,
	-4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -3,
	-3, -3, -3, -3, -3, -3, -2, -2, -2, -2, -2, -1, -1, -1, -1, -1
#endif
};

// Water ripple data (standard LZ table)
const int8_t Lz_Scroll_Data[] = {
	1,  1,  2,  2,  3,  3,  3,  3,  2,  2,  1,  1,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	-1, -1, -2, -2, -3, -3, -3, -3, -2, -2, -1, -1,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	1,  1,  2,  2,  3,  3,  3,  3,  2,  2,  1,  1,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};


// Helper to fill gradients into the background scroll buffer
static void FillGradient(int16_t count, int16_t start, int32_t delta, int16_t *dest) {
	uint32_t acc = (uint16_t)start << 16;
	for (int i = 0; i < count; i++) {
		*dest++ = (int16_t)(acc >> 16);
		acc += delta;
	}
}

void ApplyScrollUpdate(int16_t current_y, int16_t previous_y, uint8_t *block_state, uint16_t *flags, uint16_t up_bit, uint16_t down_bit) {
	if ((current_y & 0x10) != (*block_state & 0x10)) {
		*block_state ^= 0x10;
		if (current_y < previous_y)
			*flags |= up_bit;
		else
			*flags |= down_bit;
	}
}

// Applies background buffer to the H-scroll table in 16-pixel chunks (REV01 style)
	void BGScroll_X(int16_t y_pos, int16_t buf_offset) {
	int16_t *bufp = &hscroll_buffer[0][0];
	int16_t *bg_ptr = (int16_t*)&bgscroll_buffer[buf_offset];
	int16_t fg_x = -scrpos_x.f.u;

	int16_t lines_left = SCREEN_HEIGHT;
	int16_t chunk_h = 16 - (y_pos & 0xF);

	while (lines_left > 0) {
		int16_t bg_x = *bg_ptr++;
		int16_t step = (lines_left < chunk_h) ? lines_left : chunk_h;
		for (int i = 0; i < step; i++) {
			*bufp++ = fg_x;
			*bufp++ = bg_x;
		}
		lines_left -= step;
		chunk_h = 16;
	}
}

#ifdef SCP_REV01
void UpdateBGScroll(dword_s *pos, int32_t delta, uint8_t *block, uint16_t *flags, uint16_t neg_bit, uint16_t pos_bit) {
	int32_t old_v = pos->v;
	pos->v += delta;

	if ((pos->f.u & 0x10) != (*block & 0x10)) {
		*block ^= 0x10;
		if (pos->v < old_v)
			*flags |= neg_bit;
		else
			*flags |= pos_bit;
	}
}

void BGScroll_XY(int32_t x_off, int32_t y_off) {
	UpdateBGScroll(&bg_scrpos_x, x_off, &bg1_xblock, &bg1_scroll_flags, SCROLL_FLAG_LEFT, SCROLL_FLAG_RIGHT);
	UpdateBGScroll(&bg_scrpos_y, y_off, &bg1_yblock, &bg1_scroll_flags, SCROLL_FLAG_UP, SCROLL_FLAG_DOWN);
}

void BGScroll_Y(int32_t y_off) {
	UpdateBGScroll(&bg_scrpos_y, y_off, &bg1_yblock, &bg1_scroll_flags, SCROLL_FLAG_UP2, SCROLL_FLAG_DOWN2);
}

void BGScroll_YAbsolute(uint16_t y_pos) {
	int16_t old_y = bg_scrpos_y.f.u;
	bg_scrpos_y.f.u = (int16_t)y_pos;
	ApplyScrollUpdate(bg_scrpos_y.f.u, old_y, &bg1_yblock, &bg1_scroll_flags, SCROLL_FLAG_UP, SCROLL_FLAG_DOWN);
}
#endif

//Scroll draw functions
#ifdef SCP_REV00
/**
 * ScrollBlock1
 * Updates BG1 X and Y positions. Both axes trigger 16-pixel boundary flags.
 */
void ScrollBlock1(int32_t x, int32_t y) {
    // --- X-AXIS ---
    int32_t prev_x = bg_scrpos_x.v;
    bg_scrpos_x.v += x;

    // Check 16-pixel boundary
    if (!((bg_scrpos_x.f.u & 0x10) ^ bg1_xblock)) {
        bg1_xblock ^= 0x10;
        if (bg_scrpos_x.v < prev_x)
            bg1_scroll_flags |= SCROLL_FLAG_LEFT;
        else
            bg1_scroll_flags |= SCROLL_FLAG_RIGHT;
    }

    // --- Y-AXIS ---
    int32_t prev_y = bg_scrpos_y.v;
    bg_scrpos_y.v += y;

    // Check 16-pixel boundary
    if (!((bg_scrpos_y.f.u & 0x10) ^ bg1_yblock)) {
        bg1_yblock ^= 0x10;
        if (bg_scrpos_y.v < prev_y)
            bg1_scroll_flags |= SCROLL_FLAG_UP;
        else
            bg1_scroll_flags |= SCROLL_FLAG_DOWN;
    }
}

/**
 * ScrollBlock2
 * Updates BG1 X and Y positions. Only the Y axis triggers boundary flags.
 */
void ScrollBlock2(int32_t x, int32_t y) {
    // X updates silently
    bg_scrpos_x.v += x;

    // --- Y-AXIS ---
    int32_t prev_y = bg_scrpos_y.v;
    bg_scrpos_y.v += y;

    uint8_t current_block_bit = (uint8_t)(bg_scrpos_y.f.u & 0x10);
    if ((current_block_bit ^ bg1_yblock) == 0) {
        bg1_yblock ^= 0x10;
        if (bg_scrpos_y.v < prev_y)
            bg1_scroll_flags |= SCROLL_FLAG_UP;
        else
            bg1_scroll_flags |= SCROLL_FLAG_DOWN;
    }
}

/**
 * ScrollBlock3
 * Updates BG1 X and Y positions. Only the Y axis triggers boundary flags.
 * (Converted from absolute Y to delta-based Y for prototype sync)
 */
void ScrollBlock3(int32_t x, int32_t y) {
    // X updates silently
    bg_scrpos_x.v += x;

    // --- Y-AXIS ---
    int32_t prev_y = bg_scrpos_y.v;
    bg_scrpos_y.v += y;

    uint8_t current_block_bit = (uint8_t)(bg_scrpos_y.f.u & 0x10);
    if ((current_block_bit ^ bg1_yblock) == 0) {
        bg1_yblock ^= 0x10;
        if (bg_scrpos_y.v < prev_y)
            bg1_scroll_flags |= SCROLL_FLAG_UP;
        else
            bg1_scroll_flags |= SCROLL_FLAG_DOWN;
    }
}

/**
 * ScrollBlock4
 * Updates BG2 X and Y positions. Only the X axis triggers boundary flags.
 */
void ScrollBlock4(int32_t x, int32_t y) {
    // Y updates silently
    bg2_scrpos_y.v += y;

    // --- X-AXIS ---
    int32_t prev_x = bg2_scrpos_x.v;
    bg2_scrpos_x.v += x;

    uint8_t current_block_bit = (uint8_t)(bg2_scrpos_x.f.u & 0x10);
    if ((current_block_bit ^ bg2_xblock) == 0) {
        bg2_xblock ^= 0x10;
        if (bg2_scrpos_x.v < prev_x)
            bg2_scroll_flags |= SCROLL_FLAG_LEFT;
        else
            bg2_scroll_flags |= SCROLL_FLAG_RIGHT;
    }
}
#else
void BGScroll_Block1(int32_t x, uint8_t bit) {
    int32_t prev_x = bg_scrpos_x.v;
    bg_scrpos_x.v += x;
    uint8_t no_scroll = (bg_scrpos_x.f.u & 0x10) ^ bg1_xblock;
    if (no_scroll)
        return;
    bg1_xblock ^= 0x10;
    if (bg_scrpos_x.v < prev_x)
        bg1_scroll_flags |= bit;
    else
        bg1_scroll_flags |= (bit << 1);
}

void BGScroll_Block2(int32_t x, uint8_t bit) {
    int32_t prev_x = bg2_scrpos_x.v;
    bg2_scrpos_x.v += x;
    int32_t new_x = bg2_scrpos_x.v;
    uint8_t current_x_bit = (uint8_t)(bg2_scrpos_x.f.u & 0x10);
    if ((current_x_bit ^ bg2_xblock) != 0)
        return;
    bg2_xblock ^= 0x10;
    if ((new_x - prev_x) < 0)
        bg2_scroll_flags |= (1 << bit);
    else
        bg2_scroll_flags |= (1 << (bit + 1));
}

void BGScroll_Block3(int32_t x, uint8_t bit) {
    int32_t prev_x_fixed = bg3_scrpos_x.v;
    int32_t new_x_fixed = prev_x_fixed + x;
	bg3_scrpos_x.v = new_x_fixed;
    uint16_t current_block_bit = (uint16_t)((uint32_t)new_x_fixed >> 16) & 0x10;
    if ((current_block_bit ^ bg3_xblock) != 0)
        return;
    bg3_xblock ^= 0x10;
    if (new_x_fixed < prev_x_fixed) {
        bg3_scroll_flags |= (1 << bit);
    } else {
         bg3_scroll_flags |= (1 << (bit + 1));
    }
}
#endif

//Level deformation routines
static void FillHScroll(int16_t **bufp, int16_t fg_x, int16_t bg_x, int count) {
	for (int i = 0; i < count; i++) {
		*(*bufp)++ = fg_x; // Plane A (Foreground)
		*(*bufp)++ = bg_x; // Plane B (Background)
	}
}

static void FillHScrollWater(int16_t **bufp, int16_t fg_x, int16_t bg_base_x, int32_t delta, int count) {
	int32_t acc = (int32_t)bg_base_x << 16;
	for (int i = 0; i < count; i++) {
		*(*bufp)++ = fg_x;
		*(*bufp)++ = -(int16_t)(acc >> 16);
		acc += delta;
	}
}

void Deform_GHZ() {
	int16_t fg_x, bg_x;
	int16_t *bufp = &hscroll_buffer[0][0];
	int16_t y_offset;
#ifdef SCP_REV00
	int32_t dist = (int32_t)scrshift_x << 5;
	dist += (dist << 1); // shift * 96
	BGScroll_Block3(dist, SCROLL_FLAG_LEFT2);
	BGScroll_Block1(dist, SCROLL_FLAG_LEFT2);
	y_offset = 0x26 - ((scrpos_y.f.u & 0x7FF) >> 5);
	bg_scrpos_y.f.u = y_offset; // ScrollBlock3
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u; // v_bgscrposy_vdp = v_bgscreenposy
	fg_x = (gamemode == GameMode_Title) ? 0 : -scrpos_x.f.u;
	FillHScroll(&bufp, fg_x, -bg_scrpos_x.f.u, 0x70 - y_offset);
	FillHScroll(&bufp, fg_x, -bg2_scrpos_x.f.u, 0x28);
	int16_t d2_dist = (int16_t)scrpos_x.f.u - 0x200 - bg2_scrpos_x.f.u;
	int32_t delta = (int32_t)((int16_t)(((int32_t)d2_dist << 8) / 0x68)) << 8;
	FillHScrollWater(&bufp, fg_x, bg2_scrpos_x.f.u, delta, 0x48 + y_offset);
#else
	BGScroll_Block3((scrshift_x << 6) + (scrshift_x << 5), SCROLL_FLAG_LEFT2);
	BGScroll_Block2(scrshift_x << 7, SCROLL_FLAG_LEFT2);
	y_offset = 0x20 - ((scrpos_y.f.u & 0x7FF) >> 5);
	if (y_offset < 0)
		y_offset = 0;
	vid_bg_scrpos_y_dup = y_offset;
	fg_x = (gamemode == GameMode_Title) ? 0 : -scrpos_x.f.u;
	int32_t *scroll = (int32_t*)bgscroll_buffer;
	scroll[0] += 0x10000;
	scroll[1] += 0xC000;
	scroll[2] += 0x8000;
	bg_x = -(bg3_scrpos_x.f.u + (scroll[0] >> 16));
	FillHScroll(&bufp, fg_x, bg_x, 0x20 - y_offset);
	bg_x = -(bg3_scrpos_x.f.u + (scroll[1] >> 16));
	FillHScroll(&bufp, fg_x, bg_x, 0x10);
	bg_x = -(bg3_scrpos_x.f.u + (scroll[2] >> 16));
	FillHScroll(&bufp, fg_x, bg_x, 0x10);
	FillHScroll(&bufp, fg_x, -bg3_scrpos_x.f.u, 0x30);
	FillHScroll(&bufp, fg_x, -bg2_scrpos_x.f.u, 0x28);
	int16_t d2_dist = (int16_t)scrpos_x.f.u - bg2_scrpos_x.f.u;
	int32_t delta = (int32_t)((int16_t)(((int32_t)d2_dist << 8) / 0x68)) << 8;
	FillHScrollWater(&bufp, fg_x, bg2_scrpos_x.f.u, delta, 0x48 + SCREEN_TALLADD + y_offset);
#endif
}

void Deform_LZ() {
#ifdef SCP_REV00
	BGScroll_Block1(scrshift_x << 7, scrshift_y << 7);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int16_t fg_x = -scrpos_x.f.u;
	int16_t bg_x = -bg_scrpos_x.f.u;
	int16_t *bufp = &hscroll_buffer[0][0];
	for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
#else
	BGScroll_XY(scrshift_x << 7, scrshift_y << 7);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	uint8_t d2 = (uint8_t)lz_deform;
	uint8_t d3 = d2;
	lz_deform += 0x80;

	d2 += (uint8_t)bg_scrpos_y.f.u;
	d3 += (uint8_t)scrpos_y.f.u;

	int16_t fg_x_base = -scrpos_x.f.u;
	int16_t bg_x_base = -bg_scrpos_x.f.u;
	int16_t *bufp = &hscroll_buffer[0][0];

	for (int i = 0; i < SCREEN_HEIGHT; i++) {
		if ((scrpos_y.f.u + i) >= (uint16_t)wtr_pos1) {
			*bufp++ = fg_x_base + (int16_t)Lz_Scroll_Data[d3];
			*bufp++ = bg_x_base + (int16_t)Drown_WobbleData[d2];
		} else {
			*bufp++ = fg_x_base; *bufp++ = bg_x_base;
		}
		d2++; d3++;
	}
#endif
}

void Deform_MZ() {
#ifdef SCP_REV00
	BGScroll_Block1((scrshift_x << 6) * 3, 0);
	int16_t y_off = 0x200;
	int16_t dy = scrpos_y.f.u - 0x1C8;
	if (dy >= 0) y_off += (dy * 3) >> 2;
	bg2_scrpos_y.f.u = y_off;
	BGScroll_Block3(0, 0);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int16_t fg_x = -scrpos_x.f.u;
	int16_t bg_x = -bg_scrpos_x.f.u;
	int16_t *bufp = &hscroll_buffer[0][0];
	for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
#else
	BGScroll_Block1((scrshift_x << 6) * 3, SCROLL_FLAG_RIGHT); // bit 3
	BGScroll_Block3(scrshift_x << 6, SCROLL_FLAG_DOWN2);       // bit 5
	BGScroll_Block2(scrshift_x << 7, SCROLL_FLAG_UP2);         // bit 4

	int16_t y_off = 0x200;
	int16_t dy = scrpos_y.f.u - 0x1C8;
	if (dy >= 0) y_off += (dy * 3) >> 2;

	bg2_scrpos_y.f.u = bg3_scrpos_y.f.u = y_off;
	BGScroll_YAbsolute(bg2_scrpos_y.f.u);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	bg3_scroll_flags |= (bg1_scroll_flags | bg2_scroll_flags);
	bg1_scroll_flags = bg2_scroll_flags = 0;

	int16_t *buf_ptr = (int16_t*)bgscroll_buffer;
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 2) - base_x) << 12) / 5;
	FillGradient(5, base_x >> 1, delta << 4, buf_ptr); buf_ptr += 5;

	*buf_ptr++ = -bg3_scrpos_x.f.u;
	*buf_ptr++ = -bg3_scrpos_x.f.u;
	for (int i = 0; i < 9; i++) *buf_ptr++ = -bg2_scrpos_x.f.u;
	for (int i = 0; i < 16; i++) *buf_ptr++ = -bg_scrpos_x.f.u;

	int16_t scroll_y = bg_scrpos_y.f.u - 0x200;
	if (scroll_y > 0x100) scroll_y = 0x100;
	BGScroll_X(bg_scrpos_y.f.u, (scroll_y & 0x1F0) >> 3);
#endif
}

void Deform_SLZ() {
#ifdef SCP_REV00
	BGScroll_Block2(scrshift_x << 7, scrshift_y << 7);
#else
	BGScroll_Y(scrshift_y << 7);
#endif
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int16_t *buf_ptr = (int16_t*)bgscroll_buffer;
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 3) - base_x) << 12) / 28;
	FillGradient(28, base_x, delta << 4, buf_ptr); buf_ptr += 28;

	int16_t bld1 = (base_x >> 3);
#ifndef SCP_REV00
	bld1 += (bld1 >> 1);
#endif
	for (int i = 0; i < 5; i++) *buf_ptr++ = bld1;
	for (int i = 0; i < 5; i++) *buf_ptr++ = base_x >> 2;
	for (int i = 0; i < 30; i++) *buf_ptr++ = base_x >> 1;

	BGScroll_X(bg_scrpos_y.f.u, ((bg_scrpos_y.f.u - 0xC0) & 0x3F0) >> 3);
}

void Deform_SYZ() {
#ifdef SCP_REV00
	BGScroll_Block1(scrshift_x << 6, (scrshift_y << 4) * 3);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	int16_t fg_x = -scrpos_x.f.u;
	int16_t bg_x = -bg_scrpos_x.f.u;
	int16_t *bufp = &hscroll_buffer[0][0];
	for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
#else
	BGScroll_Y((scrshift_y << 4) * 3);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int16_t *buf_ptr = (int16_t*)bgscroll_buffer;
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 3) - base_x) << 11) / 8;
	FillGradient(8, base_x >> 1, delta << 5, buf_ptr); buf_ptr += 8;

	for (int i = 0; i < 5; i++) *buf_ptr++ = base_x >> 3;
	for (int i = 0; i < 6; i++) *buf_ptr++ = base_x >> 2;

	delta = (((int32_t)base_x - (base_x >> 1)) << 12) / 14;
	FillGradient(14, base_x >> 1, delta << 4, buf_ptr);

	BGScroll_X(bg_scrpos_y.f.u, (bg_scrpos_y.f.u & 0x1F0) >> 3);
#endif
}

void Deform_SBZ() {
	if (LEVEL_ACT(level_id) != 0) { // Act 2/3
		BGScroll_XY(scrshift_x << 6, scrshift_y << 5);
		vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
		int16_t fg_x = -scrpos_x.f.u;
		int16_t bg_x = -bg_scrpos_x.f.u;
		int16_t *bufp = &hscroll_buffer[0][0];
		for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
		return;
	}

#ifdef SCP_REV00
	BGScroll_Block1(scrshift_x << 6, scrshift_y << 5);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	int16_t fg_x = -scrpos_x.f.u;
	int16_t bg_x = -bg_scrpos_x.f.u;
	int16_t *bufp = &hscroll_buffer[0][0];
	for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
#else
	BGScroll_Block1(scrshift_x << 7, SCROLL_FLAG_RIGHT);
	BGScroll_Block3(scrshift_x << 6, SCROLL_FLAG_DOWN2);
	BGScroll_Block2((scrshift_x << 5) * 3, SCROLL_FLAG_UP2);

	BGScroll_Y(scrshift_y << 5);
	vid_bg_scrpos_y_dup = bg2_scrpos_y.f.u = bg3_scrpos_y.f.u = bg_scrpos_y.f.u;

	bg2_scroll_flags |= (bg1_scroll_flags | bg3_scroll_flags);
	bg1_scroll_flags = bg3_scroll_flags = 0;

	int16_t *buf_ptr = (int16_t*)bgscroll_buffer;
	int16_t d2_w = (-scrpos_x.f.u) >> 2;
	int32_t delta = (((int32_t)(d2_w >> 1) - d2_w) << 11) / 4;
	FillGradient(4, d2_w, delta << 5, buf_ptr); buf_ptr += 4;

	for (int i = 0; i < 10; i++) *buf_ptr++ = -bg3_scrpos_x.f.u;
	for (int i = 0; i < 7; i++)  *buf_ptr++ = -bg2_scrpos_x.f.u;
	for (int i = 0; i < 11; i++) *buf_ptr++ = -bg_scrpos_x.f.u;

	BGScroll_X(bg_scrpos_y.f.u, (bg_scrpos_y.f.u & 0x1F0) >> 3);
#endif
}

static void (*deform_routines[ZoneId_Num])() = {
	/* ZoneId_GHZ  */ Deform_GHZ,
	/* ZoneId_LZ   */ Deform_LZ,
	/* ZoneId_MZ   */ Deform_MZ,
	/* ZoneId_SLZ  */ Deform_SLZ,
	/* ZoneId_SYZ  */ Deform_SYZ,
	/* ZoneId_SBZ  */ Deform_SBZ,
	/* ZoneId_EndZ */ Deform_GHZ,
};

/**
 * BgScrollSpeed: Subroutine to set scroll speed of some backgrounds.
 * This handles multi-layered parallax scrolling and background positioning
 * based on the current zone and revision logic.
 */
void BgScrollSpeed(int16_t x, int16_t y) {
    /*
     * If the player is spawning from a checkpoint, the background
     * positions are loaded from saved state instead of the current camera.
     */
    if (last_lamp == 0) {
        /* Default background initialization */
        bg_scrpos_y.f.u = y;
        bg2_scrpos_y.f.u = y;
        bg_scrpos_x.f.u = x;
        bg2_scrpos_x.f.u = x;
        bg3_scrpos_x.f.u = x;
    }

    /* Perform zone-specific background manipulation */
    switch (LEVEL_ZONE(level_id)) {
        case ZoneId_GHZ: {
            #ifdef SCP_REV00
                /* Run the Green Hill Zone complex deformation routine */
                Deform_GHZ();
            #else
                /* In later revisions, simply lock all background positions to 0 */
                bg_scrpos_x.v = 0;
                bg_scrpos_y.v = 0;
                bg2_scrpos_y.v = 0;
                bg3_scrpos_y.v = 0;

                /* Clear the scrolling buffer used for cloud parallax */
                int32_t *scroll_ptr = (int32_t*)bgscroll_buffer;
                scroll_ptr[0] = 0;
                scroll_ptr[1] = 0;
                scroll_ptr[2] = 0;
            #endif
            break;
        }

        case ZoneId_LZ: {
            /* Background scrolls vertically at half the speed of the foreground */
            int32_t scroll_y = (int32_t)y >> 1;
            bg_scrpos_y.f.u = (int16_t)scroll_y;
            break;
        }

        /*
         * ZoneId_MZ: Marble Zone uses default scrolling logic.
         * Logic is empty (RTS), handled by default case.
         */

        case ZoneId_SLZ: {
            /* Apply half-speed vertical scrolling with a fixed offset */
            int32_t scroll_y = ((int32_t)y >> 1) + 0xC0;
            bg_scrpos_y.f.u = (int16_t)scroll_y;

            #ifndef SCP_REV00
                /* Force background horizontal scroll to zero for Rev 01+ */
                bg_scrpos_x.v = 0;
            #endif
            break;
        }

        case ZoneId_SYZ: {
            /*
             * Calculate specialized vertical parallax for Spring Yard Zone.
             * Resulting speed is approximately 18% of the foreground speed (y * 48 / 256).
             */
            int32_t scroll_y = (int32_t)y << 4;     /* y * 16 */
            scroll_y = (scroll_y << 1) + scroll_y; /* Multiply by 3 for y * 48 */
            scroll_y >>= 8;                        /* Divide by 256 */

            #ifndef SCP_REV00
                /* Apply small offset correction and lock horizontal scroll */
                scroll_y++;
                bg_scrpos_x.v = 0;
            #endif

            bg_scrpos_y.f.u = (int16_t)scroll_y;

            #ifdef SCP_REV00
                /* In Rev 00, mirror the vertical scroll to the secondary background */
                bg2_scrpos_y.f.u = (int16_t)scroll_y;
            #endif
            break;
        }

        case ZoneId_SBZ: {
            int32_t scroll_y;
            #ifdef SCP_REV00
                /* Scrap Brain background scrolls at 12.5% foreground speed (y / 8) */
                scroll_y = ((int32_t)y << 5) >> 8;
            #else
                /* Rev 01 uses a masked calculation with a small offset */
                scroll_y = (((int32_t)y & 0x7F8) >> 3) + 1;
            #endif
            bg_scrpos_y.f.u = (int16_t)scroll_y;
            break;
        }

        case ZoneId_EndZ: {
            #ifdef SCP_REV00
                /* The ending sequence background is locked to a static vertical position */
                bg_scrpos_y.f.u = 0x1E;
                bg2_scrpos_y.f.u = 0x1E;
            #else
                /*
                 * Handle multi-layered horizontal parallax for the ending sequence.
                 * Layers 1 and 2 move at half the speed of the foreground.
                 */
                int16_t horiz_scroll = scrpos_x.f.u >> 1;
                bg_scrpos_x.f.u = horiz_scroll;
                bg2_scrpos_x.f.u = horiz_scroll;

                /* Layer 3 moves at 75% of the speed of the other background layers */
                int16_t layer3_base = horiz_scroll >> 2;
                bg3_scrpos_x.f.u = layer3_base * 3;

                /* Reset all vertical background positions and buffers */
                bg_scrpos_y.v = 0;
                bg2_scrpos_y.v = 0;
                bg3_scrpos_y.v = 0;

                int32_t *scroll_ptr = (int32_t*)bgscroll_buffer;
                scroll_ptr[0] = 0;
                scroll_ptr[1] = 0;
                scroll_ptr[2] = 0;
            #endif
            break;
        }

        default:
            /* Unhandled zones or zones with no specific logic fall through to here */
            break;
    }
}

//Level scroll functions
static void SetScreenPosition(int16_t target_x) {
	int16_t movement_delta = target_x - scrpos_x.f.u;
	scrpos_x.f.u = target_x;
	scrshift_x = (movement_delta << 8);
}

static void MoveAheadOfMid(int16_t push_amount) {
	if ((uint16_t)push_amount >= 16)
		push_amount = 16;

	int16_t target_x = scrpos_x.f.u + push_amount;
	if (target_x >= limit_right2)
		target_x = limit_right2;

	SetScreenPosition(target_x);
}

static void MoveBehindMid(int16_t push_amount) {
#if SCP_FIX_BUGS
	if (push_amount <= -16)
		push_amount = -16;
#endif
	int16_t target_x = scrpos_x.f.u + push_amount;
	if (target_x <= limit_left2)
		target_x = limit_left2;

	SetScreenPosition(target_x);
}

void MoveScreenHoriz() {
	int16_t distance_to_player = player->pos.l.x.f.u - scrpos_x.f.u;
#if SCP_FIX_BUGS
	int16_t push_left = distance_to_player - 144;
	if (push_left < 0) {
		MoveBehindMid(push_left);
		return;
	}
	int16_t push_right = push_left - 16;
	if (push_right >= 0) {
		MoveAheadOfMid(push_right);
		return;
	}
#else
	if ((uint16_t)distance_to_player < 144) {
		MoveBehindMid(distance_to_player - 144);
		return;
	}
	int16_t push_right = distance_to_player - 144;
	if ((uint16_t)push_right >= 16) {
		MoveAheadOfMid(push_right - 16);
		return;
	}
#endif
	scrshift_x = 0;
}

void ScrollHoriz() {
	int16_t prev_x = scrpos_x.f.u;
	MoveScreenHoriz();
	if (((scrpos_x.f.u & 0x10) ^ fg_xblock) == 0) {
		fg_xblock ^= 0x10;
		if (scrpos_x.f.u < prev_x)
			fg_scroll_flags |= SCROLL_FLAG_LEFT;
		else
			fg_scroll_flags |= SCROLL_FLAG_RIGHT;
	}
}

static void LimitScrollTop(dword_s *scroll) {
	if (scroll->f.u <= (int16_t)limit_top2) {
		if (scroll->f.u <= -0x100) 	{
			// Perform vertical wrapping
			scroll->f.u &= 0x7FF;
			player->pos.l.y.f.u &= 0x7FF;
			scrpos_y.f.u &= 0x7FF;
			bg_scrpos_y.f.u &= 0x3FF;
		} else
			scroll->f.u = (int16_t)limit_top2;
	}
}

static void LimitScrollBottom(dword_s *scroll) {
	if (scroll->f.u >= (int16_t)limit_btm2) {
		if ((scroll->f.u - 0x800) >= 0) {
			// Perform vertical wrapping
			scroll->f.u -= 0x800;
			player->pos.l.y.f.u &= 0x7FF;
			scrpos_y.f.u -= 0x800;
			bg_scrpos_y.f.u &= 0x3FF;
		} else
			scroll->f.u = (int16_t)limit_btm2;
	}
}

void ScrollVertical() {
	dword_s scroll;
	int16_t y = player->pos.l.y.f.u - scrpos_y.f.u;
	uint16_t speed = 0;
	bool force_snap = false;

	if (player->status.p.f.in_ball)
		y -= 5;

	if (player->status.p.f.in_air) {
		y += 32 - look_shift;
		if (y < 0 || (y - 64) >= 0)
			speed = 0x1000;
		else if (!bgscrollvert) {
			scrshift_y = 0;
			return;
		} else {
			force_snap = true;
			bgscrollvert = false;
		}
	} else {
		y -= look_shift;
		if (y != 0) {
			if (look_shift == (96 + SCREEN_TALLADD2)) {
				uint16_t inertia_abs = (player->inertia < 0) ? -player->inertia : player->inertia;
				speed = (inertia_abs < 0x800) ? 0x600 : 0x1000;
			}
			else {
				speed = 0x200;
			}
		} else if (!bgscrollvert) {
			scrshift_y = 0;
			return;
		} else {
			force_snap = true;
			bgscrollvert = false;
		}
	}
	if (force_snap || (speed == 0x200 && y >= -2 && y <= 2) || (speed == 0x600 && y >= -6 && y <= 6) || (speed == 0x1000 && y >= -16 && y <= 16)) {
		scroll.v = 0;
		scroll.f.u = scrpos_y.f.u + (force_snap ? 0 : y);
	} else {
		int32_t delta = (int32_t)speed << 8;
		scroll.v = (y < 0) ? (scrpos_y.v - delta) : (scrpos_y.v + delta);
	}
	if (y < 0)
		LimitScrollTop(&scroll);
	else
		LimitScrollBottom(&scroll);
	int16_t old_y = scrpos_y.f.u;
	scrshift_y = (int16_t)((scroll.v - scrpos_y.v) >> 8);
	scrpos_y.v = scroll.v;
	ApplyScrollUpdate(scrpos_y.f.u, old_y, &fg_yblock, &fg_scroll_flags, SCROLL_FLAG_UP, SCROLL_FLAG_DOWN);
}

void DeformLayers()
{
	//Check if we're allowed to scroll
	if (nobgscroll)
		return;
	
	//Clear previous flags
	fg_scroll_flags = 0;
	bg1_scroll_flags = 0;
	bg2_scroll_flags = 0;
	bg3_scroll_flags = 0;
	
	//Scroll camera
	ScrollHoriz();
	ScrollVertical();
	DynamicLevelEvents();
	
	//Copy screen Y position
	vid_scrpos_y_dup = scrpos_y.f.u;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	
	//Run zone's background deformation routine
	if (deform_routines[LEVEL_ZONE(level_id)] != NULL)
		deform_routines[LEVEL_ZONE(level_id)]();
}
