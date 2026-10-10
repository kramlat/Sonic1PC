// Object 28 of the alpha (Aug 21st 1992): the animals that jump out of a destroyed badnik. It is Sonic 1's (Sonic 1's Animal.c) with twelve kinds of animal, a pair for each zone (the alpha's Offset_0x00A42A),
// the speeds and frames of each, and the routine of each kind of movement by number as the alpha numbers them: its own table, not Sonic 1's (where the routines of the end of a level are 18 and up).
#include "Object/Animals.h"

#include "Level.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Macros.h"
#include "PLC.h"

#include "Resource/Mappings/Animals.h"

#define ANIMAL_TILE_1 0x580
#define ANIMAL_TILE_2 0x594


// Which two animals a zone has, by the zone's slot (the alpha's Offset_0x00A42A)
static const uint8_t zone_animals[17][2] = {
    { 6, 5 },
    { 6, 5 },
    { 6, 5 },
    { 6, 5 },
    { 9, 7 },
    { 9, 7 },
    { 9, 7 },
    { 9, 7 },
    { 8, 3 },
    { 8, 3 },
    { 2, 3 },
    { 8, 1 },
    { 11, 5 },
    { 0, 7 },
    { 4, 1 },
    { 2, 5 },
    { 10, 1 },
};

// What each kind of animal is like (Offset_0x00A44C): the speed it runs or flies at, how high it jumps and which set of frames it has (the alpha's five, in Mappings/Animals)
static const struct { int16_t xsp, ysp; uint8_t frames; } kinds[12] = {
    { -512, -1024, 4 },
    { -512, -768, 0 },
    { -384, -768, 4 },
    { -320, -384, 3 },
    { -448, -768, 2 },
    { -768, -1024, 0 },
    { -640, -896, 1 },
    { -640, -768, 0 },
    { -512, -896, 1 },
    { -704, -768, 1 },
    { -320, -512, 1 },
    { -512, -768, 1 },
};

// The animals of the end of a level, by their subtype less 10 (Offset_0x00A4AC, $A4D8 and $A504): speeds, frames and where their art is
static const struct { int16_t xsp, ysp; uint8_t frames; uint16_t tile; } end_animals[11] = {
    { -1088, -1024, 0, 0x5A5 },
    { -1088, -1024, 0, 0x5A5 },
    { -1088, -1024, 0, 0x5A5 },
    { -768, -1024, 4, 0x553 },
    { -768, -1024, 4, 0x553 },
    { -384, -768, 4, 0x573 },
    { -384, -768, 4, 0x573 },
    { -320, -384, 3, 0x585 },
    { -448, -768, 2, 0x593 },
    { -512, -768, 0, 0x565 },
    { -640, -896, 1, 0x5B3 },
};

static const uint8_t *FrameSet(uint8_t index) {
    return Mappings_Animals + (size_t)index * 0x24; // (five sets of three frames, $24 bytes each)
}

// What each routine number does (the alpha's table at Offset_0x00A3F6, by the number over two)
typedef enum { K_INIT, K_LAND, K_WALK, K_FLY, K_WAIT, K_FLICKY_WAIT, K_FLICKY_JUMP, K_RABBIT_WAIT, K_DOUBLE_BOUNCE, K_LAND_JUMP, K_SINGLE_BOUNCE, K_FLY_BOUNCE } Kind;

static const uint8_t routine_kinds[26] = {
    K_INIT, K_LAND, K_WALK, K_FLY, K_WALK, K_WALK, K_WALK, K_FLY, K_WALK, K_FLY, K_WALK, K_WALK, K_WALK, K_WALK,
    K_WAIT, K_FLICKY_WAIT, K_FLICKY_WAIT, K_FLICKY_JUMP, K_RABBIT_WAIT, K_LAND_JUMP, K_SINGLE_BOUNCE, K_LAND_JUMP, K_SINGLE_BOUNCE, K_LAND_JUMP, K_FLY_BOUNCE, K_DOUBLE_BOUNCE,
};

uint8_t Level_AnimalsPlc(void) {
    // Flickies_Select_Array: the zone's pair of animals, as the alpha's cue numbers $32 to $3B
    static const uint8_t by_zone[18] = { 0x32, 0x32, 0x32, 0x32, 0x34, 0x34, 0x34, 0x34, 0x36, 0x36, 0x37, 0x33, 0x39, 0x3A, 0x35, 0x3B, 0x38, 0x38 };
    const unsigned zone = LEVEL_ZONE(level_id);
    return (uint8_t)(PlcId_Flicky32 + (by_zone[zone < 18 ? zone : 0] - 0x32));
}

