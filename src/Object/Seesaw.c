#include "Seesaw.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/Seesaw.h"
#include "Resource/Mappings/SeesawBall.h"
#include "Sound.h"

// Object 5E - seesaws (SLZ)
//
// Throughout these comments, "ascending" means sloping upwards from left to
// right, and "descending" means going down from left to right.
//
// Each seesaw spawns a spike ball child (unless its subtype marks it as a
// boss-fight seesaw). The ball independently catapults itself between the
// seesaw's two ends based on how the seesaw's own tilt state changes.

static const uint8_t See_DataSlope[48] = {
    0x24, 0x24,                                                             // flat
    0x26, 0x28, 0x2A, 0x2C,                                                 // ascending
    0x2A, 0x28, 0x26, 0x24,                                                 // descending
    0x23, 0x22, 0x21, 0x20, 0x1F, 0x1E, 0x1D, 0x1C, 0x1B, 0x1A, 0x19, 0x18, // descending
    0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0F, 0x0E, 0x0D, 0x0C, // (steep)
    0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03,                  //
    0x02, 0x02, 0x02, 0x02, 0x02,                                          // flat
};

static const uint8_t See_DataFlat[48] = {
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
};

// Relative Y-distances to align the spikeball with the seesaw, indexed by
// (seesaw frame) + (2 if the ball is on the left side).
static const int16_t See_Spikeball_YOffsets[5] = { -8, -28, -47, -28, -8 };

static void See_Spikeball_Action(Object *obj, Scratch_Seesaw *scratch);
static void See_Spikeball_InAir(Object *obj, Scratch_Seesaw *scratch);

// Updates the seesaw's own frame/tilt-state (0=descending,1=flat,2=ascending).
// Also called (indirectly, via see_state_see being written into by the
// spikeball) to sync the seesaw's visuals to state changes the spikeball
// itself drove.
static void See_ChgFrame(Object *obj, Scratch_Seesaw *scratch, int8_t new_state) {
    int8_t frame = (int8_t)obj->frame;
    if (frame == new_state)
        return;
    if (frame < new_state)
        frame = (int8_t)(frame + 2);
    frame = (int8_t)(frame - 1);
    obj->frame = (uint8_t)frame;
    scratch->state = new_state;

    obj->render.f.x_flip = false;
    if (frame & 2)
        obj->render.f.x_flip = true;
}

// Sets the seesaw tilt based on what side of it Sonic is standing on.
static void See_ChkSide(Object *obj, Scratch_Seesaw *scratch) {
    int16_t d0 = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
    int8_t new_state;
    if (d0 >= 0) {
        new_state = 2; // Sonic on the left side -- ascending
    } else {
        d0 = (int16_t)-d0;
        new_state = 0; // Sonic on the right side -- descending
    }
    if ((uint16_t)d0 < 8)
        new_state = 1; // within 8px of center -- flat
    See_ChgFrame(obj, scratch, new_state);
}

static void See_Seesaw_Platform(Object *obj, Scratch_Seesaw *scratch) {
    See_ChgFrame(obj, scratch, scratch->state);

    const uint8_t *heightmap = (obj->frame & 1) ? See_DataFlat : See_DataSlope;

    scratch->landspeed = player->ysp;

    SlopeObject(obj, 96 / 2, heightmap); // may set obj->routine = 4
}

static void See_Seesaw_StoodOn(Object *obj, Scratch_Seesaw *scratch) {
    See_ChkSide(obj, scratch);

    const uint8_t *heightmap = (obj->frame & 1) ? See_DataFlat : See_DataSlope;

    ExitPlatform(obj, 96 / 2, 96 / 2, NULL); // may set obj->routine = 2
    SlopeObject_AssumeStoodOn(obj, 96 / 2, heightmap, obj->pos.l.x.f.u);
}

static void See_Main(Object *obj, Scratch_Seesaw *scratch) {
    obj->routine = 2; // advance to See_Seesaw_Platform
    obj->mappings = Mappings_Seesaw;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Seesaw);
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->width_pixels = 96 / 2;
    scratch->orig_x = obj->pos.l.x.f.u;

    uint8_t self_index = (uint8_t)(obj - objects);

    if (obj->scratch.u8[0] == 0) { // spawn a spikeball? (boss-fight seesaws don't)
        Object *ball = FindNextFreeObj(obj);
        if (ball != NULL) {
            ball->type = ObjId_Seesaw;
            ball->routine = 6; // See_Spikeball_Setup
            ball->pos.l.x.f.u = obj->pos.l.x.f.u;
            ball->pos.l.y.f.u = obj->pos.l.y.f.u;
            ball->status.b = obj->status.b;
            Scratch_Seesaw *bscratch = (Scratch_Seesaw *)&ball->scratch;
            bscratch->parent_index = self_index;
        }
    }

    if (obj->status.o.f.x_flip) // never actually happens anywhere in the game
        obj->frame = 2;

    scratch->state = (int8_t)obj->frame;

    See_Seesaw_Platform(obj, scratch);
}

static void See_Spikeball_Action_Align(Object *obj, Scratch_Seesaw *scratch, Object *parent) {
    int idx = parent->frame;
    int16_t x_off = 40; // align to the right side
    if ((int16_t)(obj->pos.l.x.f.u - scratch->orig_x) < 0) {
        x_off = -40; // align to the left side
        idx += 2;
    }

    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + See_Spikeball_YOffsets[idx]);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + x_off);
    obj->pos.l.y.f.l = 0;
    obj->pos.l.x.f.l = 0;
}

