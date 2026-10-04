// Sonic 2's level drawing: Sonic 1's (a copy of LevelDraw.c) with the background drawing Nick Arcade's zones need (see LoadTilesFromStart and DrawBG_Top).
#include "LevelDraw.h"
#include "LevelDrawCore.h" // the block drawing: the engine's

#include "Constants.h"
#include "Level.h"
#include "LevelScroll.h"
#include "SplitScreen.h"
#include "Kosinski.h"

#include "Backend/VDP.h"


// Scroll blocks
int16_t scroll_block1_size, scroll_block2_size, scroll_block3_size, scroll_block4_size;

// Hand-tuned backup -- kept here, commented out, in case the real tables
// below need to be rolled back again. See the real tables' own comment for
// why they were reverted to this before.
//
// const uint8_t MZ_ScrollArray[144] = {
//     // $00-$0F: First 16 entries (matches your working table)
//     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
//     // $10-$1F: Next 16 entries
//     2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
//     // $20-$2F: Next 16 entries
//     4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
//     // $30-$3F: Next 16 entries
//     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
//     // $40-$4F: Next 16 entries
//     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
//     // $50-$5F: Next 16 entries
//     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
//     // $60-$6F: Next 16 entries
//     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
//     // $70-$7F: Next 16 entries
//     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
//     // $80-$8F: Extra 16 bytes of padding (for safety)
//     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6
// };
//
// const uint8_t SBZ_ScrollArray[48] = {
//         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
//         0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
// 		// padding
// 		0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06
//     };

// Real, byte-verified disassembly table, swapped back in with the
// bg_pos_table[3] axis fix (bg3_scrpos_x instead of _y, see its own comment
// below) already in place -- if a regression (mountains missing, clouds
// rendering narrow/wrong) shows up again, see the hand-tuned backup above.
// SBZ is left on its own hand-tuned table for now (see below).
const uint8_t MZ_ScrollArray[144] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04,
    0x04, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
    0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Real BG_ScrollBlockMap_SBZ (REV01 "Level Drawing"): which BG layer's X position each 16px row of the SBZ act 1
// background follows -- 0 = static, 2 = block 1 (lower black buildings), 4 = block 2 (upper black buildings),
// 6 = block 3 (distant brown buildings). The values double as the redraw-flag bit numbers DrawBG_ColumnForBGIndex
// tests. The original is 34 bytes (rows -1..32 once offset by the +1 the row draws use); a column strip can
// read up to 15 entries past the end of that, where the ROM just has whatever bytes follow, so here the entries
// past the end wrap back around to the start of the 32-row cycle instead.
const uint8_t SBZ_ScrollArray[48] = {
    0, 0, 0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4, // $00-$0F
    4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, // $10-$1F
    2, 0,                                           // $20-$21
    0, 0, 0, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 4        // wrap of $02-$0F
};

// Real ASM has TWO separate X-position tables: DrawBG_XPos_Ptrs (live vars
// -- v_bgscreenposx/v_bg2screenposx/v_bg3screenposx) used only by the
// one-shot initial full draw (DrawBG_RowForBGIndex, reached from
// LoadTilesFromStart), and DrawBG_XPosCopy_Ptrs (the "_dup" vars) used only
// by the incremental per-frame scroll-block redraw (DrawBG_ColumnForBGIndex,
// and DrawBlocks_BG_2 when called from the incremental Draw_MZ/Draw_SBZ
// paths). Using live vars in an incremental context would sample whatever
// position the CURRENT (not-yet-fully-processed) frame has already reached,
// instead of the frame-start snapshot every other incremental redraw uses.
const dword_s* bg_pos_table[] = {
    &bg_scrpos_x,  // Index 0
    &bg_scrpos_x,  // Index 2
    &bg2_scrpos_x, // Index 4
    &bg3_scrpos_x  // Index 6
};

const dword_s* bg_pos_table_dup[] = {
    &bg_scrpos_x_dup,  // Index 0
    &bg_scrpos_x_dup,  // Index 2
    &bg2_scrpos_x_dup, // Index 4
    &bg3_scrpos_x_dup  // Index 6
};

// Real hardware's DrawBG_XPos_Ptrs/DrawBG_XPosCopy_Ptrs entries are pointers
// to each layer's own {X,Y} position struct (X at offset 0, Y at +4) -- both
// Calc_VRAM_Pos and GetBlockData read "4(a3)" to add the SELECTED layer's
// own Y directly, raw/untransformed (its own internal masking handles plane
// wraparound). These Y-position counterparts let us do the same.
const dword_s* bg_pos_table_y[] = {
    &bg_scrpos_y,  // Index 0
    &bg_scrpos_y,  // Index 2
    &bg2_scrpos_y, // Index 4
    &bg3_scrpos_y  // Index 6
};

const dword_s* bg_pos_table_y_dup[] = {
    &bg_scrpos_y_dup,  // Index 0
    &bg_scrpos_y_dup,  // Index 2
    &bg2_scrpos_y_dup, // Index 4
    &bg3_scrpos_y_dup  // Index 6
};

