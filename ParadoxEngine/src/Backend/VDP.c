#include "VDP.h"
#include "Viewport.h"
#include "TileBank.h"

#include "MegaDrive.h"
#include "../Video.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//Render backend interface
int Render_Init(const MD_Header *header);
void Render_Quit(void);
void Render_Screen(const uint32_t *screen);
void Render_SetPictureSize(bool resize_window);

//Input backend interface
int Input_HandleEvents(void);

//Audio backend interface
void Audio_Update(void);

//VDP compile options
#define VDP_SANITY //Enable sanity checks for the VDP (slower, but technically safer, basically for testing)

//VDP masks
#define VDP_MASK_PLANEPRI (1 << 0)
#define VDP_MASK_SPRITE   (1 << 1)
#define VDP_MASK_SH_SHADOW    (1 << 2) // (shadow/highlight mode) a sprite's shadow operator (palette line 3, colour 15) is here
#define VDP_MASK_SH_HIGHLIGHT (1 << 3) // ... its highlight operator (colour 14)

//VDP internal state
// The tile space: patterns only (a game may keep a plane in it: Plane_UseTiles). The planes, scroll tables, vertical scroll and sprite tables are the viewports' own (Viewport.h).
static ALIGNED2 uint8_t vdp_vram[VRAM_SIZE];
static uint32_t vdp_cram[VDP_PALETTES][16]; // 0x00RRGGBB: the colour RAM holds true colour; the Mega Drive's 9 bit words are upscaled as they are written

static uint8_t *vdp_vram_p;
static size_t vdp_vram_left; // bytes left in the tile space after vdp_vram_p
static uint32_t *vdp_cram_p;

static uint8_t vdp_background_colour;



static MD_Vector vdp_hint, vdp_vint;

//Split screen (see VDP_SetSplitScreen)
static VDPSplitMode vdp_split_mode = VDP_SPLIT_NONE;
static uint32_t vdp_water_pal[2][VDP_PALETTES_ACTIVE][16]; // the split screen's water: [0] dry, [1] wet (VDP_SetSplitWater)
static const uint16_t *vdp_water_dry = NULL, *vdp_water_wet = NULL;
static int16_t vdp_water_line[2];
static bool vdp_initialized = false;

//Shadow/highlight mode (see VDP_SetShadowHighlight): vdp_sh_sprites is set while sprites are being drawn, when their operator colours (palette 3, colours 14 and 15) draw nothing but change the shade of what is beneath
static bool vdp_sh_enabled = false;
static bool vdp_sh_sprites = false;

//VDP interface
int VDP_Init(const MD_Header *header) {
	//Initialize backend
	if (Render_Init(header))
		return -1;
	
	//Initialize VDP state
	vdp_background_colour = 0;
	screen1p.vsram = (vsram_t){ 0, 0 };
	screen1p.hint_counter = 0;
	screen1p.hint_enable = false;

	vdp_hint = header->h_interrupt;
	vdp_vint = header->v_interrupt;
	vdp_initialized = true;
	
	return 0;
}

void VDP_Quit(void) {
	//Quit backend
	Render_Quit();
}

void VDP_SeekVRAM(size_t offset) {
	#ifdef VDP_SANITY
	if (offset >= VRAM_SIZE)
		puts("VDP_SeekVRAM: Out-of-bounds");
	#endif
	if (offset >= VRAM_SIZE) { // (writes go nowhere until the next seek)
		vdp_vram_p = vdp_vram;
		vdp_vram_left = 0;
		return;
	}
	vdp_vram_p = vdp_vram + offset;
	vdp_vram_left = VRAM_SIZE - offset;
}

void VDP_WriteVRAM(const uint8_t *data, size_t len) {
	if (len > vdp_vram_left) {
		#ifdef VDP_SANITY
		puts("VDP_WriteVRAM: Out-of-bounds");
		#endif
		len = vdp_vram_left; // (what does not fit in the space is dropped, not put into the next)
	}
	memcpy(vdp_vram_p, data, len);
	vdp_vram_p += len;
	vdp_vram_left -= len;
}

void VDP_WriteLong(uint32_t val) {
	if (vdp_vram_left < 4) {
		#ifdef VDP_SANITY
		puts("VDP_WriteLong: Out-of-bounds");
		#endif
		return;
	}
	vdp_vram_p[0] = (uint8_t)(val >> 24);
	vdp_vram_p[1] = (uint8_t)(val >> 16);
	vdp_vram_p[2] = (uint8_t)(val >> 8);
	vdp_vram_p[3] = (uint8_t)(val);
	vdp_vram_p += 4;
	vdp_vram_left -= 4;
}

void VDP_FillVRAM(uint8_t data, size_t len) {
	if (len > vdp_vram_left) {
		#ifdef VDP_SANITY
		puts("VDP_FillVRAM: Out-of-bounds");
		#endif
		len = vdp_vram_left;
	}
	memset(vdp_vram_p, data, len);
	vdp_vram_p += len;
	vdp_vram_left -= len;
}

void VDP_ClearVRAM(void) {
	memset(vdp_vram, 0, sizeof(vdp_vram));
}

uint8_t *VDP_TileSpace(void) {
	return vdp_vram;
}

// The Mega Drive's colours: 3 bits a channel, 0000bbb0ggg0rrr0, which its DAC puts out at these levels of 255 (not evenly: the real thing's). True colour is what the colour RAM holds; these are the way in and out
static const uint8_t vdp_col_level[8] = { 0, 52, 87, 116, 144, 172, 206, 255 };

uint32_t VDP_Genesis2RGB(uint16_t cv) {
	return ((uint32_t)vdp_col_level[(cv & 0x00E) >> 1] << 16) | ((uint32_t)vdp_col_level[(cv & 0x0E0) >> 5] << 8) | vdp_col_level[(cv & 0xE00) >> 9];
}

