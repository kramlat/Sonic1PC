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

// A part of the ship: a new object at the ship's place with the slot of the ship in it. The pre-beta makes every part with the ship's own id and a routine of its own; here the parts have their own id
// ($5B) and the same routine numbers: 4 the blades, 6 the body of the car, 8 a wheel, $A the spike, $C the blades lifting off, $E Eggman's face
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

// Where the arena's left edge has been put by the level's events ($28F0 in the pre-beta: the parts wait for it before they come in)
static bool ArenaIsSet(void) {
    return limit_left2 >= 0x28F0 - SCREEN_WIDEADD2;
}

// Turns at either end of the arena (Offset_0x02ABFA)
static void Boss_Turn(Sst *self) {
    int16_t x = self->x_pos.f.u;
    if (x > 0x28A0 && x < 0x2B08)
        return;
    self->status ^= SST_STATUS_X_FLIP;
    self->render_flags ^= SST_RENDER_X_FLIP;
    self->x_vel = -self->x_vel;
}

// Hit by Sonic (Offset_0x02AC1C): flashes and is safe a while after each hit; beaten when the hits are out
static void EHZ_Hit(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    if (self->routine_secondary >= 6)
        return;
    if (self->status & 0x80) { // the hits are out
        AddPoints(100);
        self->routine_secondary = 6;
        self->x_vel = 0;
        self->y_vel = -0x180;
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

// The ship's own business (Offset_0x02A9E0)
static void Boss_Ship(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    switch (self->routine_secondary) {
    case 0: // waits for the arena, then slides in from the right (and down)
        if (!ArenaIsSet())
            break;
        self->collision_flags = 0;
        if (self->x_pos.f.u > 0x29D0) {
            self->x_pos.f.u--;
            self->y_pos.f.u++;
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
    case 6: // beaten: blowing up, and falling to the floor
        if (--own->countdown >= 0) {
            Boss_Defeated(self);
            Sst_MoveSprite(self);
            {
                int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
                if (d1 < 0) {
                    self->y_pos.f.u += d1;
                    self->y_vel = 0;
                }
            }
            break;
        }
        self->x_vel = 0;
        self->routine_secondary += 2;
        own->countdown = (int16_t)0xFFDA;
        own->timer = 0xC;
        break;
    case 8: // a moment
        if (--own->timer >= 0)
            break;
        self->routine_secondary += 2;
        own->step = 0;
        break;
    case 10: // Eggman's car lifts off and drives away
        switch (own->step) {
        case 0: {
            own->flags &= (uint8_t)~BossFlag_Landed;
            Sst *lift = MakePart(self, ObjId_EHZBossPart, 12, Mappings_BossEHZParts, TILE_MAP(0, 1, 0, 0, ArtTile_BossBladesEHZ), 0x20, 3);
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
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            self->y_pos.f.u++;
            self->x_pos.f.u += 2;
            if ((uint16_t)self->x_pos.f.u >= 0x2B08 && !boss_status)
                boss_status = 1; // the boss is beaten (Boss_defeated_flag)
            break;
        }
        if (boss_status) {
            Sst_Delete(self);
            return;
        }
        break;
    }
}

// Spawns the wheels and the spike (Offset_0x02A816 on): they are made at the arena's right edge and come in each to its place when the arena is set
static void Boss_MakeWheels(Sst *self) {
    static const struct { int16_t x; uint8_t priority, frame, anim; } wheels[3] = {
        { 0x1C, 2, 4, 1 },
        { -0xC, 2, 4, 1 },
        { -0x2C, 3, 6, 2 },
    };
    for (int i = 0; i < 3; i++) {
        Sst *wheel = MakePart(self, ObjId_EHZBossPart, 8, Mappings_BossEHZWheels, TILE_MAP(0, 1, 0, 0, ArtTile_BossCarEHZ), 0x10, wheels[i].priority);
        if (wheel == NULL)
            continue;
        wheel->y_radius = 0x10;
        wheel->x_radius = 0x10;
        wheel->x_pos.f.u = (int16_t)(0x2AA0 + wheels[i].x);
        wheel->y_pos.f.u += 0xC;
        wheel->mapping_frame = wheels[i].frame;
        wheel->anim = wheels[i].anim;
        wheel->subtype = (uint8_t)i;
        ((Own_Boss *)wheel->own.u8)->timer = 0xA;
    }

    // the spike on its front
    Sst *spike = MakePart(self, ObjId_EHZBossPart, 10, Mappings_BossEHZWheels, TILE_MAP(0, 1, 0, 0, ArtTile_BossCarEHZ), 0x10, 2);
    if (spike != NULL) {
        spike->x_pos.f.u = 0x2AA0 - 0x36;
        spike->y_pos.f.u += 8;
        spike->mapping_frame = 1;
        spike->anim = 0;
    }
}

// The ship at its start (Offset_0x02A6C6): Eggman's face, the body of the car with its wheels and spike, and the blades; the ship put where it comes in from
static void Boss_Start(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    Sst *face = MakePart(self, ObjId_EHZBossPart, 14, Mappings_BossEHZ, TILE_MAP(0, 0, 0, 0, ArtTile_BossShipEHZ), 0x20, 4);
    if (face != NULL) {
        own->link = (uint8_t)(face - sst);
        face->anim = 1;
        face->render_flags = self->render_flags;
    }
    Sst *body = MakePart(self, ObjId_EHZBossPart, 6, Mappings_BossEHZWheels, TILE_MAP(0, 0, 0, 0, ArtTile_BossCarEHZ), 0x20, 3);
    if (body != NULL)
        body->x_pos.f.u = 0x2AA0;
    Boss_MakeWheels(self);
    own->home_y -= 8;
    self->x_pos.f.u = 0x2AA0;
    self->y_pos.f.u = 0x348;
    Sst *blades = MakePart(self, ObjId_EHZBossPart, 4, Mappings_BossEHZParts, TILE_MAP(0, 1, 0, 0, ArtTile_BossBladesEHZ), 0x20, 3);
    if (blades != NULL)
        ((Own_Boss *)blades->own.u8)->timer = 0x1E;
}

// The ship (Offset_0x02A6C6 on)
static void Boss_Run(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        self->mappings = Mappings_BossEHZ;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_BossShipEHZ); // (palette line 2: $23A0)
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->y_radius = 0x14;
        self->priority = 4 * 0x80;
        self->collision_flags = 0xF;
        self->collision_property = 8;
        self->routine += 2;
        own->home_x = self->x_pos.f.u;
        own->home_y = self->y_pos.f.u;
        Boss_Start(self);
        break;
    case 2:
        Boss_Ship(self);
        if (self->code != NULL) { // (it is gone when it has left)
            self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (self->status & 3));
            Sst_Draw(self);
        }
        break;
    }
}

// The parts of the car (the pre-beta's routines 4 to $E of the boss's object)
static void Boss_Part(Sst *self) {
    Own_Boss *own = (Own_Boss *)self->own.u8;
    bool alive;
    Sst *ship;

    switch (self->routine) {
    case 4: // the blades (waiting, they turn; when the car has landed they stop, and go a moment later)
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
                self->priority = 4 * 0x80;
                self->y_pos.f.u++;
                Sst_Draw(self);
            }
            break;
        }
        break;
    case 6: // the body of the car: comes in, waits, then with the ship (and left where it is when the ship leaves)
        switch (self->routine_secondary) {
        case 0:
            if (!ArenaIsSet()) {
                Sst_Draw(self);
                break;
            }
            if (self->x_pos.f.u > 0x29D0) {
                self->x_pos.f.u--;
            } else {
                self->x_pos.f.u = 0x29D0;
                self->routine_secondary += 2;
            }
            Sst_Draw(self);
            break;
        default:
            ship = Ship(self, &alive);
            if (!alive) {
                Sst_Delete(self);
                return;
            }
            {
                const uint8_t flags = ((Own_Boss *)ship->own.u8)->flags;
                if ((flags & BossFlag_Driving) && !(flags & BossFlag_Leaving)) {
                    self->x_pos.f.u = ship->x_pos.f.u;
                    self->y_pos.f.u = ship->y_pos.f.u + 8;
                    self->status = ship->status;
                    if (!(self->status & 0x80))
                        self->render_flags = ship->render_flags;
                }
            }
            Sst_Draw(self);
            break;
        }
        break;
    case 8: // a wheel
        switch (self->routine_secondary) {
        case 0: { // comes in to its place, and falls to the floor
            if (!ArenaIsSet()) {
                Sst_Draw(self);
                break;
            }
            static const int16_t targets[3] = { 0x29EC, 0x29C4, 0x29A4 };
            self->y_vel = 0x100;
            if (self->x_pos.f.u > targets[self->subtype < 3 ? self->subtype : 2]) {
                self->x_pos.f.u--;
            } else {
                self->x_pos.f.u = targets[self->subtype < 3 ? self->subtype : 2];
                self->routine_secondary += 2;
            }
            Sst_MoveSprite(self);
            {
                int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
                if (d1 < 0)
                    self->y_pos.f.u += d1;
            }
            if (self->routine_secondary != 0)
                self->x_vel = -0x200;
            Sst_Animate(self, Animation_BossEHZWheels);
            Sst_Draw(self);
            break;
        }
        case 2: // waits for the ship to drive
            ship = Ship(self, &alive);
            if (!alive) {
                Sst_Delete(self);
                return;
            }
            if (((Own_Boss *)ship->own.u8)->flags & BossFlag_Driving)
                self->routine_secondary += 2;
            Sst_Draw(self);
            break;
        case 4: // rolls with the ship, on the floor
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
            if (self->priority == 2 * 0x80) // (the two front wheels)
                ((Own_Boss *)ship->own.u8)->height += self->y_pos.f.u;
            Sst_Animate(self, Animation_BossEHZWheels);
            Sst_Draw(self);
            break;
        case 6: // the ship is beaten: the wheel waits a moment, then hops off
            if (--own->timer >= 0) {
                Sst_Draw(self);
                break;
            }
            self->routine_secondary += 2;
            own->timer = 0xA;
            self->y_vel = -0x300;
            if (self->priority != 2 * 0x80)
                self->x_vel = -self->x_vel;
            Sst_Draw(self);
            break;
        case 8: // and bounces away
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
    case 10: // the spike on its front
        if (self->routine_secondary == 0) { // comes in
            if (!ArenaIsSet()) {
                Sst_Draw(self);
                break;
            }
            if (self->x_pos.f.u > 0x299A) {
                self->x_pos.f.u--;
            } else {
                self->x_pos.f.u = 0x299A;
                self->routine_secondary += 2;
            }
            Sst_Draw(self);
            break;
        }
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
        // with one hit left, it lunges when Sonic is in front of the ship (Offset_0x02AFDA)
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
    case 12: // the lift-off blades: rise, then are the blades that follow the ship
        self->y_pos.f.u--;
        if (--own->timer >= 0) {
            Sst_Draw(self);
            break;
        }
        self->routine = 4;
        Sst_Animate(self, Animation_BossEHZParts);
        Sst_Draw(self);
        break;
    case 14: // Eggman's face: follows the ship
        ship = Ship(self, &alive);
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
