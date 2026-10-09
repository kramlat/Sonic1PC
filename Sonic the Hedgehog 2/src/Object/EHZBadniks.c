// Emerald Hill's badniks for Sonic 2 (Nick Arcade's objects 4B, 53 and 54): the Buzzer, the Masher and the Snail. Each is its own object with the parts it makes (the Buzzer's flame and shots, the Snail's
// head and flame) as objects of the same id in other routines, which find their parent by its slot (an index, checked every frame: a parent that has gone, or been replaced, ends them).
#include "Object/EHZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/Buzzer.h"
#include "Resource/Animation/Masher.h"
#include "Resource/Animation/Snail.h"
#include "Resource/Mappings/Buzzer.h"
#include "Resource/Mappings/Masher.h"
#include "Resource/Mappings/Snail.h"

// Tiles: where Emerald Hill's PLC puts their art
#define ArtTile_Buzzer 0x3E6
#define ArtTile_Snail  0x402
#define ArtTile_Masher 0x41C

// A part of another object: a new object of the same id (the slot after the parent's, as the original allocates), put to the routine given, at the parent's place and with its flips
static Object *MakePart(Object *parent, uint8_t routine, const uint8_t *mappings, uint16_t tile, uint8_t priority) {
    Object *part = FindNextFreeObj(parent + 1);
    if (part == NULL)
        return NULL;
    part->type = parent->type;
    part->routine = routine;
    part->mappings = mappings;
    part->tile = tile;
    part->render.b = parent->render.b;
    part->render.f.level_fg = true;
    part->priority = priority;
    part->width_pixels = 0x10;
    part->status.b = parent->status.b;
    part->pos.l.x.f.u = parent->pos.l.x.f.u;
    part->pos.l.y.f.u = parent->pos.l.y.f.u;
    return part;
}

// The parent of a part: its slot has to still hold an object of the same id (and be a different one than the part itself)
static Object *Parent(const Object *part, uint8_t slot, bool *alive) {
    Object *parent = &objects[slot];
    *alive = slot != 0 && parent != part && parent->type == part->type;
    return parent;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Buzzer (object 4B): a wasp that flies to and fro and shoots when Sonic is in its line of fire
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0;        // 0x29
    uint8_t parent;      // 0x2A: (a flame's) the slot of the Buzzer
    uint8_t pad1[3];     // 0x2B-0x2D
    int16_t move_timer;  // 0x2E
    int16_t turn_delay;  // 0x30: counts down, and the flame shows while it is negative
    uint8_t shot_made;   // 0x32: a shot has been made since the last turn
    uint8_t pad2;        // 0x33
    int16_t shot_timer;  // 0x34
} Scratch_Buzzer;

enum { BuzzerRoutine_Init = 0, BuzzerRoutine_Main = 2, BuzzerRoutine_Flame = 4, BuzzerRoutine_Shot = 6 };
enum { BuzzerAnim_Moving, BuzzerAnim_Flame, BuzzerAnim_Bullet, BuzzerAnim_Shoot };

// Sonic in a strip 40 to 48 pixels in front of it makes it shoot (Buzzer_ChkPlayers)
static void Buzzer_ChkPlayer(Object *obj, Scratch_Buzzer *scratch) {
    if (scratch->shot_made)
        return;
    int16_t d1 = obj->pos.l.x.f.u - player->pos.l.x.f.u;
    int16_t d0 = d1 < 0 ? -d1 : d1;
    if (d0 < 0x28 || d0 > 0x30)
        return;
    if (d1 >= 0) { // Sonic is at its left
        if (obj->render.f.x_flip)
            return;
    } else {
        if (!obj->render.f.x_flip)
            return;
    }
    scratch->shot_made = 0xFF;
    obj->routine_sec += 2;
    obj->anim = BuzzerAnim_Shoot;
    scratch->shot_timer = 0x32;
}

static void Buzzer_Shoot(Object *obj) {
    Object *shot = MakePart(obj, BuzzerRoutine_Shot, Mappings_Buzzer, ArtTile_Buzzer, 4);
    if (shot == NULL)
        return;
    shot->col_type = 0x98;
    shot->anim = BuzzerAnim_Bullet;
    shot->pos.l.y.f.u += 0x18; // (from its stinger)
    shot->ysp = 0x180;
    shot->xsp = -0x180;
    int16_t reach = 0xD;
    if (shot->render.f.x_flip) {
        shot->xsp = -shot->xsp;
        reach = -reach;
    }
    shot->pos.l.x.f.u += reach;
}

