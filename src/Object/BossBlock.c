#include "BossBlock.h"

#include "BossSpringYard.h"
#include "Level.h"
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

static void BossBlock_Main(Object *obj) {
    int16_t x = SYZ_BOSS_X + 0x10;

    for (int i = 0; i < 10; i++) {
        Object *block = obj;
        if (i > 0) {
            block = FindFreeObj();
            if (block == NULL)
                break;
        }

        Scratch_BossBlock *bscratch = (Scratch_BossBlock *)&block->scratch;

        block->type = ObjId_BossBlock;
        block->mappings = Mappings_SYZBossBlocks;
        block->tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // | Tile_Pal3
        // A freshly-FindFreeObj()'d slot can carry a stale obj->frame from
        // whatever object previously lived there. BuildSprites indexes
        // Mappings_SYZBossBlocks's 5-frame offset table by obj->frame with
        // no bounds checking (see Object.c's own BuildSprites) -- an
        // out-of-range leftover value reads garbage as the piece count,
        // which can come out as 0 and silently draw nothing at all for
        // that block, letting the background chunk underneath show through
        // untouched. Explicit reset closes that off.
        block->frame = 0;
        block->render.f.align_fg = true;
        block->width_pixels = 32 / 2;
        block->render.f.yrad_height = true;
        block->y_rad = 32 / 2;
        block->priority = 3;
        block->pos.l.x.f.u = x;
        block->pos.l.y.f.u = 0x582;
        block->scratch.u8[0] = (uint8_t)i; // subtype -- this block's own column index
        bscratch->child_cmd = (int8_t)i;   // seeded equal to subtype -- see file header comment
        block->routine = 2;

        x = (int16_t)(x + 0x20);
    }
}

static void BossBlock_Break(Object *obj) {
    obj->routine += 2; // -> BossBlock_Frag
    obj->width_pixels = 16 / 2;
    obj->y_rad = 16 / 2;

    Object *prev = obj;
    for (int i = 0; i < 4; i++) {
        Object *frag = obj;
        if (i > 0) {
            frag = FindNextFreeObj(prev);
            if (frag == NULL)
                break;
            *frag = *obj; // clone the original block's full state (mappings/tile/render/priority/pos/width/y_rad)
        }
        frag->xsp = bblock_frag_speed[i][0];
        frag->ysp = bblock_frag_speed[i][1];
        frag->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + bblock_frag_pos[i][0]);
        frag->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + bblock_frag_pos[i][1]);
        frag->frame = (uint8_t)(1 + i);
        prev = frag;
    }
    PlaySound(sfx_WallSmash);
}

static void BossBlock_Action(Object *obj, Scratch_BossBlock *scratch) {
    if (scratch->child_cmd == (int8_t)obj->scratch.u8[0]) {
        // Normal/untouched -- solid, standable block.
        SolidObject(obj, 16 + 11 /* sonic_solid_width */, 16, 17, obj->pos.l.x.f.u, NULL, NULL);
        DisplaySprite(obj);
        return;
    }

    if (scratch->child_cmd < 0) {
        // Grabbed -- track the boss's position (44px below it), predicting
        // its Y motion if the boss hasn't been processed yet this frame
        // (i.e. sits later in objects[] than this block).
        Object *parentObj = &objects[scratch->parent_index];
        if (parentObj->col_property == 0) { // obBossHits ran out mid-grab
            BossBlock_Break(obj);
            DisplaySprite(obj);
            return;
        }

        obj->pos.l.x.f.u = parentObj->pos.l.x.f.u;
        obj->pos.l.y.f.u = (int16_t)(parentObj->pos.l.y.f.u + 44);

        if (scratch->parent_index >= (uint8_t)(obj - objects)) {
            // Boss hasn't run yet this frame -- predict where it'll end up.
            obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + (parentObj->ysp >> 8));
        }
        DisplaySprite(obj);
        return;
    }

    // Breaking (child_cmd == 0xA).
    BossBlock_Break(obj);
    DisplaySprite(obj);
}

static void BossBlock_Frag(Object *obj) {
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    ObjectFall(obj);
    DisplaySprite(obj);
}

void Obj_BossBlock(Object *obj) {
    Scratch_BossBlock *scratch = (Scratch_BossBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0: BossBlock_Main(obj); break;
    case 2: BossBlock_Action(obj, scratch); break;
    case 4: BossBlock_Frag(obj); break;
    }
}
