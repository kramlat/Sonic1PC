// Neo Green Hill's badniks for Sonic 2, as the alpha has them: see NGHZBadniks.h. Each sets itself up from the alpha's table of looks (Object_Settings), whose entries are written out below.
#include "Object/NGHZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

#include "Macros.h"

#include "Resource/Animation/ChopChop.h"
#include "Resource/Animation/GrounderEmerge.h"
#include "Resource/Animation/GrounderWalk.h"
#include "Resource/Animation/Whisp.h"
#include "Resource/Mappings/ChopChop.h"
#include "Resource/Mappings/Grounder.h"
#include "Resource/Mappings/GrounderRock.h"
#include "Resource/Mappings/GrounderWall.h"
#include "Resource/Mappings/Whisp.h"

#define ObjId_Grounder 0x8E
#define ObjId_GrounderWall 0x8F
#define ObjId_GrounderRock 0x90

// The Animation scripts of the alpha: a word (the offset of the script, 2), the delay and then frames up to $FF (again) or $FC (moves the routine on, as the final's, and steps past the end of the script)
static void AlphaAnimate(Object *obj, const uint8_t *table) {
    const uint8_t *script = table + 2;
    obj->render.f.x_flip = obj->status.o.f.x_flip; // (the flips of the status are those of the render)
    if (--obj->frame_time.b >= 0)
        return;
    obj->frame_time.b = (int8_t)script[0];
    uint8_t frame = script[1 + obj->anim_frame];
    if (frame == 0xFF) {
        obj->anim_frame = 0;
        frame = script[1];
    } else if (frame == 0xFC) {
        obj->routine += 2;
        obj->frame_time.b = 0;
        obj->anim_frame++;
        return;
    }
    obj->frame = frame & 0x7F;
    obj->anim_frame++;
}

// Where Sonic is from the object (Object_Check_Player_Position): the side (0 left, 2 right) and above/below (0 above, 2 below), and how far (object minus Sonic)
typedef struct {
    int side, below;
    int16_t dx, dy;
} Where;

static Where WhereIsSonic(const Object *obj) {
    Where w;
    w.dx = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
    w.dy = (int16_t)(obj->pos.l.y.f.u - player->pos.l.y.f.u);
    w.side = (uint16_t)obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u ? 2 : 0;
    w.below = (uint16_t)obj->pos.l.y.f.u < (uint16_t)player->pos.l.y.f.u ? 2 : 0;
    return w;
}

static void SetFlip(Object *obj, bool flip) {
    obj->status.o.f.x_flip = flip;
    obj->render.f.x_flip = flip;
}

static void Settings(Object *obj, const void *mappings, uint16_t tile, uint8_t render, uint8_t priority, uint8_t width, uint8_t col) {
    obj->mappings = mappings;
    obj->tile = tile;
    obj->render.b |= render | SPRITE_CAM_FIELD;
    obj->priority = priority;
    obj->width_pixels = width;
    obj->col_type = col;
    obj->routine += 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Whisp (8C): a fly that flutters at Sonic. After a short wait it goes up (it does that up to 7 times: the count is at $2B, the wait at $2A), follows Sonic for $60 frames with a speed that is kept to $200 either
// way and then waits again; the eighth time it flies off to the left
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint8_t timer; // 0x2A
    uint8_t count; // 0x2B
} Scratch_Whisp;

enum { WhispRoutine_Init = 0, WhispRoutine_Wait = 2, WhispRoutine_Chase = 4, WhispRoutine_Leave = 6 };

