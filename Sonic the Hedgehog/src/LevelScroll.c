#include <string.h>
#include "EditorLive.h"
#include "LevelScroll.h"
#include "ScrollEngine.h"

#include "Video.h"
#include "Level.h"
#include "LevelDraw.h"
#include "Game.h"

#include "Object/Sonic.h"

//Level scroll state
uint8_t nobgscroll, bgscrollvert;


dword_s bg2_scrpos_x, bg2_scrpos_y;
dword_s scrpos_x_dup, scrpos_y_dup, bg_scrpos_x_dup, bg_scrpos_y_dup, bg2_scrpos_x_dup, bg2_scrpos_y_dup, bg3_scrpos_x_dup, bg3_scrpos_y_dup;

int16_t scrshift_x, scrshift_y;


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

//Level deformation routines
void Deform_GHZ(void)
{
	int16_t fg_x;
	//Scroll background layers
	bg3_scrpos_x.v += (scrshift_x << 6) + (scrshift_x << 5); //Upper mountains
	bg2_scrpos_x.v += scrshift_x << 7; //Hills and waterfalls

	//Get Y position
	vid_bg_scrpos_y_dup =  0x20 - ((scrpos_y.f.u & 0x7FF) >> 5);
	if (vid_bg_scrpos_y_dup < 0)
		vid_bg_scrpos_y_dup = 0;

	//Get foreground position
	if (gamemode == GameMode_Title)
		fg_x = 0;
	else
		fg_x = scrpos_x.f.u;

	//Scroll clouds
	int32_t *scroll = (int32_t*)bgscroll_buffer;
	scroll[0] += 0x10000;
	scroll[1] += 0xC000;
	scroll[2] += 0x8000;

	// The background's bands, top of the screen down, one position each (ScrollEngine_ApplyDeformation): three cloud layers (the first
	// shortened as the view climbs), the upper mountains, the hills and waterfalls, and the water, whose every line has its own position
	int water_lines = 0x48 + SCREEN_TALLADD + vid_bg_scrpos_y_dup;
	uint16_t heights[6] = { (uint16_t)(0x20 - vid_bg_scrpos_y_dup), 0x10, 0x10, 0x30, 0x28, (uint16_t)(water_lines | SCROLL_BAND_PER_LINE) };
	static int16_t positions[5 + SCREEN_MAX_HEIGHT];
	positions[0] = (int16_t)(bg3_scrpos_x.f.u + (scroll[0] >> 16));
	positions[1] = (int16_t)(bg3_scrpos_x.f.u + (scroll[1] >> 16));
	positions[2] = (int16_t)(bg3_scrpos_x.f.u + (scroll[2] >> 16));
	positions[3] = bg3_scrpos_x.f.u;
	positions[4] = bg2_scrpos_x.f.u;
	int32_t wx = bg2_scrpos_x.v;
	int32_t wi = (((scrpos_x.f.u - bg2_scrpos_x.f.u) << 8) / 0x68) << 8;
	for (int i = 0; i < water_lines; i++)
	{
		positions[5 + i] = (int16_t)(wx >> 16);
		wx += wi;
	}
	ScrollEngine_ApplyDeformation(heights, 6, positions, 0, fg_x, true, SCREEN_HEIGHT, hscroll_buffer);
}

void Deform_MZ(void) {
	// The three layers of buildings move by their own fractions of the camera's move
	bg_scrpos_x.v += (scrshift_x << 6) * 3;
	bg3_scrpos_x.v += scrshift_x << 6;
	bg2_scrpos_x.v += scrshift_x << 7;

	int16_t y_off = 0x200;
	int16_t dy = scrpos_y.f.u - 0x1C8;
	if (dy >= 0) y_off += (dy * 3) >> 2;

	bg2_scrpos_y.f.u = bg3_scrpos_y.f.u = y_off;
	bg_scrpos_y.f.u = y_off;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	// The background's lines: a position for each row of 16 (ScrollEngine_ApplyDeformation): the clouds' gradient over five rows, two of the third layer, nine of the
	// second, and sixteen of the first
	int16_t pos[32];
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 2) - base_x) << 12) / 5;
	FillGradient(5, base_x >> 1, delta << 4, pos);
	pos[5] = pos[6] = -bg3_scrpos_x.f.u;
	for (int i = 0; i < 9; i++) pos[7 + i] = -bg2_scrpos_x.f.u;
	for (int i = 0; i < 16; i++) pos[16 + i] = -bg_scrpos_x.f.u;
	for (int i = 0; i < 32; i++) pos[i] = (int16_t)-pos[i]; // (they are given as the scroll lines hold them)

	static const uint16_t heights[32] = { 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16 };
	int16_t scroll_y = bg_scrpos_y.f.u - 0x200;
	if (scroll_y > 0x100) scroll_y = 0x100;
	ScrollEngine_ApplyDeformation(heights, 32, pos, (int16_t)((scroll_y & 0x1F0) + (bg_scrpos_y.f.u & 0xF)), scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
}

