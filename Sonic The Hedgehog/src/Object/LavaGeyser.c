#include "LavaGeyser.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/LavaGeyser.h"
#include "Resource/Mappings/LavaGeyser.h"
#include "Sound.h"

// Object 4D - lava geyser / lavafall (MZ). Spawned by GeyserMaker (object
// 4C) as either 2 pieces (geyser: a bubbling top tip + a tall solid lava
// wall trailing beneath it) or 3 (lavafall: same two, plus a second
// bubbling tip at the bottom).

static const int16_t geyser_speeds[2] = { -0x500, 0 }; // geyser, lavafall

static void Geyser_Type00(Sst *self, Own_LavaGeyser *own) {
    self->y_vel += 0x18;
    if (own->orig_y >= self->y_pos.f.u)
        return;
    self->routine += 4; // -> Delete
    sst[own->parent_index].anim = 3; // ".bubble3"
}

static void Geyser_Type01(Sst *self, Own_LavaGeyser *own) {
    self->y_vel += 0x18;
    if (own->orig_y >= self->y_pos.f.u)
        return;
    self->routine += 4; // -> Delete
    sst[own->parent_index].anim = 1; // ".bubble1"
}

static void Geyser_Action(Sst *self, Own_LavaGeyser *own) {
    if (self->subtype == 0)
        Geyser_Type00(self, own);
    else
        Geyser_Type01(self, own);

    Sst_MoveSprite2(self);
    Sst_Animate(self, Animation_LavaGeyser);

    SstOnScreenTest2(self, self->x_pos.f.u);
}

// Routine 4 -- the tall solid lava wall trailing beneath the bubbling top
// tip; picks one of 3 lengths of sprite (each with 2 alternating frames)
// based on how far the tip has currently risen above its own origin.
static void Geyser_BigLavaWall(Sst *self, Own_LavaGeyser *own) {
    Sst *tip = &sst[own->parent_index];

    if (tip->routine == 6) { // tip has marked itself for deletion
        Sst_Delete(self);
        return;
    }

    self->y_pos.f.u = (int16_t)(tip->y_pos.f.u + 0x60);

    int16_t risen = (int16_t)(own->orig_y - self->y_pos.f.u);
    uint8_t base_frame = 8; // medium
    if (risen < 0x40)
        base_frame = 0xB; // short
    if (risen > 0x80)
        base_frame = 0xE; // long

    if (--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 8 - 1;
        self->anim_frame++;
        if (self->anim_frame >= 2)
            self->anim_frame = 0;
    }
    self->mapping_frame = (uint8_t)(self->anim_frame + base_frame);

    SstOnScreenTest2(self, self->x_pos.f.u);
}

static void Geyser_Main(Sst *self, Own_LavaGeyser *own) {
    self->routine += 2; // -> Action

    own->orig_y = self->y_pos.f.u;
    uint8_t subtype = self->subtype;
    if (subtype != 0)
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u - 0x250);

    self->y_vel = geyser_speeds[subtype & 1];

    // Spawns the bubbling top tip (reusing this very object) and the lava
    // wall beneath it. If object RAM is full for the wall, real hardware
    // would corrupt the tip itself by applying the wall's own adjustments
    // to it instead (a1 retaining the tip's own address on failure) --
    // not reproduced here, matching this session's earlier precedent
    // (see GlassBlock.c's own comment on the same kind of edge case).
    Sst *middle = NULL;
    for (int i = 0; i < 2; i++) {
        Sst *seg = self;
        if (i > 0) {
            seg = Sst_AllocateAfter(self);
            if (seg == NULL)
                break;
            middle = seg;
        }
        seg->code = Obj_LavaGeyser;
        seg->mappings = Mappings_LavaGeyser;
        seg->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_MZ_Lava); // | Tile_Pal4
        seg->render_flags = SST_RENDER_LEVEL;
        seg->width_pixels = 112 / 2;
        seg->x_pos.f.u = self->x_pos.f.u;
        seg->y_pos.f.u = self->y_pos.f.u;
        seg->subtype = subtype;
        seg->priority = 1 * 0x80;
        seg->anim = (subtype == 0) ? 5 : 2; // ".bubble4" / ".end"
    }
    if (middle == NULL)
        return;

    middle->y_pos.f.u = (int16_t)(middle->y_pos.f.u + 0x60);
    Own_LavaGeyser *mscratch = (Own_LavaGeyser *)middle->own.u8;
    mscratch->orig_y = (int16_t)(own->orig_y + 0x60);
    middle->collision_flags = 0x93; // col_64x224 | col_hurt
    // 256/2 doesn't fit a signed byte -- real hardware's own obHeight
    // field is sign-extended the same way when read (ext.w), so this
    // wraps to -128 there too; kept bit-for-bit identical rather than
    // "fixed", since it's unclear whether that's visually load-bearing.
    middle->y_radius = (int8_t)(256 / 2);
    middle->height_pixels = middle->y_radius;
    middle->routine = 4; // BigLavaWall
    mscratch->parent_index = (uint8_t)(self - sst);

    if (subtype != 0) {
        Sst *bottom = Sst_AllocateAfter(middle);
        if (bottom != NULL) {
            bottom->code = Obj_LavaGeyser;
            bottom->mappings = Mappings_LavaGeyser;
            bottom->art_tile = TILE_MAP(0, 3, 0, 0, (ArtTile_MZ_Lava+0x10)); // | Tile_Pal4
            bottom->render_flags = SST_RENDER_LEVEL;
            bottom->width_pixels = 112 / 2;
            bottom->x_pos.f.u = self->x_pos.f.u;
            bottom->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x100);
            bottom->subtype = subtype;
            bottom->priority = 0 * 0x80;
            bottom->anim = 2; // ".end"
            bottom->routine = 2; // Action

            Own_LavaGeyser *bscratch = (Own_LavaGeyser *)bottom->own.u8;
            bscratch->orig_y = own->orig_y;
            bscratch->parent_index = own->parent_index;
        }
        self->subtype = 0; // force the top tip to geyser-type behavior henceforth
    }

    QueueSound2(sfx_Burning);
}

void Obj_LavaGeyser(Sst *self) {
    Own_LavaGeyser *own = (Own_LavaGeyser *)self->own.u8;

    switch (self->routine) {
    case 0:
        Geyser_Main(self, own);
        __attribute__((fallthrough));
    case 2:
        Geyser_Action(self, own);
        break;
    case 4:
        Geyser_BigLavaWall(self, own);
        break;
    case 6:
        Sst_Delete(self);
        break;
    }
}
