#include "SpikeBall.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Resource/Mappings/SpikeBallLZ.h"
#include "Resource/Mappings/SpikeBallSYZ.h"

// Object 57 - spiked balls twirling on a chain (SYZ, LZ). One parent object
// (the twirl centre) spawns up to 7 child link/ball objects; all of them
// (children plus the parent itself, stored as the last entry) get
// repositioned every frame along a circle by Sball_Twirl, using each
// object's own radius to place it along the chain.

static void Sball_Twirl(Object *obj, Scratch_SpikeBall *scratch) {
    // Real hardware does `add.w speed,obAngle(a0)` -- a 16-bit add into a
    // word straddling obAngle and the following (otherwise-unused) byte,
    // with only the high byte (obj->angle) ever read back for CalcSine.
    // That makes the effective turn rate speed/256 per frame rather than
    // the raw speed value.
    uint16_t accum = (uint16_t)((obj->angle << 8) | scratch->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)scratch->speed);
    obj->angle = (uint8_t)(accum >> 8);
    scratch->angle_frac = (uint8_t)accum;

    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);

    for (int i = 0; i <= scratch->children; i++) {
        Object *link = &objects[scratch->child_idx[i]];
        Scratch_SpikeBall *link_scratch = (Scratch_SpikeBall *)&link->scratch;

        int16_t radius = link_scratch->radius;
        link->pos.l.y.f.u = (int16_t)(scratch->orig_y + (((int32_t)radius * sin) >> 8));
        link->pos.l.x.f.u = (int16_t)(scratch->orig_x + (((int32_t)radius * cos) >> 8));
    }
}

static void SBall_ChkDel(Object *obj, Scratch_SpikeBall *scratch) {
    if (IS_OFFSCREEN(scratch->orig_x)) {
        for (int i = 0; i <= scratch->children; i++) {
            Object *link = &objects[scratch->child_idx[i]];
            if (link != obj)
                ObjectDelete(link);
        }
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

static void SBall_Main(Object *obj, Scratch_SpikeBall *scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_SpikeBallSYZ;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SYZ_Spikeball_Chain);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->width_pixels = 16 / 2;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;

    obj->col_type = 0x18 | 0x80; // col_8x8 | col_hurt -- SYZ: chain hurts Sonic
    bool is_lz = LEVEL_ZONE(level_id) == ZoneId_LZ;
    if (is_lz) {
        obj->col_type = 0; // col_none -- LZ: chain doesn't hurt Sonic
        obj->mappings = Mappings_SpikeBallLZ;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Spikeball_Chain);
    }

    uint8_t subtype = obj->scratch.u8[0];
    int8_t speed = (int8_t)(subtype & 0xF0);
    scratch->speed = (int16_t)(speed << 3);

    obj->angle = (uint8_t)(((obj->status.o.f.x_flip ? 1 : 0) << 6) | ((obj->status.o.f.y_flip ? 1 : 0) << 7));

    int count = subtype & 7;
    int radius = count << 4;
    scratch->children = 0;
    scratch->radius = (uint8_t)radius;

    int loop_count = count - 1;
    if (loop_count >= 0 && (subtype & 8))
        loop_count--;

    for (int i = 0; loop_count >= 0 && i <= loop_count; i++) {
        Object *link = FindNextFreeObj(obj);
        if (link == NULL)
            break;

        scratch->children++;
        scratch->child_idx[scratch->children - 1] = (uint8_t)(link - objects);

        link->routine = 4; // SBall_Child
        link->type = obj->type;
        link->mappings = obj->mappings;
        link->tile = obj->tile;
        link->render = obj->render;
        link->priority = obj->priority;
        link->width_pixels = obj->width_pixels;
        link->col_type = obj->col_type;

        radius -= 0x10;
        Scratch_SpikeBall *link_scratch = (Scratch_SpikeBall *)&link->scratch;
        link_scratch->radius = (uint8_t)radius;

        if (is_lz && radius == 0)
            link->frame = 2; // LZ stem attached to floor
    }

    scratch->child_idx[scratch->children] = (uint8_t)(obj - objects);

    if (is_lz) {
        obj->col_type = 0x0B | 0x80; // col_16x16 | col_hurt -- tip of chain is harmful
        obj->frame = 1; // larger spikeball frame for tip
    }
}

void Obj_SpikeBall(Object *obj) {
    Scratch_SpikeBall *scratch = (Scratch_SpikeBall *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        SBall_Main(obj, scratch);
        __attribute__((fallthrough));
    case 2:
        Sball_Twirl(obj, scratch);
        SBall_ChkDel(obj, scratch);
        break;
    case 4: // SBall_Child -- rotation/deletion handled by parent, just display
        DisplaySprite(obj);
        break;
    }
}
