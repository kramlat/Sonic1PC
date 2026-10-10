// Sonic 2's level scrolling: Sonic 1's (a copy of LevelScroll.c) with Nick Arcade's zones: their background starts and deformation routines.
#include "LevelScroll.h"
#include "HTZQuake.h"
#include "HTZBackground.h"
#include "SplitScreen.h"

#include <string.h>
#include "Video.h"
#include "Level.h"
#include "LevelDraw.h"
#include "Game.h"

#include "Object/Sonic.h"

//Level scroll state
uint8_t nobgscroll, bgscrollvert;

uint16_t bg1_scroll_flags, bg2_scroll_flags, bg3_scroll_flags;
uint16_t bg1_scroll_flags_dup, bg2_scroll_flags_dup, bg3_scroll_flags_dup;

dword_s bg2_scrpos_x, bg2_scrpos_y;
dword_s scrpos_x_dup, scrpos_y_dup, bg_scrpos_x_dup, bg_scrpos_y_dup, bg2_scrpos_x_dup, bg2_scrpos_y_dup, bg3_scrpos_x_dup, bg3_scrpos_y_dup;

int16_t scrshift_x, scrshift_y;

uint8_t bg1_xblock, bg2_xblock, bg3_xblock;
uint8_t bg1_yblock, bg2_yblock, bg3_yblock;

// The foreground plane and what keeps it drawn as the camera moves (the engine's LevelPlane)
LevelPlane fg_plane = { .plane = &screen1p.plane_a, .camera_x = &scrpos_x, .camera_y = &scrpos_y }; // (valid before a level starts too)

int16_t look_shift;

uint16_t cam_x_delay;
uint8_t cam_y_delay;

static ALIGNED4 uint8_t bgscroll_buffer[0x200];