// The nearest of the machine's levels for each channel (a colour of the machine's own comes back as it was)
static unsigned VDP_NearestLevel(uint8_t v) {
	unsigned best = 0;
	for (unsigned n = 1; n < 8; n++)
		if (abs((int)v - vdp_col_level[n]) < abs((int)v - vdp_col_level[best]))
			best = n;
	return best;
}

uint16_t VDP_RGB2Genesis(uint32_t rgb) {
	return (uint16_t)((VDP_NearestLevel((uint8_t)(rgb >> 16)) << 1) | (VDP_NearestLevel((uint8_t)(rgb >> 8)) << 5) | (VDP_NearestLevel((uint8_t)rgb) << 9));
}

void VDP_SeekCRAM(size_t offset) {
	#ifdef VDP_SANITY
	if (offset >= COLOURS) {
		puts("VDP_SeekCRAM: Out-of-bounds");
		return;
	}
	#endif
	vdp_cram_p = &vdp_cram[0][0] + offset;
}

void VDP_WriteCRAM(const uint16_t *data, size_t len) {
	#ifdef VDP_SANITY
	if ((vdp_cram_p - &vdp_cram[0][0]) >= COLOURS || (vdp_cram_p - &vdp_cram[0][0] + len) > COLOURS) {
		puts("VDP_WriteCRAM: Out-of-bounds");
		return;
	}
	#endif
	for (size_t i = 0; i < len; i++)
		*vdp_cram_p++ = VDP_Genesis2RGB(data[i]);
}

void VDP_WriteCRAM_RGB(const uint32_t *data, size_t len) {
	#ifdef VDP_SANITY
	if ((vdp_cram_p - &vdp_cram[0][0]) >= COLOURS || (vdp_cram_p - &vdp_cram[0][0] + len) > COLOURS) {
		puts("VDP_WriteCRAM_RGB: Out-of-bounds");
		return;
	}
	#endif
	for (size_t i = 0; i < len; i++)
		*vdp_cram_p++ = data[i] & 0xFFFFFF;
}

void VDP_FillCRAM(uint16_t data, size_t len) {
	#ifdef VDP_SANITY
	if ((vdp_cram_p - &vdp_cram[0][0]) >= COLOURS || (vdp_cram_p - &vdp_cram[0][0] + len) > COLOURS) {
		puts("VDP_WriteCRAM: Out-of-bounds");
		return;
	}
	#endif
	uint32_t rgb = VDP_Genesis2RGB(data);
	while (len-- > 0)
		*vdp_cram_p++ = rgb;
}

void VDP_SetBackgroundColour(uint8_t index) {
	#ifdef VDP_SANITY
	if (index >= COLOURS) {
		puts("VDP_SetBackgroundColour: Illegal colour index");
		return;
	}
	#endif
	vdp_background_colour = index;
}

int VDP_OutputRows(void) {
	return SCREEN_HEIGHT; //(a stacked split screen squashes its two views into the picture's own height)
}

void VDP_SetShadowHighlight(bool enable) {
	vdp_sh_enabled = enable;
}

void VDP_SetSplitWater(const uint16_t *dry, const uint16_t *wet, int16_t line1, int16_t line2) {
	vdp_water_dry = (dry != NULL && wet != NULL) ? dry : NULL;
	vdp_water_wet = wet;
	vdp_water_line[0] = line1;
	vdp_water_line[1] = line2;
}

void VDP_SetSplitScreen(VDPSplitMode mode) {
	vdp_split_mode = mode; // (the second view is screen2p)
}

//VDP rendering
#define SCREEN_PITCH (SCREEN_WIDTH + (VDP_INTERNAL_PAD * 2))

// Real H40-mode Genesis hardware caps this at 40 (320px / 8px-per-unit) --
// raised to match the new BUFFER_SPRITES (Video.h) for the same reason: a
// deliberate accuracy trade-off for this PC port, not a bug fix. Kept in
// sync with BUFFER_SPRITES since a single scanline can never legitimately
// need more sprites than the whole-frame total.
#define SCANLINE_SPRITES 0x78 // keep in sync with Video.h's BUFFER_SPRITES -- see that constant's own comment

// Big enough for the largest picture (the real size is chosen at run time, see Video.h); rows are SCREEN_PITCH apart.
#define SCREEN_MAX_PITCH (SCREEN_MAX_WIDTH + (VDP_INTERNAL_PAD * 2))
// (Twice the height: the stacked split screen draws two views one under the other.)
static uint32_t vdp_screen_internal[SCREEN_MAX_HEIGHT * 2 * SCREEN_MAX_PITCH];
static uint8_t vdp_mask_internal[SCREEN_MAX_HEIGHT * 2 * SCREEN_MAX_PITCH];

static uint32_t *vdp_screen;
static uint8_t *vdp_mask;

static uint32_t vdp_screen_pal[VDP_PALETTES_ACTIVE][16];
static uint32_t vdp_screen_pal2[VDP_PALETTES_ACTIVE][16]; // the second view's palette, when it has its own
static uint32_t (*vdp_draw_pal)[16] = vdp_screen_pal; // the palette the row being drawn uses

// One entry per output line (the second view's lines follow the first's, whichever way the views are laid out)
static struct VDP_SpriteCache {
	const uint16_t *sprite[SCANLINE_SPRITES];
	uint8_t pushind;
	uint16_t pixels;
} vdp_sprite_cache[SCREEN_MAX_HEIGHT * 2];

// A colour as the screen holds it: 0xRRGGBBAA
static inline uint32_t VDP_RGBAOf(uint32_t rgb) {
	return (rgb << 8) | 0xFF;
}

static inline uint32_t VDP_ConvertColour(uint16_t cv) {
	return VDP_RGBAOf(VDP_Genesis2RGB(cv));
}