void Deform_SLZ(void) {
	bg_scrpos_y.v += scrshift_y << 7;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	// The lines: a position for each row of 16 (ScrollEngine_ApplyDeformation): 28 rows of a gradient, five of the first buildings, five of the second and thirty of the far ones; the rows past those are not set
	int16_t pos[80] = { 0 };
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 3) - base_x) << 12) / 28;
	FillGradient(28, base_x, delta << 4, pos);
	int16_t bld1 = (base_x >> 3);
	bld1 += (bld1 >> 1);
	for (int i = 0; i < 5; i++) pos[28 + i] = bld1;
	for (int i = 0; i < 5; i++) pos[33 + i] = base_x >> 2;
	for (int i = 0; i < 30; i++) pos[38 + i] = base_x >> 1;
	for (int i = 0; i < 68; i++) pos[i] = (int16_t)-pos[i]; // (they are given as the scroll lines hold them)

	uint16_t heights[80];
	for (int i = 0; i < 80; i++)
		heights[i] = 16;
	ScrollEngine_ApplyDeformation(heights, 80, pos, (int16_t)((((bg_scrpos_y.f.u - 0xC0) & 0x3F0)) + (bg_scrpos_y.f.u & 0xF)), scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
}

void Deform_SYZ(void) {
	bg_scrpos_y.v += (scrshift_y << 4) * 3;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

	// The lines: eight rows of a gradient, five and six of the nearer layers and fourteen of a second gradient
	int16_t pos[48] = { 0 };
	int16_t base_x = -scrpos_x.f.u;
	int32_t delta = (((int32_t)(base_x >> 3) - base_x) << 11) / 8;
	FillGradient(8, base_x >> 1, delta << 5, pos);
	for (int i = 0; i < 5; i++) pos[8 + i] = base_x >> 3;
	for (int i = 0; i < 6; i++) pos[13 + i] = base_x >> 2;
	delta = (((int32_t)base_x - (base_x >> 1)) << 12) / 14;
	FillGradient(14, base_x >> 1, delta << 4, pos + 19);
	for (int i = 0; i < 33; i++) pos[i] = (int16_t)-pos[i];

	uint16_t heights[48];
	for (int i = 0; i < 48; i++)
		heights[i] = 16;
	ScrollEngine_ApplyDeformation(heights, 48, pos, (int16_t)(bg_scrpos_y.f.u & 0x1FF), scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
}

void Deform_SBZ(void) {
	if (LEVEL_ACT(level_id) != 0) { // Act 2/3: one background, moved as a whole
		bg_scrpos_x.v += scrshift_x << 6;
		bg_scrpos_y.v += scrshift_y << 5;
		vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
		static const uint16_t whole[1] = { 0x100 };
		const int16_t pos = bg_scrpos_x.f.u;
		ScrollEngine_ApplyDeformation(whole, 1, &pos, 0, scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
		return;
	}

	// The three layers of buildings move by their own fractions of the camera's move, and the background down with it
	bg_scrpos_x.v += scrshift_x << 7;
	bg3_scrpos_x.v += scrshift_x << 6;
	bg2_scrpos_x.v += (scrshift_x << 5) * 3;
	bg_scrpos_y.v += scrshift_y << 5;
	vid_bg_scrpos_y_dup = bg2_scrpos_y.f.u = bg3_scrpos_y.f.u = bg_scrpos_y.f.u;

	// The background's lines: a position for each row of 16 (ScrollEngine_ApplyDeformation): the sky's gradient over four rows, ten of the third layer, seven of the second and
	// eleven of the first. The rows past those are not set (the original reads whatever follows its table there: nothing, for it is cleared)
	int16_t pos[48] = { 0 };
	int16_t d2_w = (-scrpos_x.f.u) >> 2;
	int32_t delta = (((int32_t)(d2_w >> 1) - d2_w) << 11) / 4;
	FillGradient(4, d2_w, delta << 5, pos);
	for (int i = 0; i < 10; i++) pos[4 + i] = -bg3_scrpos_x.f.u;
	for (int i = 0; i < 7; i++)  pos[14 + i] = -bg2_scrpos_x.f.u;
	for (int i = 0; i < 11; i++) pos[21 + i] = -bg_scrpos_x.f.u;
	for (int i = 0; i < 32; i++) pos[i] = (int16_t)-pos[i]; // (they are given as the scroll lines hold them)

	static const uint16_t heights[48] = {
		16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
		16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
	};
	ScrollEngine_ApplyDeformation(heights, 48, pos, (int16_t)(bg_scrpos_y.f.u & 0x1FF), scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
}

// (for the tests: the zones' routines registered with the engine, as the first frame does)
void LevelScroll_RegisterZones(void);

// Each zone's background deformation, registered with the scrolling engine (ScrollEngine.h) as its background event: these are the old deformation routines, kept until each zone is written for the engine
static void RegisterZones(void) {
    static bool done;
    if (done)
        return;
    done = true;
    ScrollEngine_SetZone(ZoneId_GHZ, -1, &(ScrollZone){ .background_event = Deform_GHZ });
    ScrollEngine_SetZone(ZoneId_MZ, -1, &(ScrollZone){ .background_event = Deform_MZ });
    ScrollEngine_SetZone(ZoneId_SLZ, -1, &(ScrollZone){ .background_event = Deform_SLZ });
    ScrollEngine_SetZone(ZoneId_SYZ, -1, &(ScrollZone){ .background_event = Deform_SYZ });
    ScrollEngine_SetZone(ZoneId_SBZ, -1, &(ScrollZone){ .background_event = Deform_SBZ });
    ScrollEngine_SetZone(ZoneId_EndZ, -1, &(ScrollZone){ .background_event = Deform_GHZ });
    // (Labyrinth has none: its background scroll and its ripple are in LZWaterFeatures)
}

void LevelScroll_RegisterZones(void) {
    RegisterZones();
}


/**
 * BgScrollSpeed: Subroutine to set scroll speed of some backgrounds.
 * This handles multi-layered parallax scrolling and background positioning
 * based on the current zone and revision logic.
 */
void BgScrollSpeed(int16_t x, int16_t y) {
    memset(bgscroll_buffer, 0, sizeof(bgscroll_buffer)); // (a level starts with the deformation's table cleared: what its rows past the table's end read)
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
		distance_to_player = player->x_pos.f.u - scrpos_x.f.u;
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
			player->y_pos.f.u &= 0x7FF;
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
			player->y_pos.f.u &= 0x7FF;
			scrpos_y.f.u -= 0x800;
			bg_scrpos_y.f.u &= 0x3FF;
		} else
			scroll->f.u = (int16_t)limit_btm2;
	}
}

void ScrollVertical(void) {
	dword_s scroll;
	int16_t y = player->y_pos.f.u - scrpos_y.f.u;
	uint16_t speed = 0;
	bool force_snap = false;

	if (((player->status & SST_PSTATUS_IN_BALL) != 0))
		y -= 5;

	if (((player->status & SST_PSTATUS_IN_AIR) != 0)) {
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
				uint16_t inertia_abs = (player->ground_vel < 0) ? -player->ground_vel : player->ground_vel;
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
	
	//Scroll camera
	// While the level is edited the camera follows the editor's cursor and is free of the level's own locks (the end of level's, a boss arena's: the limits the level's size gives are the only ones)
	int16_t kept[5] = { limit_left2, limit_right2, limit_top2, limit_btm2, limit_btm1 };
	const bool editing = Editor_Editing();
	if (editing) {
		const int16_t *size = LevelSizes(LEVEL_ZONE(level_id), LEVEL_ACT(level_id));
		limit_left2 = size[1];
		limit_right2 = size[2];
		limit_top2 = size[3];
		limit_btm2 = limit_btm1 = size[4];
	}
	Editor_CameraBegin();
	ScrollHoriz();
	ScrollVertical();
	Editor_CameraEnd();
	if (editing) { // (the level's own limits stay as they were: they come back with a playtest)
		limit_left2 = kept[0];
		limit_right2 = kept[1];
		limit_top2 = kept[2];
		limit_btm2 = kept[3];
		limit_btm1 = kept[4];
	}
	if (!Editor_Editing()) // (the level's events are for the player: the editor does not trigger them)
		DynamicLevelEvents();
	
	//Copy screen Y position
	vid_scrpos_y_dup = scrpos_y.f.u;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	
	//Run the zone's routines (the scrolling engine's frame)
	RegisterZones();
	ScrollEngine_Events(LEVEL_ZONE(level_id), LEVEL_ACT(level_id));
}

// The zones whose background plane is kept by the engine's band layers (ScrollBands) rather than by scroll flags. A layer is its bands' heights, and for each band which of the three
// background positions it follows (1: the first, 2: the second, 3: the third; 0: none, the band is drawn once and only wraps, its scroll lines move it).
typedef struct {
	const uint16_t *heights;
	const uint8_t *source;
	int count;
	int fixed_entry_source; // which position (0-3 as in source) a fixed band's row comes into view drawn at (the ROM's map has entry 0 mean the first)
	int moving_margin; // blocks left of the view a row of a band that follows a position starts (a band that follows none starts at the camera's block)
	bool chunk_init;   // the level's first draw is the whole view in chunks (DrawChunks) and the layer takes that as drawn
	int row_width;     // blocks wide a row is when it comes into view (0: the plane's width)
} BandLayer;

// Green Hill's (also the ending's, which scrolls the same way), as the original keeps it: the clouds and the water are drawn once and only wrap; the upper mountains (lines $40-$6F)
// follow the third position and the hills (lines $70-$9F) the second.
static const uint16_t ghz_heights[4] = { 0x40, 0x30, 0x30, 0x10 };
static const uint8_t ghz_source[4] = { 0, 3, 2, 0 };
static const BandLayer ghz_layer = { ghz_heights, ghz_source, 4, 0, 0, false, 0 };

// Marble's: BG_ScrollBlockMap_MZ gives each 16-pixel row of the background (counting from a row $20 down the layer, and itself a row late) the position it follows: 0 none, 2 the first,
// 4 the second, 6 the third. Made into bands over the layer's 128 rows (2048 lines) once.
static uint16_t mz_heights[SCROLL_MAX_BANDS];
static uint8_t mz_source[SCROLL_MAX_BANDS];
static BandLayer mz_layer;

static void BuildMarbleLayer(void)
{
	int n = 0;
	for (int row = 0; row < 128; row++) {
		uint8_t v = MZ_ScrollArray[1 + ((row - 0x20) & 0x7F)] >> 1;
		if (n > 0 && mz_source[n - 1] == v)
			mz_heights[n - 1] += 16;
		else if (n < SCROLL_MAX_BANDS) {
			mz_source[n] = v;
			mz_heights[n++] = 16;
		}
	}
	mz_layer = (BandLayer){ mz_heights, mz_source, n, 0, 1, false, 0 };
}

// Scrap Brain act 1's: BG_ScrollBlockMap_SBZ, the same way over the 32 rows of its 512-pixel layer, repeated down the plane (a fixed band's row comes into view at the first position)
static uint16_t sbz_heights[SCROLL_MAX_BANDS];
static uint8_t sbz_source[SCROLL_MAX_BANDS];
static BandLayer sbz_layer;

static void BuildScrapBrainLayer(void)
{
	int n = 0;
	for (int row = 0; row < 128; row++) {
		uint8_t v = SBZ_ScrollArray[1 + (row & 0x1F)] >> 1;
		if (n > 0 && sbz_source[n - 1] == v)
			sbz_heights[n - 1] += 16;
		else if (n < SCROLL_MAX_BANDS) {
			sbz_source[n] = v;
			sbz_heights[n++] = 16;
		}
	}
	sbz_layer = (BandLayer){ sbz_heights, sbz_source, n, 0, 1, false, 0 };
}

// The plain backgrounds, one band that follows the first position: Labyrinth's and Scrap Brain's later acts move it both ways (rows a screen and two blocks wide, from a block left of the view); Star Light's and
// Spring Yard's move only down (rows a plane wide from the layer's first column, which is the view's: their X is 0)
static const uint16_t plain_heights[1] = { 0x800 };
static const uint8_t plain_source[1] = { 1 };
static const BandLayer plain_layer = { plain_heights, plain_source, 1, 1, 1, true, -1 };
static const BandLayer down_layer = { plain_heights, plain_source, 1, 1, 0, true, 0 };

static const BandLayer *band_layer;
static ScrollBands bg_bands;
static bool bands_redraw = true;

bool LevelScroll_UsesBands(void)
{
	return band_layer != NULL;
}

// A level begins (or the background is wiped): which layer the zone has, and the next draw is the whole view
void LevelScroll_BackgroundReset(void)
{
	switch (LEVEL_ZONE(level_id)) {
	case ZoneId_GHZ:
	case ZoneId_EndZ:
		band_layer = &ghz_layer;
		break;
	case ZoneId_MZ:
		if (mz_layer.count == 0)
			BuildMarbleLayer();
		band_layer = &mz_layer;
		break;
	case ZoneId_SBZ:
		if (LEVEL_ACT(level_id) == 0) {
			if (sbz_layer.count == 0)
				BuildScrapBrainLayer();
			band_layer = &sbz_layer;
		} else
			band_layer = &plain_layer;
		break;
	case ZoneId_LZ:
		band_layer = &plain_layer;
		break;
	case ZoneId_SLZ:
	case ZoneId_SYZ:
		band_layer = &down_layer;
		break;
	default:
		band_layer = NULL;
		break;
	}
	bands_redraw = true;
	if (band_layer != NULL) {
		ScrollBands_Init(&bg_bands, &screen1p.plane_b, band_layer->heights, band_layer->count);
		for (int b = 0; b < band_layer->count; b++)
		{
			bg_bands.left_margin[b] = band_layer->source[b] ? (uint8_t)band_layer->moving_margin : 0;
			bg_bands.fixed[b] = band_layer->source[b] == 0;
		}
		bg_bands.entering_width = band_layer->row_width;
		if (band_layer->row_width < 0) // (a screen and two blocks wide, as the original's rows are)
			bg_bands.entering_width = (RIGHT_EDGE_X + 32) / 16 > PLANE_WIDTH / 2 ? PLANE_WIDTH / 2 : (RIGHT_EDGE_X + 32) / 16;
	}
}

// Brings the background plane up to the given positions (the ones the scroll lines were made from)
void LevelScroll_DrawBands(const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y)
{
	const dword_s *pos[4] = { bg1_x, bg1_x, bg2_x, bg3_x }; // (a band that follows none is drawn at the first position, as the original's map has it: 0 and 2 are the same)
	for (int b = 0; b < band_layer->count; b++)
		bg_bands.x[b] = pos[band_layer->source[b]]->f.u;
	bg_bands.y = bg_y->f.u;
	bg_bands.fixed_entry_x = pos[band_layer->fixed_entry_source]->f.u;
	if (bands_redraw) {
		if (band_layer->chunk_init) {
			DrawChunks(pos[1]->f.u, bg_y->f.u, LEVEL_LAYOUT_PLANE_BG, &screen1p.plane_b);
			ScrollBands_Sync(&bg_bands);
		} else
			ScrollBands_DrawAll(&bg_bands, LEVEL_LAYOUT_PLANE_BG);
		bands_redraw = false;
	} else
		ScrollBands_Draw(&bg_bands, LEVEL_LAYOUT_PLANE_BG);
}