const int8_t Drown_WobbleData[] = {
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

// Real BGScroll_YRelative: the same relative Y move as BGScroll_Y, but flagging redraws in bits 0/1 (top/bottom row)
// instead of 4/5 -- SBZ act 1's own drawing (Draw_SBZ) only looks at bits 0/1.
void BGScroll_YRelative(int32_t y_off) {
	UpdateBGScroll(&bg_scrpos_y, y_off, &bg1_yblock, &bg1_scroll_flags, SCROLL_FLAG_UP, SCROLL_FLAG_DOWN);
}

void BGScroll_YAbsolute(uint16_t y_pos) {
	int16_t old_y = bg_scrpos_y.f.u;
	bg_scrpos_y.f.u = (int16_t)y_pos;
	ApplyScrollUpdate(bg_scrpos_y.f.u, old_y, &bg1_yblock, &bg1_scroll_flags, SCROLL_FLAG_UP, SCROLL_FLAG_DOWN);
}

//Scroll draw functions
void BGScroll_Block1(int32_t x, uint8_t bit)
{
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

void BGScroll_Block2(int32_t x, uint8_t bit)
{
	int32_t prev_x = bg2_scrpos_x.v;
	bg2_scrpos_x.v += x;
	uint8_t no_scroll = (bg2_scrpos_x.f.u & 0x10) ^ bg2_xblock;
	if (no_scroll)
		return;
	bg2_xblock ^= 0x10;
	if (bg2_scrpos_x.v < prev_x)
		bg2_scroll_flags |= bit;
	else
		bg2_scroll_flags |= (bit << 1);
}

void BGScroll_Block3(int32_t x, uint8_t bit)
{
	int32_t prev_x = bg3_scrpos_x.v;
	bg3_scrpos_x.v += x;
	uint8_t no_scroll = (bg3_scrpos_x.f.u & 0x10) ^ bg3_xblock;
	if (no_scroll)
		return;
	bg3_xblock ^= 0x10;
	if (bg3_scrpos_x.v < prev_x)
		bg3_scroll_flags |= bit;
	else
		bg3_scroll_flags |= (bit << 1);
}

//Level deformation routines
void Deform_GHZ(void)
{
	int16_t fg_x, bg_x;
	int16_t *bufp = &hscroll_buffer[0][0];
	//Scroll background layers
	BGScroll_Block3((scrshift_x << 6) + (scrshift_x << 5), SCROLL_FLAG_LEFT2); //Upper mountains
	BGScroll_Block2(scrshift_x << 7, SCROLL_FLAG_LEFT2); //Hills and waterfalls

	//Get Y position
	vid_bg_scrpos_y_dup =  0x20 - ((scrpos_y.f.u & 0x7FF) >> 5);
	if (vid_bg_scrpos_y_dup < 0)
		vid_bg_scrpos_y_dup = 0;

	//Get foreground position
	if (gamemode == GameMode_Title)
		fg_x = 0;
	else
		fg_x = -scrpos_x.f.u;

	//Scroll clouds
	int32_t *scroll = (int32_t*)bgscroll_buffer;
	scroll[0] += 0x10000;
	scroll[1] += 0xC000;
	scroll[2] += 0x8000;

	//Scroll cloud layer 1
	bg_x = -(bg3_scrpos_x.f.u + (scroll[0] >> 16));
	for (int i = 0; i < 0x20 - vid_bg_scrpos_y_dup; i++)
	{ *bufp++ = fg_x; *bufp++ = bg_x; }

	//Scroll cloud layer 2
	bg_x = -(bg3_scrpos_x.f.u + (scroll[1] >> 16));
	for (int i = 0; i < 0x10; i++)
	{ *bufp++ = fg_x; *bufp++ = bg_x; }

	//Scroll cloud layer 3
	bg_x = -(bg3_scrpos_x.f.u + (scroll[2] >> 16));
	for (int i = 0; i < 0x10; i++)
	{ *bufp++ = fg_x; *bufp++ = bg_x; }

	//Scroll upper mountains
	bg_x = -bg3_scrpos_x.f.u;
	for (int i = 0; i < 0x30; i++)
	{ *bufp++ = fg_x; *bufp++ = bg_x; }

	//Scroll hills and waterfalls
	bg_x = -bg2_scrpos_x.f.u;
	for (int i = 0; i < 0x28; i++)
	{ *bufp++ = fg_x; *bufp++ = bg_x; }

	//Scroll water
	int32_t wx = bg2_scrpos_x.v;
	int32_t wi = (((scrpos_x.f.u - bg2_scrpos_x.f.u) << 8) / 0x68) << 8;

	for (int i = 0; i < 0x48 + SCREEN_TALLADD + vid_bg_scrpos_y_dup; i++)
	{
		*bufp++ = fg_x; *bufp++ = -(wx >> 16);
		wx += wi;
	}
}

void Deform_MZ(void) {
	// Real ASM passes these as raw bit-INDICES (2, 6, 4) to bset, not the
	// named SCROLL_FLAG_* masks -- BGScroll_Block1/2/3 use the base bit for
	// a leftward/decreasing scroll and base+1 for rightward/increasing (see
	// their own real-ASM comment: "d6 = bit to set for redraw direction").
	// Block1's and Block3's masks here were previously off by one bit
	// (SCROLL_FLAG_RIGHT/DOWN2, i.e. index 3/5, instead of the real 2/6),
	// corrupting which rows/edges Draw_MZ's bit-masking logic (0xA8 = bits
	// 7,5,3) thinks need redrawing. Block2's SCROLL_FLAG_UP2 (index 4) was
	// already correct.
	BGScroll_Block1((scrshift_x << 6) * 3, SCROLL_FLAG_LEFT);  // bit 2 (real: moveq #2,d6)
	BGScroll_Block3(scrshift_x << 6, (uint8_t)(1 << 6));       // bit 6 (real: moveq #6,d6)
	BGScroll_Block2(scrshift_x << 7, SCROLL_FLAG_UP2);         // bit 4 (real: moveq #4,d6)

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
}

void Deform_SLZ(void) {
	BGScroll_Y(scrshift_y << 7);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int16_t *buf_ptr = (int16_t*)bgscroll_buffer;
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 3) - base_x) << 12) / 28;
	FillGradient(28, base_x, delta << 4, buf_ptr); buf_ptr += 28;

	int16_t bld1 = (base_x >> 3);
	bld1 += (bld1 >> 1);
	for (int i = 0; i < 5; i++) *buf_ptr++ = bld1;
	for (int i = 0; i < 5; i++) *buf_ptr++ = base_x >> 2;
	for (int i = 0; i < 30; i++) *buf_ptr++ = base_x >> 1;

	BGScroll_X(bg_scrpos_y.f.u, ((bg_scrpos_y.f.u - 0xC0) & 0x3F0) >> 3);
}

void Deform_SYZ(void) {
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
}

void Deform_SBZ(void) {
	if (LEVEL_ACT(level_id) != 0) { // Act 2/3
		BGScroll_XY(scrshift_x << 6, scrshift_y << 5);
		vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
		int16_t fg_x = -scrpos_x.f.u;
		int16_t bg_x = -bg_scrpos_x.f.u;
		int16_t *bufp = &hscroll_buffer[0][0];
		for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
		return;
	}

	// The last argument is the left-redraw flag's bit mask (right is the next bit up): real moveq #2/#6/#4,d6.
	// Block1 used to get SCROLL_FLAG_RIGHT and Block3 SCROLL_FLAG_DOWN2 -- one bit too high, so the redraw
	// flags landed on the wrong blocks and new right-edge columns were drawn from the wrong layer.
	BGScroll_Block1(scrshift_x << 7, SCROLL_FLAG_LEFT);        // bits 2/3
	BGScroll_Block3(scrshift_x << 6, (uint8_t)(1 << 6));       // bits 6/7
	BGScroll_Block2((scrshift_x << 5) * 3, SCROLL_FLAG_UP2);   // bits 4/5

	BGScroll_YRelative(scrshift_y << 5);
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
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Sonic 2 (Nick Arcade): the zones' deformation. Each zone's routine fills the horizontal scroll table (a foreground and a background X for every line), as the
// prototype's DeformBGLayer routines do (_inc/BgScrollSpeed & DeformBGLayer.asm); the 16.16 fixed point gradients are the original's own (a long that is swapped around
// an add), written here as a plain accumulator.
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
	int16_t *p;
	int left;
	int16_t fg, last_bg;
} HScroll;

