// The boss of Emerald Hill for Sonic 2 (the alpha's objects 56 and 5B, with the ship's code of its 56 subtype $81; Nick Arcade's were 55 and 58, and 57 had the ship's code): Eggman in a drill car. The ship (object 56, subtype $81, put there by the zone's dynamic level events) slides in, comes down, and
// drives to and fro over the arena on three wheels (object 58 parts that follow it and each other) with a spike on its front; Sonic spins into it (a hit counter of eight, with a flash and a moment of safety after
// each), and when it is beaten it blows up, the wheels roll off, and Eggman's car drives away to the right. The parts find the ship by its slot (checked every frame: it has to be an object of the ship's own id).
#include "Object/EHZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "MathUtil.h"
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
#include "Resource/Mappings/BossExplosion.h"

#define ObjId_EHZBoss     0x56
#define ObjId_EHZBossPart 0x5B
#define ObjId_BossExplosion 0x58

// Where the art goes (the boss' PLC, the alpha's $29): the ship at $3A0, the car at $400, its blades at $5E8 (and the explosions' at $580)
#define ArtTile_BossShipEHZ  0x3A0
#define ArtTile_BossCarEHZ   0x400
#define ArtTile_BossBladesEHZ 0x5E8
#define ArtTile_BossExplosion 0x580

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
} Own_Boss;

enum { BossFlag_Landed = 1 << 0, BossFlag_Driving = 1 << 1, BossFlag_Leaving = 1 << 2, BossFlag_Beaten = 1 << 3 };

// A part of the ship (object 58 or, for its cockpit, 55): a new object at the ship's place with the slot of the ship in it
static Sst *MakePart(Sst *boss, uint8_t type, uint8_t routine, const uint8_t *mappings, uint16_t tile, uint8_t width, uint8_t priority) {
    Sst *part = Sst_AllocateAfter(boss);
    if (part == NULL)
        return NULL;
    part->code = sst_object_codes[type];
    part->routine = routine;
    part->mappings = mappings;
    part->art_tile = tile;
    part->render_flags = SST_RENDER_LEVEL;
    part->width_pixels = width;
    part->priority = (uint16_t)(priority * 0x80);
    part->x_pos = boss->x_pos;
    part->y_pos = boss->y_pos;
    ((Own_Boss *)part->own.u8)->link = (uint8_t)(boss - sst);
    return part;
}

// The ship of a part: its slot must still hold an object of the ship's id
static Sst *Ship(const Sst *part, bool *alive) {
    const Own_Boss *own = (const Own_Boss *)part->own.u8;
    Sst *ship = &sst[own->link];
    *alive = own->link != 0 && ship != part && ship->code == sst_object_codes[ObjId_EHZBoss];
    return ship;
}

static void CopyLook(Sst *part, const Sst *ship) {
    part->status = ship->status;
    part->render_flags = ship->render_flags;
}

// Turns at either end of the arena (sub_17A6A)
static void Boss_Turn(Sst *self) {
    int16_t x = self->x_pos.f.u;
    if (x > 0x2720 && x < 0x2B08)
        return;
    self->status ^= SST_STATUS_X_FLIP;
    self->render_flags ^= SST_RENDER_X_FLIP;
    self->x_vel = -self->x_vel;
}