static inline uint32_t VDP_GetColour(size_t index) {
	#ifdef VDP_SANITY
	if (index >= COLOURS) {
		puts("VDP_GetColour: Illegal colour index");
		return 0xFF0000FF;
	}
	#endif
	
	return VDP_RGBAOf(vdp_cram[index >> 4][index & 0xF]);
}

static inline uint8_t *VDP_GetPatternAddress(size_t pattern) {
	#ifdef VDP_SANITY
	if (pattern >= (VRAM_SIZE >> 5)) {
		puts("VDP_GetPatternAddress: Out-of-bounds");
		return vdp_vram;
	}
	#endif
	
	return vdp_vram + (pattern << 5);
}

#define WRITE_NIBBLE(from, to, tom, pal, and, or, nibs) { \
	uint8_t v = (*from >> nibs) & 0xF;                    \
	if (v != 0) {                                         \
		if (vdp_sh_sprites && (pal) == 3 && v >= 14) {    \
			*tom |= (v == 14) ? VDP_MASK_SH_HIGHLIGHT : VDP_MASK_SH_SHADOW; \
			to++;                                         \
		} else if (*tom & and) {                          \
			*tom |= or;                                   \
			to++;                                         \
		} else {                                          \
			*tom |= or;                                   \
			*to++ = vdp_draw_pal[(pal)][v];             \
		}                                                 \
		tom++;                                            \
	} else {                                              \
		to++;                                             \
		tom++;                                            \
	}                                                     \
}

#define WRITE_BYTE(from, to, tom, pal, and, or) {         \
	WRITE_NIBBLE(from, to, tom, pal, and, or, 4)          \
	WRITE_NIBBLE(from, to, tom, pal, and, or, 0)          \
	from++;                                               \
}

#define WRITE_BYTE_FLIP(from, to, tom, pal, and, or) {    \
	WRITE_NIBBLE(from, to, tom, pal, and, or, 0)          \
	WRITE_NIBBLE(from, to, tom, pal, and, or, 4)          \
	from--;                                               \
}

// --- 8 bits a pixel ---
// An 8bpp tile is 64 bytes, a byte a pixel, rows of 8 bytes, the byte the colour RAM index (0 to 255: any of the 16 lines' colours; 0 is transparent). Patterns are numbered in 64 byte steps. Nothing draws the planes
// or sprites with it yet (they are 4bpp: the tile format has no bit to say otherwise); this is the row handler a later tile format would call, working to the same rules as the 4bpp one (the priority masks).
static inline const uint8_t *VDP_GetPattern8Address(size_t pattern) {
	#ifdef VDP_SANITY
	if (pattern >= (VRAM_SIZE >> 6)) {
		puts("VDP_GetPattern8Address: Out-of-bounds");
		return vdp_vram;
	}
	#endif
	
	return vdp_vram + (pattern << 6);
}

void VDP_DrawTileRow8(uint32_t *to, uint8_t *tom, size_t pattern, int y, bool x_flip, bool y_flip, uint8_t and, uint8_t or) {
	const uint8_t *from = VDP_GetPattern8Address(pattern) + (((y_flip ? (y ^ 7) : y) & 7) << 3);
	for (int i = 0; i < 8; i++) {
		uint8_t v = from[x_flip ? (7 - i) : i];
		if (v != 0) {
			if (!(tom[i] & and))
				to[i] = VDP_GetColour(v);
			tom[i] |= or;
		}
	}
}

