// The boss of Emerald Hill for Sonic 2 (Nick Arcade's objects 55 and 58, with 57's code): Eggman in a drill car. The ship (object 55, subtype $81, put there by the zone's dynamic level events) slides in, comes down, and
// drives to and fro over the arena on three wheels (object 58 parts that follow it and each other) with a spike on its front; Sonic spins into it (a hit counter of eight, with a flash and a moment of safety after
// each), and when it is beaten it blows up, the wheels roll off, and Eggman's car drives away to the right. The parts find the ship by its slot (checked every frame: it has to be an object of the ship's own id).
#include "Object/EHZBoss.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/BossEHZ.h"
#include "Resource/Animation/BossEHZParts.h"
#include "Resource/Animation/BossEHZWheels.h"
#include "Resource/Mappings/BossEHZ.h"
#include "Resource/Mappings/BossEHZParts.h"
#include "Resource/Mappings/BossEHZWheels.h"

#define ObjId_EHZBoss     0x55
#define ObjId_EHZBossPart 0x58

// Where the art goes (the boss' PLC): the ship at $460, the car at $4C0, its blades at $540
#define ArtTile_BossShipEHZ  0x460
#define ArtTile_BossCarEHZ   0x4C0
#define ArtTile_BossBladesEHZ 0x540

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0;      // 0x29
    int16_t timer;     // 0x2A
    uint8_t step;      // 0x2C: a step of a routine
    uint8_t flags;     // 0x2D: (the ship's) 0 landed, 1 driving, 2 driving away, 3 beaten (and, as the spike sets it, about to lunge)
    int16_t height;    // 0x2E: the wheels' heights added up
    int16_t home_x;    // 0x30
    int16_t pad1;      // 0x32
    uint8_t link;      // 0x34: the slot of the ship (a part's), or of the part (the ship's)
    uint8_t pad2[3];   // 0x35-0x37
    int16_t home_y;    // 0x38
    uint8_t pad3[2];   // 0x3A-0x3B
    int16_t countdown; // 0x3C
    uint8_t flash;     // 0x3E: how long its hit flash lasts
    uint8_t pad4;      // 0x3F
} Scratch_Boss;

enum { BossFlag_Landed = 1 << 0, BossFlag_Driving = 1 << 1, BossFlag_Leaving = 1 << 2, BossFlag_Beaten = 1 << 3 };

// A part of the ship (object 58 or, for its cockpit, 55): a new object at the ship's place with the slot of the ship in it
static Object *MakePart(Object *boss, uint8_t type, uint8_t routine, const uint8_t *mappings, uint16_t tile, uint8_t width, uint8_t priority) {
    Object *part = FindNextFreeObj(boss + 1);
    if (part == NULL)
        return NULL;
    part->type = type;
    part->routine = routine;
    part->mappings = mappings;
    part->tile = tile;
    part->render.b = 0;
    part->render.f.level_fg = true;
    part->width_pixels = width;
    part->priority = priority;
    part->pos.l.x = boss->pos.l.x;
    part->pos.l.y = boss->pos.l.y;
    ((Scratch_Boss *)&part->scratch)->link = (uint8_t)(boss - objects);
    return part;
}

// The ship of a part: its slot must still hold an object of the ship's id
static Object *Ship(const Object *part, bool *alive) {
    const Scratch_Boss *scratch = (const Scratch_Boss *)&part->scratch;
    Object *ship = &objects[scratch->link];
    *alive = scratch->link != 0 && ship != part && ship->type == ObjId_EHZBoss;
    return ship;
}

static void CopyLook(Object *part, const Object *ship) {
    part->status.b = ship->status.b;
    part->render.b = ship->render.b;
}

// Turns at either end of the arena (sub_17A6A)
static void Boss_Turn(Object *obj) {
    int16_t x = obj->pos.l.x.f.u;
    if (x > 0x2720 && x < 0x2B08)
        return;
    obj->status.o.f.x_flip ^= 1;
    obj->render.f.x_flip ^= 1;
    obj->xsp = -obj->xsp;
}

// Hit by Sonic (sub_17A8C): flashes and is safe a while after each hit; beaten when the hits are out
static void Boss_Hit(Object *obj) {
    Scratch_Boss *scratch = (Scratch_Boss *)&obj->scratch;

    if (obj->routine_sec >= 6)
        return;
    if (obj->status.b & 0x80) { // the hits are out
        AddPoints(100);
        obj->routine_sec = 6;
        scratch->countdown = 0xB3;
        scratch->flags |= BossFlag_Beaten;
        return;
    }
    if (obj->col_type != 0)
        return;
    if (scratch->flash == 0) {
        scratch->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--scratch->flash == 0)
        obj->col_type = 0xF;
}

