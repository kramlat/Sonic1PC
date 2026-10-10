#include "LevelScroll.h"

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

static void (*deform_routines[ZoneId_Num])(void) = {
	/* ZoneId_GHZ  */ Deform_GHZ,
	/* ZoneId_LZ   */ NULL, // BG scroll + ripple both moved to LZWaterFeatures() -- see LZWaterFeatures.c
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

                /* Force background horizontal scroll to zero for Rev 01+ */
                bg_scrpos_x.v = 0;
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

                /* Apply small offset correction and lock horizontal scroll */
                scroll_y++;
                bg_scrpos_x.v = 0;

            bg_scrpos_y.f.u = (int16_t)scroll_y;

            break;
        }

        case ZoneId_SBZ: {
            int32_t scroll_y;
                /* Rev 01 uses a masked calculation with a small offset */
                scroll_y = (((int32_t)y & 0x7F8) >> 3) + 1;
            bg_scrpos_y.f.u = (int16_t)scroll_y;
            break;
        }

        case ZoneId_EndZ: {
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

void MoveScreenHoriz(void) {
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
}