static void HS_Line(HScroll *h, int16_t bg) {
	if (h->left <= 0)
		return;
	*h->p++ = h->fg;
	*h->p++ = bg;
	h->left--;
	h->last_bg = bg;
}

static void HS_Lines(HScroll *h, int n, int16_t bg) {
	while (n-- > 0)
		HS_Line(h, bg);
}

static void HS_Start(HScroll *h, int16_t fg) {
	h->p = &hscroll_buffer[0][0];
	h->left = SCREEN_HEIGHT;
	h->fg = fg;
	h->last_bg = 0;
}

// (a picture taller than the original's lines goes on with the last line's scroll)
static void HS_Finish(HScroll *h) {
	while (h->left > 0)
		HS_Line(h, h->last_bg);
}

// The per-line step of a gradient: divs.w of (diff << shift) by divisor, then to 16.16 (ext.l, asl #4, asl #8)
static int32_t HS_Delta(int16_t diff, int shift, int divisor) {
	return (int32_t)(int16_t)(((int32_t)diff << shift) / divisor) << 12;
}

// Cycling table of Emerald Hill's wavy water line (Deform_EHZ_Data)
static const int8_t Deform_EHZ_Data[0x40] = {
	1, 2, 1, 3, 1, 2, 2, 1, 2, 3, 1, 2, 1, 2, 0, 0,
	2, 0, 3, 2, 2, 3, 2, 2, 1, 3, 0, 0, 1, 0, 1, 3,
	1, 2, 1, 3, 1, 2, 2, 1, 2, 3, 1, 2, 1, 2, 0, 0,
	2, 0, 3, 2, 2, 3, 2, 2, 1, 3, 0, 0, 1, 0, 1, 3,
};
static int16_t ehz_wave;     // (TempArray_LayerDef's first word)
static uint8_t ehz_runcount; // (the low byte of Vint_runcount: the wave steps every eighth frame)

// Emerald Hill's lines, also the title screen's (which passes 0 for the foreground, and the title's own camera for d2 = the foreground's X)
static void EHZ_Lines(int16_t fg, int16_t d2) {
	HScroll h;
	HS_Start(&h, fg);
	HS_Lines(&h, 0x16, 0);        // sky
	int16_t d0 = d2 >> 6;
	HS_Lines(&h, 0x3A, d0);       // clouds
	if ((ehz_runcount++ & 7) == 0)
		ehz_wave--;
	const int8_t *wave = Deform_EHZ_Data + (ehz_wave & 0x1F);
	for (int i = 0; i < 0x15; i++) // the wavy line
		HS_Line(&h, (int16_t)(d0 + wave[i]));
	HS_Lines(&h, 0x0B, 0);
	d0 = d2 >> 4;
	HS_Lines(&h, 0x10, d0);
	d0 = (int16_t)(d0 + (d0 >> 1));
	HS_Lines(&h, 0x10, d0);
	// the water: a gradient of the foreground's X down to a third of it, in steps of one, two and three lines
	int32_t delta = HS_Delta((int16_t)((d2 >> 1) - (d2 >> 3)), 4, 0x30);
	uint32_t acc = (uint32_t)(uint16_t)(d2 >> 3) << 16;
	for (int i = 0; i < 0x0F; i++) {
		HS_Line(&h, (int16_t)(acc >> 16));
		acc += (uint32_t)delta;
	}
	for (int i = 0; i < 9; i++) {
		HS_Lines(&h, 2, (int16_t)(acc >> 16));
		acc += 2 * (uint32_t)delta;
	}
	for (int i = 0; i < 0x0F; i++) {
		HS_Lines(&h, 3, (int16_t)(acc >> 16));
		acc += 3 * (uint32_t)delta;
	}
	HS_Finish(&h);
}

void Deform_EHZ(void) {
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	EHZ_Lines((int16_t)-scrpos_x.f.u, (int16_t)-scrpos_x.f.u);
}

// The title screen (Deform_TitleScreen): its camera runs on by 8 a frame up to $1C00, the foreground stands still, and the background has Emerald Hill's lines
void Deform_TitleScreen(void) {
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	uint16_t x = (uint16_t)scrpos_x.f.u;
	if (x < 0x1C00)
		x += 8;
	scrpos_x.f.u = (int16_t)x;
	EHZ_Lines(0, (int16_t)-x);
}