void DrawBlocks_BG_2(size_t offset, int16_t sx, int16_t sy, int16_t index_bias, int16_t y, const uint8_t *layout, const uint8_t *array, size_t entries, const dword_s *const *pos_table, const dword_s *const *pos_table_y) {
	// Matches the disassembly's "move.w v_bgscreenposy,d0 ; add.w d4,d0 ;
	// andi.w #$1F0,d0" (mask varies per table size): combine the camera
	// position with the relative row offset (y can legitimately be
	// negative, e.g. -16 for "one row above the screen"), then wrap via an
	// unsigned mask rather than a signed shift -- plain "y >> 4" on a
	// negative y indexed before the start of the array. index_bias is MZ's
	// extra "subi.w #$200,d0" (its table is addressed 512px further along
	// than its own BG camera position) -- zero for every other zone.
	uint16_t mask = (uint16_t)((entries << 4) - 16);
	uint8_t bg_pos_i = array[((uint16_t)(sy + y + index_bias) & mask) >> 4];
	if (bg_pos_i != 0) {
		sx = pos_table[bg_pos_i >> 1]->f.u;
		// Real Calc_VRAM_Pos/GetBlockData add the SELECTED layer's own raw
		// Y register directly (its own internal masking/wraparound handles
		// the rest) -- NOT a transform of the generic sy this function was
		// called with. Using (sy & ~0xF) % SCROLL_HEIGHT here sampled a
		// wildly wrong Y (e.g. 518 -> 64), landing the row read far from
		// where it should be.
		sy = pos_table_y[bg_pos_i >> 1]->f.u;
		// Real DrawBG_RowForBGIndex explicitly sets d5=-16 (the usual
		// one-tile-left-margin convention used everywhere else in this
		// file) before BOTH the VRAM-position calc and the draw call --
		// this was passing x=0 for both instead.
		DrawBlocks_LR(offset, CalcVRAMPos(sx, sy, -16, y), sx, sy, -16, y, layout);
	}
	else
		// Matches the disassembly's ".bgXPos0" branch of DrawBG_RowForBGIndex
		// exactly: DrawBlocks_LR_3 (REV01's VRAM-wrapping variant, matching
		// every other "draw a full-width strip" call in this file) at
		// PLANE_WIDTH/2 blocks (512px, the plane's real width) -- not plain
		// DrawBlocks_LR_2 at a full PLANE_WIDTH (1024px, double the plane),
		// which wrapped the destination VRAM position around twice per row
		// and drew over itself with the wrong tiles.
		DrawBlocks_LR_3(offset, CalcVRAMPos(sx, sy, 0, y), sx, sy, 0, y, layout, PLANE_WIDTH / 2);
}

void DrawBlocks_BG(size_t offset, int16_t sx, int16_t sy, int16_t y, const uint8_t *layout, const uint8_t *array, size_t entries, const dword_s *const *pos_table, const dword_s *const *pos_table_y) {
	DrawBlocks_BG_2(offset, sx, sy, 0, y, layout, array, entries, pos_table, pos_table_y);
}

// Matches DrawBG_ColumnForBGIndex in the disassembly: draws a full 16-row
// vertical strip when more than one scroll-block redraw flag is pending at
// once (diagonal scrolling in MZ/SBZ). Unlike DrawBlocks_BG (one row, one
// table lookup derived from the camera position), this walks 16 consecutive
// table entries -- one per row of the strip, starting from table[0] --
// checking each directly against the shared redraw-flags byte. Only ever
// reached from incremental (per-frame) redraw paths, so it always uses the
// "_dup" position table (real ASM: DrawBG_XPosCopy_Ptrs), never the live one.
void DrawBG_ColumnForBGIndex(size_t offset, int16_t x, int16_t y, int16_t sy, const uint8_t *layout, const uint8_t *table, uint16_t *flag) {
	for (size_t i = 0; i < SCROLL_ROWS; i++, y += 16) {
		uint8_t bit = table[i];
		if (*flag & (1 << bit)) {
			const uint8_t *meta, *block;
			int16_t row_sx = bg_pos_table_dup[bit >> 1]->f.u;
			GetBlockData(&meta, &block, row_sx, sy, x, y, layout);
			DrawBlock(meta, block, offset + CalcVRAMPos(row_sx, sy, x, y));
		}
	}
	// Matches "clr.b (a2)" -- only the low (SBZ/MZ-specific) byte of the
	// redraw-flags word is cleared, not the whole 16-bit value.
	*flag &= 0xFF00;
}

void Draw_GHZ_Bg(int16_t sy, const uint8_t *layout, size_t offset) {
	int16_t y = 0;
	for (size_t i = 0; i < SCROLL_ROWS; i++) {
		static const uint8_t bg_array[] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x06, 0x06, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		DrawBlocks_BG(offset, bg_scrpos_y.f.u, sy, y, layout, bg_array, 16, bg_pos_table, bg_pos_table_y);
		y += 16;
	}
}