void Obj_Buzzer(Object *obj) {
    Scratch_Buzzer *scratch = (Scratch_Buzzer *)&obj->scratch;

    switch (obj->routine) {
    case BuzzerRoutine_Init: {
        obj->mappings = Mappings_Buzzer;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Buzzer);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->col_type = 0xA;
        obj->width_pixels = 0x10;
        obj->y_rad = 0x10;
        obj->x_rad = 0x18;
        obj->priority = 3;
        obj->routine += 2;

        Object *flame = MakePart(obj, BuzzerRoutine_Flame, Mappings_Buzzer, ArtTile_Buzzer, 4);
        if (flame == NULL)
            break;
        flame->anim = BuzzerAnim_Flame;
        ((Scratch_Buzzer *)&flame->scratch)->parent = (uint8_t)(obj - objects);
        scratch->move_timer = 0x100;
        obj->xsp = -0x100;
        if (obj->render.f.x_flip)
            obj->xsp = -obj->xsp;
        break;
    }
    case BuzzerRoutine_Main:
        if (obj->routine_sec == 0) { // roaming
            Buzzer_ChkPlayer(obj, scratch);
            scratch->turn_delay--;
            if (scratch->turn_delay == 0xF) { // turns around
                scratch->shot_made = 0;
                obj->xsp = -obj->xsp;
                obj->render.f.x_flip ^= 1;
                obj->status.o.f.x_flip ^= 1;
                scratch->move_timer = 0x100;
            } else if (scratch->turn_delay < 0) {
                if (--scratch->move_timer > 0)
                    SpeedToPos(obj);
                else
                    scratch->turn_delay = 0x1E;
            }
        } else { // shooting
            int16_t timer = scratch->shot_timer - 1;
            if (timer < 0) {
                obj->routine_sec -= 2;
            } else {
                scratch->shot_timer = timer;
                if (timer == 0x14)
                    Buzzer_Shoot(obj);
            }
        }
        AnimateSprite(obj, Animation_Buzzer);
        RememberState(obj);
        break;
    case BuzzerRoutine_Flame: {
        bool alive;
        Object *parent = Parent(obj, scratch->parent, &alive);
        if (!alive) {
            ObjectDelete(obj);
            break;
        }
        if (((Scratch_Buzzer *)&parent->scratch)->turn_delay >= 0)
            break; // (not shown while it turns)
        obj->pos.l.x.f.u = parent->pos.l.x.f.u;
        obj->pos.l.y.f.u = parent->pos.l.y.f.u;
        obj->status.b = parent->status.b;
        obj->render.b = parent->render.b;
        AnimateSprite(obj, Animation_Buzzer);
        RememberState(obj);
        break;
    }
    case BuzzerRoutine_Shot:
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_Buzzer);
        RememberState(obj);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Masher (object 53): a fish that jumps out of the water again and again
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[7];  // 0x29-0x2F
    int16_t origin_y; // 0x30
} Scratch_Masher;

enum { MasherAnim_Rising, MasherAnim_Chomping, MasherAnim_Falling };

void Obj_Masher(Object *obj) {
    Scratch_Masher *scratch = (Scratch_Masher *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_Masher;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Masher);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->col_type = 9;
        obj->width_pixels = 0x10;
        obj->ysp = -0x400;
        scratch->origin_y = obj->pos.l.y.f.u;
    }

    AnimateSprite(obj, Animation_Masher);
    SpeedToPos(obj);
    obj->ysp += 0x18;
    uint16_t home = (uint16_t)scratch->origin_y;
    if (home < (uint16_t)obj->pos.l.y.f.u) { // back at the water: jumps again
        obj->pos.l.y.f.u = (int16_t)home;
        obj->ysp = -0x500;
    }
    obj->anim = MasherAnim_Chomping;
    if ((uint16_t)(home - 0xC0) < (uint16_t)obj->pos.l.y.f.u) {
        obj->anim = MasherAnim_Rising;
        if (obj->ysp >= 0)
            obj->anim = MasherAnim_Falling;
    }
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Snail (object 54): a slow crawler that charges when Sonic is in front of it, and turns at the ends of its floor
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;      // 0x28
    uint8_t pad0;         // 0x29
    uint8_t parent;       // 0x2A: (a head's or flame's) the slot of the Snail
    uint8_t pad1[5];      // 0x2B-0x2F
    int16_t timer;        // 0x30: how long it waits to turn
    uint8_t pad2[2];      // 0x32-0x33
    uint8_t flame_ends;   // 0x34: the flame is to go
    uint8_t charging;     // 0x35
} Scratch_Snail;