// Chemical Plant (Bg_Scroll_CPz): two backgrounds in one plane: the upper rows (down to the wavy line at row 18 of the 16-line rows) follow the background's X, an eighth of the way across, the rows below
// a second one that goes four times as fast; Emerald Hill's wavy line runs along row 18. The redraw flags of both go to the third background's (the drawing of the plane's rows by which background
// they belong to is DrawBG_Block3's Draw_CPZ).
void Deform_CPZ(void) {
	BGScroll_XY((int32_t)scrshift_x << 5, (int32_t)scrshift_y << 6);
	UpdateBGScroll(&bg2_scrpos_x, (int32_t)scrshift_x << 7, &bg2_xblock, &bg2_scroll_flags, 1 << 4, 1 << 5);
	bg2_scrpos_y.f.u = bg_scrpos_y.f.u;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	bg3_scroll_flags = (uint16_t)((bg1_scroll_flags | bg2_scroll_flags) & 0xFF);
	bg1_scroll_flags = 0;
	bg2_scroll_flags = 0;
	if ((ehz_runcount++ & 7) == 0)
		ehz_wave--;

	uint16_t by = (uint16_t)bg_scrpos_y.f.u;
	int row = (by & 0x3F0) >> 4;
	int first = 16 - (by & 0xF);
	int16_t x1 = (int16_t)-bg_scrpos_x.f.u, x2 = (int16_t)-bg2_scrpos_x.f.u;
	const int8_t *wave = Deform_EHZ_Data + (ehz_wave & 0x1F);
	HScroll h;
	HS_Start(&h, (int16_t)-scrpos_x.f.u);
	for (int n = first; h.left > 0; n = 16, row++) {
		if (row == 0x12) {
			for (int i = 0; i < n; i++)
				HS_Line(&h, (int16_t)(x1 + wave[i]));
		} else {
			HS_Lines(&h, n, row < 0x12 ? x1 : x2);
		}
	}
}

// Hidden Palace (Deform_HPZ): bands of the background scroll at their own speeds, picked by the background's height (Deform_All)
void Water_Ripple(void);
void Deform_HPZ(void) {
	BGScroll_Block1((int32_t)scrshift_x << 6, SCROLL_FLAG_LEFT);
	UpdateBGScroll(&bg_scrpos_y, (int32_t)scrshift_y << 7, &bg1_yblock, &bg1_scroll_flags, 1 << 6, 1 << 7); // (BGScroll_SetupY with d6 = 6)
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int16_t layer[96] = { 0 };
	int16_t d2 = (int16_t)-scrpos_x.f.u;
	for (int i = 0; i < 8; i++)
		layer[i] = d2 >> 1;
	int32_t delta = HS_Delta((int16_t)((d2 >> 3) - d2), 3, 8);
	uint32_t acc = (uint32_t)(uint16_t)(d2 >> 1) << 16;
	acc += (uint32_t)delta;
	layer[8] = layer[9] = layer[10] = layer[47] = layer[46] = layer[45] = (int16_t)(acc >> 16);
	acc += (uint32_t)delta;
	layer[11] = layer[12] = layer[44] = layer[43] = (int16_t)(acc >> 16);
	acc += (uint32_t)delta;
	layer[13] = layer[42] = (int16_t)(acc >> 16);
	acc += (uint32_t)delta;
	layer[14] = layer[41] = (int16_t)(acc >> 16);
	for (int i = 15; i < 41; i++)
		layer[i] = (int16_t)-bg_scrpos_x.f.u;
	for (int i = 48; i < 72; i++)
		layer[i] = d2 >> 1;

	// Deform_All: a word of the table for every 16 lines, from the one for the background's height (the first 16 lines start part way through)
	uint16_t by = (uint16_t)bg_scrpos_y.f.u;
	const int16_t *band = &layer[(by & 0x3F0) >> 4];
	HScroll h;
	HS_Start(&h, d2);
	int first = 16 - (by & 0xF);
	HS_Lines(&h, first, *band++);
	while (h.left > 0)
		HS_Lines(&h, 16, *band++);
    if (Level_HasWater())
        Water_Ripple(); // the underwater ripple (LZWaterFeatures.c)
}

// Hill Top (Deform_HTZ): the top of the background a eighth of the way across, then bands that run on to a half
static bool deforming_p2; // the deformation is running for the second view of a split screen
void Deform_HTZ(void) {
	const int view = deforming_p2 ? 1 : 0;
	if (htz_quake && HTZQuake_Owner() == view) { // a quake's stretch of this view's camera: the prototype's shaking branch
		HTZQuake_Deform();
		return;
	}
	if (view == 0)
		HTZQuake_SpritesNormal();
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	HTZBackground_Deform(view); // the usual branch (loc_6108), whose layers the mountains' animated art follows
	HTZQuake_ShakeView();   // (when the ground moves for the other view's quake, this one shakes too)
}

