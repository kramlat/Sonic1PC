// Sonic 2's level scrolling: Sonic 1's (a copy of LevelScroll.c) with Nick Arcade's zones: their background starts and deformation routines.
#include "EditorLive.h"
#include "LevelScroll.h"
#include "ScrollEngine.h"
#include "BackgroundLayer.h"
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


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Sonic 2 (Nick Arcade): the zones' deformation. Each zone's routine fills the horizontal scroll table (a foreground and a background X for every line), as the
// prototype's DeformBGLayer routines do (_inc/BgScrollSpeed & DeformBGLayer.asm); the 16.16 fixed point gradients are the original's own (a long that is swapped around
// an add), written here as a plain accumulator.
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
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
	// The lines as a band table (ScrollEngine_ApplyDeformation negates each position, so they are given the other way round): the sky, the clouds, the wavy line (a position for
	// every line), the hills' strip, two bands of hills and the water, whose gradient has a position for every line
	enum { WAVE = 0x15, WATER = 15 + 9 * 2 + 15 * 3 };
	static const uint16_t heights[7] = { 0x16, 0x3A, WAVE | SCROLL_BAND_PER_LINE, 0x0B, 0x10, 0x10, WATER | SCROLL_BAND_PER_LINE };
	int16_t pos[1 + 1 + WAVE + 1 + 1 + 1 + WATER];
	int n = 0;
	pos[n++] = 0;                 // sky
	int16_t d0 = d2 >> 6;
	pos[n++] = (int16_t)-d0;      // clouds
	if ((ehz_runcount++ & 7) == 0)
		ehz_wave--;
	const int8_t *wave = Deform_EHZ_Data + (ehz_wave & 0x1F);
	for (int i = 0; i < WAVE; i++) // the wavy line
		pos[n++] = (int16_t)-(int16_t)(d0 + wave[i]);
	pos[n++] = 0;
	d0 = d2 >> 4;
	pos[n++] = (int16_t)-d0;
	d0 = (int16_t)(d0 + (d0 >> 1));
	pos[n++] = (int16_t)-d0;
	// the water: a gradient of the foreground's X down to a third of it, in steps of one, two and three lines
	int32_t delta = HS_Delta((int16_t)((d2 >> 1) - (d2 >> 3)), 4, 0x30);
	uint32_t acc = (uint32_t)(uint16_t)(d2 >> 3) << 16;
	for (int i = 0; i < 15; i++) {
		pos[n++] = (int16_t)-(int16_t)(acc >> 16);
		acc += (uint32_t)delta;
	}
	for (int i = 0; i < 9; i++) {
		int16_t v = (int16_t)-(int16_t)(acc >> 16);
		pos[n++] = v;
		pos[n++] = v;
		acc += 2 * (uint32_t)delta;
	}
	for (int i = 0; i < 15; i++) {
		int16_t v = (int16_t)-(int16_t)(acc >> 16);
		pos[n++] = v;
		pos[n++] = v;
		pos[n++] = v;
		acc += 3 * (uint32_t)delta;
	}
	ScrollEngine_ApplyDeformation(heights, 7, pos, 0, (int16_t)-fg, true, SCREEN_HEIGHT, hscroll_buffer);
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
	bg_scrpos_x.v += (int32_t)scrshift_x << 5;
	bg_scrpos_y.v += (int32_t)scrshift_y << 6;
	bg2_scrpos_x.v += (int32_t)scrshift_x << 7;
	bg2_scrpos_y.f.u = bg_scrpos_y.f.u;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	if ((ehz_runcount++ & 7) == 0)
		ehz_wave--;

	// The lines as a band table: 64 rows of 16, the first 18 following the first background, row 18 (a position for every line) Emerald Hill's wavy line on it, the rest the second; the table's end holds
	const int8_t *wave = Deform_EHZ_Data + (ehz_wave & 0x1F);
	const int16_t x1 = (int16_t)-bg_scrpos_x.f.u, x2 = (int16_t)-bg2_scrpos_x.f.u;
	uint16_t heights[64];
	int16_t pos[63 + 16];
	int n = 0;
	// (the wavy line is read from its start for the lines that show, also when the view starts part way down it: the first row shows its wave from the top)
	const int top_row = ((uint16_t)bg_scrpos_y.f.u & 0x3F0) >> 4, top_skip = (uint16_t)bg_scrpos_y.f.u & 0xF;
	for (int row = 0; row < 64; row++) {
		heights[row] = row == 0x12 ? (16 | SCROLL_BAND_PER_LINE) : 16;
		if (row == 0x12)
			for (int i = 0; i < 16; i++)
				pos[n++] = (int16_t)-(int16_t)(x1 + wave[top_row == 0x12 && i >= top_skip ? i - top_skip : i]);
		else
			pos[n++] = (int16_t)-(row < 0x12 ? x1 : x2);
	}
	ScrollEngine_ApplyDeformation(heights, 64, pos, (int16_t)((uint16_t)bg_scrpos_y.f.u & 0x3FF), scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
}