// The ship's own business (Obj57)
static void Boss_Ship(Object *obj) {
    Scratch_Boss *scratch = (Scratch_Boss *)&obj->scratch;

    switch (obj->routine_sec) {
    case 0: // slides in from the right
        obj->col_type = 0;
        if (obj->pos.l.x.f.u > 0x29D0) {
            obj->pos.l.x.f.u--;
        } else {
            obj->pos.l.x.f.u = 0x29D0;
            obj->routine_sec += 2;
        }
        break;
    case 2: // comes down, waits a second, and sets off
        if (scratch->step == 0) {
            if (obj->pos.l.y.f.u < 0x41E) {
                obj->pos.l.y.f.u++;
            } else {
                scratch->step += 2;
                scratch->flags |= BossFlag_Landed;
                scratch->timer = 0x3C;
            }
        } else if (--scratch->timer < 0) {
            obj->xsp = -0x200;
            obj->routine_sec += 2;
            obj->col_type = 0xF;
            scratch->flags |= BossFlag_Driving;
        }
        break;
    case 4: // driving to and fro: its height from its wheels
        Boss_Hit(obj);
        Boss_Turn(obj);
        obj->pos.l.y.f.u = (int16_t)((scratch->height >> 1) - 0x14);
        scratch->height = 0;
        obj->pos.l.x.v += (int32_t)obj->xsp << 8;
        break;
    case 6: // beaten: blowing up
        if (--scratch->countdown >= 0) {
            BossDefeated(obj);
            break;
        }
        obj->status.o.f.x_flip = true;
        obj->status.b &= 0x7F;
        obj->xsp = 0;
        obj->routine_sec += 2;
        scratch->countdown = (int16_t)0xFFDA;
        scratch->timer = 0xC;
        break;
    case 8: // sinks a little
        obj->pos.l.y.f.u++;
        if (--scratch->timer >= 0) {
            break;
        }
        obj->routine_sec += 2;
        scratch->step = 0;
        break;
    case 10: // Eggman's car lifts off and drives away
        switch (scratch->step) {
        case 0: {
            scratch->flags &= (uint8_t)~BossFlag_Landed;
            Object *lift = MakePart(obj, ObjId_EHZBossPart, 8, Mappings_BossEHZParts, TILE_MAP(0, 1, 0, 0, 0x540), 0x20, 4);
            if (lift != NULL) {
                lift->pos.l.y.f.u += 0xC;
                CopyLook(lift, obj);
                lift->anim = 2;
                ((Scratch_Boss *)&lift->scratch)->timer = 0x10;
                scratch->timer = 0x32;
                scratch->step += 2;
            }
            break;
        }
        case 2:
            if (--scratch->timer < 0) {
                scratch->flags |= BossFlag_Leaving;
                scratch->timer = 0x60;
                scratch->step += 2;
            }
            break;
        case 4:
            obj->pos.l.y.f.u--;
            if (--scratch->timer >= 0)
                break;
            obj->pos.l.y.f.u++;
            obj->pos.l.x.f.u += 2;
            if ((uint16_t)obj->pos.l.x.f.u >= 0x2B08 && !boss_status) {
                boss_status = 1; // the boss is beaten (Boss_defeated_flag)
                ObjectDelete(obj);
                return;
            }
            break;
        }
        break;
    }
}

// Spawns the wheels and the spike (sub_17D9A)
static void Boss_MakeWheels(Object *obj) {
    static const struct { int16_t x; uint8_t priority, frame, anim; int16_t timer; } wheels[3] = {
        { 0x1C, 1, 4, 1, 0x16 },
        { -0xC, 1, 4, 1, 0x4B },
        { -0x2C, 2, 6, 2, 0x30 },
    };
    for (int i = 0; i < 3; i++) {
        Object *wheel = MakePart(obj, ObjId_EHZBossPart, 4, Mappings_BossEHZWheels, TILE_MAP(0, 1, 0, 0, ArtTile_BossCarEHZ), 0x10, wheels[i].priority);
        if (wheel == NULL)
            continue;
        wheel->y_rad = 0x10;
        wheel->x_rad = 0x10;
        wheel->pos.l.x.f.u += wheels[i].x;
        wheel->pos.l.y.f.u += 0xC;
        wheel->xsp = -0x200;
        wheel->frame = wheels[i].frame;
        wheel->anim = wheels[i].anim;
        ((Scratch_Boss *)&wheel->scratch)->timer = wheels[i].timer;
    }

    // the spike on its front
    Object *spike = MakePart(obj, ObjId_EHZBossPart, 6, Mappings_BossEHZWheels, TILE_MAP(0, 1, 0, 0, ArtTile_BossCarEHZ), 0x10, 1);
    if (spike != NULL) {
        spike->pos.l.x.f.u -= 0x36;
        spike->pos.l.y.f.u += 8;
        spike->frame = 1;
        spike->anim = 0;
    }
}

