#include "Orbinaut.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Resource/Animation/Orbinaut.h"
#include "Resource/Mappings/Orbinaut.h"

// Object 60 - Orbinaut enemy (LZ, SLZ, SBZ). Spawns 4 spiked balls that
// orbit it; in LZ it waits until Sonic gets close, then goes "angry" and
// fires the balls off one at a time (whichever is currently at the bottom
// of the orbit) before finally moving; in SLZ/SBZ it just moves and fires
// immediately, skipping the distance check.

static void Orb_DeleteWithSpikeballs(Object *obj, Scratch_Orbinaut *scratch) {
    if (obj->respawn_index)
        objstate[obj->respawn_index] &= 0x7F;

    for (int i = 0; i < scratch->ammo; i++)
        ObjectDelete(&objects[scratch->ball_idx[i]]);
    ObjectDelete(obj);
}

static void Orb_DisplayNoMove(Object *obj, Scratch_Orbinaut *scratch) {
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        Orb_DeleteWithSpikeballs(obj, scratch);
        return;
    }
    DisplaySprite(obj);
}

static void Orb_DisplayAndMove(Object *obj, Scratch_Orbinaut *scratch) {
    SpeedToPos(obj);
    Orb_DisplayNoMove(obj, scratch);
}

static void Orb_CheckSonic(Object *obj, Scratch_Orbinaut *scratch) {
    int16_t dx = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (dx < 0)
        dx = (int16_t)-dx;
    if (dx < 160) {
        int16_t dy = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u);
        if (dy < 0)
            dy = (int16_t)-dy;
        if (dy < 80 && !debug_use)
            obj->anim = 1; // angry -- triggers launch in Orb_CircleSpikeball
    }

    AnimateSprite(obj, Animation_Orbinaut);
    Orb_DisplayNoMove(obj, scratch);
}

static void Orb_CircleSpikeball(Object *obj, Scratch_Orbinaut *scratch) {
    Object *parent = &objects[scratch->parent_index];

    if (parent->type != ObjId_Orbinaut) {
        ObjectDelete(obj);
        return;
    }

    if (parent->frame == 2 && obj->angle == 0x40) {
        // Directly under the Orbinaut while it's angry -- fire
        obj->routine += 2; // -> Orb_FiredSpikeball
        Scratch_Orbinaut *pscratch = (Scratch_Orbinaut *)&parent->scratch;
        if (--pscratch->ammo == 0)
            parent->routine += 2; // out of ammo -- let the Orbinaut start moving

        obj->xsp = -0x200;
        if (parent->status.o.f.x_flip)
            obj->xsp = (int16_t)-obj->xsp;
        DisplaySprite(obj);
        return;
    }

    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    obj->pos.l.x.f.u = (int16_t)((cos >> 4) + parent->pos.l.x.f.u);
    obj->pos.l.y.f.u = (int16_t)((sin >> 4) + parent->pos.l.y.f.u);

    Scratch_Orbinaut *pscratch = (Scratch_Orbinaut *)&parent->scratch;
    obj->angle = (uint8_t)(obj->angle + pscratch->circledir);
    DisplaySprite(obj);
}

static void Orb_FiredSpikeball(Object *obj) {
    SpeedToPos(obj);
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

static void Orb_Main(Object *obj, Scratch_Orbinaut *scratch) {
    obj->mappings = Mappings_Orbinaut;

    ZoneId zone = LEVEL_ZONE(level_id);
    if (zone == ZoneId_SBZ)
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x429); // ArtTile_SBZ_Orbinaut
    else
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x429); // ArtTile_SLZ_Orbinaut | Tile_Pal2
    if (zone == ZoneId_LZ)
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x467); // ArtTile_LZ_Orbinaut

    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->col_type = 0x0B; // col_16x16 | col_badnik
    obj->width_pixels = 24 / 2;

    uint8_t count = 0;
    uint8_t angle = 0;
    for (int i = 0; i < 4; i++) {
        Object *ball = FindNextFreeObj(obj);
        if (ball == NULL)
            break;

        scratch->ball_idx[count] = (uint8_t)(ball - objects);
        count++;

        ball->type = obj->type;
        ball->routine = 6; // Orb_CircleSpikeball
        ball->mappings = obj->mappings;
        ball->tile = obj->tile;
        ball->render.f.align_fg = true;
        ball->priority = 4;
        ball->width_pixels = 16 / 2;
        ball->frame = 3; // spikeball frame
        ball->col_type = 0x18 | 0x80; // col_8x8 | col_hurt
        ball->angle = angle;
        angle = (uint8_t)(angle + 0x40);

        Scratch_Orbinaut *bscratch = (Scratch_Orbinaut *)&ball->scratch;
        bscratch->parent_index = (uint8_t)(obj - objects);
    }
    scratch->ammo = count;

    scratch->circledir = 1;
    if (obj->status.o.f.x_flip)
        scratch->circledir = -1;

    obj->routine = (uint8_t)(obj->scratch.u8[0] + 2); // subtype (0 = LZ, 2 = SLZ/SBZ) as base routine

    obj->xsp = -0x40;
    if (obj->status.o.f.x_flip)
        obj->xsp = (int16_t)-obj->xsp;
}

void Obj_Orbinaut(Object *obj) {
    Scratch_Orbinaut *scratch = (Scratch_Orbinaut *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        Orb_Main(obj, scratch);
        break;
    case 2:
        Orb_CheckSonic(obj, scratch);
        break;
    case 4:
        Orb_DisplayAndMove(obj, scratch);
        break;
    case 6:
        Orb_CircleSpikeball(obj, scratch);
        break;
    case 8:
        Orb_FiredSpikeball(obj);
        break;
    }
}