void Draw_MZ_Bg(int16_t sy, const uint8_t *layout, size_t offset) {
    // Matches Draw_MZ_BG in the disassembly: starts one row above the top of
    // the screen (y = -16, not 0), reads MZ_ScrollArray+1 (its row-index
    // table is addressed with a +1 offset -- see BG_ScrollBlockMap_MZ+1),
    // and biases the row index by -512 (v_bgscreenposy - $200) before
    // masking, unlike every other zone's table.
    int16_t y = -16;
    for (size_t i = 0; i < SCROLL_ROWS; i++) {
        DrawBlocks_BG_2(offset, bg_scrpos_x.f.u, sy, -0x200, y, layout, MZ_ScrollArray + 1, 128, bg_pos_table, bg_pos_table_y);
        y += 16;
    }
}

void Draw_SBZ_Bg(int16_t sy, const uint8_t *layout, size_t offset) {
    // Matches Draw_SBZ_act1_BG in the disassembly: y = -16 (not 0), and
    // SBZ_ScrollArray+1 (see BG_ScrollBlockMap_SBZ+1).
    int16_t y = -16;
    for (size_t i = 0; i < SCROLL_ROWS; i++) {
        DrawBlocks_BG(offset, bg_scrpos_x.f.u, sy, y, layout, SBZ_ScrollArray + 1, 32, bg_pos_table, bg_pos_table_y);
        y += 16;
    }
}

// Level drawing functions
void S2_LoadAnimatedBlocks(void); // AnimatedArt.c

void LoadTilesFromStart(void) {
    S2_LoadAnimatedBlocks(); // (Nick Arcade patches the animated blocks into the block table before the tiles are drawn)
    DrawChunks(scrpos_x.f.u, scrpos_y.f.u, LEVEL_LAYOUT_FG(0), VRAM_FG);
    // (the backgrounds of Sonic 2's zones are plain)
    DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, LEVEL_LAYOUT_BG(0), VRAM_BG);
}

void DrawBG_Top(int16_t sx, int16_t sy, uint16_t *flag, const uint8_t *layout, size_t offset) {
	//Check if any flags have been set
	if (*flag == 0)
		return;
	// Real DrawBG_Top's bit0/bit1 (top/bottom) use plain DrawBlocks_LR --
	// its own default width ((320+16+16)/16, a screen-width strip) -- NOT
	// the full 32-block plane width. That full-plane draw only belongs to
	// bit4/bit5 ("TopAll"/"BottomAll") below, via DrawBlocks_LR_3.
	if (*flag & SCROLL_FLAG_UP) {
		DrawBlocks_LR(offset, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout);
		*flag &= ~SCROLL_FLAG_UP;
	}
	if (*flag & SCROLL_FLAG_DOWN) {
		DrawBlocks_LR(offset, CalcVRAMPos(sx, sy, -16, SCROLL_HEIGHT), sx, sy, -16, SCROLL_HEIGHT, layout);
		*flag &= ~SCROLL_FLAG_DOWN;
	}
	if (*flag & SCROLL_FLAG_LEFT) {
		DrawBlocks_TB(offset, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout);
		*flag &= ~SCROLL_FLAG_LEFT;
	}
	if (*flag & SCROLL_FLAG_RIGHT) {
		DrawBlocks_TB(offset, CalcVRAMPos(sx, sy, RIGHT_EDGE_X, -16), sx, sy, RIGHT_EDGE_X, -16, layout);
		*flag &= ~SCROLL_FLAG_RIGHT;
	}
	if (*flag & SCROLL_FLAG_UP2) {
		DrawBlocks_LR_3(offset, CalcVRAMPos(0, sy, 0, -16), 0, sy, 0, -16, layout, PLANE_WIDTH / 2);
		*flag &= ~SCROLL_FLAG_UP2;
	}
	if (*flag & SCROLL_FLAG_DOWN2) {
		DrawBlocks_LR_3(offset, CalcVRAMPos(0, sy, 0, SCROLL_HEIGHT), 0, sy, 0, SCROLL_HEIGHT, layout, PLANE_WIDTH / 2);
		*flag &= ~SCROLL_FLAG_DOWN2;
	}
	// Bits 6 and 7 (Nick Arcade's Draw_BG1 has them: Hidden Palace's background moves by them): a whole plane's width of blocks of the row above and below the view, from one block left of it
	if (*flag & (1 << 6)) {
		DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, -16, -16), sx, sy, -16, -16, layout, PLANE_WIDTH / 2);
		*flag &= ~(1 << 6);
	}
	if (*flag & (1 << 7)) {
		DrawBlocks_LR_2(offset, CalcVRAMPos(sx, sy, -16, SCROLL_HEIGHT), sx, sy, -16, SCROLL_HEIGHT, layout, PLANE_WIDTH / 2);
		*flag &= ~(1 << 7);
	}
}


