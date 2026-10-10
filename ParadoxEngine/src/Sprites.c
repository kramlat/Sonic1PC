#include "Sprites.h"

#include "EngineObject.h"

#include "Camera.h"
#include "EngineConstants.h"
#include "Video.h"

#include "Backend/VDP.h"

// The sprite pipeline: objects queue themselves by priority (DisplaySprite), and BuildSprites turns the queues into the VDP's sprite table, front to back,
// placing each against the camera layer its render flags name (sprite_view). With the split screen on (sprite_split_screen) it builds two tables, one for
// each player's view, with ordinary tiles, stacked (each squashed into its half) or side by side.

//Object draw queue
struct SpriteQueue {
	uint32_t size;
	// 0x3F (63) was undersized: this per-priority-tier bucket is this
	// port's own internal pre-sort mechanism, not something modeled on real
	// hardware (real Genesis VDP has no concept of per-priority sprite
	// buckets, just one flat 80-slot table) -- DisplaySprite silently drops
	// anything past this cap with zero fallback, so if enough objects ever
	// share one priority value, a genuinely below-hardware-budget number of
	// objects could vanish from render even while the real 80-sprite total
	// (BUFFER_SPRITES) stays nowhere near exhausted. 0x50 matches
	// BUFFER_SPRITES -- no single tier could legitimately need more than
	// the real hardware total anyway.
	Object *obj[0x50];
} sprite_queue[8];

// The camera layers sprites are placed against, by render flags (level_bg << 1 | level_fg): none (screen coordinates), the foreground, the background, and both
// (Sonic 1: the third scroll layer). A game assigns its own.
SpriteView sprite_view = {{
	{ NULL, NULL },
	{ &scrpos_x.f.u,     &scrpos_y.f.u },
	{ &bg_scrpos_x.f.u,  &bg_scrpos_y.f.u },
	{ &bg3_scrpos_x.f.u, &bg3_scrpos_y.f.u },
}, 0};