// Draws one row of a plane `view_w` pixels wide. With double_cells (the stacked split screen's double-height mode) a cell is 8x16: a name table entry names a pair
// of patterns, one above the other, at twice its number.
static inline void VDP_DrawPlaneRow(uint32_t *to, uint8_t *tom, const tile_entry_t *plane, size_t plane_w, size_t plane_h, int16_t x, int16_t y, int view_w, bool double_cells) {
	//Get plane tile to use
	size_t px = (x >> 3) % plane_w;
	size_t py = (double_cells ? (y >> 4) : (y >> 3)) % plane_h;
	const tile_entry_t *pb = plane + py * plane_w;
	
	//Draw plane row
	uint32_t *toend = to + view_w;
	to -= x & 7;
	tom -= x & 7;
	y &= double_cells ? 15 : 7;
	
	for (; to < toend; px = (px + 1) % plane_w) {
		//Get tile information: the entry names its tile by bank, generation and pattern (a tile that is not there draws nothing)
		const tile_entry_t *entry = &pb[px];
		const uint16_t tile = entry->attrs;
		uint8_t or = (tile & TILE_PRIORITY_AND) ? VDP_MASK_PLANEPRI : 0;
		uint8_t palette = (tile & TILE_PALETTE_AND) >> TILE_PALETTE_SHIFT;
		uint8_t y_flip = (tile & TILE_Y_FLIP_AND) != 0;
		uint8_t x_flip = (tile & TILE_X_FLIP_AND) != 0;
		
		//Write tile
		const uint8_t *from;
		if (!double_cells) {
			from = TileBank_Pattern(entry);
			if (from != NULL)
				from += (y_flip ? (y ^ 7) : y) << 2;
		} else {
			// (the stacked split screen's cells are 8x16: an entry names a pair of patterns, the second at the next number)
			int row = y_flip ? (y ^ 15) : y;
			tile_entry_t half = *entry;
			half.pattern = (entry->pattern << 1) + (row >> 3);
			from = TileBank_Pattern(&half);
			if (from != NULL)
				from += (row & 7) << 2;
		}
		if (from == NULL) { // not there: the eight pixels are left as they are
			to += 8;
			tom += 8;
			continue;
		}
		if (x_flip) {
			from += 3;
			WRITE_BYTE_FLIP(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
			WRITE_BYTE_FLIP(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
			WRITE_BYTE_FLIP(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
			WRITE_BYTE_FLIP(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
		} else {
			WRITE_BYTE(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
			WRITE_BYTE(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
			WRITE_BYTE(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
			WRITE_BYTE(from, to, tom, palette, VDP_MASK_PLANEPRI, or)
		}
	}
}

// Draws one row of one sprite. `y` is the line, counted from the view's top, and `ybase` the sprite Y coordinate of that top (128; the stacked split screen's
// views are drawn in double-height coordinates, where the screen starts at 256 and a sprite cell is 16 lines tall).
static inline void VDP_DrawSpriteRow(uint32_t *to, uint8_t *tom, const uint16_t *sprite, int16_t y, int view_w, int ybase, bool double_cells) {
	//Get sprite information
	uint16_t sprite_y = *sprite++;
	uint16_t sprite_sl = *sprite++;
	uint16_t sprite_tile = *sprite++;
	uint16_t sprite_x = *sprite++;
	
	uint8_t width = (sprite_sl & SPRITE_SL_W_AND) >> SPRITE_SL_W_SHIFT;
	uint8_t height = (sprite_sl & SPRITE_SL_H_AND) >> SPRITE_SL_H_SHIFT;
	
	uint8_t and = (sprite_tile & TILE_PRIORITY_AND) ? VDP_MASK_SPRITE : (VDP_MASK_PLANEPRI | VDP_MASK_SPRITE);
	uint16_t palette = (sprite_tile & TILE_PALETTE_AND) >> TILE_PALETTE_SHIFT;
	uint8_t y_flip = (sprite_tile & TILE_Y_FLIP_AND) != 0;
	uint8_t x_flip = (sprite_tile & TILE_X_FLIP_AND) != 0;
	uint16_t pattern = (sprite_tile & TILE_PATTERN_AND) >> TILE_PATTERN_SHIFT;
	
	//Get sprite left and right coordinates
	int16_t width_pixels = (width + 1) << 3;
	
	int16_t left = sprite_x - 128;
	if (left <= -width_pixels || left >= view_w)
		return;
	to += left;
	tom += left;
	
	int16_t right = left + width_pixels;
	
	if (double_cells) {
		//Cells are 8x16: a sprite is (width + 1) columns of (height + 1) cells, each cell a pair of patterns
		y -= (sprite_y - ybase);
		size_t ty = y >> 4;
		int row;
		if (y_flip) {
			ty = height - ty;
			row = (y & 15) ^ 15;
		} else
			row = y & 15;
		size_t cols = (size_t)width + 1;
		for (size_t col = 0; col < cols; col++, left += 8) {
			size_t src_col = x_flip ? (cols - 1 - col) : col;
			size_t cell = (size_t)pattern + ty + src_col * (height + 1);
			const uint8_t *from = VDP_GetPatternAddress((cell << 1) + (row >> 3)) + ((row & 7) << 2);
			if (x_flip) {
				from += 3;
				WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
				WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
				WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
				WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
			} else {
				WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
				WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
				WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
				WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
			}
		}
		return;
	}
	
	//Get Y tile
	y -= (sprite_y - ybase);
	size_t ty = y >> 3;
	if (y_flip) {
		ty = height - ty;
		y = (y & 7) ^ 7;
	} else
		y &= 7;

	pattern += ty;
	
	//Get X tile
	if (x_flip) {
		pattern += width * (height + 1);
		for (; left < right; left += 8) {
			//Write tile
			const uint8_t *from = VDP_GetPatternAddress(pattern) + (y << 2) + 3;
			WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
			WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
			WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
			WRITE_BYTE_FLIP(from, to, tom, palette, and, VDP_MASK_SPRITE)
			pattern -= height + 1;
		}
	} else {
		for (; left < right; left += 8) {
			//Write tile
			const uint8_t *from = VDP_GetPatternAddress(pattern) + (y << 2);
			WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
			WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
			WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
			WRITE_BYTE(from, to, tom, palette, and, VDP_MASK_SPRITE)
			pattern += height + 1;
		}
	}
}

bool VDP_PALETTE_DISPLAY = false;
uint16_t VRAMADDR = 0;
uint8_t CRAMPAL = 0;
bool Z80_PEEK_DISPLAY = false;

// The VDP peek's own VRAM-address/palette-ID readout, drawn with the same
// Art_Text glyph font Z80 Peek uses (see HUD_WriteHex/GM_Title.c's
// LevSelCharToTile for the other two places this same asset is used).
// Declared, not #included -- Game.c is the one place that includes the real
// header, so including it again here would double-define the array at link
// time (same reasoning as GM_Title.c's/Render.c's own copy of this extern).
extern const uint8_t Art_Text[];

// Draws one 8px-tall row (glyph_row, 0-7) of every character in `text`
// (digits/uppercase letters only -- matches LevSelCharToTile's charset),
// starting at pixel column x, into scanline buffer `to`. Called once per
// scanline from VDP_DrawScanline, same as the rest of this debug overlay --
// there's no separate "draw the whole string" pass since this whole
// function operates one scanline at a time.
static inline void VDP_DrawDebugText(uint32_t *to, const char *text, size_t x, size_t glyph_row) {
	for (; *text; text++, x += 8) {
		char c = *text;
		uint8_t tile;
		if (c >= '0' && c <= '9')
			tile = (uint8_t)(c - '0');
		else if (c >= 'A' && c <= 'F')
			tile = (uint8_t)(0x11 + (c - 'A')); // matches LevSelCharToTile's 'A'-'X' run
		else
			continue; // blank/unsupported -- just skip the column

		const uint8_t *src = Art_Text + tile * 32 + glyph_row * 4;
		for (size_t col = 0; col < 8; col++) {
			uint8_t byte = src[col / 2];
			uint8_t nibble = (col & 1) ? (byte & 0xF) : (byte >> 4);
			if (nibble)
				to[x + col] = 0xFFFFFFFF;
		}
	}
}


// Shadow/highlight mode: a pixel is shadowed (halved) unless a high-priority plane or a sprite drew it, a sprite's highlight operator raises it a step (shadowed to normal, normal to highlighted: half
// way to white), and its shadow operator shadows it
static inline void VDP_ApplyShadowHighlight(uint32_t *to, const uint8_t *tom, int w) {
	for (int i = 0; i < w; i++) {
		uint8_t m = tom[i];
		int state = (m & (VDP_MASK_PLANEPRI | VDP_MASK_SPRITE)) ? 1 : 0; // 0 shadow, 1 normal, 2 highlight
		if (m & VDP_MASK_SH_HIGHLIGHT)
			state++;
		if (m & VDP_MASK_SH_SHADOW)
			state = 0;
		if (state > 2)
			state = 2;
		if (state == 1)
			continue;
		uint32_t c = to[i];
		uint32_t r = (c >> 24) & 0xFF, g = (c >> 16) & 0xFF, b = (c >> 8) & 0xFF;
		if (state == 0) {
			r >>= 1; g >>= 1; b >>= 1;
		} else {
			r = 127 + (r >> 1); g = 127 + (g >> 1); b = 127 + (b >> 1);
		}
		to[i] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
	}
}

// Draws one row of one view, `view_w` pixels wide, into `to`: the backdrop, plane B, plane A, then the row's sprites. `y` is the line from the view's top and
// `ybase` the sprite Y coordinate of that top.
static inline void VDP_DrawViewRow(size_t y, uint32_t *to, uint8_t *tom, int view_w, int ybase, bool double_cells, const viewport_t *vp,
                                   struct VDP_SpriteCache *scache, const int16_t *hscroll) {
	//Clear scanline
	for (int i = 0; i < view_w; i++)
		to[i] = vdp_draw_pal[0][vdp_background_colour];
	memset(tom, 0, (size_t)view_w);
	
	//Draw planes
	VDP_DrawPlaneRow(to, tom, vp->plane_b.entries, vp->plane_width, vp->plane_height, -hscroll[1], y + vp->vsram.b, view_w, double_cells);
	VDP_DrawPlaneRow(to, tom, vp->plane_a.entries, vp->plane_width, vp->plane_height, -hscroll[0], y + vp->vsram.a, view_w, double_cells);
	hbla_pos = (int16_t)y;

	vdp_sh_sprites = vdp_sh_enabled;
	//Draw sprites
	for (uint8_t i = 0; i < scache->pushind; i++)
		VDP_DrawSpriteRow(to, tom, scache->sprite[i], y, view_w, ybase, double_cells);
	vdp_sh_sprites = false;
	if (vdp_sh_enabled)
		VDP_ApplyShadowHighlight(to, tom, view_w);
}

static inline void VDP_DrawScanline(size_t y, uint32_t *to, uint8_t *tom, struct VDP_SpriteCache *scache, const int16_t *hscroll) {
	VDP_DrawViewRow(y, to, tom, SCREEN_WIDTH, 128, false, &screen1p, scache, hscroll);
	
	// VRAM address + selected CRAM palette readout -- drawn in the 32px this
	// display's own shift-down (below) vacated above it, using the same
	// Art_Text glyph font Z80 Peek uses (see VDP_DrawDebugText), so both
	// debug overlays read consistently. Two rows: 4 hex digits of VRAMADDR,
	// then 1 hex digit of CRAMPAL.
	if (VDP_PALETTE_DISPLAY && y < 16) {
		char text[5];
		text[0] = "0123456789ABCDEF"[(VRAMADDR >> 12) & 0xF];
		text[1] = "0123456789ABCDEF"[(VRAMADDR >> 8) & 0xF];
		text[2] = "0123456789ABCDEF"[(VRAMADDR >> 4) & 0xF];
		text[3] = "0123456789ABCDEF"[VRAMADDR & 0xF];
		text[4] = '\0';
		if (y < 8)
			VDP_DrawDebugText(to, text, SCREEN_WIDTH - 128, y);
		else {
			char pal[2] = {(char)('0' + CRAMPAL), '\0'};
			VDP_DrawDebugText(to, pal, SCREEN_WIDTH - 128, y - 8);
		}
	}

	if (VDP_PALETTE_DISPLAY & (y > 31) & (y < 41))
		for (size_t i = 0; i < 16; i++)
			for (size_t j =0; j < 9; j++)
				to[(SCREEN_WIDTH - 128) + ((i * 8)+ j)] = vdp_screen_pal[0][i];
	if (VDP_PALETTE_DISPLAY & (y > 40) & (y < 49))
		for (size_t i = 0; i < 16; i++)
			for (size_t j =0; j < 9; j++)
				to[(SCREEN_WIDTH - 128) + ((i * 8)+ j)] = vdp_screen_pal[1][i];
	if (VDP_PALETTE_DISPLAY & (y > 48) & (y < 57))
		for (size_t i = 0; i < 16; i++)
			for (size_t j =0; j < 9; j++)
				to[(SCREEN_WIDTH - 128) + ((i * 8)+ j)] = vdp_screen_pal[2][i];
	if (VDP_PALETTE_DISPLAY & (y > 56) & (y < 65))
		for (size_t i = 0; i < 16; i++)
			for (size_t j =0; j < 9; j++)
				to[(SCREEN_WIDTH - 128) + ((i * 8)+ j)] = vdp_screen_pal[3][i];
	if (VDP_PALETTE_DISPLAY & (y > 64) & (y < 73))
		for (size_t i = 0; i < 16; i++) {
			to[(SCREEN_WIDTH - 128) + (i * 8)]       = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + VRAMADDR + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 1)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + VRAMADDR+ (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 2)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 1 + VRAMADDR+ (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 3)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 1 + VRAMADDR+ (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 4)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + 2 + VRAMADDR+ (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 5)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 2 + VRAMADDR+ (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 6)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 3 + VRAMADDR+ (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 7)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 3 + VRAMADDR+ (32 * i)] & 0xF];
		}
	if (VDP_PALETTE_DISPLAY & (y > 72) & (y < 81))
		for (size_t i = 0; i < 16; i++) {
			to[(SCREEN_WIDTH - 128) + (i * 8)]       = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + VRAMADDR + 0x200 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 1)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + VRAMADDR + 0x200 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 2)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 1 + VRAMADDR + 0x200 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 3)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 1 + VRAMADDR + 0x200 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 4)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + 2 + VRAMADDR + 0x200 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 5)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 2 + VRAMADDR + 0x200 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 6)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 3 + VRAMADDR + 0x200 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 7)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 3 + VRAMADDR + 0x200 + (32 * i)] & 0xF];
		}
	if (VDP_PALETTE_DISPLAY & (y > 80) & (y < 89))
		for (size_t i = 0; i < 16; i++) {
			to[(SCREEN_WIDTH - 128) + (i * 8)]       = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + VRAMADDR + 0x400 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 1)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + VRAMADDR + 0x400 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 2)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 1 + VRAMADDR + 0x400 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 3)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 1 + VRAMADDR + 0x400 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 4)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + 2 + VRAMADDR + 0x400 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 5)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 2 + VRAMADDR + 0x400 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 6)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 3 + VRAMADDR + 0x400 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 7)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 3 + VRAMADDR + 0x400 + (32 * i)] & 0xF];
		}
	if (VDP_PALETTE_DISPLAY & (y > 88) & (y < 97))
		for (size_t i = 0; i < 16; i++) {
			to[(SCREEN_WIDTH - 128) + (i * 8)]       = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + VRAMADDR + 0x600 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 1)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + VRAMADDR + 0x600 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 2)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 1 + VRAMADDR + 0x600 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 3)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 1 + VRAMADDR + 0x600 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 4)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4) + 2 + VRAMADDR + 0x600 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 5)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 2 + VRAMADDR + 0x600 + (32 * i)] & 0xF];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 6)] = vdp_screen_pal[CRAMPAL][(vdp_vram[(y * 4)+ 3 + VRAMADDR + 0x600 + (32 * i)] & 0xF0) >> 4];
			to[(SCREEN_WIDTH - 128) + ((i * 8) + 7)] = vdp_screen_pal[CRAMPAL][vdp_vram[(y * 4) + 3 + VRAMADDR + 0x600 + (32 * i)] & 0xF];
		}
}

