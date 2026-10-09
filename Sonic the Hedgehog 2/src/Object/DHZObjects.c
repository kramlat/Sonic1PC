// Dust Hill's objects for Sonic 2 (the Simon Wai prototype's 2A, 47, 6A, 75, 76 and 77; the collapsing platform, 1F, is in CollapsingPlatform.c): the stomper that rises slowly and drops (2A), the switch that is stood
// on (47, shared with Oil Ocean and Metropolis), the boxes that move round a path (6A), the spiked ball on a chain that circles (75), the platform with spikes on its sides that slides away from whoever
// comes near (76) and the drawbridge that a switch lowers (77). All are solid for Sonic and Tails.
#include "Object/DHZObjects.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/DHZGate.h"
#include "Resource/Mappings/DHZStomper.h"
#include "Resource/Mappings/PlatformSpikes.h"
#include "Resource/Mappings/RotatingBoxes.h"
#include "Resource/Mappings/SpikeballChain.h"
#include "Resource/Mappings/Switch.h"

// (the spikes' hurt, Touch_ChkHurt2: Spikes.c)
void Spikes_HurtCharacter(Object *obj, Object *chr, int who);

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// MarkObjGone2: gone, and forgotten (so that it can come back), when x is out of sight
static void GoneIfOffscreen(Object *obj, int16_t x) {
    if (IS_OFFSCREEN(x)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2A: the stomper. It rises $60 pixels a frame at a time and then drops 8 a frame, over and over
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F
    int16_t lift;      // 0x30: how far it is raised
    int16_t base_y;    // 0x32
} Scratch_Stomper;

void Obj_DHZStomper(Object *obj) {
    Scratch_Stomper *scratch = (Scratch_Stomper *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_DHZStomper;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0);
        obj->render.b |= SPRITE_CAM_FIELD; // ori.b #4,1(a0): the layout's facing stays
        obj->width_pixels = 0x10;
        obj->priority = 4;
        scratch->base_y = obj->pos.l.y.f.u;
        obj->y_rad = 0x50;
        obj->render.f.explicit_height = true;
    }

    if (obj->routine_sec == 0) { // rising
        scratch->lift++;
        if (scratch->lift == 0x60)
            obj->routine_sec = 2;
    } else { // dropping
        scratch->lift -= 8;
        if (scratch->lift <= 0) {
            scratch->lift = 0;
            obj->routine_sec = 0;
        }
    }
    obj->pos.l.y.f.u = (int16_t)(scratch->base_y - scratch->lift);

    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 0x40, 0x41, obj->pos.l.x.f.u, NULL);
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 47: the switch. A character standing on it sets a bit in the level's table of switches (the subtype's low nibble says which, bit 6 of it the bit: 0 or 7)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_Switch(Object *obj) {
    const uint8_t subtype = obj->scratch.u8[0];

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_Switch;
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x424);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        obj->pos.l.y.f.u += 4;
    }

    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, 0x1B, 4, 5, obj->pos.l.x.f.u, NULL);
        obj->frame = 0;
        uint8_t *flag = &f_switch[subtype & 0xF];
        const uint8_t bit = (subtype & 0x40) ? 0x80 : 0x01;
        if (!(obj->status.b & 0x18)) {
            *flag &= (uint8_t)~bit;
        } else {
            if (*flag == 0)
                PlaySound(sfx_Switch);
            *flag |= bit;
            obj->frame = 1;
        }
    }
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6A: the boxes. They move along a path of steps (a speed in each direction and for how long), solid, forever; the subtype is where in the path they start, and $18 makes
// two more (at $40 right and left of it and $40 below, starting a step ahead of it and a step behind) with it
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    int16_t base_x;    // 0x32: where it is, for when it is forgotten
    int16_t timer;     // 0x34: frames left of the step
    uint8_t pad1[2];   // 0x36-0x37
    uint8_t step;      // 0x38: the index of the next step in the path
} Scratch_Boxes;