// The second player's view (the split screen): all the level layers follow the second camera, as in Sonic 2
SpriteView sprite_view_p2 = {{
	{ NULL, NULL },
	{ &scrpos_x_p2.f.u, &scrpos_y_p2.f.u },
	{ &scrpos_x_p2.f.u, &scrpos_y_p2.f.u },
	{ &scrpos_x_p2.f.u, &scrpos_y_p2.f.u },
}, 0};

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
static void DrawPieces(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t base_tile, const tilebank_t *bank, uint8_t palette_group, bool x_flip, bool y_flip, bool two_p, const uint8_t *piece, unsigned count) {
	while (count-- > 0) {
		//Don't overflow the sprite buffer
		if (*sprite_i >= BUFFER_SPRITES)
			break;

		//Read mappings
		int8_t map_y = Mappings_PieceY(piece);
		uint8_t map_size = Mappings_PieceSize(piece);
		uint16_t map_tile = two_p ? Mappings_PieceTile2P(piece) : Mappings_PieceTile(piece);
		int16_t map_x = Mappings_PieceX(piece);
		piece += SPRITE_PIECE_SIZE;

		//The piece's size in pixels, for mirroring
		int piece_height = ((map_size << 3) & 0x18) + 8;
		int piece_width = ((map_size << 1) & 0x18) + 8;

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

void BuildSpr_Normal(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces) {
	DrawPieces(sprite, sprite_i, x, y, tile, NULL, 0, false, false, false, mappings, (unsigned)pieces + 1);
}

// One frame of an object's mappings, pieces and all, at a place on screen (Sonic 2's ChkDrawSprite)
static void DrawFrame(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, const Object *obj, unsigned frame, bool two_p) {
	const uint8_t *pieces;
	uint16_t count = Mappings_FramePieces((const uint8_t *)obj->mappings, frame, &pieces);
	if (count)
		DrawPieces(sprite, sprite_i, x, y, obj->tile, obj->bank, obj->palette_group, obj->render.f.x_flip, obj->render.f.y_flip, two_p, pieces, count);
}

// BuildSprites_MultiDraw: an object that is a main sprite (frame, width_pixels, y_rad) and up to OBJECT_CHILDREN children placed in the level. Always against the
// foreground camera. Out of the screen horizontally, and none of it is drawn; the main sprite's frame 0 means no main sprite.
static void DrawMultiSprite(sprite_t **sprite, uint8_t *sprite_i, Object *obj, const SpriteView *view, int16_t top, bool two_p, int view_w) {
	const SpriteLayer *layer = &view->layer[1];
	int16_t cam_x = *layer->x, cam_y = *layer->y;
	bool wrap = view->wrap_y && !two_p;

	//Check if the object is within X bounds
	int16_t ox = obj->pos.l.x.f.u - cam_x;
	int16_t width = obj->width_pixels;
	if (ox + width < 0 || ox - width >= view_w)
		return;
	uint16_t x = (uint16_t)(128 + ox);

	//... and Y bounds
	uint16_t y;
	if (obj->render.f.explicit_height) {
		int16_t oy = obj->pos.l.y.f.u - cam_y;
		uint8_t height = (uint8_t)obj->y_rad;
		if (oy + height < 0 || oy - height >= SCREEN_HEIGHT)
			return;
		y = (uint16_t)(top + oy);
	} else {
		int16_t oy = obj->pos.l.y.f.u - cam_y + 0x80;
		if (wrap)
			oy &= 0x7FF;
		if (oy < 0x60 || oy >= (0x180 + SCREEN_TALLADD))
			return;
		y = (uint16_t)(oy + (top - SPRITE_TOP_1P));
	}

	if (obj->frame != 0)
		DrawFrame(sprite, sprite_i, x, y, obj, obj->frame, two_p);
	obj->render.f.on_screen = true;

	for (int i = 0; i < obj->child_count && i < OBJECT_CHILDREN; i++) {
		// A child far from the view is not drawn: the VDP's sprite positions are 9 bits, so one a few hundred pixels away (a wide object's far corner, as it comes into view) would come round the other side of the
		// picture. (Sonic 2's own multi-draw has no such check: its children are close together.)
		int16_t dx = obj->children[i].x - cam_x;
		int16_t dy = obj->children[i].y - cam_y;
		if (wrap)
			dy = (int16_t)(((dy + 0x400) & 0x7FF) - 0x400);
		if (dx < -64 || dx > view_w + 64 || dy < -64 || dy > SCREEN_HEIGHT + 64)
			continue;
		int16_t cx = obj->children[i].x - cam_x + 128;
		int16_t cy = obj->children[i].y - cam_y + 128;
		if (wrap)
			cy &= 0x7FF;
		DrawFrame(sprite, sprite_i, (uint16_t)cx, (uint16_t)(cy + (top - SPRITE_TOP_1P)), obj, obj->children[i].frame, two_p);
	}
}

// One pass over the queued objects for one view: Sonic 2's BuildSprites_LevelLoop and the 2-player passes. `top` is where the view's top is in sprite
// coordinates, `view_w` how wide it is (the cull checks use it). The first pass clears each object's on-screen flag; a second one only adds to it, so an object is
// on screen if it is in either view.
static void BuildPass(sprite_t **sprite, uint8_t *sprite_i, const SpriteView *view, int16_t top, bool two_p, bool clear_on_screen, int view_w) {
	struct SpriteQueue *queue = sprite_queue;
	for (int i = 0; i < 8; i++, queue++) {
		//Iterate through all queued objects
		for (uint32_t j = 0; j < queue->size; j++) {
			Object *obj = queue->obj[j];
			if (obj->mappings == NULL) //This line isn't in the original, but without it, the title screen segfaults
				continue;              //Basically, the bug that causes the 'PRESS START BUTTON' text to not appear gives the object null mappings
			if (obj->type == OBJECT_NULL)
				continue;

			//Get object position on screen and check if visible
			if (clear_on_screen)
				obj->render.f.on_screen = false;

			//An object with child sprites draws them all together (Sonic 2's BuildSprites_MultiDraw)
			if (obj->render.f.multi_sprite) {
				DrawMultiSprite(sprite, sprite_i, obj, view, top, two_p, view_w);
				continue;
			}

			uint16_t x, y;
			if (obj->render.f.level_bg || obj->render.f.level_fg) {
				//Get screen position to use
				const SpriteLayer *layer = &view->layer[(obj->render.f.level_bg << 1) | obj->render.f.level_fg];

				//Get object X position
				int16_t ox = obj->pos.l.x.f.u - *layer->x;
				if ((ox + obj->width_pixels) < 0 || (ox - obj->width_pixels) >= view_w)
					continue;
				x = 128 + ox; //VDP sprites start at 128

				//Get object Y position
				if (obj->render.f.explicit_height) {
					int16_t oy = obj->pos.l.y.f.u - *layer->y;
					// Real hardware zero-extends this byte (moveq #0,d0 / move.b
					// obHeight(a0),d0) rather than sign-extending it -- matters
					// because fragmentated objects (see FragmentatePlatform) never
					// get y_rad re-initialized on freshly allocated slots, so it
					// can hold garbage >=128 that must still read as a large
					// positive height here, not a negative one.
					uint8_t height = (uint8_t)obj->y_rad;
					if ((oy + height) < 0 || (oy - height) >= SCREEN_HEIGHT)
						continue;
					y = top + oy;
				} else {
					int16_t oy = obj->pos.l.y.f.u - *layer->y + 0x80;
					if (view->wrap_y && !two_p)
						oy &= 0x7FF;
					if (oy < 0x60 || oy >= (0x180 + SCREEN_TALLADD))
						continue;
					y = oy + (top - SPRITE_TOP_1P);
				}
			} else {
				//Positions map directly to VDP coordinates (rebased to the view)
				x = obj->pos.s.x;
				y = obj->pos.s.y + (top - SPRITE_TOP_1P);
			}

			//Get object mappings to use
			if (!obj->render.f.static_mappings) {
				//Index mapping by frame
				const uint8_t *pieces;
				uint16_t count = Mappings_FramePieces((const uint8_t *)obj->mappings, obj->frame, &pieces);
				if (count)
					DrawPieces(sprite, sprite_i, x, y, obj->tile, obj->bank, obj->palette_group, obj->render.f.x_flip, obj->render.f.y_flip, two_p, pieces, count);
			} else {
				//Directly use object mappings pointer: one piece
				DrawPieces(sprite, sprite_i, x, y, obj->tile, obj->bank, obj->palette_group, obj->render.f.x_flip, obj->render.f.y_flip, two_p, (const uint8_t *)obj->mappings, 1);
			}
			obj->render.f.on_screen = true;
		}
	}
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
		BuildPass(&sprite, &sprite_i, &sprite_view, SPRITE_TOP_1P, false, true, SCREEN_WIDTH);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);
	} else if (sprite_split_screen == SPRITE_SPLIT_STACKED) {
		//Stacked: two ordinary views, each the whole picture (the VDP squashes each into its half), with ordinary tiles and sizes
		sprite_t *sprite = screen1p.sprite_table;
		BuildPass(&sprite, &sprite_i, &sprite_view, SPRITE_TOP_1P, false, true, SCREEN_WIDTH);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);

		sprite = screen2p.sprite_table;
		sprite_i = 0;
		BuildPass(&sprite, &sprite_i, &sprite_view_p2, SPRITE_TOP_1P, false, false, SCREEN_WIDTH);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);
	} else {
		//Side by side: ordinary tiles and sizes, each view the left or right part of the picture (the VDP counts sprite X from each view's own left edge), both
		//tops at the usual 128
		const int w1 = SCREEN_WIDTH / 2, w2 = SCREEN_WIDTH - w1;
		sprite_t *sprite = screen1p.sprite_table;
		BuildPass(&sprite, &sprite_i, &sprite_view, SPRITE_TOP_1P, false, true, w1);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);

		sprite = screen2p.sprite_table;
		sprite_i = 0;
		BuildPass(&sprite, &sprite_i, &sprite_view_p2, SPRITE_TOP_1P, false, false, w2);
		sprite_count = sprite_i;
		EndSpriteList(sprite, sprite_i);
	}

	//Every queued object has been drawn (or left out): the queues start empty next frame (Sonic 2 keeps them while its Teleport_flag is set, in the split screen)
	if (!(sprite_split_screen == SPRITE_SPLIT_STACKED && sprite_teleport_flag))
		for (int i = 0; i < 8; i++)
			sprite_queue[i].size = 0;

	if (sprite_io != NULL)
		*sprite_io = sprite_i;
}

void DisplaySprite(Object *obj) {
	//Get queue to use
	struct SpriteQueue *queue = &sprite_queue[obj->priority & 7];

	//Push to queue
	if (queue->size >= (sizeof(queue->obj) / sizeof(Object*)))
		return;
	queue->obj[queue->size++] = obj;
}

// Sonic 2's Adjust2PArtPointer: the real hardware halves the tile numbers in its split screen (its double-height cells). This port draws two ordinary views instead, so there is nothing to do
void Object_Adjust2PArtPointer(Object *obj) {
	(void)obj; // (no halved tiles: the split screen draws ordinary views, the stacked one squashed, so the art is used as it is)
}