// Split out of DrawBG_Bottom's SBZ branch, named to match real ASM's own
// Draw_SBZ label for easier cross-referencing: a full row above/below the
// screen if the top/bottom flag is set, then any remaining flag bits
// trigger a vertical strip via DrawBG_ColumnForBGIndex.
static void Draw_SBZ(int16_t sx, uint16_t *flag, const uint8_t *layout, size_t offset) {
	int16_t vertical_offset = -16; // Margin for drawing new tiles
	if (*flag & 0x01) {
		*flag &= ~0x01;
		DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, vertical_offset, layout, SBZ_ScrollArray + 1, 32, bg_pos_table_dup, bg_pos_table_y_dup);
	} else if (*flag & 0x02) {
		*flag &= ~0x02;
		vertical_offset = SCREEN_HEIGHT; // Draw slice at the bottom of the screen
		DrawBlocks_BG(offset, sx, bg_scrpos_y.f.u, vertical_offset, layout, SBZ_ScrollArray + 1, 32, bg_pos_table_dup, bg_pos_table_y_dup);
	}
	if (*flag & 0xFF) {
		// Matches Draw_SBZ's ".more"/".doMore": any remaining flag bits
		// trigger a full vertical strip via DrawBG_ColumnForBGIndex (not
		// another single-row lookup -- unlike the top/bottom case above,
		// no +1 table offset here), at the screen's left edge by default,
		// or the right edge if any of bits 7/5/3 ($A8) are among those
		// remaining (which also get folded down into bits 6/4/2 for the
		// strip's own per-row bit tests).
		int16_t col_x = -16;
		uint8_t submask = (uint8_t)(*flag & 0xA8);
		if (submask != 0) {
			*flag = (*flag & 0xFF00) | (submask >> 1);
			col_x = RIGHT_EDGE_X;
		}
		uint16_t idx = (uint16_t)bg_scrpos_y.f.u & 0x1F0;
		DrawBG_ColumnForBGIndex(offset, col_x, -16, bg_scrpos_y.f.u, layout, SBZ_ScrollArray + (idx >> 4), flag);
	}
}

void DrawBG_Bottom(int16_t sx, int16_t sy, uint16_t *flag, const uint8_t *layout, size_t offset) {
	if (*flag == 0)
		return;
		if (LEVEL_ZONE(level_id) != ZoneId_HTZ) {
			if (*flag & SCROLL_FLAG_LEFT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, -16, 0x70), sx, sy, -16, 0x70, layout, 3);
				*flag &= ~SCROLL_FLAG_LEFT2;
			}
			if (*flag & SCROLL_FLAG_RIGHT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, RIGHT_EDGE_X, 0x70), sx, sy, RIGHT_EDGE_X, 0x70, layout, 3);
				*flag &= ~SCROLL_FLAG_RIGHT2;
			}
		} else {
			Draw_SBZ(sx, flag, layout, offset);
			return;
		}
}

// Split out of DrawBG_Block3's MZ branch, named to match real ASM's
// own Draw_MZ label for easier cross-referencing: a full row above/below
// the screen if the top/bottom flag is set, then any remaining flag bits
// trigger a vertical strip via DrawBG_ColumnForBGIndex.
static void Draw_MZ(int16_t sx, uint16_t *flag, const uint8_t *layout, size_t offset) {
	int16_t y_rel = -16;
	// Real Draw_MZ checks bit0 ("top")/bit1 ("bottom") here, set by
	// BGScroll_YAbsolute -- NOT SCROLL_FLAG_LEFT/RIGHT (bit2/bit3), which
	// are Block1's own X-scroll redraw bits and would collide with this
	// check once Block1 uses its correct real bit numbers (see Deform_MZ's
	// own comment on that fix).
	if (!(*flag & SCROLL_FLAG_UP)) {
		if (*flag & SCROLL_FLAG_DOWN) {
			*flag &= ~SCROLL_FLAG_DOWN;
			y_rel = SCREEN_HEIGHT;
		} else goto check_mz_col;
	} else *flag &= ~SCROLL_FLAG_UP;
	// MZ's row lookup is biased by -512 for the table index only -- the
	// actual draw position/content still uses the real, unbiased
	// bg_scrpos_y_dup -- and (like Draw_MZ_BG) reads MZ_ScrollArray+1 for
	// this single-row case.
	DrawBlocks_BG_2(offset, sx, bg_scrpos_y_dup.f.u, -0x200, y_rel, layout, MZ_ScrollArray + 1, 128, bg_pos_table_dup, bg_pos_table_y_dup);
	check_mz_col:
	if ((*flag & 0xFF) == 0) return;
	// Matches Draw_MZ's ".more"/".doMore": any remaining flag bits trigger
	// a full vertical strip via DrawBG_ColumnForBGIndex (no +1 table
	// offset here, unlike the single-row case above), at the screen's left
	// edge by default, or the right edge if any of bits 7/5/3 ($A8) are
	// among those remaining (which also get folded down into bits 6/4/2
	// for the strip's own per-row bit tests).
	int16_t col_x = -16;
	uint8_t cf = (uint8_t)(*flag & 0xFF);
	if (cf & 0xA8) {
		cf >>= 1;
		*flag = (*flag & 0xFF00) | cf;
		col_x = RIGHT_EDGE_X;
	}
	uint16_t idx = (uint16_t)(bg_scrpos_y_dup.f.u - 0x200) & 0x7F0;
	DrawBG_ColumnForBGIndex(offset, col_x, -16, bg_scrpos_y_dup.f.u, layout, MZ_ScrollArray + (idx >> 4), flag);
}