// The ship at its start (loc_17F54): the car's parts, and the ship put where it comes in from
static void Boss_Start(Object *obj) {
    Scratch_Boss *scratch = (Scratch_Boss *)&obj->scratch;

    Object *body = MakePart(obj, ObjId_EHZBossPart, 2, Mappings_BossEHZWheels, TILE_MAP(0, 0, 0, 0, ArtTile_BossCarEHZ), 0x20, 2);
    (void)body;
    Boss_MakeWheels(obj);
    scratch->home_y -= 8;
    obj->pos.l.x.f.u = 0x2A00;
    obj->pos.l.y.f.u = 0x2C0;
    Object *cockpit = MakePart(obj, ObjId_EHZBossPart, 0, Mappings_BossEHZParts, TILE_MAP(0, 1, 0, 0, 0x540), 0x20, 4);
    if (cockpit != NULL)
        ((Scratch_Boss *)&cockpit->scratch)->timer = 0x1E;
}

// The ship (object 55)
static void Boss_Run(Object *obj) {
    Scratch_Boss *scratch = (Scratch_Boss *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Initialization
        obj->mappings = Mappings_BossEHZ;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_BossShipEHZ); // (palette line 2; $2400 + $60 for the subtype $81)
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x20;
        obj->priority = 3;
        obj->col_type = 0xF;
        obj->col_property = 8;
        obj->routine += 2;
        scratch->home_x = obj->pos.l.x.f.u;
        scratch->home_y = obj->pos.l.y.f.u;

        // Eggman's face and hands: an object that follows the ship
        Object *face = MakePart(obj, ObjId_EHZBoss, 4, Mappings_BossEHZ, TILE_MAP(0, 0, 0, 0, ArtTile_BossShipEHZ), 0x20, 3);
        if (face != NULL) {
            scratch->link = (uint8_t)(face - objects);
            face->anim = 1;
            face->render.b = obj->render.b;
            face->render.f.level_fg = true;
        }
        Boss_Start(obj);
        break;
    }
    case 2:
        Boss_Ship(obj);
        AnimateSprite(obj, Animation_BossEHZ);
        obj->render.b = (uint8_t)((obj->render.b & 0xFC) | (obj->status.b & 3));
        DisplaySprite(obj);
        break;
    case 4: { // the face: follows the ship
        bool alive;
        Object *ship = Ship(obj, &alive);
        if (!alive) {
            ObjectDelete(obj);
            break;
        }
        obj->pos.l.x = ship->pos.l.x;
        obj->pos.l.y = ship->pos.l.y;
        CopyLook(obj, ship);
        AnimateSprite(obj, Animation_BossEHZ);
        DisplaySprite(obj);
        break;
    }
    }
}

