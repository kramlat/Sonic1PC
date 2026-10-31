#include "Sprites.h"

#include "EngineObject.h"
#include "Sst.h"
#include "Rings.h"
#include "ScrollEngine.h"

#include "Camera.h"
#include "EngineConstants.h"
#include "Video.h"

#include "Backend/VDP.h"

// The sprite pipeline: objects queue themselves by priority (Sst_Draw), and BuildSprites turns the queues into the VDP's sprite table, front to back,
// placing each against the camera layer its render flags name (sprite_view). With the split screen on (sprite_split_screen) it builds two tables, one for
// each player's view, with ordinary tiles, stacked (each squashed into its half) or side by side.

// The camera layers sprites are placed against, by render flags (level_bg << 1 | level_fg): none (screen coordinates), the foreground, the background, and both
// (Sonic 1: the third scroll layer). A game assigns its own.
SpriteView sprite_view = {{
	{ NULL, NULL },
	{ &scrpos_x.f.u,     &scrpos_y.f.u },
	{ &bg_scrpos_x.f.u,  &bg_scrpos_y.f.u },
	{ &bg3_scrpos_x.f.u, &bg3_scrpos_y.f.u },
}};

// The second player's view (the split screen): all the level layers follow the second camera, as in Sonic 2
SpriteView sprite_view_p2 = {{
	{ NULL, NULL },
	{ &scrpos_x_p2.f.u, &scrpos_y_p2.f.u },
	{ &scrpos_x_p2.f.u, &scrpos_y_p2.f.u },
	{ &scrpos_x_p2.f.u, &scrpos_y_p2.f.u },
}};

int sprite_split_screen = 0;
uint8_t sprite_teleport_flag = 0;

// Y of the top of a view, in sprite coordinates: the normal screen starts at 128; the split screen is drawn in the VDP's double-height mode, where
// everything is twice as tall and each player's half is a screen high (Sonic 2's spriteScreenPositionY2P)
#define SPRITE_TOP_1P 0x80
#define SPRITE_TOP_2P_P1 (0x80 * 2)
#define SPRITE_TOP_2P_P2 (0x80 * 2 + SCREEN_HEIGHT)

// Cells are twice as tall in the double-height mode, so a piece has half the rows (Sonic 2's SpriteSizes_2P). Indexed by the piece's size byte.
static const uint8_t sprite_sizes_2p[16] = { 0, 0, 1, 1, 4, 4, 5, 5, 8, 8, 9, 9, 0xC, 0xC, 0xD, 0xD };

// Draws `count` consecutive sprite pieces (Mappings.h) with their origin at (x, y) in sprite coordinates. The flips mirror the whole sprite about that origin;
// `base_tile` is added to every piece's tile; the 2-player form uses the pieces' second tile word and the half-height sizes.
// `s3` reads Sonic 3's pieces (6 bytes: no second tile word). `cull_w` (not 0) skips a piece that is out of the view as Sonic 3's multi-draw does (sub_1B070): its sprite Y at or above -32 or at 224 and below, its X at or
// above -32 or at `cull_w` and past (all in sprite coordinates, so +128), without using a sprite for it.
static void DrawPiecesEx(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t base_tile, const tilebank_t *bank, uint8_t palette_group, bool x_flip, bool y_flip, bool two_p, const uint8_t *piece, unsigned count, bool s3, int cull_w) {
	while (count-- > 0) {
		//Don't overflow the sprite buffer
		if (*sprite_i >= BUFFER_SPRITES)
			break;

		//Read mappings
		int8_t map_y = Mappings_PieceY(piece);
		uint8_t map_size = Mappings_PieceSize(piece);
		uint16_t map_tile = s3 ? Mappings_PieceTileS3(piece) : two_p ? Mappings_PieceTile2P(piece) : Mappings_PieceTile(piece);
		int16_t map_x = s3 ? Mappings_PieceXS3(piece) : Mappings_PieceX(piece);
		piece += s3 ? SPRITE_PIECE_SIZE_S3 : SPRITE_PIECE_SIZE;

		//The piece's size in pixels, for mirroring
		int piece_height = ((map_size << 3) & 0x18) + 8;
		int piece_width = ((map_size << 1) & 0x18) + 8;

		if (cull_w) {
			int16_t cy = y_flip ? (int16_t)(y - map_y - piece_height) : (int16_t)(y + map_y);
			int16_t cx = x_flip ? (int16_t)(x - map_x - piece_width) : (int16_t)(x + map_x);
			if (cy <= -32 + 128 || cy >= SCREEN_HEIGHT + 128 || cx <= -32 + 128 || cx >= cull_w + 128)
				continue;
		}

		//Write sprite
		sprite_t *out = (*sprite)++;
		out->y = y_flip ? (uint16_t)(y - map_y - piece_height) : (uint16_t)(y + map_y);
		out->size_link = (uint16_t)(((two_p ? sprite_sizes_2p[map_size & 0xF] : map_size) << 8) | ++(*sprite_i));
		uint16_t tile = map_tile + base_tile;
		if (x_flip)
			tile ^= TILE_X_FLIP_AND;
		if (y_flip)
			tile ^= TILE_Y_FLIP_AND;
		const tilebank_t *tile_bank = bank != NULL ? bank : TileBank_Main();
		out->pattern = tile & TILE_PATTERN_AND;
		out->attr = TileAttr_FromWord(tile, tile_bank, palette_group);
		out->generation = tile_bank->generation;
		uint16_t px = x_flip ? (uint16_t)(x - map_x - piece_width) : (uint16_t)(x + map_x);
		// The VDP's sprite x is 9 bits. SCREEN_WIDTH is a variable, not a constant: a preprocessor test of it was always true (an unknown name is 0), so every picture wrapped at 512, and a child sprite far
		// out on one side (a wide object's far corner) came in at the other, even in a widescreen picture, which has no use for the wrap
		if (SCREEN_WIDTH <= 320) {
			if ((px &= 0x1FF) == 0)
				px++; //Prevent sprite from being x=0 (acts as a mask)
		} else if (px == 0) {
			px++;
		}
		out->x = px;
	}
}