static void See_Spikeball_Setup(Object *obj, Scratch_Seesaw *scratch) {
    obj->routine = 8; // advance to See_Spikeball_Action
    obj->mappings = Mappings_SeesawBall;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Spikeball);
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->col_type = 0x0B | 0x80; // col_16x16 | col_hurt
    obj->width_pixels = 24 / 2;

    scratch->orig_x = obj->pos.l.x.f.u;
    obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + 40);
    scratch->orig_y = obj->pos.l.y.f.u;
    obj->frame = 1; // silver spikeball

    if (obj->status.o.f.x_flip) {
        obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u - 80); // move to the left side instead
        scratch->state = 2; // on right side (i.e. mirrored -- see comment on Scratch_Seesaw::state)
    }

    See_Spikeball_Action(obj, scratch);
}

static void See_Spikeball_Action(Object *obj, Scratch_Seesaw *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_Seesaw *pscratch = (Scratch_Seesaw *)&parent->scratch;

    int8_t d0 = (int8_t)(scratch->state - pscratch->state);
    if (d0 == 0) {
        See_Spikeball_Action_Align(obj, scratch, parent);
        return;
    }
    if (d0 < 0)
        d0 = (int8_t)-d0;

    int16_t vy, vx;
    if (d0 == 1) {
        vy = -0x818; vx = -0x114; // slow catapult (partial state change)
    } else if (pscratch->landspeed < (int16_t)0xA00) {
        vy = -0xAF0; vx = -0xCC; // faster catapult (opposite ends)
    } else {
        vy = -0xE00; vx = -0xA0; // fastest catapult (Sonic landed very fast)
    }

    obj->ysp = vy;
    obj->xsp = vx;
    if ((int16_t)(obj->pos.l.x.f.u - scratch->orig_x) < 0)
        obj->xsp = (int16_t)-obj->xsp;

    obj->routine = 0xA; // See_Spikeball_InAir
    See_Spikeball_InAir(obj, scratch);
}

static void See_Spikeball_InAir_FallingDown(Object *obj, Scratch_Seesaw *scratch) {
    ObjectFall(obj);

    Object *parent = &objects[scratch->parent_index];
    Scratch_Seesaw *pscratch = (Scratch_Seesaw *)&parent->scratch;

    int idx = parent->frame;
    if ((int16_t)(obj->pos.l.x.f.u - scratch->orig_x) < 0)
        idx += 2;

    int16_t landing_y = (int16_t)(scratch->orig_y + See_Spikeball_YOffsets[idx]);
    if (landing_y > obj->pos.l.y.f.u)
        return; // hasn't reached landing height yet

    int8_t new_state = (obj->xsp < 0) ? 2 : 0; // bounce side based on ball's horizontal direction
    pscratch->state = new_state;
    scratch->state = new_state;

    if (new_state != parent->frame) { // is the seesaw NOT already lowered on this side?
        bool was_standing = parent->status.o.f.player_stand;
        parent->status.o.f.player_stand = false;
        if (was_standing) {
            parent->routine = 2; // reset seesaw back to See_Seesaw_Platform
            player->ysp = (int16_t)-obj->ysp; // bounce Sonic based on the ball's landing speed
            player->status.p.f.in_air = true;
            player->status.p.f.object_stand = false;
            ((Scratch_Sonic *)&player->scratch)->jumping = 0;
            player->anim = SonAnimId_Spring;
            player->routine = 2; // Sonic_Control
            QueueSound2(sfx_Spring);
        }
    }

    obj->xsp = 0;
    obj->ysp = 0;
    obj->routine = 8; // See_Spikeball_Action
}

static void See_Spikeball_InAir(Object *obj, Scratch_Seesaw *scratch) {
    if (obj->ysp >= 0) {
        See_Spikeball_InAir_FallingDown(obj, scratch);
        return;
    }

    // Ball is still going up
    ObjectFall(obj);
    int16_t threshold = (int16_t)(scratch->orig_y - 47);
    if (threshold > obj->pos.l.y.f.u)
        return; // more than 47px above the seesaw -- single gravity only
    ObjectFall(obj); // double gravity while within 47px of the seesaw
}

static bool See_OutOfRange(int16_t x) {
    // Real disasm's out_of_range macro call here also has a "bmi" check,
    // but per its own comment that check is redundant with the unsigned
    // distance-threshold check below (a negative distance always wraps to
    // an unsigned value well past the threshold anyway).
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

void Obj_Seesaw(Object *obj) {
    Scratch_Seesaw *scratch = (Scratch_Seesaw *)&obj->scratch;

    switch (obj->routine) {
    case 0: See_Main(obj, scratch); break;
    case 2: See_Seesaw_Platform(obj, scratch); break;
    case 4: See_Seesaw_StoodOn(obj, scratch); break;
    case 6: See_Spikeball_Setup(obj, scratch); break;
    case 8: See_Spikeball_Action(obj, scratch); break;
    case 0xA: See_Spikeball_InAir(obj, scratch); break;
    }

    if (See_OutOfRange(scratch->orig_x))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}