enum { SnailRoutine_Init = 0, SnailRoutine_Move = 2, SnailRoutine_Turn = 4, SnailRoutine_Head = 6, SnailRoutine_Flame = 8 };

static void Snail_CreateFlame(Object *obj) {
    Object *flame = MakePart(obj, SnailRoutine_Flame, Mappings_Buzzer, ArtTile_Buzzer, 4);
    if (flame == NULL)
        return;
    ((Scratch_Snail *)&flame->scratch)->parent = (uint8_t)(obj - objects);
    flame->pos.l.y.f.u += 7;
    flame->pos.l.x.f.u += 0xD;
    flame->anim = BuzzerAnim_Flame;
}

// It notices Sonic within 100 pixels in front of it, and charges (four times as fast, with a flame)
static void Snail_Charge(Object *obj, Scratch_Snail *scratch) {
    if (scratch->charging)
        return;
    int16_t d0 = player->pos.l.x.f.u - obj->pos.l.x.f.u;
    if (d0 > 100 || d0 < -100)
        return;
    if (d0 < 0) {
        if (obj->status.o.f.x_flip)
            return;
    } else {
        if (!obj->status.o.f.x_flip)
            return;
    }
    obj->xsp <<= 2;
    scratch->charging = 0xFF;
    Snail_CreateFlame(obj);
}

void Obj_Snail(Object *obj) {
    Scratch_Snail *scratch = (Scratch_Snail *)&obj->scratch;

    switch (obj->routine) {
    case SnailRoutine_Init: {
        obj->mappings = Mappings_Snail;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Snail);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->col_type = 0xA;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->y_rad = 0x10;
        obj->x_rad = 0xE;

        Object *head = MakePart(obj, SnailRoutine_Head, Mappings_Snail, TILE_MAP(0, 1, 0, 0, ArtTile_Snail), 3);
        if (head != NULL) {
            ((Scratch_Snail *)&head->scratch)->parent = (uint8_t)(obj - objects);
            head->frame = 2;
        }
        obj->routine += 2;
        obj->xsp = obj->status.o.f.x_flip ? 0x80 : -0x80;
        break;
    }
    case SnailRoutine_Move: {
        Snail_Charge(obj, scratch);
        SpeedToPos(obj);
        int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d1 < -8 || d1 >= 0xC) { // the floor ends: turns
            obj->routine += 2;
            scratch->timer = 0x14;
            scratch->flame_ends = 0xFF;
        } else {
            obj->pos.l.y.f.u += d1;
        }
        AnimateSprite(obj, Animation_Snail);
        RememberState(obj);
        break;
    }
    case SnailRoutine_Turn:
        if (--scratch->timer >= 0) {
            RememberState(obj);
            break;
        }
        obj->xsp = -obj->xsp;
        ObjectFall(obj);
        obj->xsp >>= 2;
        obj->status.o.f.x_flip ^= 1;
        obj->render.f.x_flip ^= 1;
        obj->routine -= 2;
        scratch->flame_ends = 0;
        scratch->charging = 0;
        RememberState(obj);
        break;
    case SnailRoutine_Head: {
        bool alive;
        Object *parent = Parent(obj, scratch->parent, &alive);
        if (!alive) {
            ObjectDelete(obj);
            break;
        }
        obj->pos.l.x.f.u = parent->pos.l.x.f.u;
        obj->pos.l.y.f.u = parent->pos.l.y.f.u;
        obj->status.b = parent->status.b;
        obj->render.b = parent->render.b;
        RememberState(obj);
        break;
    }
    case SnailRoutine_Flame: {
        bool alive;
        Object *parent = Parent(obj, scratch->parent, &alive);
        if (!alive || ((Scratch_Snail *)&parent->scratch)->flame_ends) {
            ObjectDelete(obj);
            break;
        }
        obj->pos.l.x.f.u = parent->pos.l.x.f.u;
        obj->pos.l.y.f.u = parent->pos.l.y.f.u + 7;
        obj->pos.l.x.f.u += obj->status.o.f.x_flip ? -0xD : 0xD;
        AnimateSprite(obj, Animation_Buzzer);
        RememberState(obj);
        break;
    }
    }
}