void Obj_Whisp(Object *obj) {
    Scratch_Whisp *scratch = (Scratch_Whisp *)&obj->scratch;

    switch (obj->routine) {
    case WhispRoutine_Init:
        Settings(obj, Mappings_Whisp, TILE_MAP(0, 1, 0, 0, 0x500), 4, 4, 0x0C, 0x0B);
        scratch->timer = 0x10;
        scratch->count = 6;
        return;

    case WhispRoutine_Wait:
        if ((int8_t)--scratch->timer < 0) {
            if ((int8_t)--scratch->count >= 0) { // up it goes
                obj->routine += 2;
                obj->ysp = (int16_t)0xFF00;
                scratch->timer = 0x60;
            } else { // the last: away to the left
                obj->routine = WhispRoutine_Leave;
                obj->status.o.f.x_flip = false;
                obj->xsp = (int16_t)0xFE00;
                obj->ysp = 0;
                SpeedToPos(obj);
                AlphaAnimate(obj, Animation_Whisp);
                RememberState(obj);
                return;
            }
            // (it goes on with the chase's code in the same frame)
        } else {
            AlphaAnimate(obj, Animation_Whisp);
            RememberState(obj);
            return;
        }
        // fallthrough
    case WhispRoutine_Chase: {
        if ((int8_t)--scratch->timer < 0) { // done: stops, waits $10 and starts again
            obj->routine -= 2;
            scratch->timer = 0x10;
            obj->xsp = 0;
            obj->ysp = 0;
            AlphaAnimate(obj, Animation_Whisp);
            RememberState(obj);
            return;
        }
        const Where w = WhereIsSonic(obj);
        obj->status.o.f.x_flip = w.side != 0;
        obj->xsp += w.side ? 0x10 : -0x10;
        obj->ysp += w.below ? 0x10 : -0x10;
        if (obj->xsp > 0x200)
            obj->xsp = 0x200;
        else if (obj->xsp < -0x200)
            obj->xsp = -0x200;
        if (obj->ysp > 0x200)
            obj->ysp = 0x200;
        else if (obj->ysp < -0x200)
            obj->ysp = -0x200;
        SpeedToPos(obj);
        AlphaAnimate(obj, Animation_Whisp);
        RememberState(obj);
        return;
    }

    case WhispRoutine_Leave:
        SpeedToPos(obj);
        AlphaAnimate(obj, Animation_Whisp);
        RememberState(obj);
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Grounder (8D in its wall of bricks, 8E in the open): it sits, and when Sonic comes within $60 it rises (the bricks, 8F, fly apart and the rocks, 90, spring from it), then walks to Sonic's side along the
// ground until the floor ends, waits a second and turns round
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint8_t timer;  // 0x2A
    uint8_t active; // 0x2B: set when it has been woken (the bricks and the rocks look at it)
} Scratch_Grounder;

enum { GrounderRoutine_Init = 0, GrounderRoutine_Sit = 2, GrounderRoutine_Emerge = 4, GrounderRoutine_Face = 6, GrounderRoutine_Walk = 8, GrounderRoutine_Wait = 0xA };

typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint8_t parent; // 0x2A: the slot of the Grounder (a word in the alpha: its address)
    uint8_t pad2;
    uint16_t index; // 0x2C
} Scratch_GrounderPart;

// Offset_0x02823C: the rocks (5)
static void Grounder_Rocks(Object *obj) {
    uint16_t index = 0;
    for (int i = 0; i < 5; i++) {
        Object *rock = FindFreeObj();
        if (rock == NULL)
            return;
        rock->type = ObjId_GrounderRock;
        Scratch_GrounderPart *part = (Scratch_GrounderPart *)&rock->scratch;
        part->subtype = 6;
        part->parent = (uint8_t)(obj - objects);
        part->index = index;
        rock->pos.l.x.f.u = obj->pos.l.x.f.u;
        rock->pos.l.y.f.u = obj->pos.l.y.f.u;
        index += 2;
    }
}

// Offset_0x028274: the bricks (4) round it
static void Grounder_Bricks(Object *obj) {
    static const int8_t offsets[8] = { 0x00, (int8_t)0xEC, 0x10, 0x00, 0x00, 0x0C, (int8_t)0xF0, 0x00 };
    uint16_t index = 0;
    for (int i = 0; i < 4; i++) {
        Object *brick = FindFreeObj();
        if (brick == NULL)
            return;
        brick->type = ObjId_GrounderWall;
        Scratch_GrounderPart *part = (Scratch_GrounderPart *)&brick->scratch;
        part->subtype = 4;
        part->parent = (uint8_t)(obj - objects);
        part->index = index;
        brick->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + offsets[index]);
        brick->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + offsets[index + 1]);
        index += 2;
    }
}