// Hidden Palace (Deform_HPZ): bands of the background scroll at their own speeds, picked by the background's height (Deform_All)
void Water_Ripple(void);
void Deform_HPZ(void) {
	bg_scrpos_x.v += (int32_t)scrshift_x << 6;
	bg_scrpos_y.v += (int32_t)scrshift_y << 7;
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
	uint16_t heights[96];
	int16_t pos[96];
	for (int i = 0; i < 96; i++) {
		heights[i] = 16;
		pos[i] = (int16_t)-layer[i];
	}
	ScrollEngine_ApplyDeformation(heights, 96, pos, (int16_t)((uint16_t)bg_scrpos_y.f.u & 0x3FF), (int16_t)-d2, true, SCREEN_HEIGHT, hscroll_buffer);
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
	bg_scrpos_x.v += (int32_t)scrshift_x << x_shift;
	bg_scrpos_y.v += (int32_t)scrshift_y << y_shift;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	static const uint16_t whole[1] = { 0x100 };
	const int16_t pos = bg_scrpos_x.f.u;
	ScrollEngine_ApplyDeformation(whole, 1, &pos, 0, scrpos_x.f.u, true, SCREEN_HEIGHT, hscroll_buffer);
}

// Wood (Bg_Scroll_Wz) and Metropolis (Bg_Scroll_Mz): an eighth of the way across, a quarter down
static void Deform_WZ(void) { Deform_Plain(5, 6); }
// Oil Ocean (Bg_Scroll_OOz)
static void Deform_OOZ(void) { Deform_Plain(5, 5); }
// Casino Night (Bg_Scroll_CNz): a plain background that never redraws (its flags are cleared as they are set: the picture is the whole plane)
static void Deform_CNZ(void) {
	Deform_Plain(6, 2);
}

// The prototype's bands of a background (the loop at loc_6526 and loc_6A36): `heights` are the bands' heights in lines and `values` their scroll; the background's own Y says which band is at the top and how
// much of it shows. Each band's value is the background's X as the original writes it (negated here).
static void Deform_Bands(const uint8_t *heights, int count, const int16_t *values, uint16_t bg_y, int16_t fg) {
	uint16_t h[24];
	int16_t pos[24];
	for (int i = 0; i < count; i++) {
		h[i] = heights[i];
		pos[i] = values[i];
	}
	ScrollEngine_ApplyDeformation(h, count, pos, (int16_t)bg_y, (int16_t)-fg, true, SCREEN_HEIGHT, hscroll_buffer);
}

// Sand Shower: the background (night sky, a horizon about 240 lines down, sand below) moves an eighth of the way across and an eighth down, like Oil Ocean's, in five bands. The clouds drift as Green Hill's do (Sonic
// 1's Deform_GHZ: three layers that move on their own by 1, 3/4 and 1/2 of a pixel a frame, over the rocks' own scroll), the rocks on the horizon move with the background (an eighth of the camera's X) and the sand, the
// nearest, a quarter. The bands are in lines of the background: its X and Y are the plain ones, so the redraw is Oil Ocean's.
static void Deform_SSZ(void) {
	bg_scrpos_x.v += (int32_t)scrshift_x << 5;
	bg_scrpos_y.v += (int32_t)scrshift_y << 5;
	vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;
	int32_t *drift = (int32_t *)bgscroll_buffer; // (cleared where the level starts)
	drift[0] += 0x10000;
	drift[1] += 0xC000;
	drift[2] += 0x8000;
	static const uint8_t heights[5] = { 0x58, 0x30, 0x30, 0x38, 0xFF };
	const int16_t layer[5] = { (int16_t)(bg_scrpos_x.f.u + (drift[0] >> 16)), (int16_t)(bg_scrpos_x.f.u + (drift[1] >> 16)), (int16_t)(bg_scrpos_x.f.u + (drift[2] >> 16)),
	                           bg_scrpos_x.f.u, (int16_t)(scrpos_x.f.u >> 2) };
	Deform_Bands(heights, 5, layer, (uint16_t)bg_scrpos_y.f.u, (int16_t)-scrpos_x.f.u);
}