static inline void VDP_RefreshPalette(void) {
	uint32_t *pal_to = &vdp_screen_pal[0][0];
	for (size_t i = 0; i < ACTIVE_COLOURS; i++)
		*pal_to++ = VDP_GetColour(i);
	if (vdp_split_mode != VDP_SPLIT_NONE && vdp_water_dry != NULL) {
		for (size_t i = 0; i < ACTIVE_COLOURS; i++) {
			vdp_water_pal[0][i >> 4][i & 15] = VDP_ConvertColour(vdp_water_dry[i]);
			vdp_water_pal[1][i >> 4][i & 15] = VDP_ConvertColour(vdp_water_wet[i]);
		}
	}
	if (vdp_split_mode != VDP_SPLIT_NONE && screen2p.palette != NULL) {
		pal_to = &vdp_screen_pal2[0][0];
		for (size_t i = 0; i < ACTIVE_COLOURS; i++)
			*pal_to++ = VDP_ConvertColour(screen2p.palette[i]);
	}
}

// Fills `cache` (one entry per line of a view, `rows` of them) with the sprites each line shows, from a sprite table: the dedicated buffer if there is one,
// the VDP's own table in VRAM otherwise.
static void VDP_BuildSpriteCache(struct VDP_SpriteCache *cache, int rows, const uint16_t *table, int ybase, bool double_cells) {
	for (uint8_t i = 0;;) {
		//Get sprite values -- from the dedicated external buffer if one's
		//been registered (see VDP_SetSpriteBuffer), otherwise fall back to
		//the real-hardware-accurate VRAM location for anything that doesn't
		//use it.
		if (table == NULL)
			return; // (no sprite table: no sprites)
		const uint16_t *sprite = table + ((uint16_t)i << 2);
		uint16_t sprite_y = sprite[0];
		uint16_t sprite_sl = sprite[1];
		uint8_t sprite_width = (sprite_sl & SPRITE_SL_W_AND) >> SPRITE_SL_W_SHIFT;
		uint8_t sprite_height = (sprite_sl & SPRITE_SL_H_AND) >> SPRITE_SL_H_SHIFT;
		uint8_t sprite_link = (sprite_sl & SPRITE_SL_L_AND) >> SPRITE_SL_L_SHIFT;

		//Get sprite bounding area
		int top = sprite_y - ybase;
		int bottom = top + ((sprite_height + 1) << (double_cells ? 4 : 3));
		if (top < 0)
			top = 0;
		if (bottom > rows)
			bottom = rows;

		//Write sprite cache
		for (int v = top; v < bottom; v++) {
			struct VDP_SpriteCache *scache = &cache[v];
			scache->pixels += sprite_width + 1;
			if (scache->pixels <= SCANLINE_SPRITES)
				scache->sprite[scache->pushind++] = sprite;
		}

		//Go to next sprite
		if (sprite_link != 0)
			i = sprite_link;
		else
			break;
	}
}