void Obj_Grounder(Object *obj) {
    Scratch_Grounder *scratch = (Scratch_Grounder *)&obj->scratch;

    switch (obj->routine) {
    case GrounderRoutine_Init:
        Settings(obj, Mappings_Grounder, TILE_MAP(0, 1, 0, 0, 0x509), 4, 5, 0x10, 0x02);
        obj->y_rad = 0x14;
        obj->x_rad = 0x10;
        {
            const int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (dist < 0) {
                obj->pos.l.y.f.u += dist;
                obj->ysp = 0;
            }
        }
        if (obj->type == 0x8D) { // (hidden: its bricks first)
            Grounder_Bricks(obj);
        } else {
            obj->routine = GrounderRoutine_Face;
        }
        return;

    case GrounderRoutine_Sit: {
        int16_t dx = WhereIsSonic(obj).dx;
        if (dx < 0)
            dx = (int16_t)-dx;
        if ((uint16_t)dx <= 0x60) {
            obj->routine += 2;
            scratch->active = 0xFF;
            Grounder_Rocks(obj);
        }
        RememberState(obj);
        return;
    }

    case GrounderRoutine_Emerge:
        AlphaAnimate(obj, Animation_GrounderEmerge);
        RememberState(obj);
        return;

    case GrounderRoutine_Face: {
        obj->routine += 2;
        const Where w = WhereIsSonic(obj);
        obj->xsp = w.side ? 0x100 : -0x100;
        SetFlip(obj, w.side != 0);
        RememberState(obj);
        return;
    }

    case GrounderRoutine_Walk: {
        SpeedToPos(obj);
        const int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (dist >= -2 && dist < 0x0C) {
            obj->pos.l.y.f.u += dist;
            AlphaAnimate(obj, Animation_GrounderWalk);
        } else {
            obj->routine += 2;
            scratch->timer = 0x3B;
        }
        RememberState(obj);
        return;
    }

    case GrounderRoutine_Wait:
        if ((int8_t)--scratch->timer < 0) {
            obj->routine = GrounderRoutine_Walk;
            obj->xsp = (int16_t)-obj->xsp;
            SetFlip(obj, !obj->status.o.f.x_flip);
        }
        RememberState(obj);
        return;
    }
}

// The bricks (8F): they wait for the Grounder to wake and then break away, up and out, by one of four speeds
void Obj_GrounderWall(Object *obj) {
    Scratch_GrounderPart *scratch = (Scratch_GrounderPart *)&obj->scratch;
    static const int8_t speeds[8] = { 0x01, (int8_t)0xFE, 0x01, (int8_t)0xFF, (int8_t)0xFF, (int8_t)0xFE, (int8_t)0xFF, (int8_t)0xFF };

    switch (obj->routine) {
    case 0:
        Settings(obj, Mappings_GrounderWall, TILE_MAP(0, 1, 0, 0, 0x434), 0x84, 4, 0x10, 0);
        return;

    case 2:
        if (((Scratch_Grounder *)&objects[scratch->parent].scratch)->active != 0) {
            obj->routine += 2;
            obj->xsp = (int16_t)(speeds[scratch->index] << 8);
            obj->ysp = (int16_t)(speeds[scratch->index + 1] << 8);
        }
        RememberState(obj);
        return;

    default: // 4: falls until it is out of sight
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        ObjectFall(obj);
        RememberState(obj);
        return;
    }
}