// Chemical Plant's third background (Draw_BG3's CPz branch): the same row-by-owner drawing as Metropolis's, with Chemical Plant's own table: the plane's rows down to row 18 follow the first
// background's X (value 2), those below the second's (4); rows are 16 lines, the table 64 of them
static const uint8_t CPZ_ScrollArrayRaw[1 + 64 + 16] = {
    2, // (loc_718E: the strip of columns starts a row above the view, so it reads the table from one entry before the view's row)
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, // (what a strip reads past the end: the same)
};
#define CPZ_ScrollArray (CPZ_ScrollArrayRaw + 1)

static void Draw_CPZ(int16_t sx, uint16_t *flag, const uint8_t *layout, size_t offset) {
	(void)sx;
	int16_t y_rel = -16;
	if (*flag & SCROLL_FLAG_UP) {
		*flag &= ~SCROLL_FLAG_UP;
	} else if (*flag & SCROLL_FLAG_DOWN) {
		*flag &= ~SCROLL_FLAG_DOWN;
		y_rel = SCREEN_HEIGHT;
	} else {
		goto columns;
	}
	DrawBlocks_BG(offset, bg_scrpos_x_dup.f.u, bg_scrpos_y_dup.f.u, y_rel, layout, CPZ_ScrollArray, 64, bg_pos_table_dup, bg_pos_table_y_dup);
columns:
	if ((*flag & 0xFF) == 0)
		return;
	int16_t col_x = -16;
	uint8_t cf = (uint8_t)(*flag & 0xFF);
	if (cf & 0xA8) {
		cf >>= 1;
		*flag = (*flag & 0xFF00) | cf;
		col_x = RIGHT_EDGE_X;
	}
	uint16_t idx = (uint16_t)bg_scrpos_y_dup.f.u & 0x3F0;
	DrawBG_ColumnForBGIndex(offset, col_x, -16, bg_scrpos_y_dup.f.u, layout, CPZ_ScrollArray + (idx >> 4) - 1, flag);
}

void DrawBG_Block3(int16_t sx, int16_t sy, uint16_t *flag, const uint8_t *layout, size_t offset) {
	//Check if any flags have been set
	if (*flag == 0)
		return;
		//Run completely different code if in Marble Zone (what)
		if (LEVEL_ZONE(level_id) != ZoneId_CPZ) {
			if (*flag & SCROLL_FLAG_LEFT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, -16, 64), sx, sy, -16, 64, layout, 3);
				*flag &= ~SCROLL_FLAG_LEFT2;
			}
			if (*flag & SCROLL_FLAG_RIGHT2) {
				DrawBlocks_TB_2(offset, CalcVRAMPos(sx, sy, RIGHT_EDGE_X, 64), sx, sy, RIGHT_EDGE_X, 64, layout, 3);
				*flag &= ~SCROLL_FLAG_RIGHT2;
			}
		} else {
			Draw_CPZ(sx, flag, layout, offset);
		}
}

void LoadTilesAsYouMove(void) {
    SplitScreen_VBlank();
    DrawBG_Top(bg_scrpos_x_dup.f.u, bg_scrpos_y_dup.f.u,
                       &bg1_scroll_flags_dup, LEVEL_LAYOUT_BG(0), VRAM_BG);

    DrawBG_Bottom(bg2_scrpos_x_dup.f.u, bg2_scrpos_y_dup.f.u,
                       &bg2_scroll_flags_dup, LEVEL_LAYOUT_BG(0), VRAM_BG);

    // REV01 added a third scroll block call
    DrawBG_Block3(bg3_scrpos_x_dup.f.u, bg3_scrpos_y_dup.f.u,
                       &bg3_scroll_flags_dup, LEVEL_LAYOUT_BG(0), VRAM_BG);
    LevelPlane_DrawPending(&fg_plane, LEVEL_LAYOUT_FG(0));
}

void LoadTilesAsYouMove_BGOnly(void) {
    DrawBG_Top(bg_scrpos_x.f.u, bg_scrpos_y.f.u, &bg1_scroll_flags, LEVEL_LAYOUT_BG(0), VRAM_BG);
    DrawBG_Bottom(bg2_scrpos_x.f.u, bg2_scrpos_y.f.u, &bg2_scroll_flags, LEVEL_LAYOUT_BG(0), VRAM_BG);
    // No scroll block 3, even in REV01... odd
}