// Draws the whole frame into the VDP's own picture buffer (and runs the vertical and horizontal interrupts as it goes)
void VDP_DrawFrame(void) {
	//Get VDP screen pointer
	vdp_screen = &vdp_screen_internal[VDP_INTERNAL_PAD];
	vdp_mask = &vdp_mask_internal[VDP_INTERNAL_PAD];

	//Send vertical interrupt. On a real VDP the vertical blank comes first: its handler copies the camera, the scroll tables, the
	//palette and the sprite table for the frame that is drawn after it, all together. Running it after the frame was drawn (as this
	//once did) showed the planes scrolled to the previous camera while the sprites, which are read live, had the new one: objects
	//drifted against the background whenever the screen scrolled.
	if (vdp_vint != NULL)
		vdp_vint();

	//Calculate sprite cache
	memset(vdp_sprite_cache, 0, sizeof(vdp_sprite_cache));
	const int H = SCREEN_HEIGHT;
	const bool stacked = vdp_split_mode == VDP_SPLIT_STACKED;
	const bool side = vdp_split_mode == VDP_SPLIT_SIDE;
	if (!stacked && !side) {
		VDP_BuildSpriteCache(vdp_sprite_cache, H, screen1p.sprites, 128, false);
	} else if (stacked) {
		//Two ordinary views, one above the other in the sprite caches as they are on the screen's rows (each is squashed when drawn)
		VDP_BuildSpriteCache(vdp_sprite_cache, H, screen1p.sprites, 128, false);
		VDP_BuildSpriteCache(vdp_sprite_cache + H, H, screen2p.sprites, 128, false);
	} else {
		VDP_BuildSpriteCache(vdp_sprite_cache, H, screen1p.sprites, 128, false);
		VDP_BuildSpriteCache(vdp_sprite_cache + H, H, screen2p.sprites, 128, false);
	}
	
	//Render VDP screen
	VDP_RefreshPalette();

	uint32_t *to = vdp_screen;
	uint8_t *tom = vdp_mask;
	const int16_t *hscroll = screen1p.hscroll;
	const size_t rows = (size_t)VDP_OutputRows();
	const bool water_split = (stacked || side) && vdp_water_dry != NULL;

	//Real HBlank hardware is an 8-bit down-counter (VDP register $0A) that reloads and fires every (vdp_hint_counter + 1) lines for as long as H-ints are
	//enabled -- not a single one-shot interrupt at a fixed position. This loop covers the entire picture itself; there is no "remainder" left to draw
	//afterward (a leftover second pass here, from before this loop was unified, kept advancing scache/hscroll/to/tom another full screen's worth past
	//the end of their buffers, reading/writing out of bounds -- that's what was crashing LZ, the only zone with H-ints actually enabled).
	int32_t countdown = screen1p.hint_counter;
	for (size_t y = 0; y < rows; y++, to += SCREEN_PITCH, tom += SCREEN_PITCH) {
		if (!stacked && !side) {
			VDP_DrawScanline(y, to, tom, &vdp_sprite_cache[y], hscroll + 2 * y);
		} else if (stacked) {
			//Two ordinary views (each a whole picture, with its own planes, scroll, sprites and palette), the first squashed into the top half of the picture and the second into the bottom half:
			//each output row is the average of the rows of the view it covers
			static uint32_t src_pixels[SCREEN_MAX_PITCH];
			static uint8_t src_mask[SCREEN_MAX_PITCH];
			const bool second = y >= (size_t)(H / 2);
			const size_t local = second ? y - (size_t)(H / 2) : y;
			const size_t span = second ? (size_t)H - (size_t)(H / 2) : (size_t)(H / 2);
			size_t first_row = local * (size_t)H / span, end_row = (local + 1) * (size_t)H / span;
			if (end_row <= first_row)
				end_row = first_row + 1;
			if (end_row > (size_t)H)
				end_row = (size_t)H;
			const viewport_t *vp = second ? &screen2p : &screen1p;
			const int16_t *hs = vp->hscroll;
			uint32_t (*view_pal)[16] = (second && screen2p.palette != NULL) ? vdp_screen_pal2 : vdp_screen_pal;
			uint32_t sum_r[SCREEN_MAX_WIDTH], sum_g[SCREEN_MAX_WIDTH], sum_b[SCREEN_MAX_WIDTH];
			memset(sum_r, 0, sizeof(uint32_t) * SCREEN_WIDTH);
			memset(sum_g, 0, sizeof(uint32_t) * SCREEN_WIDTH);
			memset(sum_b, 0, sizeof(uint32_t) * SCREEN_WIDTH);
			for (size_t src = first_row; src < end_row; src++) {
				vdp_draw_pal = water_split ? vdp_water_pal[(int16_t)src > vdp_water_line[second ? 1 : 0]] : view_pal;
				VDP_DrawViewRow(src, src_pixels + VDP_INTERNAL_PAD, src_mask + VDP_INTERNAL_PAD, SCREEN_WIDTH, 128, false, vp, &vdp_sprite_cache[(second ? (size_t)H : 0) + src], hs + 2 * src);
				for (int x = 0; x < SCREEN_WIDTH; x++) {
					uint32_t c = src_pixels[VDP_INTERNAL_PAD + x];
					sum_r[x] += (c >> 24) & 0xFF;
					sum_g[x] += (c >> 16) & 0xFF;
					sum_b[x] += (c >> 8) & 0xFF;
				}
			}
			vdp_draw_pal = vdp_screen_pal;
			const uint32_t n = (uint32_t)(end_row - first_row);
			for (int x = 0; x < SCREEN_WIDTH; x++)
				to[x] = ((sum_r[x] / n) << 24) | ((sum_g[x] / n) << 16) | ((sum_b[x] / n) << 8) | 0xFF;
		} else {
			//Two views side by side: each is drawn on its own row buffer, then the halves go into the picture
			static uint32_t half_pixels[SCREEN_MAX_PITCH];
			static uint8_t half_mask[SCREEN_MAX_PITCH];
			const int w1 = SCREEN_WIDTH / 2, w2 = SCREEN_WIDTH - w1;
			vdp_draw_pal = water_split ? vdp_water_pal[(int16_t)y > vdp_water_line[0]] : vdp_screen_pal;
			VDP_DrawViewRow(y, half_pixels + VDP_INTERNAL_PAD, half_mask + VDP_INTERNAL_PAD, w1, 128, false, &screen1p, &vdp_sprite_cache[y], hscroll + 2 * y);
			memcpy(to, half_pixels + VDP_INTERNAL_PAD, (size_t)w1 * sizeof(uint32_t));
			vdp_draw_pal = water_split ? vdp_water_pal[(int16_t)y > vdp_water_line[1]] : (screen2p.palette != NULL ? vdp_screen_pal2 : vdp_screen_pal);
			const int16_t *hs2 = screen2p.hscroll + 2 * y;
			VDP_DrawViewRow(y, half_pixels + VDP_INTERNAL_PAD, half_mask + VDP_INTERNAL_PAD, w2, 128, false, &screen2p, &vdp_sprite_cache[(size_t)H + y], hs2);
			vdp_draw_pal = vdp_screen_pal;
			memcpy(to + w1, half_pixels + VDP_INTERNAL_PAD, (size_t)w2 * sizeof(uint32_t));
		}

		if (screen1p.hint_enable && !water_split && countdown-- <= 0) {
			countdown = screen1p.hint_counter;

			//Send horizontal interrupt
			vdp_hint();
			VDP_RefreshPalette();
		}
	}
	
}