// The rocks (90): they spring out by one of five speeds, and fall until they are out of sight
void Obj_GrounderRock(Object *obj) {
    Scratch_GrounderPart *scratch = (Scratch_GrounderPart *)&obj->scratch;
    static const uint8_t frames[6] = { 0, 2, 0, 1, 0, 0 };
    static const int8_t speeds[10] = { (int8_t)0xFF, (int8_t)0xFC, 0x04, (int8_t)0xFD, 0x02, 0x00, (int8_t)0xFD, (int8_t)0xFF, (int8_t)0xFD, (int8_t)0xFD };

    if (obj->routine == 0) {
        Settings(obj, Mappings_GrounderRock, TILE_MAP(0, 2, 0, 0, 0x509), 0x84, 4, 0x08, 0);
        obj->xsp = (int16_t)(speeds[scratch->index] << 8);
        obj->ysp = (int16_t)(speeds[scratch->index + 1] << 8);
        obj->frame = frames[scratch->index >> 1];
        RememberState(obj);
        return;
    }
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    ObjectFall(obj);
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Chop Chop (91): a fish that swims to and fro ($200 frames each way), spits a bubble (object 0A, subtype 6) every $50, and when Sonic is ahead of it and near (within $20 to $A0 across, $20 up or down) it stops,
// waits $10 and dashes at him
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad;
    int16_t timer;   // 0x2A
    uint8_t bubbles; // 0x2C: the high byte of the word that is set to $50 (the code counts the byte at $2C, which is 0: so it counts a whole 256 frames)
    uint8_t pad2;
} Scratch_ChopChop;

enum { ChopRoutine_Init = 0, ChopRoutine_Swim = 2, ChopRoutine_Stop = 4, ChopRoutine_Dash = 6 };

static void ChopChop_Bubble(Object *obj) {
    Scratch_ChopChop *scratch = (Scratch_ChopChop *)&obj->scratch;
    scratch->bubbles = 0;
    Object *bubble = FindFreeObj();
    if (bubble == NULL)
        return;
    bubble->type = 0x0A;
    bubble->scratch.u8[0] = 6;
    bubble->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + (obj->render.f.x_flip ? -0x14 : 0x14));
    bubble->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 6);
}

// Offset_0x0284C2: is Sonic ahead of it, within reach? (D2 across, D3 up and down)
static bool ChopChop_SeesSonic(const Object *obj, const Where *w) {
    uint16_t dy = (uint16_t)(w->dy + 0x20);
    if (dy >= 0x40)
        return false;
    int16_t dx = w->dx;
    if (dx >= 0) { // Sonic is to the left
        if (obj->xsp >= 0)
            return false;
    } else { // to the right
        if (obj->xsp < 0)
            return false;
        dx = (int16_t)-dx;
    }
    return (uint16_t)dx >= 0x20 && (uint16_t)dx < 0xA0;
}

void Obj_ChopChop(Object *obj) {
    Scratch_ChopChop *scratch = (Scratch_ChopChop *)&obj->scratch;

    switch (obj->routine) {
    case ChopRoutine_Init:
        Settings(obj, Mappings_ChopChop, TILE_MAP(0, 1, 0, 0, 0x53B), 4, 4, 0x10, 0x02);
        scratch->timer = 0x200;
        scratch->bubbles = 0;
        obj->xsp = obj->status.o.f.x_flip ? 0x40 : -0x40;
        return;

    case ChopRoutine_Swim: {
        if (--scratch->bubbles == 0)
            ChopChop_Bubble(obj);
        if (--scratch->timer < 0) { // (a word: it turns round every $200 frames)
            scratch->timer = 0x200;
            obj->status.o.f.x_flip = !obj->status.o.f.x_flip;
            obj->render.f.x_flip = !obj->render.f.x_flip;
            obj->xsp = (int16_t)-obj->xsp;
        }
        SpeedToPos(obj);
        const Where w = WhereIsSonic(obj);
        if (ChopChop_SeesSonic(obj, &w)) {
            obj->routine += 2;
            scratch->timer = 0x10;
            obj->xsp = 0;
        }
        RememberState(obj);
        return;
    }

    case ChopRoutine_Stop:
        if ((int8_t)--scratch->timer < 0) { // dashes (the speed from the side, and the byte at $13 from above or below)
            obj->routine += 2;
            const Where w = WhereIsSonic(obj);
            obj->xsp = w.side ? 0x200 : -0x200;
            if ((uint16_t)(w.dy + 0x10) >= 0x20)
                obj->ysp = (int16_t)((obj->ysp & 0xFF00) | 0x80); // (its low byte, from a table of $80 and $80: it always sinks)
        }
        AlphaAnimate(obj, Animation_ChopChop);
        RememberState(obj);
        return;

    case ChopRoutine_Dash:
        SpeedToPos(obj);
        AlphaAnimate(obj, Animation_ChopChop);
        RememberState(obj);
        return;
    }
}