void LoadTiles(const uint8_t *source, uint16_t count) {
	do {
		VDP_WriteVRAM(source, TILE_SIZE);
		source += TILE_SIZE;
	} while (count-- != 0);
}

// Level art animation
#include "Resource/Art/GHZFlowerLarge.h"
#include "Resource/Art/GHZFlowerSmall.h"
#include "Resource/Art/EndFlowerExtra.h"
#include "Resource/Art/GHZWaterfall.h"
#include "Resource/Art/BigRing.h"

static void AniArt_GiantRing(void) {
    const uint16_t size = 14;

    if (gfx_big_ring == 0) {
        return;
    }

    gfx_big_ring -= size * 32;

    VDP_SeekVRAM((ArtTile_Giant_Ring * 32) + gfx_big_ring);
    VDP_WriteVRAM(Art_BigRing + gfx_big_ring, size * 0x20);
}

// Animate waterfall
void AniArt_GHZWaterfall(void) {
    if (--level_anim[0].time < 0) {
        // Increment frame and reset timer
        level_anim[0].time = 5;
        uint8_t frame = level_anim[0].frame++ & 1;

        // Write to VRAM
        VDP_SeekVRAM(0x6F00);
        VDP_WriteVRAM(Art_GHZWaterfall + (frame * 8 * 0x20), 8 * 0x20);
    }
}

// Animate large flowers
void AniArt_GHZFlowerLarge(void) {
    if (--level_anim[1].time < 0) {
        // Increment frame and reset timer
        level_anim[1].time = 15;
        uint8_t frame = level_anim[1].frame++ & 1;

        // Write to VRAM
        VDP_SeekVRAM(ART_VRAM(ArtTile_GHZ_Big_Flower_1));
        VDP_WriteVRAM(Art_GHZFlowerLarge + (frame * 16 * 0x20), 16 * 0x20);
    }
}

// Animate small flowers
void AniArt_GHZFlowerSmall(void) {
    if (--level_anim[2].time < 0) {

        // Increment frame and reset timer
        level_anim[2].time = 7;

        static const uint8_t seq[4] = { 0, 1, 2, 1 };
        uint8_t frame = seq[level_anim[2].frame++ & 3];
        if (!(frame & 1))
            level_anim[2].time = 127;

        // Write to VRAM
        VDP_SeekVRAM(ART_VRAM(ArtTile_GHZ_Small_Flower));
        VDP_WriteVRAM(Art_GHZFlowerSmall + (frame * 12 * 0x20), 12 * 0x20);
    }
}

// Level Animation Index Mappings for Marble Zone
#define LAVA_ANIM    0 // uses time and frame
#define MAGMA_ANIM   1 // uses time (frame was for prototype UFO)
#define TORCH_TIMER  2 // uses time (v_lani2_time)
#define TORCH_ANIM   3 // uses frame (v_lani3_frame)
typedef void (*MagmaShiftFunc)(const uint8_t*, uint16_t);

#include "Resource/Art/MZLava1.h"
#include "Resource/Art/MZLava2.h"
#include "Resource/Art/MZTorch.h"

void MagmaRow_Shift0(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[0] << 24) | (src[1] << 16) | (src[2] << 8) | src[3]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift1(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[1] << 24) | (src[2] << 16) | (src[3] << 8) | src[4]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift2(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[2] << 24) | (src[3] << 16) | (src[4] << 8) | src[5]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift3(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[3] << 24) | (src[4] << 16) | (src[5] << 8) | src[6]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift4(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[4] << 24) | (src[5] << 16) | (src[6] << 8) | src[7]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift5(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[5] << 24) | (src[6] << 16) | (src[7] << 8) | src[8]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift6(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[6] << 24) | (src[7] << 16) | (src[8] << 8) | src[9]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift7(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[7] << 24) | (src[8] << 16) | (src[9] << 8) | src[10]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift8(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[8] << 24) | (src[9] << 16) | (src[10] << 8) | src[11]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift9(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[9] << 24) | (src[10] << 16) | (src[11] << 8) | src[12]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift10(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[10] << 24) | (src[11] << 16) | (src[12] << 8) | src[13]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift11(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[11] << 24) | (src[12] << 16) | (src[13] << 8) | src[14]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift12(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[12] << 24) | (src[13] << 16) | (src[14] << 8) | src[15]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift13(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[13] << 24) | (src[14] << 16) | (src[15] << 8) | src[0]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift14(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[14] << 24) | (src[15] << 16) | (src[0] << 8) | src[1]);
		src += 16;
	} while (lines-- != 0);
}

void MagmaRow_Shift15(const uint8_t *src, uint16_t lines) {
	do {
		VDP_WriteLong((src[15] << 24) | (src[0] << 16) | (src[1] << 8) | src[2]);
		src += 16;
	} while (lines-- != 0);
}