// The path (loc_1BC74; loc_1BC92 for a box that is flipped, which goes the other way): x speed, y speed and frames of each step. Its fifth step is not in the cycle: it is only where a box that starts
// with subtype $18 begins
static const int16_t boxes_path[5][3] = {
    { 0x0000, 0x0100, 0x0040 }, { -0x0100, 0x0000, 0x0080 }, { 0x0000, -0x0100, 0x0040 }, { 0x0100, 0x0000, 0x0080 }, { 0x0100, 0x0000, 0x0040 },
};
static const int16_t boxes_path_flipped[5][3] = {
    { 0x0000, 0x0100, 0x0040 }, { 0x0100, 0x0000, 0x0080 }, { 0x0000, -0x0100, 0x0040 }, { -0x0100, 0x0000, 0x0080 }, { -0x0100, 0x0000, 0x0040 },
};

// loc_1BC22: the next step
static void Boxes_NextStep(Object *obj, Scratch_Boxes *scratch) {
    int index = scratch->step / 6;
    if (index > 4)
        index = 0; // (the prototype reads past its path here; no layout starts a box so)
    const int16_t *step = obj->status.o.f.x_flip ? boxes_path_flipped[index] : boxes_path[index];
    obj->xsp = step[0];
    obj->ysp = step[1];
    scratch->timer = step[2];
    scratch->step += 6;
    if (scratch->step >= 0x18)
        scratch->step = 0;
}

void Obj_RotatingBoxes(Object *obj) {
    Scratch_Boxes *scratch = (Scratch_Boxes *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2; // (Metropolis's boxes, which wait for a character to step on them, are routine 2 and come with that zone)
        obj->routine += 2;
        obj->mappings = Mappings_RotatingBoxes;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x3D4);
        obj->render.b |= SPRITE_CAM_FIELD; // ori.b #4,1(a0): the layout's facing stays
        obj->priority = 4;
        obj->width_pixels = 0x20;
        obj->y_rad = 0x20;
        obj->frame = 0;
        if (scratch->subtype == 0x18) {
            const bool flipped = obj->status.o.f.x_flip;
            for (int i = 0; i < 2; i++) {
                Object *box = FindNextFreeObj(obj + 1);
                if (box == NULL)
                    break;
                box->type = obj->type;
                box->pos.l.x.f.u = obj->pos.l.x.f.u + (i == 0 ? 0x40 : -0x40);
                box->pos.l.y.f.u = obj->pos.l.y.f.u + 0x40;
                Scratch_Boxes *other = (Scratch_Boxes *)&box->scratch;
                other->base_x = obj->pos.l.x.f.u;
                other->base_y = obj->pos.l.y.f.u;
                box->status.b = obj->status.b;
                other->subtype = (uint8_t)((i == 0) != flipped ? 6 : 0xC);
            }
            scratch->base_x = obj->pos.l.x.f.u;
            scratch->base_y = obj->pos.l.y.f.u;
        }
        scratch->step = scratch->subtype;
        Boxes_NextStep(obj, scratch);
        return; // (the first frame only sets it up: loc_1BC22 ends the routine)
    }

    const int16_t old_x = obj->pos.l.x.f.u;
    SpeedToPos(obj);
    if (--scratch->timer == 0)
        Boxes_NextStep(obj, scratch);

    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), old_x, NULL);
    }
    GoneIfOffscreen(obj, scratch->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 75: the spiked ball on a chain. It circles its pivot at the speed in the high nibble of the subtype (times 8, either way) through as many links as the low nibble says
// (frame 1 each, drawn as child sprites of an object of its own); a subtype whose low nibble is $F is a solid block (frame 2) instead. The object's flipping gives the angle it starts at
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t pivot_x;   // 0x30
    int16_t pivot_y;   // 0x32
    int16_t speed;     // 0x34
    uint16_t angle;    // 0x36: the high byte is the angle
    uint8_t pad1[4];   // 0x38-0x3B
    uint8_t chain;     // 0x3C: the slot of the object that draws the chain
} Scratch_SpikeballChain;

enum { ChainRoutine_Init = 0, ChainRoutine_Circle = 2, ChainRoutine_Block = 4 };

static Object *SpikeballChain_Links(Object *obj, Scratch_SpikeballChain *scratch) {
    Object *chain = (scratch->chain != 0) ? &objects[scratch->chain] : NULL;
    if (chain == NULL || chain == obj || chain->type != obj->type || !chain->render.f.multi_sprite)
        return NULL;
    return chain;
}