// Dust Hill (Bg_Scroll_DHz): the background is a third (act 1) or a sixth (act 2) of the way down, moved as a whole (its redraw flags are 6 and 7); 24 bands of it scroll at nine speeds, tenths of the
// camera's X, mirrored about the middle band
static void Deform_MCZ(void) {
	uint16_t cy = (uint16_t)scrpos_y.f.u;
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
	HTZQuake_ShakeDustHill(); // (the boss of act 2 digs: the alpha's Bg_Scroll_DHz shakes the screen then)
}

// Neo Green Hill (Bg_Scroll_NGHz): 12 bands, the upper and lower ones on a background that crawls (a 233rd of the camera's move), those between at multiples of a tenth of the camera's X
static void Deform_ARZ(void) {
	bg_scrpos_x.v += (int32_t)(int16_t)(scrshift_x) * 0x119;
	int32_t dy = (int32_t)scrshift_y << 7;
	if (LEVEL_ACT(level_id) == 0)
		dy <<= 1;
	bg_scrpos_y.v += dy;
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
	HTZQuake_ShakeDustHill(); // (the boss of act 2 shakes the screen as its walls rise: the alpha's Bg_Scroll_NGHz)
}

// Each zone's background deformation, registered with the scrolling engine (ScrollEngine.h) as its background event: these are the old deformation routines, kept until each zone is written for the engine
static void RegisterZones(void) {
    static bool done;
    if (done)
        return;
    done = true;
    ScrollEngine_SetZone(ZoneId_WZ, -1, &(ScrollZone){ .background_event = Deform_WZ });
    ScrollEngine_SetZone(ZoneId_MTZ, -1, &(ScrollZone){ .background_event = Deform_WZ });
    ScrollEngine_SetZone(ZoneId_OOZ, -1, &(ScrollZone){ .background_event = Deform_OOZ });
    ScrollEngine_SetZone(ZoneId_MCZ, -1, &(ScrollZone){ .background_event = Deform_MCZ });
    ScrollEngine_SetZone(ZoneId_CNZ, -1, &(ScrollZone){ .background_event = Deform_CNZ });
    ScrollEngine_SetZone(ZoneId_GCZ, -1, &(ScrollZone){ .background_event = Deform_WZ });
    ScrollEngine_SetZone(ZoneId_SSZ, -1, &(ScrollZone){ .background_event = Deform_SSZ });
    ScrollEngine_SetZone(ZoneId_ARZ, -1, &(ScrollZone){ .background_event = Deform_ARZ });
    ScrollEngine_SetZone(ZoneId_CPZ, -1, &(ScrollZone){ .background_event = Deform_CPZ });
    ScrollEngine_SetZone(ZoneId_EHZ, -1, &(ScrollZone){ .background_event = Deform_EHZ });
    ScrollEngine_SetZone(ZoneId_HPZ, -1, &(ScrollZone){ .background_event = Deform_HPZ });
    ScrollEngine_SetZone(ZoneId_HTZ, -1, &(ScrollZone){ .background_event = Deform_HTZ });
}


// (for the tests: the zones' routines registered with the engine, as the first frame does)
void LevelScroll_RegisterZones(void) {
    RegisterZones();
}

// ---- the background planes, kept by the engine's band layers (ScrollBands) ----
// A zone's background is a table of bands, each following one of the three background positions (1: the first, 2: the second, 3: the third): all of Sonic 2's are one band, except Chemical Plant's two.
// Some zones never move their plane (Emerald Hill, Casino Night: only their scroll lines move) and have none. The first view has a ScrollBands of its own, and the second view of a split screen another.
typedef struct {
	const uint16_t *heights;
	const uint8_t *source;
	int count;
	bool columns_only; // the original draws no rows for this background (Hill Top's quake moves it up and down without drawing)
	bool full_rows; // a row that comes into view is a whole plane wide (the 1 << 6 and 1 << 7 flags of Hidden Palace, Dust Hill and Neo Green Hill), not a screen and two blocks
} BandLayer;