static bool IsOnScreen(const Object *obj) {
    return (obj->render.b & 0x80) != 0;
}

// Offset_0x00A734: the one set free goes when it is off the screen and in the stretch ahead of the player
static void Gone(Object *obj) {
    const uint16_t ahead = (uint16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
    if (obj->pos.l.x.f.u >= player->pos.l.x.f.u && ahead < 0x180 && !IsOnScreen(obj)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// Offset_0x00A914: how far the player is, less $B8; the alpha tests it by its sign (Reach) and the flickies by the carry (PlayerClose)
static int16_t Reach(const Object *obj) {
    return (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u - 0xB8);
}

static bool PlayerClose(const Object *obj) {
    return (uint16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u) < 0xB8;
}

// Offset_0x00A8D4: the frame, and the landing, of a jump
static void JumpFrame(Object *obj, const Scratch_Flicky *a) {
    obj->frame = 1;
    if (obj->ysp < 0)
        return;
    obj->frame = 0;
    const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (d < 0) {
        obj->pos.l.y.f.u += d;
        obj->ysp = a->ysp;
    }
}

// Offset_0x00A8FC: it turns to the player
static void FacePlayer(Object *obj) {
    obj->render.b |= 1;
    if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u)
        obj->render.b &= (uint8_t)~1;
}

static void Flap(Object *obj) {
    if (--obj->frame_time.b < 0) {
        obj->frame_time.b = 1;
        obj->frame = (obj->frame + 1) & 1;
    }
}

static void Turn(Object *obj) {
    obj->xsp = -obj->xsp;
    obj->render.b ^= 1;
}

// A bounce that lands (A7E6, A880): it turns round every other time
static void BounceTurn(Object *obj, Scratch_Flicky *a, int16_t d) {
    a->flip_flag = (uint8_t)~a->flip_flag;
    if (!a->flip_flag)
        Turn(obj);
    obj->pos.l.y.f.u += d;
    obj->ysp = a->ysp;
}

static void Init(Object *obj, Scratch_Flicky *a) {
    obj->x_rad = 0xC;
    obj->render.b = 4;
    obj->render.b |= 1;
    obj->priority = 6;
    obj->width_pixels = 8;
    obj->frame_time.b = 7;

    if (a->subtype != 0) { // the end of a level: the kind of movement is the subtype
        const unsigned i = (unsigned)a->subtype - 10;
        obj->routine = (uint8_t)(a->subtype * 2);
        if (i >= 11) {
            ObjectDelete(obj);
            return;
        }
        obj->tile = TILE_MAP(0, 0, 0, 0, end_animals[i].tile);
        obj->mappings = FrameSet(end_animals[i].frames);
        a->xsp = obj->xsp = end_animals[i].xsp;
        a->ysp = obj->ysp = end_animals[i].ysp;
        DisplaySprite(obj);
        return;
    }

    obj->routine += 2; // from a badnik: one of the zone's two
    const unsigned pick = RandomNumber() & 1;
    const unsigned zone = LEVEL_ZONE(level_id);
    obj->tile = TILE_MAP(0, 0, 0, 0, pick ? ANIMAL_TILE_2 : ANIMAL_TILE_1);
    a->kind = zone_animals[zone < 17 ? zone : 0][pick];
    a->xsp = kinds[a->kind].xsp;
    a->ysp = kinds[a->kind].ysp;
    obj->mappings = FrameSet(kinds[a->kind].frames);
    obj->frame = 2;
    obj->ysp = -0x400;

    if (boss_status) { // after a boss they wait where they are
        obj->routine = 0x1C;
        obj->xsp = 0;
    } else {
        Object *points = FindFreeObj();
        if (points != NULL) {
            points->type = ObjId_Points;
            points->pos.l.x.f.u = obj->pos.l.x.f.u;
            points->pos.l.y.f.u = obj->pos.l.y.f.u;
            points->frame = (uint8_t)(a->points >> 1);
        }
    }
    DisplaySprite(obj);
}

// Offset_0x00A63A: falling out of the badnik until it lands, and then moving off as its kind does
static void Land(Object *obj, Scratch_Flicky *a) {
    if (!IsOnScreen(obj)) {
        ObjectDelete(obj);
        return;
    }
    ObjectFall(obj);
    if (obj->ysp >= 0) {
        const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d < 0) {
            obj->pos.l.y.f.u += d;
            obj->xsp = a->xsp;
            obj->ysp = a->ysp;
            obj->frame = 1;
            obj->routine = (uint8_t)(a->kind * 2 + 4);
            if (boss_status && (frame_count & 0x10))
                Turn(obj);
        }
    }
    DisplaySprite(obj);
}

// Offset_0x00A694: running along, jumping when it lands
static void Walk(Object *obj, Scratch_Flicky *a) {
    ObjectFall(obj);
    obj->frame = 1;
    if (obj->ysp >= 0) {
        obj->frame = 0;
        const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d < 0) {
            obj->pos.l.y.f.u += d;
            obj->ysp = a->ysp;
        }
    }
    if (a->subtype != 0) {
        Gone(obj);
    } else if (IsOnScreen(obj)) {
        DisplaySprite(obj);
    } else {
        ObjectDelete(obj);
    }
}