// Hit by Sonic (sub_17A8C): flashes and is safe a while after each hit; beaten when the hits are out
static void EHZ_Hit(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    if (self->routine_secondary >= 6)
        return;
    if (self->status & 0x80) { // the hits are out
        AddPoints(100);
        self->routine_secondary = 6;
        own->countdown = 0xB3;
        own->flags |= BossFlag_Beaten;
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (own->flash == 0) {
        own->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash == 0)
        self->collision_flags = 0xF;
}

// The ship's own business (Obj57)
static void Boss_Ship(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    switch (self->routine_secondary) {
    case 0: // slides in from the right
        self->collision_flags = 0;
        if (self->x_pos.f.u > 0x29D0) {
            self->x_pos.f.u--;
        } else {
            self->x_pos.f.u = 0x29D0;
            self->routine_secondary += 2;
        }
        break;
    case 2: // comes down, waits a second, and sets off
        if (own->step == 0) {
            if (self->y_pos.f.u < 0x41E) {
                self->y_pos.f.u++;
            } else {
                own->step += 2;
                own->flags |= BossFlag_Landed;
                own->timer = 0x3C;
            }
        } else if (--own->timer < 0) {
            self->x_vel = -0x200;
            self->routine_secondary += 2;
            self->collision_flags = 0xF;
            own->flags |= BossFlag_Driving;
        }
        break;
    case 4: // driving to and fro: its height from its wheels
        EHZ_Hit(self);
        Boss_Turn(self);
        self->y_pos.f.u = (int16_t)((own->height >> 1) - 0x14);
        own->height = 0;
        self->x_pos.v += (int32_t)self->x_vel << 8;
        break;
    case 6: // beaten: blowing up
        if (--own->countdown >= 0) {
            Boss_Defeated(self);
            break;
        }
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
        self->status &= 0x7F;
        self->x_vel = 0;
        self->routine_secondary += 2;
        own->countdown = (int16_t)0xFFDA;
        own->timer = 0xC;
        break;
    case 8: // sinks a little
        self->y_pos.f.u++;
        if (--own->timer >= 0) {
            break;
        }
        self->routine_secondary += 2;
        own->step = 0;
        break;
    case 10: // Eggman's car lifts off and drives away
        switch (own->step) {
        case 0: {
            own->flags &= (uint8_t)~BossFlag_Landed;
            Sst *lift = MakePart(self, ObjId_EHZBossPart, 8, Mappings_BossEHZParts, TILE_MAP(0, 1, 0, 0, ArtTile_BossBladesEHZ), 0x20, 4);
            if (lift != NULL) {
                lift->y_pos.f.u += 0xC;
                CopyLook(lift, self);
                lift->anim = 2;
                ((Own_Boss *)lift->own.u8)->timer = 0x10;
                own->timer = 0x32;
                own->step += 2;
            }
            break;
        }
        case 2:
            if (--own->timer < 0) {
                own->flags |= BossFlag_Leaving;
                own->timer = 0x60;
                own->step += 2;
            }
            break;
        case 4:
            self->y_pos.f.u--;
            if (--own->timer >= 0)
                break;
            self->y_pos.f.u++;
            self->x_pos.f.u += 2;
            if ((uint16_t)self->x_pos.f.u >= 0x2B08 && !boss_status) {
                boss_status = 1; // the boss is beaten (Boss_defeated_flag)
                Sst_Delete(self);
                return;
            }
            break;
        }
        break;
    }
}

// Spawns the wheels and the spike (sub_17D9A)
static void Boss_MakeWheels(Sst *self) {
    static const struct { int16_t x; uint8_t priority, frame, anim; int16_t timer; } wheels[3] = {
        { 0x1C, 1, 4, 1, 0x16 },
        { -0xC, 1, 4, 1, 0x4B },
        { -0x2C, 2, 6, 2, 0x30 },
    };
    for (int i = 0; i < 3; i++) {
        Sst *wheel = MakePart(self, ObjId_EHZBossPart, 4, Mappings_BossEHZWheels, TILE_MAP(0, 1, 0, 0, ArtTile_BossCarEHZ), 0x10, wheels[i].priority);
        if (wheel == NULL)
            continue;
        wheel->y_radius = 0x10;
        wheel->x_radius = 0x10;
        wheel->x_pos.f.u += wheels[i].x;
        wheel->y_pos.f.u += 0xC;
        wheel->x_vel = -0x200;
        wheel->mapping_frame = wheels[i].frame;
        wheel->anim = wheels[i].anim;
        ((Own_Boss *)wheel->own.u8)->timer = wheels[i].timer;
    }

    // the spike on its front
    Sst *spike = MakePart(self, ObjId_EHZBossPart, 6, Mappings_BossEHZWheels, TILE_MAP(0, 1, 0, 0, ArtTile_BossCarEHZ), 0x10, 1);
    if (spike != NULL) {
        spike->x_pos.f.u -= 0x36;
        spike->y_pos.f.u += 8;
        spike->mapping_frame = 1;
        spike->anim = 0;
    }
}

// The ship at its start (loc_17F54): the car's parts, and the ship put where it comes in from
static void Boss_Start(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    Sst *body = MakePart(self, ObjId_EHZBossPart, 2, Mappings_BossEHZWheels, TILE_MAP(0, 0, 0, 0, ArtTile_BossCarEHZ), 0x20, 2);
    (void)body;
    Boss_MakeWheels(self);
    own->home_y -= 8;
    self->x_pos.f.u = 0x2A00;
    self->y_pos.f.u = 0x2C0;
    Sst *cockpit = MakePart(self, ObjId_EHZBossPart, 0, Mappings_BossEHZParts, TILE_MAP(0, 1, 0, 0, ArtTile_BossBladesEHZ), 0x20, 4);
    if (cockpit != NULL)
        ((Own_Boss *)cockpit->own.u8)->timer = 0x1E;
}

// The ship (object 55)
static void Boss_Run(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    switch (self->routine) {
    case 0: { // Initialization
        self->mappings = Mappings_BossEHZ;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_BossShipEHZ); // (palette line 2: the alpha's $23A0)
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->priority = 3 * 0x80;
        self->collision_flags = 0xF;
        self->collision_property = 8;
        self->routine += 2;
        own->home_x = self->x_pos.f.u;
        own->home_y = self->y_pos.f.u;

        // Eggman's face and hands: an object that follows the ship
        Sst *face = MakePart(self, ObjId_EHZBoss, 4, Mappings_BossEHZ, TILE_MAP(0, 0, 0, 0, ArtTile_BossShipEHZ), 0x20, 3);
        if (face != NULL) {
            own->link = (uint8_t)(face - sst);
            face->anim = 1;
            face->render_flags = self->render_flags;
            face->render_flags |= SST_RENDER_LEVEL;
        }
        Boss_Start(self);
        break;
    }
    case 2:
        Boss_Ship(self);
        Sst_Animate(self, Animation_BossEHZ);
        self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (self->status & 3));
        Sst_Draw(self);
        break;
    case 4: { // the face: follows the ship
        bool alive;
        Sst *ship = Ship(self, &alive);
        if (!alive) {
            Sst_Delete(self);
            break;
        }
        self->x_pos = ship->x_pos;
        self->y_pos = ship->y_pos;
        CopyLook(self, ship);
        Sst_Animate(self, Animation_BossEHZ);
        Sst_Draw(self);
        break;
    }
    }
}