void AniArt_MZLava(void) {
	const uint8_t TILE_COUNT = 8;
	if (--level_anim[LAVA_ANIM].time < 0) {
		level_anim[LAVA_ANIM].time = 0x14 - 1;
		if (++level_anim[LAVA_ANIM].frame >= 3)
			level_anim[LAVA_ANIM].frame = 0;
		const uint8_t *src = Art_MZLava1 + (level_anim[LAVA_ANIM].frame * TILE_COUNT * TILE_SIZE);
		VDP_SeekVRAM(ArtTile_MZ_Animated_Lava * TILE_SIZE);
		LoadTiles(src, TILE_COUNT - 1);
	}
}

static const MagmaShiftFunc MagmaDistortionTable[] = {
	MagmaRow_Shift0,  MagmaRow_Shift1,  MagmaRow_Shift2,  MagmaRow_Shift3,
	MagmaRow_Shift4,  MagmaRow_Shift5,  MagmaRow_Shift6,  MagmaRow_Shift7,
	MagmaRow_Shift8,  MagmaRow_Shift9,  MagmaRow_Shift10, MagmaRow_Shift11,
	MagmaRow_Shift12, MagmaRow_Shift13, MagmaRow_Shift14, MagmaRow_Shift15
};

void AniArt_MZMagma(void) {
	if (--level_anim[MAGMA_ANIM].time < 0) {
		level_anim[MAGMA_ANIM].time = 2 - 1;
		uint32_t bank_offset = (uint32_t)level_anim[LAVA_ANIM].frame << 9;
		const uint8_t *magma_art = Art_MZLava2 + bank_offset;
		VDP_SeekVRAM(ArtTile_MZ_Animated_Magma * TILE_SIZE);
		uint8_t osc_val = (uint8_t)(oscillatory.state[4][0] >> 8);
		for (int chunk = 0; chunk < 4; chunk++) {
			uint8_t table_idx = (osc_val * 2) & 0x1E;
			MagmaShiftFunc ApplyShift = MagmaDistortionTable[(table_idx >> 1) & 15];
			ApplyShift(magma_art, 0x1F);
			osc_val += 4;
		}
	}
}

void AniArt_MZTorch(void) {
	const uint8_t TILE_COUNT = 6;
	if (--level_anim[TORCH_TIMER].time < 0) {
		level_anim[TORCH_TIMER].time = 8 - 1;
		uint8_t current_frame = level_anim[TORCH_ANIM].frame;
		level_anim[TORCH_ANIM].frame = (level_anim[TORCH_ANIM].frame + 1) & 3;
		const uint8_t *src = Art_MZTorch + (current_frame * TILE_COUNT * TILE_SIZE);
		VDP_SeekVRAM(ArtTile_MZ_Torch * TILE_SIZE);
		LoadTiles(src, TILE_COUNT - 1);
	}
}

// Level Animation Index Mappings for Scrap Brain Zone
#define SBZ_SMOKE1       0 // uses time and frame (v_lani0)
#define SBZ_SMOKE2       1 // uses time and frame (v_lani1)
#define SBZ_SMOKE_TIMER  2 // frame = primary puff cooldown (v_lani2_frame), time = secondary puff cooldown (v_lani2_time)

#include "Resource/Art/SBZSmoke.h"

// Background pollution smoke -- two independently-timed puffs, each
// cycling through 7 animation frames before going blank and waiting out
// a cooldown (3 seconds for the primary puff, 2 for the secondary) before
// starting again. The "blank" write reuses Art_SBZSmoke's own first
// size/2 tiles (written twice) rather than storing a separate blank asset.
void AniArt_SBZSmoke(void) {
	const uint8_t SIZE = 12;

	// Primary puff
	if (level_anim[SBZ_SMOKE_TIMER].frame == 0) {
		if (--level_anim[SBZ_SMOKE1].time < 0) {
			level_anim[SBZ_SMOKE1].time = 8 - 1;
			VDP_SeekVRAM(ArtTile_SBZ_Smoke_Puff_1 * TILE_SIZE);
			uint8_t frame = level_anim[SBZ_SMOKE1].frame++;
			frame &= 7;
			if (frame == 0) {
				level_anim[SBZ_SMOKE_TIMER].frame = 3 * 60;
				LoadTiles(Art_SBZSmoke, SIZE / 2 - 1);
				LoadTiles(Art_SBZSmoke, SIZE / 2 - 1);
			} else {
				LoadTiles(Art_SBZSmoke + (frame - 1) * SIZE * TILE_SIZE, SIZE - 1);
			}
		}
	} else {
		level_anim[SBZ_SMOKE_TIMER].frame--;
	}

	// Secondary puff
	if (level_anim[SBZ_SMOKE_TIMER].time == 0) {
		if (--level_anim[SBZ_SMOKE2].time < 0) {
			level_anim[SBZ_SMOKE2].time = 8 - 1;
			VDP_SeekVRAM(ArtTile_SBZ_Smoke_Puff_2 * TILE_SIZE);
			uint8_t frame = level_anim[SBZ_SMOKE2].frame++;
			frame &= 7;
			if (frame == 0) {
				level_anim[SBZ_SMOKE_TIMER].time = 2 * 60;
				LoadTiles(Art_SBZSmoke, SIZE / 2 - 1);
				LoadTiles(Art_SBZSmoke, SIZE / 2 - 1);
			} else {
				LoadTiles(Art_SBZSmoke + (frame - 1) * SIZE * TILE_SIZE, SIZE - 1);
			}
		}
	} else {
		level_anim[SBZ_SMOKE_TIMER].time--;
	}
}

