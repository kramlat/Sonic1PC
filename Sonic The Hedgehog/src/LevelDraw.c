#include "LevelDraw.h"
#include "LevelDrawCore.h" // the block drawing: the engine's

#include "Constants.h"
#include "Level.h"
#include "LevelScroll.h"
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

// Level drawing functions
void LoadTilesFromStart(void) {
    DrawChunks(scrpos_x.f.u, scrpos_y.f.u, LEVEL_LAYOUT_PLANE_FG, &screen1p.plane_a);
    if (LevelScroll_UsesBands())
        LevelScroll_DrawBands(&bg_scrpos_x, &bg2_scrpos_x, &bg3_scrpos_x, &bg_scrpos_y);
    else
    DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, LEVEL_LAYOUT_PLANE_BG, &screen1p.plane_b);
}

void LoadTilesAsYouMove(void) {
    if (LevelScroll_UsesBands())
        LevelScroll_DrawBands(&bg_scrpos_x_dup, &bg2_scrpos_x_dup, &bg3_scrpos_x_dup, &bg_scrpos_y_dup);
    LevelPlane_DrawPending(&fg_plane, LEVEL_LAYOUT_PLANE_FG);
}

void LoadTilesAsYouMove_BGOnly(void) {
    if (LevelScroll_UsesBands())
        LevelScroll_DrawBands(&bg_scrpos_x, &bg2_scrpos_x, &bg3_scrpos_x, &bg_scrpos_y);
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

void AnimateLevelGfx(void) {
    // Don't run if game is paused
    if (pause_state)
        return;

    // Animate giant ring
    AniArt_GiantRing();

    // Run level animation
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
        AniArt_GHZWaterfall();
        AniArt_GHZFlowerLarge();
        AniArt_GHZFlowerSmall();
        break;
    case ZoneId_LZ:
        break;
    case ZoneId_MZ:
        AniArt_MZLava();
        AniArt_MZMagma();
        AniArt_MZTorch();
        break;
    case ZoneId_SLZ:
        break;
    case ZoneId_SYZ:
        break;
    case ZoneId_SBZ:
        AniArt_SBZSmoke();
        break;
    case ZoneId_EndZ:
        AniArt_Ending();
        break;
    }
}