// The parts of the car (object 58)
static void Boss_Part(Object *obj) {
    Scratch_Boss *scratch = (Scratch_Boss *)&obj->scratch;
    bool alive;
    Object *ship;

    switch (obj->routine) {
    case 0: // the cockpit's glass? (it goes when the car is landed and the ship has stopped)
        switch (obj->routine_sec) {
        case 0:
            ship = Ship(obj, &alive);
            if (!alive) {
                ObjectDelete(obj);
                return;
            }
            if (((Scratch_Boss *)&ship->scratch)->flags & BossFlag_Landed) {
                obj->anim = 1;
                scratch->timer = 0x18;
                obj->routine_sec += 2;
            }
            obj->pos.l.x.f.u = ship->pos.l.x.f.u;
            obj->pos.l.y.f.u = ship->pos.l.y.f.u;
            CopyLook(obj, ship);
            AnimateSprite(obj, Animation_BossEHZParts);
            DisplaySprite(obj);
            break;
        case 2:
            if (--scratch->timer >= 0) {
                AnimateSprite(obj, Animation_BossEHZParts);
                DisplaySprite(obj);
            } else if (scratch->timer <= -0x10) {
                ObjectDelete(obj);
            } else {
                obj->pos.l.y.f.u++;
                DisplaySprite(obj);
            }
            break;
        }
        break;
    case 2: // the body of the car: still until it drives, then with the ship (and out of the way when it leaves)
        ship = Ship(obj, &alive);
        if (!alive) {
            ObjectDelete(obj);
            return;
        }
        if (!(((Scratch_Boss *)&ship->scratch)->flags & BossFlag_Driving)) {
            DisplaySprite(obj);
            return;
        }
        if (((Scratch_Boss *)&ship->scratch)->flags & BossFlag_Leaving) {
            obj->frame = 8;
            obj->priority = 0;
            DisplaySprite(obj);
            return;
        }
        obj->pos.l.x.f.u = ship->pos.l.x.f.u;
        obj->pos.l.y.f.u = ship->pos.l.y.f.u + 8;
        CopyLook(obj, ship);
        DisplaySprite(obj);
        break;
    case 4: // a wheel
        switch (obj->routine_sec) {
        case 0: // waits for the ship to drive
            ship = Ship(obj, &alive);
            if (!alive) {
                ObjectDelete(obj);
                return;
            }
            if (((Scratch_Boss *)&ship->scratch)->flags & BossFlag_Driving)
                obj->routine_sec += 2;
            DisplaySprite(obj);
            break;
        case 2: // rolls with the ship, on the floor
            ship = Ship(obj, &alive);
            if (!alive) {
                ObjectDelete(obj);
                return;
            }
            CopyLook(obj, ship);
            if (obj->status.b & 0x80)
                obj->routine_sec += 2;
            Boss_Turn(obj);
            ObjectFall(obj);
            {
                int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
                if (d1 < 0)
                    obj->pos.l.y.f.u += d1;
            }
            obj->ysp = 0x100;
            if (obj->priority == 1)
                ((Scratch_Boss *)&ship->scratch)->height += obj->pos.l.y.f.u;
            AnimateSprite(obj, Animation_BossEHZWheels);
            DisplaySprite(obj);
            break;
        case 4: // the ship is beaten: the wheel hops off
            if (--scratch->timer >= 0) {
                DisplaySprite(obj);
                break;
            }
            obj->routine_sec += 2;
            scratch->timer = 0xA;
            obj->ysp = -0x300;
            if (obj->priority != 1)
                obj->xsp = -obj->xsp;
            DisplaySprite(obj);
            break;
        case 6: // and bounces away
            if (--scratch->timer >= 0) {
                DisplaySprite(obj);
                break;
            }
            ObjectFall(obj);
            {
                int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
                if (d1 < 0) {
                    obj->ysp = -0x200;
                    obj->pos.l.y.f.u += d1;
                }
            }
            RememberState(obj);
            break;
        }
        break;
    case 6: // the spike on its front
        ship = Ship(obj, &alive);
        if (!alive) {
            ObjectDelete(obj);
            return;
        }
        if (((Scratch_Boss *)&ship->scratch)->flags & BossFlag_Beaten) {
            obj->pos.l.x.f.u += (obj->status.b & 1) ? 3 : -3;
            AnimateSprite(obj, Animation_BossEHZWheels);
            DisplaySprite(obj);
            break;
        }
        // with one hit left, it lunges when Sonic is in front of the ship (sub_17D6A)
        if (ship->col_property == 1) {
            bool left = obj->pos.l.x.f.u - player->pos.l.x.f.u >= 0;
            bool flipped = (ship->status.b & 1) != 0;
            if (left ? !flipped : flipped)
                ((Scratch_Boss *)&ship->scratch)->flags |= BossFlag_Beaten;
        }
        if (!(((Scratch_Boss *)&ship->scratch)->flags & BossFlag_Driving)) {
            DisplaySprite(obj);
            break;
        }
        obj->col_type = 0x8B;
        obj->pos.l.x.f.u = ship->pos.l.x.f.u;
        obj->pos.l.y.f.u = ship->pos.l.y.f.u + 0x10;
        CopyLook(obj, ship);
        obj->pos.l.x.f.u += (obj->status.b & 1) ? 0x36 : -0x36;
        AnimateSprite(obj, Animation_BossEHZWheels);
        DisplaySprite(obj);
        break;
    case 8: // the lift-off flame: rises and goes
        obj->pos.l.y.f.u--;
        if (--scratch->timer >= 0) {
            DisplaySprite(obj);
            break;
        }
        obj->routine = 0;
        AnimateSprite(obj, Animation_BossEHZParts);
        DisplaySprite(obj);
        break;
    }
}

void Obj_EHZBoss(Object *obj) {
    Boss_Run(obj);
}

void Obj_EHZBossPart(Object *obj) {
    Boss_Part(obj);
}