// Offset_0x00A6D0: flying, flapping as it goes
static void Fly(Object *obj, Scratch_Flicky *a) {
    SpeedToPos(obj);
    obj->ysp += 0x18;
    if (obj->ysp >= 0) {
        const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d < 0) {
            obj->pos.l.y.f.u += d;
            obj->ysp = a->ysp;
            if (a->subtype != 0 && a->subtype != 0x0A)
                Turn(obj);
        }
    }
    Flap(obj);
    if (a->subtype != 0)
        Gone(obj);
    else if (IsOnScreen(obj))
        DisplaySprite(obj);
    else
        ObjectDelete(obj);
}

void Obj_FlickyAnimals(Object *obj) {
    Scratch_Flicky *a = (Scratch_Flicky *)&obj->scratch;
    const unsigned index = obj->routine >> 1;
    if (index >= sizeof(routine_kinds)) {
        ObjectDelete(obj);
        return;
    }

    switch (routine_kinds[index]) {
    case K_INIT:
        Init(obj, a);
        break;
    case K_LAND:
        Land(obj, a);
        break;
    case K_WALK:
        Walk(obj, a);
        break;
    case K_FLY:
        Fly(obj, a);
        break;
    case K_WAIT: // (Offset_0x00A750: they wait where they are, after a boss, until the capsule counts down)
        if (!IsOnScreen(obj)) {
            ObjectDelete(obj);
            return;
        }
        if (--a->timer == 0) {
            obj->routine = 2;
            obj->priority = 3;
        }
        DisplaySprite(obj);
        break;
    case K_FLICKY_WAIT: // (they fly off when the player is close)
        if (PlayerClose(obj)) {
            obj->xsp = a->xsp;
            obj->ysp = a->ysp;
            obj->routine = 0xE;
            Fly(obj, a);
        } else {
            Gone(obj);
        }
        break;
    case K_FLICKY_JUMP: // (it jumps on the spot, facing the player, when he is near)
        if (Reach(obj) < 0) {
            obj->xsp = 0;
            a->xsp = 0;
            SpeedToPos(obj);
            obj->ysp += 0x18;
            JumpFrame(obj, a);
            FacePlayer(obj);
            Flap(obj);
        }
        Gone(obj);
        break;
    case K_RABBIT_WAIT: // (it runs when the player is near)
        if (Reach(obj) < 0) {
            obj->xsp = a->xsp;
            obj->ysp = a->ysp;
            obj->routine = 4;
            Walk(obj, a);
        } else {
            Gone(obj);
        }
        break;
    case K_DOUBLE_BOUNCE: // (Offset_0x00A7E6: no check of the player's nearness)
        ObjectFall(obj);
        obj->frame = 1;
        if (obj->ysp >= 0) {
            obj->frame = 0;
            const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (d < 0)
                BounceTurn(obj, a, d);
        }
        Gone(obj);
        break;
    case K_LAND_JUMP: // (it jumps in place once the player is near)
        if (Reach(obj) < 0) {
            obj->xsp = 0;
            a->xsp = 0;
            ObjectFall(obj);
            JumpFrame(obj, a);
            FacePlayer(obj);
        }
        Gone(obj);
        break;
    case K_SINGLE_BOUNCE:
        if (Reach(obj) < 0) {
            ObjectFall(obj);
            obj->frame = 1;
            if (obj->ysp >= 0) {
                obj->frame = 0;
                const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
                if (d < 0) {
                    Turn(obj);
                    obj->pos.l.y.f.u += d;
                    obj->ysp = a->ysp;
                }
            }
        }
        Gone(obj);
        break;
    case K_FLY_BOUNCE:
        if (Reach(obj) < 0) {
            SpeedToPos(obj);
            obj->ysp += 0x18;
            if (obj->ysp >= 0) {
                const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
                if (d < 0)
                    BounceTurn(obj, a, d);
            }
            Flap(obj);
        }
        Gone(obj);
        break;
    }
}
