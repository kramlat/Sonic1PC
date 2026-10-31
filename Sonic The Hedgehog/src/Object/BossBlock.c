#include "BossBlock.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "BossSpringYard.h"
#include "Level.h"
#include "LevelScroll.h"
#include "LevelCollision.h"
#include "Macros.h"
#include "Resource/Mappings/SYZBossBlocks.h"
#include "Sound.h"

// Object 76 - blocks that Eggman picks up (SYZ boss). BossSpringYard.c's own
// spike attack grabs one of these by writing a per-block command byte
// (child_cmd) and a back-reference (parent_index) into it -- see
// BossSpringYard.c's BSYZ_Descend/BSYZ_BreakBlock.
//
// Each block's own "am I still in my normal, untouched state?" check is
// child_cmd == subtype, not child_cmd == 0 -- BossBlock_Main deliberately
// seeds both bytes to the SAME per-block index (matching real ASM's
// `move.w d4,obSubtype` word write, where d4's high/low bytes are always
// equal), so any of the 10 blocks can use a single shared boss-controller
// command value (-1 grabbed, 0xA breaking) without needing a separate
// "normal" sentinel that could collide with a real block index.

#define SYZ_BOSS_X 0x2C00

static const int16_t bblock_frag_speed[4][2] = {
    { (int16_t)-0x180, (int16_t)-0x200 }, // top-left
    {          0x180,  (int16_t)-0x200 }, // top-right
    { (int16_t)-0x100, (int16_t)-0x100 }, // bottom-left
    {          0x100,  (int16_t)-0x100 }, // bottom-right
};
static const int16_t bblock_frag_pos[4][2] = {
    { -8,   -8 },   // top-left
    { 0x10,  0 },   // top-right
    { 0,    0x10 }, // bottom-left
    { 0x10, 0x10 }, // bottom-right
};

static void BossBlock_Main(Sst *self) {
    int16_t x = SYZ_BOSS_X + 0x10;

    for (int i = 0; i < 10; i++) {
        Sst *block = self;
        if (i > 0) {
            block = Sst_Allocate();
            if (block == NULL)
                break;
        }

        Own_BossBlock *bscratch = (Own_BossBlock *)block->own.u8;

        block->code = Obj_BossBlock;
        block->mappings = Mappings_SYZBossBlocks;
        block->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // | Tile_Pal3
        // A freshly-Sst_Allocate()'d slot can carry a stale self->mapping_frame from
        // whatever object previously lived there. BuildSprites indexes
        // Mappings_SYZBossBlocks's 5-frame offset table by self->mapping_frame with
        // no bounds checking (see Object.c's own BuildSprites) -- an
        // out-of-range leftover value reads garbage as the piece count,
        // which can come out as 0 and silently draw nothing at all for
        // that block, letting the background chunk underneath show through
        // untouched. Explicit reset closes that off.
        block->mapping_frame = 0;
        block->render_flags |= SST_RENDER_LEVEL;
        block->width_pixels = 32 / 2;
        block->height_pixels = block->y_radius;
        block->y_radius = 32 / 2;
        block->priority = 3 * 0x80;
        block->x_pos.f.u = x;
        block->y_pos.f.u = 0x582;
        block->subtype = (uint8_t)i; // subtype -- this block's own column index
        bscratch->child_cmd = (int8_t)i;   // seeded equal to subtype -- see file header comment
        block->routine = 2;

        x = (int16_t)(x + 0x20);
    }
}

static void BossBlock_Break(Sst *self) {
    self->routine += 2; // -> BossBlock_Frag
    self->width_pixels = 16 / 2;
    self->y_radius = 16 / 2;

    Sst *prev = self;
    for (int i = 0; i < 4; i++) {
        Sst *frag = self;
        if (i > 0) {
            frag = Sst_AllocateAfter(prev);
            if (frag == NULL)
                break;
            *frag = *self; // clone the original block's full state (mappings/tile/render/priority/pos/width/y_rad)
        }
        frag->x_vel = bblock_frag_speed[i][0];
        frag->y_vel = bblock_frag_speed[i][1];
        frag->x_pos.f.u = (int16_t)(self->x_pos.f.u + bblock_frag_pos[i][0]);
        frag->y_pos.f.u = (int16_t)(self->y_pos.f.u + bblock_frag_pos[i][1]);
        frag->mapping_frame = (uint8_t)(1 + i);
        prev = frag;
    }
    PlaySound(sfx_WallSmash);
}

static void BossBlock_Action(Sst *self, Own_BossBlock *own) {
    if (own->child_cmd == (int8_t)self->subtype) {
        // Normal/untouched -- solid, standable block.
        SstSolidObject(self, 16 + 11 /* sonic_solid_width */, 16, 17, self->x_pos.f.u, NULL, NULL);
        Sst_Draw(self);
        return;
    }

    if (own->child_cmd < 0) {
        // Grabbed -- track the boss's position (44px below it), predicting
        // its Y motion if the boss hasn't been processed yet this frame
        // (i.e. sits later in sst[] than this block).
        Sst *parentObj = &sst[own->parent_index];
        if (parentObj->collision_property == 0) { // obBossHits ran out mid-grab
            BossBlock_Break(self);
            Sst_Draw(self);
            return;
        }

        self->x_pos.f.u = parentObj->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(parentObj->y_pos.f.u + 44);

        if (own->parent_index >= (uint8_t)(self - sst)) {
            // Boss hasn't run yet this frame -- predict where it'll end up.
            self->y_pos.f.u = (int16_t)(self->y_pos.f.u + (parentObj->y_vel >> 8));
        }
        Sst_Draw(self);
        return;
    }

    // Breaking (child_cmd == 0xA).
    BossBlock_Break(self);
    Sst_Draw(self);
}

static void BossBlock_Frag(Sst *self) {
    if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_MoveSprite(self);
    Sst_Draw(self);
}

void Obj_BossBlock(Sst *self) {
    Own_BossBlock *own = (Own_BossBlock *)self->own.u8;

    switch (self->routine) {
    case 0: BossBlock_Main(self); break;
    case 2: BossBlock_Action(self, own); break;
    case 4: BossBlock_Frag(self); break;
    }
}