// The parts of the car (object 58)
static void Boss_Part(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;
    bool alive;
    Sst *ship;

    switch (self->routine) {
    case 0: // the cockpit's glass? (it goes when the car is landed and the ship has stopped)
        switch (self->routine_secondary) {
        case 0:
            ship = Ship(self, &alive);
            if (!alive) {
                Sst_Delete(self);
                return;
            }
            if (((Own_Boss *)ship->own.u8)->flags & BossFlag_Landed) {
                self->anim = 1;
                own->timer = 0x18;
                self->routine_secondary += 2;
            }
            self->x_pos.f.u = ship->x_pos.f.u;
            self->y_pos.f.u = ship->y_pos.f.u;
            CopyLook(self, ship);
            Sst_Animate(self, Animation_BossEHZParts);
            Sst_Draw(self);
            break;
        case 2:
            if (--own->timer >= 0) {
                Sst_Animate(self, Animation_BossEHZParts);
                Sst_Draw(self);
            } else if (own->timer <= -0x10) {
                Sst_Delete(self);
            } else {
                self->y_pos.f.u++;
                Sst_Draw(self);
            }
            break;
        }
        break;
    case 2: // the body of the car: still until it drives, then with the ship (and out of the way when it leaves)
        ship = Ship(self, &alive);
        if (!alive) {
            Sst_Delete(self);
            return;
        }
        if (!(((Own_Boss *)ship->own.u8)->flags & BossFlag_Driving)) {
            Sst_Draw(self);
            return;
        }
        if (((Own_Boss *)ship->own.u8)->flags & BossFlag_Leaving) {
            self->mapping_frame = 8;
            self->priority = 0 * 0x80;
            Sst_Draw(self);
            return;
        }
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = ship->y_pos.f.u + 8;
        CopyLook(self, ship);
        Sst_Draw(self);
        break;
    case 4: // a wheel
        switch (self->routine_secondary) {
        case 0: // waits for the ship to drive
            ship = Ship(self, &alive);
            if (!alive) {
                Sst_Delete(self);
                return;
            }
            if (((Own_Boss *)ship->own.u8)->flags & BossFlag_Driving)
                self->routine_secondary += 2;
            Sst_Draw(self);
            break;
        case 2: // rolls with the ship, on the floor
            ship = Ship(self, &alive);
            if (!alive) {
                Sst_Delete(self);
                return;
            }
            CopyLook(self, ship);
            if (self->status & 0x80)
                self->routine_secondary += 2;
            Boss_Turn(self);
            Sst_MoveSprite(self);
            {
                int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
                if (d1 < 0)
                    self->y_pos.f.u += d1;
            }
            self->y_vel = 0x100;
            if (self->priority == 1 * 0x80) // (the front wheels; priority is in units of $80)
                ((Own_Boss *)ship->own.u8)->height += self->y_pos.f.u;
            Sst_Animate(self, Animation_BossEHZWheels);
            Sst_Draw(self);
            break;
        case 4: // the ship is beaten: the wheel hops off
            if (--own->timer >= 0) {
                Sst_Draw(self);
                break;
            }
            self->routine_secondary += 2;
            own->timer = 0xA;
            self->y_vel = -0x300;
            if (self->priority != 1 * 0x80)
                self->x_vel = -self->x_vel;
            Sst_Draw(self);
            break;
        case 6: // and bounces away
            if (--own->timer >= 0) {
                Sst_Draw(self);
                break;
            }
            Sst_MoveSprite(self);
            {
                int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
                if (d1 < 0) {
                    self->y_vel = -0x200;
                    self->y_pos.f.u += d1;
                }
            }
            SstOnScreenTest(self);
            break;
        }
        break;
    case 6: // the spike on its front
        ship = Ship(self, &alive);
        if (!alive) {
            Sst_Delete(self);
            return;
        }
        if (((Own_Boss *)ship->own.u8)->flags & BossFlag_Beaten) {
            self->x_pos.f.u += (self->status & 1) ? 3 : -3;
            Sst_Animate(self, Animation_BossEHZWheels);
            Sst_Draw(self);
            break;
        }
        // with one hit left, it lunges when Sonic is in front of the ship (sub_17D6A)
        if (ship->collision_property == 1) {
            bool left = self->x_pos.f.u - player->x_pos.f.u >= 0;
            bool flipped = (ship->status & 1) != 0;
            if (left ? !flipped : flipped)
                ((Own_Boss *)ship->own.u8)->flags |= BossFlag_Beaten;
        }
        if (!(((Own_Boss *)ship->own.u8)->flags & BossFlag_Driving)) {
            Sst_Draw(self);
            break;
        }
        self->collision_flags = 0x8B;
        self->x_pos.f.u = ship->x_pos.f.u;
        self->y_pos.f.u = ship->y_pos.f.u + 0x10;
        CopyLook(self, ship);
        self->x_pos.f.u += (self->status & 1) ? 0x36 : -0x36;
        Sst_Animate(self, Animation_BossEHZWheels);
        Sst_Draw(self);
        break;
    case 8: // the lift-off flame: rises and goes
        self->y_pos.f.u--;
        if (--own->timer >= 0) {
            Sst_Draw(self);
            break;
        }
        self->routine = 0;
        Sst_Animate(self, Animation_BossEHZParts);
        Sst_Draw(self);
        break;
    }
}

// An explosion of the wreck (the alpha's object 58): seven frames of seven ticks, with the explosion's sound as it starts
void Obj_BossExplosion(Sst *self) {
    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_BossExplosion;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_BossExplosion);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->collision_flags = 0;
        self->width_pixels = 0xC;
        self->anim_frame_timer = 7;
        self->mapping_frame = 0;
        QueueSound2(sfx_Bomb);
        return;
    }
    if (--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 7;
        if (++self->mapping_frame == 7) {
            Sst_Delete(self);
            return;
        }
    }
    Sst_Draw(self);
}

void Obj_EHZBoss(Sst *self) {
    Boss_Run(self);
}

void Obj_EHZBossPart(Sst *self) {
    Boss_Part(self);
}