#define DrawPieces(sprite, sprite_i, x, y, base_tile, bank, palette_group, x_flip, y_flip, two_p, piece, count) \
	DrawPiecesEx(sprite, sprite_i, x, y, base_tile, bank, palette_group, x_flip, y_flip, two_p, piece, count, false, 0)

void BuildSpr_Normal(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces) {
	DrawPieces(sprite, sprite_i, x, y, tile, NULL, 0, false, false, false, mappings, (unsigned)pieces + 1);
}

void BuildSpr_NormalS3(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces) {
	DrawPiecesEx(sprite, sprite_i, x, y, tile, NULL, 0, false, false, false, mappings, (unsigned)pieces + 1, true, 0);
}

// Draws `count` Sonic 3 pieces with their origin at (x, y) in sprite coordinates, each piece culled singly against a view `cull_w` wide (the ring handler's sprites)
void BuildSpr_PiecesS3(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *pieces, unsigned count, int cull_w) {
	DrawPiecesEx(sprite, sprite_i, x, y, tile, NULL, 0, false, false, false, pieces, count, true, cull_w);
}

// Render_Sprites for the Sonic 3 objects (Sst.h) of one draw queue (sonic3k.asm line 36348): the same culling and the same pieces, in the sprite pipeline's table. A level object is placed against the foreground camera, other
// positions are sprite coordinates already; a multi-draw object has a main sprite (frame 0: none) and its children, each piece culled singly.
void SstRenderQueue(int queue, sprite_t **sprite, uint8_t *sprite_i, const SpriteView *view, int16_t top, int view_w, bool clear_on_screen) {
	const SpriteLayer *layer = &view->layer[1];
	const int16_t cam_x = layer->x ? *layer->x : 0, cam_y = layer->y ? *layer->y : 0;
	const int16_t y_base = (int16_t)(top - SPRITE_TOP_1P);
	for (int j = 0; j < Sst_QueueCount(queue); j++) {
		Sst *obj = Sst_QueueEntry(queue, j);
		if (clear_on_screen)
			obj->render_flags &= 0x7F;
		if (obj->mappings == NULL)
			continue; // (an object that is not set up yet has nothing to show)
		const uint8_t flags = obj->render_flags;
		const bool level = (flags & SST_RENDER_LEVEL) != 0;
		const bool x_flip = (flags & SST_RENDER_X_FLIP) != 0, y_flip = (flags & SST_RENDER_Y_FLIP) != 0;
		int16_t x = obj->x_pos.f.u, y = obj->y_pos.f.u;
		const int w = obj->width_pixels, h = obj->height_pixels;

		if (flags & SST_RENDER_MULTI_DRAW) {
			if (level) {
				x = (int16_t)(x - cam_x);
				if (x + w < 0 || x - w >= view_w)
					continue;
				x = (int16_t)(x + 128);
				y = (int16_t)((y - cam_y + h) & level_screen_y_wrap);
				if ((uint16_t)y >= (uint16_t)(2 * h + SCREEN_HEIGHT + SCREEN_TALLADD))
					continue;
				y = (int16_t)(y + 128 - h);
			} else {
				x = (int16_t)(x - 128);
				if (x + w < 0 || x - w >= view_w)
					continue;
				x = (int16_t)(x + 128);
				y = (int16_t)(y - 128);
				if (y + h < 0 || y - h >= SCREEN_HEIGHT)
					continue;
				y = (int16_t)(y + 128);
			}
			obj->render_flags |= SST_RENDER_ON_SCREEN;
			if (*sprite_i >= BUFFER_SPRITES)
				continue;
			if (obj->mapping_frame != 0) {
				const uint8_t *pieces;
				uint16_t count = Mappings_FramePieces(obj->mappings, obj->mapping_frame, &pieces);
				if (count)
					DrawPiecesEx(sprite, sprite_i, (uint16_t)x, (uint16_t)(y + y_base), obj->art_tile, obj->bank, obj->palette_group, x_flip, y_flip, false, pieces, count, true, view_w);
			}
			for (int c = 0; c < obj->child_count && c < SST_CHILDREN && *sprite_i < BUFFER_SPRITES; c++) {
				int16_t cx = obj->children[c].x, cy = obj->children[c].y;
				if (level) {
					cx = (int16_t)(cx - cam_x + 128);
					cy = (int16_t)((cy - cam_y + 128) & level_screen_y_wrap);
				}
				const uint8_t *pieces;
				uint16_t count = Mappings_FramePieces(obj->mappings, obj->children[c].frame, &pieces);
				if (count)
					DrawPiecesEx(sprite, sprite_i, (uint16_t)cx, (uint16_t)(cy + y_base), obj->art_tile, obj->bank, obj->palette_group, x_flip, y_flip, false, pieces, count, true, view_w);
			}
			continue;
		}

		if (level) {
			x = (int16_t)(x - cam_x);
			if (x + w < 0 || x - w >= view_w)
				continue;
			x = (int16_t)(x + 128);
			y = (int16_t)((y - cam_y + h) & level_screen_y_wrap);
			if ((uint16_t)y >= (uint16_t)(2 * h + SCREEN_HEIGHT + SCREEN_TALLADD))
				continue;
			y = (int16_t)(y + 128 - h);
		}
		obj->render_flags |= SST_RENDER_ON_SCREEN;
		if (*sprite_i >= BUFFER_SPRITES)
			continue;
		const uint8_t *pieces;
		uint16_t count;
		if (flags & SST_RENDER_STATIC) {
			pieces = obj->mappings; // (one lone piece)
			count = 1;
		} else {
			count = Mappings_FramePieces(obj->mappings, obj->mapping_frame, &pieces);
		}
		if (count)
			DrawPiecesEx(sprite, sprite_i, (uint16_t)x, (uint16_t)(y + y_base), obj->art_tile, obj->bank, obj->palette_group, x_flip, y_flip, false, pieces, count, true, 0);
	}
}