static const uint16_t one_band_heights[1] = { 0x800 };
static const uint8_t one_band_source[1] = { 1 };
static const BandLayer plain_layer = { one_band_heights, one_band_source, 1, false, false };
static const BandLayer columns_layer = { one_band_heights, one_band_source, 1, true, false };
static const BandLayer full_rows_layer = { one_band_heights, one_band_source, 1, false, true };

// Chemical Plant's: the rows down to row 18 (of 16 lines) follow the first background's X, those below the second's, twice over the 64 rows of its table
static const uint16_t cpz_heights[4] = { 19 * 16, 45 * 16, 19 * 16, 45 * 16 };
static const uint8_t cpz_source[4] = { 1, 2, 1, 2 };
static const BandLayer cpz_layer = { cpz_heights, cpz_source, 4, false, false };

static const BandLayer *band_layer;
static ScrollBands band_view[2];

static const BandLayer *LayerOfZone(void)
{
	switch (LEVEL_ZONE(level_id)) {
	case ZoneId_WZ: case ZoneId_MTZ: case ZoneId_GCZ: case ZoneId_OOZ: case ZoneId_SSZ:
		return &plain_layer;
	case ZoneId_HTZ:
		return &columns_layer;
	case ZoneId_HPZ: case ZoneId_MCZ: case ZoneId_ARZ:
		return &full_rows_layer;
	case ZoneId_CPZ:
		return &cpz_layer;
	default:
		return NULL;
	}
}

// A level begins (or its layout loads again): which layer the zone has
void LevelScroll_BackgroundReset(void)
{
	band_layer = LayerOfZone();
	if (band_layer == NULL)
		return;
	ScrollBands_Init(&band_view[0], &screen1p.plane_b, band_layer->heights, band_layer->count);
	ScrollBands_Init(&band_view[1], &screen2p.plane_b, band_layer->heights, band_layer->count);
	// (a row that comes into view is a screen and two blocks wide, from a block left of it, as the original draws it; or a whole plane wide)
	int row_width = band_layer->full_rows ? 0 : (RIGHT_EDGE_X + 32) / 16;
	if (row_width > PLANE_WIDTH / 2)
		row_width = PLANE_WIDTH / 2;
	band_view[0].columns_only = band_view[1].columns_only = band_layer->columns_only;
	band_view[0].entering_width = band_view[1].entering_width = row_width;
}

static void SetBandPositions(int view, const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y)
{
	const dword_s *pos[4] = { bg1_x, bg1_x, bg2_x, bg3_x };
	for (int b = 0; b < band_layer->count; b++)
		band_view[view].x[b] = pos[band_layer->source[b]]->f.u;
	band_view[view].y = bg_y->f.u;
}

// The background was drawn whole at these positions (the level's start, or the second view's): the layer takes them as drawn
void LevelScroll_BackgroundSync(int view, const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y)
{
	if (band_layer == NULL)
		return;
	SetBandPositions(view, bg1_x, bg2_x, bg3_x, bg_y);
	ScrollBands_Sync(&band_view[view]);
}

// Brings a view's background plane up to the given positions
void LevelScroll_BackgroundDraw(int view, const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y)
{
	if (band_layer == NULL)
		return;
	SetBandPositions(view, bg1_x, bg2_x, bg3_x, bg_y);
	ScrollBands_Draw(&band_view[view], LEVEL_LAYOUT_PLANE_BG);
}

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

        case ZoneId_MTZ: /* Metropolis: a quarter of the height, an eighth of the width */
        case ZoneId_GCZ: /* Cyber City: InitCam_Std (the camera's Y shifted right twice and its X three times, arithmetically) */
            bg_scrpos_y.f.u = (int16_t)(y >> 2);
            bg_scrpos_x.f.u = (int16_t)(x >> 3);
            break;

        case ZoneId_SSZ: /* Sand Shower: an eighth of the height and of the width */
            bg_scrpos_y.f.u = (int16_t)((uint16_t)y >> 3);
            bg_scrpos_x.f.u = (int16_t)(x >> 3);
            memset(bgscroll_buffer, 0, 12); // (the clouds' drift starts over)
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
		distance_to_player = player->x_pos.f.u - scrpos_x.f.u;
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