// The prototype's plain backgrounds (Bg_Scroll_Wz, _Mz, _OOz and _CNz): the whole background in one piece, moved by a share of the camera's move (the delta is the camera's move shifted left by x_shift and y_shift
// into 16.16), its scroll one value for all the lines
static void Deform_Plain(int x_shift, int y_shift) {
	BGScroll_XY((int32_t)scrshift_x << x_shift, (int32_t)scrshift_y << y_shift);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	HScroll h;
	HS_Start(&h, (int16_t)-scrpos_x.f.u);
	HS_Lines(&h, 0xE0, (int16_t)-bg_scrpos_x.f.u);
	HS_Finish(&h);
}

// Wood (Bg_Scroll_Wz) and Metropolis (Bg_Scroll_Mz): an eighth of the way across, a quarter down
static void Deform_WZ(void) { Deform_Plain(5, 6); }
// Oil Ocean (Bg_Scroll_OOz)
static void Deform_OOZ(void) { Deform_Plain(5, 5); }
// Casino Night (Bg_Scroll_CNz): a plain background that never redraws (its flags are cleared as they are set: the picture is the whole plane)
static void Deform_CNZ(void) {
	Deform_Plain(6, 2);
	bg1_scroll_flags = 0;
}

// The prototype's bands of a background (the loop at loc_6526 and loc_6A36): `heights` are the bands' heights in lines and `values` their scroll; the background's own Y says which band is at the top and how
// much of it shows. Each band's value is the background's X as the original writes it (negated here).
static void Deform_Bands(const uint8_t *heights, int count, const int16_t *values, uint16_t bg_y, int16_t fg) {
	int band = 0;
	uint16_t left = bg_y;
	while (band < count - 1 && left >= heights[band])
		left = (uint16_t)(left - heights[band++]);
	int lines = heights[band] - left;
	HScroll h;
	HS_Start(&h, fg);
	while (h.left > 0) {
		HS_Lines(&h, lines, (int16_t)-values[band]);
		if (band < count - 1)
			band++;
		lines = heights[band];
	}
}

// Dust Hill (Bg_Scroll_DHz): the background is a third (act 1) or a sixth (act 2) of the way down, moved as a whole (its redraw flags are 6 and 7); 24 bands of it scroll at nine speeds, tenths of the
// camera's X, mirrored about the middle band
static void Deform_MCZ(void) {
	uint16_t cy = (uint16_t)scrpos_y.f.u;
	int32_t old = bg_scrpos_y.v;
	int16_t q;
	uint16_t r;
	if (LEVEL_ACT(level_id) == 0) {
		q = (int16_t)(cy / 3 - 0x140);
		r = cy % 3;
	} else {
		q = (int16_t)(cy / 6 - 0x10);
		r = cy % 6;
	}
	bg_scrpos_y.v = (int32_t)(((uint32_t)(uint16_t)q << 16) | r);
	if ((bg_scrpos_y.f.u & 0x10) != (bg1_yblock & 0x10)) {
		bg1_yblock ^= 0x10;
		bg1_scroll_flags |= (bg_scrpos_y.v < old) ? (1 << 6) : (1 << 7);
	}
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int32_t d0 = (int32_t)(int16_t)(((int32_t)scrpos_x.f.u * 16) / 10) << 12;
	int16_t v[9];
	int32_t acc = d0;
	for (int i = 0; i < 9; i++, acc += d0)
		v[i] = (int16_t)(acc >> 16);
	int16_t layer[24];
	layer[7] = v[0];
	layer[6] = v[1];
	layer[5] = v[2];
	layer[4] = v[3];
	layer[3] = layer[8] = layer[14] = v[4];
	layer[2] = layer[9] = layer[13] = v[6];
	layer[1] = layer[10] = layer[12] = v[7];
	layer[0] = layer[11] = v[8];
	for (int i = 0; i < 9; i++)
		layer[15 + i] = v[i];
	static const uint8_t heights[24] = {
		0x25, 0x17, 0x12, 0x07, 0x07, 0x02, 0x02, 0x30, 0x0D, 0x13, 0x20, 0x40,
		0x20, 0x13, 0x0D, 0x30, 0x02, 0x02, 0x07, 0x07, 0x20, 0x12, 0x17, 0x25,
	};
	Deform_Bands(heights, 24, layer, (uint16_t)bg_scrpos_y.f.u, (int16_t)-scrpos_x.f.u);
}