// One pass over the queued objects for one view: `top` is where the view's top is in sprite coordinates, `view_w` how wide it is (the cull checks use it). The first pass clears each object's on-screen flag; a second one
// only adds to it, so an object is on screen if it is in either view.
static void BuildPass(sprite_t **sprite, uint8_t *sprite_i, const SpriteView *view, int16_t top, bool clear_on_screen, int view_w) {
	// The level's rings come first (Render_Sprites: the HUD, the rings, then the queues)
	Rings_Render(sprite, sprite_i, view, top, view_w);
	for (int i = 0; i < SST_QUEUES; i++)
		SstRenderQueue(i, sprite, sprite_i, view, top, view_w, clear_on_screen);
}

// Ends a sprite list. If the table is full, the last entry's link is cleared; otherwise the next entry is zeroed (a terminator)
static void EndSpriteList(sprite_t *sprite, uint8_t sprite_i) {
	if (sprite_i >= BUFFER_SPRITES) {
		sprite[-1].size_link &= 0xFF00; //Clear link byte
	} else {
		sprite->y = 0;
		sprite->size_link = 0;
	}
}

void BuildSprites(uint8_t *sprite_io) {
	uint8_t sprite_i = 0;

	if (sprite_split_screen == SPRITE_SPLIT_NONE) {
		sprite_t *sprite = screen1p.sprite_table;
		BuildPass(&sprite, &sprite_i, &sprite_view, SPRITE_TOP_1P, true, SCREEN_WIDTH);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);
	} else if (sprite_split_screen == SPRITE_SPLIT_STACKED) {
		//Stacked: two ordinary views, each the whole picture (the VDP squashes each into its half), with ordinary tiles and sizes
		sprite_t *sprite = screen1p.sprite_table;
		BuildPass(&sprite, &sprite_i, &sprite_view, SPRITE_TOP_1P, true, SCREEN_WIDTH);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);

		sprite = screen2p.sprite_table;
		sprite_i = 0;
		BuildPass(&sprite, &sprite_i, &sprite_view_p2, SPRITE_TOP_1P, false, SCREEN_WIDTH);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);
	} else {
		//Side by side: ordinary tiles and sizes, each view the left or right part of the picture (the VDP counts sprite X from each view's own left edge), both
		//tops at the usual 128
		const int w1 = SCREEN_WIDTH / 2, w2 = SCREEN_WIDTH - w1;
		sprite_t *sprite = screen1p.sprite_table;
		BuildPass(&sprite, &sprite_i, &sprite_view, SPRITE_TOP_1P, true, w1);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);

		sprite = screen2p.sprite_table;
		sprite_i = 0;
		BuildPass(&sprite, &sprite_i, &sprite_view_p2, SPRITE_TOP_1P, false, w2);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);
	}

	// Every queued object has been drawn (or left out): the queues start empty next frame (Sonic 2 keeps them while its Teleport_flag is set, in the split screen)
	if (!(sprite_split_screen == SPRITE_SPLIT_STACKED && sprite_teleport_flag)) {
		Sst_ClearQueues();
	}

	if (sprite_io != NULL)
		*sprite_io = sprite_i;
}