// A level whose top limit is exactly $FF00 wraps vertically (Metropolis, above all, uses it in both directions): the camera and the characters go on past the top or the bottom as if the level's 8 rows were
// round a drum, so everything that compares places in height does it modulo $800
static bool LevelWrapsVertically(void) {
	return limit_top2 == 0xFF00;
}

void ScrollVertical(void) {
	dword_s scroll;
	int16_t y = player->y_pos.f.u - scrpos_y.f.u;
	uint16_t speed = 0;
	bool force_snap = false;

	// The distance between the player and the camera is taken modulo $800 in a wrapping level, so that he and the camera on either side of the wrap (he at the top of the level, it just before the bottom) are close
	if (LevelWrapsVertically())
		y &= 0x7FF;

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
} DeformState;
static DeformState deform_p2;
static dword_s p2_dup[3][2]; // the background positions as of the last blank (the flags apply to these)

#define DEFORM_SWAP(a, b) do { __typeof__(a) t_ = (a); (a) = (b); (b) = t_; } while (0)
static void DeformSwap(DeformState *s) {
	DEFORM_SWAP(bg_scrpos_x, s->bg_x); DEFORM_SWAP(bg_scrpos_y, s->bg_y);
	DEFORM_SWAP(bg2_scrpos_x, s->bg2_x); DEFORM_SWAP(bg2_scrpos_y, s->bg2_y);
	DEFORM_SWAP(bg3_scrpos_x, s->bg3_x); DEFORM_SWAP(bg3_scrpos_y, s->bg3_y);
	DEFORM_SWAP(ehz_wave, s->ehz_wave); DEFORM_SWAP(ehz_runcount, s->ehz_runcount);
}

void DeformLayersP2_Init(void) {
	deform_p2 = (DeformState){ bg_scrpos_x, bg_scrpos_y, bg2_scrpos_x, bg2_scrpos_y, bg3_scrpos_x, bg3_scrpos_y, ehz_wave, ehz_runcount };
	LevelScroll_BackgroundSync(1, &deform_p2.bg_x, &deform_p2.bg2_x, &deform_p2.bg3_x, &deform_p2.bg_y); // (its plane was drawn whole where the first's is)
}

void DeformLayersP2(int16_t (*lines)[2], int16_t *bg_y) {
	RegisterZones();
	const ScrollZone *zone = ScrollEngine_GetZone(LEVEL_ZONE(level_id), LEVEL_ACT(level_id));
	void (*deform)(void) = zone ? zone->background_event : NULL;
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
	DeformSwap(&deform_p2);
	scrpos_x = scrpos_x_p2; scrpos_y = scrpos_y_p2;
	scrshift_x = scrshift_x_p2; scrshift_y = scrshift_y_p2;
	deforming_p2 = true;
	HTZQuake_SetView(1);
	if (LEVEL_ZONE(level_id) == ZoneId_HTZ)
		HTZQuake_EventsP2(); // (the second camera's own quake events, with it swapped in)
	deform();
	HTZQuake_SetView(0);
	deforming_p2 = false;
	*bg_y = vid_bg_scrpos_y_dup;
	DeformSwap(&deform_p2);
	memcpy(lines, hscroll_buffer, sizeof(saved));
	memcpy(hscroll_buffer, saved, sizeof(saved));
	scrpos_x = sx; scrpos_y = sy; scrshift_x = shx; scrshift_y = shy; vid_bg_scrpos_y_dup = bgy_dup; vid_scrpos_y_dup = fgy_dup;
}

// At the blank: the second background's pending rows and columns, for its camera as the blank saw it (as the first's, from the "dup" copies)
void DeformLayersP2_Draw(void) {
	p2_dup[0][0] = deform_p2.bg_x; p2_dup[0][1] = deform_p2.bg_y;
	p2_dup[1][0] = deform_p2.bg2_x; p2_dup[1][1] = deform_p2.bg2_y;
	p2_dup[2][0] = deform_p2.bg3_x; p2_dup[2][1] = deform_p2.bg3_y;
	LevelScroll_BackgroundDraw(1, &p2_dup[0][0], &p2_dup[1][0], &p2_dup[2][0], &p2_dup[0][1]);
}