void Obj_SpikeballChain(Object *obj) {
    Scratch_SpikeballChain *scratch = (Scratch_SpikeballChain *)&obj->scratch;

    if (obj->render.f.multi_sprite) { // the object that draws the chain: its links are set by the ball's
        DisplaySprite(obj);
        return;
    }

    switch (obj->routine) {
    case ChainRoutine_Init: {
        obj->routine += 2;
        obj->mappings = Mappings_SpikeballChain;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 5;
        obj->width_pixels = 0x10;
        scratch->pivot_x = obj->pos.l.x.f.u;
        scratch->pivot_y = obj->pos.l.y.f.u;
        const int links = scratch->subtype & 0xF;
        scratch->speed = (int16_t)(((int16_t)(int8_t)(scratch->subtype & 0xF0)) * 8);
        scratch->angle = (uint16_t)(((obj->status.b << 6) | (obj->status.b >> 2)) & 0xC0) << 8; // (ror.b #2, and $C0: the flip bits)
        scratch->chain = 0;
        if (links == 0xF) { // a solid block
            obj->routine += 2;
            obj->priority = 4;
            obj->frame = 2;
            return;
        }
        obj->col_type = 0x9A;
        Object *chain = FindNextFreeObj(obj + 1);
        if (chain != NULL) {
            chain->type = obj->type;
            chain->mappings = obj->mappings;
            chain->tile = obj->tile;
            chain->render.b = 0;
            chain->render.f.level_fg = true;
            chain->render.f.multi_sprite = true;
            chain->render.f.explicit_height = true;
            chain->width_pixels = 0x40;
            chain->y_rad = 0x40;
            chain->priority = 5;
            chain->frame = 0;
            chain->child_count = (uint8_t)links;
            for (int i = 0; i < links && i < OBJECT_CHILDREN; i++) {
                chain->children[i].x = obj->pos.l.x.f.u;
                chain->children[i].y = obj->pos.l.y.f.u;
                chain->children[i].frame = 1;
            }
            chain->pos.l.x.f.u = obj->pos.l.x.f.u;
            chain->pos.l.y.f.u = obj->pos.l.y.f.u;
            scratch->chain = (uint8_t)(chain - objects);
        }
    }
        // Fallthrough
    case ChainRoutine_Circle: {
        scratch->angle = (uint16_t)(scratch->angle + scratch->speed);
        int16_t sin, cos;
        CalcSine((uint8_t)(scratch->angle >> 8), &sin, &cos);
        Object *chain = SpikeballChain_Links(obj, scratch);
        int links = chain ? chain->child_count : 0;
        // (a link a step of 16 pixels along the angle from the pivot, as 16.16 sums that are cut to whole pixels each)
        const int32_t step_y = (int32_t)(int16_t)(sin * 16) << 8, step_x = (int32_t)(int16_t)(cos * 16) << 8;
        int32_t acc_y = 0, acc_x = 0;
        for (int i = 0; i < links && i < OBJECT_CHILDREN; i++) {
            chain->children[i].x = (int16_t)((acc_x >> 16) + scratch->pivot_x);
            chain->children[i].y = (int16_t)((acc_y >> 16) + scratch->pivot_y);
            acc_x += step_x;
            acc_y += step_y;
        }
        if (chain != NULL) {
            obj->pos.l.x.f.u = (int16_t)((acc_x >> 16) + scratch->pivot_x); // (the ball is where the next link would be)
            obj->pos.l.y.f.u = (int16_t)((acc_y >> 16) + scratch->pivot_y);
            chain->pos.l.x.f.u = chain->children[4].x; // (the chain is where its fifth link is: that is what culls it)
            chain->pos.l.y.f.u = chain->children[4].y;
        }
        if (camera_split || !IS_OFFSCREEN(scratch->pivot_x)) {
            DisplaySprite(obj);
            return;
        }
        if (chain != NULL)
            ObjectDelete(chain);
        ObjectDelete(obj);
        break;
    }
    case ChainRoutine_Block:
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 0x10, 0x11, obj->pos.l.x.f.u, NULL);
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            if (obj->respawn_index)
                objstate[obj->respawn_index] &= 0x7F;
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 76: the platform with spikes at its sides. It waits until a character who is not in the air comes within reach at its level, then slides $80 pixels (to the left; to the right if
// flipped). Anyone who pushes against its sides is hurt
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t state;     // 0x28: 0 waiting, 2 sliding
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t pad1;      // 0x30
    int16_t pad2;      // 0x32
    int16_t base_x;    // 0x34
    int16_t slide;     // 0x36: frames of sliding left
} Scratch_PlatformSpikes;

void Obj_PlatformSpikes(Object *obj) {
    Scratch_PlatformSpikes *scratch = (Scratch_PlatformSpikes *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_PlatformSpikes;
        obj->tile = TILE_MAP(0, 0, 0, 0, 0);
        obj->render.b |= SPRITE_CAM_FIELD; // ori.b #4,1(a0): the layout's facing stays
        obj->priority = 4;
        // The size by the subtype's high nibble (loc_1D08A: only the first of its four bytes' rows is data in the prototype): $40 wide, $10 high, frame 0
        obj->width_pixels = 0x40;
        obj->y_rad = 0x10;
        obj->frame = 0;
        scratch->base_x = obj->pos.l.x.f.u;
        scratch->state &= 0xF;
    }

    const int16_t old_x = obj->pos.l.x.f.u;
    if (scratch->state == 0) {
        FOR_EACH_CHARACTER(chr, who) {
            if (chr->status.p.f.in_air)
                continue;
            int16_t d0 = (int16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u + 0xC0);
            if (obj->status.o.f.x_flip)
                d0 -= 0x100;
            if ((uint16_t)d0 >= 0x80)
                continue;
            if ((uint16_t)(chr->pos.l.y.f.u - obj->pos.l.y.f.u + 0x10) >= 0x20)
                continue;
            scratch->state = 2;
            scratch->slide = 0x80;
        }
    } else if (scratch->slide != 0) {
        scratch->slide--;
        obj->pos.l.x.f.u += obj->status.o.f.x_flip ? 1 : -1;
    }

    if (obj->render.f.on_screen) {
        FOR_EACH_CHARACTER(chr, who) {
            int32_t side = Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), old_x, NULL);
            if (side == 1) {
                Spikes_HurtCharacter(obj, chr, who);
                obj->status.b &= (uint8_t)~(1 << (5 + who));
            }
        }
    }
    GoneIfOffscreen(obj, scratch->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 77: the drawbridge. The first time its switch (the subtype) is down it lowers, or raises (flat it is a platform; up it is not, and lets go of whoever stands on it)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the switch
    uint8_t pad0[0xB]; // 0x29-0x33
    uint8_t moved;     // 0x34: it has been triggered (once)
} Scratch_Gate;

static const uint8_t gate_animations[] = {
    0x00, 0x04, 0x00, 0x0A,
    0x03, 0x02, 0x01, 0x00, 0xFE, 0x01, // (to flat)
    0x03, 0x00, 0x01, 0x02, 0xFE, 0x01, // (to raised)
};

void Obj_DHZGate(Object *obj) {
    Scratch_Gate *scratch = (Scratch_Gate *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_DHZGate;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x43C);
        obj->render.b |= SPRITE_CAM_FIELD; // ori.b #4,1(a0): the layout's facing stays
        obj->width_pixels = 0x80;
        obj->priority = 0;
    }

    if (!scratch->moved && (f_switch[scratch->subtype & 0xF] & 1)) {
        scratch->moved = 1;
        obj->anim ^= 1;
        if (obj->render.f.on_screen)
            PlaySound(sfx_Door);
    }
    AnimateSprite(obj, gate_animations);

    if (obj->frame == 0) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, 0x4B, 8, 9, obj->pos.l.x.f.u, NULL);
    } else {
        FOR_EACH_CHARACTER(chr, who) {
            if (obj->status.b & (1 << (3 + who)))
                chr->status.p.f.object_stand = false;
        }
        obj->status.b &= (uint8_t)~0x18;
    }
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}