// Level Animation Index Mapping for the ending sequence
#define ENDING_BIG_FLOWER_TIMER   1 // uses time and frame (v_lani1) -- same slot GHZ's own big flower uses
#define ENDING_SMALL_FLOWER_TIMER 2 // uses time and frame (v_lani2) -- same slot GHZ's own small flower uses, safe to share since the two zones never run at once

// The ending's extra flower art ("Flowers at Ending", Kosinski): the sunflower wall, then two more flower animations.
// Real hardware decompresses it into unused chunk RAM; here it has its own buffer. Layout: the second big flower's two
// frames (16 tiles each) at +0, flower 3's three frames at +0x400, flower 4's three frames at +0xA00.
static uint8_t ending_flower_art[0x2000];

void Ending_LoadFlowerArt(void) {
	KosDec(Art_EndFlowerExtra, ending_flower_art);
}

// Ending sequence -- big flower. Its first half reuses GHZ's own big-flower VRAM slot and art (Art_GHZFlowerLarge)
// directly, just on the ending's own independent timer; the second is the sunflower wall from the extra art.
static void AniArt_Ending_BigFlower(void) {
	if (--level_anim[ENDING_BIG_FLOWER_TIMER].time < 0) {
		level_anim[ENDING_BIG_FLOWER_TIMER].time = 8 - 1;
		uint8_t frame = level_anim[ENDING_BIG_FLOWER_TIMER].frame++ & 1;

		VDP_SeekVRAM(ART_VRAM(ArtTile_GHZ_Big_Flower_1));
		VDP_WriteVRAM(Art_GHZFlowerLarge + (frame * 16 * 0x20), 16 * 0x20);

		VDP_SeekVRAM(ART_VRAM(ArtTile_GHZ_Big_Flower_2));
		VDP_WriteVRAM(ending_flower_art + (frame * 16 * 0x20), 16 * 0x20);
	}
}

// Ending sequence -- small flower (reuses GHZ's own small-flower VRAM slot
// and art, Art_GHZFlowerSmall, just with a different frame sequence/timing
// since the ending runs its own independent animation here).
static void AniArt_Ending_SmallFlower(void) {
	if (--level_anim[ENDING_SMALL_FLOWER_TIMER].time < 0) {
		level_anim[ENDING_SMALL_FLOWER_TIMER].time = 8 - 1;

		static const uint8_t seq[8] = { 0, 0, 0, 1, 2, 2, 2, 1 };
		uint8_t frame = seq[level_anim[ENDING_SMALL_FLOWER_TIMER].frame++ & 7];

		VDP_SeekVRAM(ART_VRAM(ArtTile_GHZ_Small_Flower));
		VDP_WriteVRAM(Art_GHZFlowerSmall + (frame * 12 * 0x20), 12 * 0x20);
	}
}

// Ending sequence -- flowers 3 and 4: three frames each (played 0,1,2,1), 16 tiles per frame.
#define ENDING_FLOWER3_TIMER 4
#define ENDING_FLOWER4_TIMER 5

static void AniArt_Ending_Flower(int timer, int delay, uint16_t tile, size_t base) {
	if (--level_anim[timer].time < 0) {
		level_anim[timer].time = (int8_t)(delay - 1);
		static const uint8_t seq[4] = { 0, 1, 2, 1 };
		uint8_t frame = seq[level_anim[timer].frame++ & 3];

		VDP_SeekVRAM(ART_VRAM(tile));
		VDP_WriteVRAM(ending_flower_art + base + frame * 16 * 0x20, 16 * 0x20);
	}
}

void AniArt_Ending(void) {
	AniArt_Ending_BigFlower();
	AniArt_Ending_SmallFlower();
	AniArt_Ending_Flower(ENDING_FLOWER3_TIMER, 15, ArtTile_GHZ_Flower_3, 0x400);
	AniArt_Ending_Flower(ENDING_FLOWER4_TIMER, 12, ArtTile_GHZ_Flower_4, 0xA00);
}

void S2_AnimateLevelArt(void); // AnimatedArt.c
void S2_LoadAnimatedBlocks(void);

void AnimateLevelGfx(void) {
    // Don't run if game is paused
    if (pause_state)
        return;

    // Animate giant ring
    AniArt_GiantRing();

    // Run level animation (Nick Arcade's: AnimatedArt.c)
    S2_AnimateLevelArt();
}