const uint32_t *VDP_GetFrame(int *pitch, int *rows) {
	if (pitch != NULL)
		*pitch = SCREEN_PITCH;
	if (rows != NULL)
		*rows = VDP_OutputRows();
	return vdp_screen;
}

void VDP_Render(void) {
	VDP_DrawFrame();

	//Render screen
	{ // TEMP-HASH
		extern int vdp_hash_hook(const uint32_t *screen);
		vdp_hash_hook(vdp_screen);
	}
	Render_Screen(vdp_screen);

	//Generate and queue this frame's audio
	Audio_Update();

	//Handle events
	if (Input_HandleEvents()) {
		//Game should close
		MegaDrive_Quit();
		exit(0);
	}
}

// Debug viewer accessors (see VDP.h)
const uint8_t *VDP_PeekVRAM(void) {
	return vdp_vram;
}

uint16_t VDP_PeekCRAM(int pal, int index) {
	return VDP_RGB2Genesis(vdp_cram[pal & (VDP_PALETTES - 1)][index & 0xF]);
}

uint32_t VDP_PeekColour(int pal, int index) {
	return VDP_GetColour((size_t)(((pal & (VDP_PALETTES - 1)) << 4) | (index & 0xF)));
}

int VDP_PeekSprites(VdpSpritePeek *out, int max) {
	int n = 0;
	uint8_t i = 0;
	while (n < max) {
		if (screen1p.sprites == NULL)
			break;
		const uint16_t *sprite = screen1p.sprites + ((uint16_t)i << 2);
		uint16_t sl = sprite[1], tile = sprite[2];
		VdpSpritePeek *e = &out[n++];
		e->index = i;
		e->link = (sl & SPRITE_SL_L_AND) >> SPRITE_SL_L_SHIFT;
		e->y = (int16_t)((sprite[0] & SPRITE_Y_AND) - 128);
		e->x = (int16_t)((sprite[3] & SPRITE_X_AND) - 128);
		e->width = ((sl & SPRITE_SL_W_AND) >> SPRITE_SL_W_SHIFT) + 1;
		e->height = ((sl & SPRITE_SL_H_AND) >> SPRITE_SL_H_SHIFT) + 1;
		e->pattern = (tile & TILE_PATTERN_AND) >> TILE_PATTERN_SHIFT;
		e->palette = (tile & TILE_PALETTE_AND) >> TILE_PALETTE_SHIFT;
		e->priority = (tile & TILE_PRIORITY_AND) != 0;
		e->x_flip = (tile & TILE_X_FLIP_AND) != 0;
		e->y_flip = (tile & TILE_Y_FLIP_AND) != 0;
		if (e->link == 0)
			break;
		i = e->link;
	}
	return n;
}