// Neo Green Hill (Bg_Scroll_NGHz): 12 bands, the upper and lower ones on a background that crawls (a 233rd of the camera's move), those between at multiples of a tenth of the camera's X
static void Deform_ARZ(void) {
	UpdateBGScroll(&bg_scrpos_x, (int32_t)(int16_t)(scrshift_x) * 0x119, &bg1_xblock, &bg1_scroll_flags, SCROLL_FLAG_LEFT, SCROLL_FLAG_RIGHT);
	int32_t dy = (int32_t)scrshift_y << 7;
	if (LEVEL_ACT(level_id) == 0)
		dy <<= 1;
	UpdateBGScroll(&bg_scrpos_y, dy, &bg1_yblock, &bg1_scroll_flags, 1 << 6, 1 << 7);
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	int32_t d0 = (int32_t)(int16_t)(((int32_t)scrpos_x.f.u * 16) / 10) << 12;
	int16_t bgx = bg_scrpos_x.f.u;
	static const int mult[8] = { 1, 3, 4, 5, 6, 7, 8, 9 };
	int16_t layer[12] = { bgx, bgx, bgx };
	for (int i = 0; i < 8; i++)
		layer[3 + i] = (int16_t)(((int32_t)mult[i] * d0) >> 16);
	layer[11] = bgx;
	static const uint8_t heights[12] = { 0xB0, 0x70, 0x30, 0x60, 0x15, 0x0C, 0x0E, 0x06, 0x0C, 0x1F, 0x30, 0xC0 };
	Deform_Bands(heights, 12, layer, (uint16_t)bg_scrpos_y.f.u, (int16_t)-scrpos_x.f.u);
}

static void (*deform_routines[ZoneId_Num])(void) = {
	[ZoneId_WZ] = Deform_WZ,
	[ZoneId_MTZ] = Deform_WZ,
	[ZoneId_MTZ3] = Deform_WZ,
	[ZoneId_OOZ] = Deform_OOZ,
	[ZoneId_MCZ] = Deform_MCZ,
	[ZoneId_CNZ] = Deform_CNZ,
	[ZoneId_ARZ] = Deform_ARZ,
	[ZoneId_CPZ] = Deform_CPZ,
	[ZoneId_EHZ] = Deform_EHZ,
	[ZoneId_HPZ] = Deform_HPZ,
	[ZoneId_HTZ] = Deform_HTZ,
};

/* BgScrollSpeed (Nick Arcade's): the background positions each zone starts with */
void BgScrollSpeed(int16_t x, int16_t y) {
    /* A checkpoint restore brings its own background positions */
    if (last_lamp == 0) {
        bg_scrpos_y.f.u = y;
        bg2_scrpos_y.f.u = y;
        bg_scrpos_x.f.u = x;
        bg2_scrpos_x.f.u = x;
        bg3_scrpos_x.f.u = x;
    }

    switch (LEVEL_ZONE(level_id)) {
        case ZoneId_EHZ:
        case ZoneId_HTZ: /* Hill Top uses Emerald Hill's */ {
            bg_scrpos_x.v = 0;
            bg_scrpos_y.v = 0;
            bg2_scrpos_y.v = 0;
            bg3_scrpos_y.v = 0;
            int32_t *scroll_ptr = (int32_t*)bgscroll_buffer;
            scroll_ptr[0] = 0;
            scroll_ptr[1] = 0;
            scroll_ptr[2] = 0;
            ehz_wave = 0;
            break;
        }

        case ZoneId_CPZ: /* Chemical Plant: a quarter of the height, no width */
            bg_scrpos_y.f.u = (int16_t)((uint16_t)y >> 2);
            bg2_scrpos_y.f.u = bg_scrpos_y.f.u;
            bg_scrpos_x.v = 0;
            bg2_scrpos_x.v = 0;
            break;

        case ZoneId_HPZ: /* Hidden Palace: half the height, no width */
            bg_scrpos_y.f.u = (int16_t)(y >> 1);
            bg_scrpos_x.v = 0;
            break;

        case ZoneId_WZ: /* Wood: a quarter of the height (and $400 down), an eighth of the width */
            bg_scrpos_y.f.u = (int16_t)((y >> 2) + 0x400);
            bg_scrpos_x.f.u = (int16_t)(x >> 3);
            break;

        case ZoneId_MTZ:
        case ZoneId_MTZ3: /* Metropolis: a quarter of the height, an eighth of the width */
            bg_scrpos_y.f.u = (int16_t)(y >> 2);
            bg_scrpos_x.f.u = (int16_t)(x >> 3);
            break;

        case ZoneId_OOZ: /* Oil Ocean: an eighth of the height and $50, no width */
            bg_scrpos_y.f.u = (int16_t)(((uint16_t)y >> 3) + 0x50);
            bg_scrpos_x.v = 0;
            break;

        case ZoneId_MCZ: /* Dust Hill: a third (act 1) or a sixth (act 2) of the height, less $140 or $10 */
            bg_scrpos_x.v = 0;
            if (LEVEL_ACT(level_id) == 0)
                bg_scrpos_y.f.u = (int16_t)((uint16_t)y / 3 - 0x140);
            else
                bg_scrpos_y.f.u = (int16_t)((uint16_t)y / 6 - 0x10);
            break;

        case ZoneId_CNZ: /* Casino Night: a 64th of the height, no width */
            bg_scrpos_y.f.u = (int16_t)((uint16_t)y >> 6);
            bg_scrpos_x.v = 0;
            bg2_scrpos_y.v = 0;
            bg3_scrpos_y.v = 0;
            break;

        case ZoneId_ARZ: /* Neo Green Hill: the height less $180 (act 1) or less $E0 and halved (act 2), no width */
            if (LEVEL_ACT(level_id) == 0)
                bg_scrpos_y.f.u = (int16_t)(y - 0x180);
            else
                bg_scrpos_y.f.u = (int16_t)((uint16_t)(y - 0xE0) >> 1);
            bg_scrpos_x.v = 0;
            bg2_scrpos_y.v = 0;
            bg3_scrpos_y.v = 0;
            break;

        default:
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

void MoveScreenHoriz(void) {
	const int16_t follow = SplitScreen_FollowX(); // (144, or the middle of a half-width view)
	int16_t distance_to_player;
	if (cam_x_delay) {
		// Lagging camera after a Spin Dash release: use Sonic's position
		// from a few frames ago (tracked in track_sonic/track_pos, the
		// same buffer ShieldInvincibility uses to trail behind Sonic)
		// instead of his real-time position, easing the delay out as
		// cam_x_delay counts down.
		cam_x_delay -= 0x100;
		uint8_t delay_hi = (uint8_t)(cam_x_delay >> 8);
		uint8_t offset = (uint8_t)((delay_hi << 2) + 4);
		uint8_t index = (uint8_t)(track_pos.f.l - offset);
		int16_t tracked_x = track_sonic[index >> 2][0];
		distance_to_player = (int16_t)(tracked_x & 0x3FFF) - scrpos_x.f.u;
	} else {
		distance_to_player = player->pos.l.x.f.u - scrpos_x.f.u;
	}
#if SCP_FIX_BUGS
	int16_t push_left = distance_to_player - follow;
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
	if ((uint16_t)distance_to_player < follow) {
		MoveBehindMid(distance_to_player - follow);
		return;
	}
	int16_t push_right = distance_to_player - follow;
	if ((uint16_t)push_right >= 16) {
		MoveAheadOfMid(push_right - 16);
		return;
	}
#endif
	scrshift_x = 0;
}

void ScrollHoriz(void) {
	int16_t prev_x = scrpos_x.f.u;
	MoveScreenHoriz();
	LevelPlane_CameraMovedX(&fg_plane, prev_x);
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

void ScrollVertical(void) {
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
	LevelPlane_CameraMovedY(&fg_plane, old_y);
}

void DeformLayers(void)
{
	//Check if we're allowed to scroll
	if (nobgscroll)
		return;
	
	//Clear previous flags
	LevelPlane_ClearFlags(&fg_plane);
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

	//The second view of a split screen follows the second player (from the first's scroll lines)
	SplitScreen_Scroll();
}

// The second view of a split screen scrolls a background of its own (a plane at VRAM_BG_P2) the way the zone's deformation does for the first: the deformation runs a second time with the second camera
// and its own background state, and leaves the scroll lines in `lines` (the first's in hscroll_buffer stay) and the background's Y in *bg_y. The background's new rows and columns are drawn at the
// vertical blank (DeformLayersP2_Draw) from the flags that deformation raised.
typedef struct {
	dword_s bg_x, bg_y, bg2_x, bg2_y, bg3_x, bg3_y;
	int16_t ehz_wave;
	uint8_t ehz_runcount;
	uint8_t xblock[3], yblock[3]; // (which 16 pixel block each background was last in: what decides that a row or column is new, so each view needs its own)
} DeformState;
static DeformState deform_p2;
static uint16_t p2_flags[3], p2_flags_dup[3];
static dword_s p2_dup[3][2]; // the background positions as of the last blank (the flags apply to these)

#define DEFORM_SWAP(a, b) do { __typeof__(a) t_ = (a); (a) = (b); (b) = t_; } while (0)
static void DeformSwap(DeformState *s) {
	DEFORM_SWAP(bg_scrpos_x, s->bg_x); DEFORM_SWAP(bg_scrpos_y, s->bg_y);
	DEFORM_SWAP(bg2_scrpos_x, s->bg2_x); DEFORM_SWAP(bg2_scrpos_y, s->bg2_y);
	DEFORM_SWAP(bg3_scrpos_x, s->bg3_x); DEFORM_SWAP(bg3_scrpos_y, s->bg3_y);
	DEFORM_SWAP(ehz_wave, s->ehz_wave); DEFORM_SWAP(ehz_runcount, s->ehz_runcount);
	DEFORM_SWAP(bg1_xblock, s->xblock[0]); DEFORM_SWAP(bg2_xblock, s->xblock[1]); DEFORM_SWAP(bg3_xblock, s->xblock[2]);
	DEFORM_SWAP(bg1_yblock, s->yblock[0]); DEFORM_SWAP(bg2_yblock, s->yblock[1]); DEFORM_SWAP(bg3_yblock, s->yblock[2]);
}

void DeformLayersP2_Init(void) {
	deform_p2 = (DeformState){ bg_scrpos_x, bg_scrpos_y, bg2_scrpos_x, bg2_scrpos_y, bg3_scrpos_x, bg3_scrpos_y, ehz_wave, ehz_runcount, { bg1_xblock, bg2_xblock, bg3_xblock }, { bg1_yblock, bg2_yblock, bg3_yblock } };
	memset(p2_flags, 0, sizeof(p2_flags));
	memset(p2_flags_dup, 0, sizeof(p2_flags_dup));
}

void DeformLayersP2(int16_t (*lines)[2], int16_t *bg_y) {
	void (*deform)(void) = deform_routines[LEVEL_ZONE(level_id)];
	if (deform == NULL) {
		memcpy(lines, hscroll_buffer, sizeof(hscroll_buffer));
		for (int i = 0; i < SCREEN_HEIGHT; i++)
			lines[i][0] = (int16_t)-scrpos_x_p2.f.u;
		*bg_y = vid_bg_scrpos_y_dup;
		return;
	}
	static int16_t saved[SCREEN_MAX_HEIGHT][2];
	memcpy(saved, hscroll_buffer, sizeof(saved));
	dword_s sx = scrpos_x, sy = scrpos_y;
	int16_t shx = scrshift_x, shy = scrshift_y, bgy_dup = vid_bg_scrpos_y_dup, fgy_dup = vid_scrpos_y_dup; // (the first view's own scroll dups: the second camera's quake branch writes them too)
	uint16_t f1 = bg1_scroll_flags, f2 = bg2_scroll_flags, f3 = bg3_scroll_flags;
	DeformSwap(&deform_p2);
	scrpos_x = scrpos_x_p2; scrpos_y = scrpos_y_p2;
	scrshift_x = scrshift_x_p2; scrshift_y = scrshift_y_p2;
	bg1_scroll_flags = p2_flags[0]; bg2_scroll_flags = p2_flags[1]; bg3_scroll_flags = p2_flags[2];
	deforming_p2 = true;
	HTZQuake_SetView(1);
	if (LEVEL_ZONE(level_id) == ZoneId_HTZ)
		HTZQuake_EventsP2(); // (the second camera's own quake events, with it swapped in)
	deform();
	HTZQuake_SetView(0);
	deforming_p2 = false;
	*bg_y = vid_bg_scrpos_y_dup;
	p2_flags[0] = bg1_scroll_flags; p2_flags[1] = bg2_scroll_flags; p2_flags[2] = bg3_scroll_flags;
	DeformSwap(&deform_p2);
	memcpy(lines, hscroll_buffer, sizeof(saved));
	memcpy(hscroll_buffer, saved, sizeof(saved));
	scrpos_x = sx; scrpos_y = sy; scrshift_x = shx; scrshift_y = shy; vid_bg_scrpos_y_dup = bgy_dup; vid_scrpos_y_dup = fgy_dup;
	bg1_scroll_flags = f1; bg2_scroll_flags = f2; bg3_scroll_flags = f3;
}

// At the blank: the second background's pending rows and columns, for its camera as the blank saw it (as the first's, from the "dup" copies)
void DeformLayersP2_Draw(void) {
	p2_dup[0][0] = deform_p2.bg_x; p2_dup[0][1] = deform_p2.bg_y;
	p2_dup[1][0] = deform_p2.bg2_x; p2_dup[1][1] = deform_p2.bg2_y;
	p2_dup[2][0] = deform_p2.bg3_x; p2_dup[2][1] = deform_p2.bg3_y;
	for (int i = 0; i < 3; i++) {
		p2_flags_dup[i] = p2_flags[i];
	}
	DrawBG_Top(p2_dup[0][0].f.u, p2_dup[0][1].f.u, &p2_flags_dup[0], LEVEL_LAYOUT_BG(0), &screen2p.plane_b);
	DrawBG_Bottom(p2_dup[1][0].f.u, p2_dup[1][1].f.u, &p2_flags_dup[1], LEVEL_LAYOUT_BG(0), &screen2p.plane_b);
	DrawBG_Block3(p2_dup[2][0].f.u, p2_dup[2][1].f.u, &p2_flags_dup[2], LEVEL_LAYOUT_BG(0), &screen2p.plane_b);
}