// TEMP-HASH: frame hash dump for refactor verification (SONIC_HASH=file SONIC_FRAMES=n)
#include <stdio.h>
#include <stdlib.h>
int vdp_hash_hook(const uint32_t *screen) {
	static FILE *hf; static int n, init;
	if (!init) { init = 1; const char *p = getenv("SONIC_HASH"); if (p) hf = fopen(p, "w"); }
	if (!hf) return 0;
	uint32_t h = 2166136261u;
	for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) h = (h ^ screen[i]) * 16777619u;
	{ const char *d = getenv("SONIC_DUMP"); // frame:prefix
		if (d) { int f0 = atoi(d); const char *pre = strchr(d, ':') + 1; if (n >= f0 && n < f0 + 8) { char nm[256]; snprintf(nm, sizeof nm, "%s%d.ppm", pre, n); FILE *pf = fopen(nm, "wb"); fprintf(pf, "P6 %d %d 255\n", SCREEN_WIDTH, SCREEN_HEIGHT); for (int yy = 0; yy < SCREEN_HEIGHT; yy++) for (int xx = 0; xx < SCREEN_WIDTH; xx++) { uint32_t c = screen[yy * SCREEN_PITCH + xx]; fputc((c >> 24) & 255, pf); fputc((c >> 16) & 255, pf); fputc((c >> 8) & 255, pf); } fclose(pf); } } }
	fprintf(hf, "%d %08x\n", n++, h);
	const char *fr = getenv("SONIC_FRAMES");
	if (fr && n >= atoi(fr)) { fflush(hf); exit(0); }
	return 0;
}
