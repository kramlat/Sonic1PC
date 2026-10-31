// Emerald Hill's badniks for Sonic 2 (Nick Arcade's objects 4B, 53 and 54): the Buzzer, the Masher and the Snail. Each is its own object with the parts it makes (the Buzzer's flame and shots, the Snail's
// head and flame) as objects of the same id in other routines, which find their parent by its slot (an index, checked every frame: a parent that has gone, or been replaced, ends them).
#include "Object/EHZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
#define ArtTile_Buzzer 0x3D2 // (the alpha's list: Buzzer.nem at $7A40)
#define ArtTile_Snail  0x402 // (no Snail in the alpha: the prototype's place for it)
#define ArtTile_Masher 0x414 // (masher.nem at $8280)

// A part of another object: a new object of the same id (the slot after the parent's, as the original allocates), put to the routine given, at the parent's place and with its flips
static Sst *MakePart(Sst *parent, uint8_t routine, const uint8_t *mappings, uint16_t tile, uint8_t priority) {
    Sst *part = Sst_AllocateAfter(parent);
    if (part == NULL)
        return NULL;
    part->code = parent->code;
    part->routine = routine;
    part->mappings = mappings;
    part->art_tile = tile;
    part->render_flags = parent->render_flags;
    part->render_flags |= SST_RENDER_LEVEL;
    part->priority = (uint16_t)(priority * 0x80);
    part->width_pixels = 0x10;
    part->status = parent->status;
    part->x_pos.f.u = parent->x_pos.f.u;
    part->y_pos.f.u = parent->y_pos.f.u;
    return part;
}

// The parent of a part: its slot has to still hold an object of the same id (and be a different one than the part itself)
static Sst *Parent(const Sst *part, uint8_t slot, bool *alive) {
    Sst *parent = &sst[slot];
    *alive = slot != 0 && parent != part && parent->code == part->code;
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
} Own_Buzzer;

enum { BuzzerRoutine_Init = 0, BuzzerRoutine_Main = 2, BuzzerRoutine_Flame = 4, BuzzerRoutine_Shot = 6 };
enum { BuzzerAnim_Moving, BuzzerAnim_Flame, BuzzerAnim_Bullet, BuzzerAnim_Shoot };

// Sonic in a strip 40 to 48 pixels in front of it makes it shoot (Buzzer_ChkPlayers)
static void Buzzer_ChkPlayer(Sst *self, Own_Buzzer *own) {
    if (own->shot_made)
        return;
    int16_t d1 = self->x_pos.f.u - player->x_pos.f.u;
    int16_t d0 = d1 < 0 ? -d1 : d1;
    if (d0 < 0x28 || d0 > 0x30)
        return;
    if (d1 >= 0) { // Sonic is at its left
        if (((self->render_flags & SST_RENDER_X_FLIP) != 0))
            return;
    } else {
        if (!((self->render_flags & SST_RENDER_X_FLIP) != 0))
            return;
    }
    own->shot_made = 0xFF;
    self->routine_secondary += 2;
    self->anim = BuzzerAnim_Shoot;
    own->shot_timer = 0x32;
}

static void Buzzer_Shoot(Sst *self) {
    Sst *shot = MakePart(self, BuzzerRoutine_Shot, Mappings_Buzzer, ArtTile_Buzzer, 4);
    if (shot == NULL)
        return;
    shot->collision_flags = 0x98;
    shot->anim = BuzzerAnim_Bullet;
    shot->y_pos.f.u += 0x18; // (from its stinger)
    shot->y_vel = 0x180;
    shot->x_vel = -0x180;
    int16_t reach = 0xD;
    if (((shot->render_flags & SST_RENDER_X_FLIP) != 0)) {
        shot->x_vel = -shot->x_vel;
        reach = -reach;
    }
    shot->x_pos.f.u += reach;
}

void Obj_Buzzer(Sst *self) {
    Own_Buzzer *own = (Own_Buzzer *)self->own.u8;

    switch (self->routine) {
    case BuzzerRoutine_Init: {
        self->mappings = Mappings_Buzzer;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Buzzer);
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0xA;
        self->width_pixels = 0x10;
        self->y_radius = 0x10;
        self->x_radius = 0x18;
        self->priority = 3 * 0x80;
        self->routine += 2;

        Sst *flame = MakePart(self, BuzzerRoutine_Flame, Mappings_Buzzer, ArtTile_Buzzer, 4);
        if (flame == NULL)
            break;
        flame->anim = BuzzerAnim_Flame;
        ((Own_Buzzer *)flame->own.u8)->parent = (uint8_t)(self - sst);
        own->move_timer = 0x100;
        self->x_vel = -0x100;
        if (((self->render_flags & SST_RENDER_X_FLIP) != 0))
            self->x_vel = -self->x_vel;
        break;
    }
    case BuzzerRoutine_Main:
        if (self->routine_secondary == 0) { // roaming
            Buzzer_ChkPlayer(self, own);
            own->turn_delay--;
            if (own->turn_delay == 0xF) { // turns around
                own->shot_made = 0;
                self->x_vel = -self->x_vel;
                self->render_flags ^= SST_RENDER_X_FLIP;
                self->status ^= SST_STATUS_X_FLIP;
                own->move_timer = 0x100;
            } else if (own->turn_delay < 0) {
                if (--own->move_timer > 0)
                    Sst_MoveSprite2(self);
                else
                    own->turn_delay = 0x1E;
            }
        } else { // shooting
            int16_t timer = own->shot_timer - 1;
            if (timer < 0) {
                self->routine_secondary -= 2;
            } else {
                own->shot_timer = timer;
                if (timer == 0x14)
                    Buzzer_Shoot(self);
            }
        }
        Sst_Animate(self, Animation_Buzzer);
        SstOnScreenTest(self);
        break;
    case BuzzerRoutine_Flame: {
        bool alive;
        Sst *parent = Parent(self, own->parent, &alive);
        if (!alive) {
            Sst_Delete(self);
            break;
        }
        if (((Own_Buzzer *)parent->own.u8)->turn_delay >= 0)
            break; // (not shown while it turns)
        self->x_pos.f.u = parent->x_pos.f.u;
        self->y_pos.f.u = parent->y_pos.f.u;
        self->status = parent->status;
        self->render_flags = parent->render_flags;
        Sst_Animate(self, Animation_Buzzer);
        SstOnScreenTest(self);
        break;
    }
    case BuzzerRoutine_Shot:
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_Buzzer);
        SstOnScreenTest(self);
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
} Own_Masher;

enum { MasherAnim_Rising, MasherAnim_Chomping, MasherAnim_Falling };

void Obj_Masher(Sst *self) {
    Own_Masher *own = (Own_Masher *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_Masher;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Masher);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->collision_flags = 9;
        self->width_pixels = 0x10;
        self->y_vel = -0x400;
        own->origin_y = self->y_pos.f.u;
    }

    Sst_Animate(self, Animation_Masher);
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;
    uint16_t home = (uint16_t)own->origin_y;
    if (home < (uint16_t)self->y_pos.f.u) { // back at the water: jumps again
        self->y_pos.f.u = (int16_t)home;
        self->y_vel = -0x500;
    }
    self->anim = MasherAnim_Chomping;
    if ((uint16_t)(home - 0xC0) < (uint16_t)self->y_pos.f.u) {
        self->anim = MasherAnim_Rising;
        if (self->y_vel >= 0)
            self->anim = MasherAnim_Falling;
    }
    SstOnScreenTest(self);
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
} Own_Snail;

enum { SnailRoutine_Init = 0, SnailRoutine_Move = 2, SnailRoutine_Turn = 4, SnailRoutine_Head = 6, SnailRoutine_Flame = 8 };

static void Snail_CreateFlame(Sst *self) {
    Sst *flame = MakePart(self, SnailRoutine_Flame, Mappings_Buzzer, ArtTile_Buzzer, 4);
    if (flame == NULL)
        return;
    ((Own_Snail *)flame->own.u8)->parent = (uint8_t)(self - sst);
    flame->y_pos.f.u += 7;
    flame->x_pos.f.u += 0xD;
    flame->anim = BuzzerAnim_Flame;
}

// It notices Sonic within 100 pixels in front of it, and charges (four times as fast, with a flame)
static void Snail_Charge(Sst *self, Own_Snail *own) {
    if (own->charging)
        return;
    int16_t d0 = player->x_pos.f.u - self->x_pos.f.u;
    if (d0 > 100 || d0 < -100)
        return;
    if (d0 < 0) {
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            return;
    } else {
        if (!((self->status & SST_STATUS_X_FLIP) != 0))
            return;
    }
    self->x_vel <<= 2;
    own->charging = 0xFF;
    Snail_CreateFlame(self);
}

void Obj_Snail(Sst *self) {
    Own_Snail *own = (Own_Snail *)self->own.u8;

    switch (self->routine) {
    case SnailRoutine_Init: {
        self->mappings = Mappings_Snail;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Snail);
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0xA;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->y_radius = 0x10;
        self->x_radius = 0xE;

        Sst *head = MakePart(self, SnailRoutine_Head, Mappings_Snail, TILE_MAP(0, 1, 0, 0, ArtTile_Snail), 3);
        if (head != NULL) {
            ((Own_Snail *)head->own.u8)->parent = (uint8_t)(self - sst);
            head->mapping_frame = 2;
        }
        self->routine += 2;
        self->x_vel = ((self->status & SST_STATUS_X_FLIP) != 0) ? 0x80 : -0x80;
        break;
    }
    case SnailRoutine_Move: {
        Snail_Charge(self, own);
        Sst_MoveSprite2(self);
        int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < -8 || d1 >= 0xC) { // the floor ends: turns
            self->routine += 2;
            own->timer = 0x14;
            own->flame_ends = 0xFF;
        } else {
            self->y_pos.f.u += d1;
        }
        Sst_Animate(self, Animation_Snail);
        SstOnScreenTest(self);
        break;
    }
    case SnailRoutine_Turn:
        if (--own->timer >= 0) {
            SstOnScreenTest(self);
            break;
        }
        self->x_vel = -self->x_vel;
        Sst_MoveSprite(self);
        self->x_vel >>= 2;
        self->status ^= SST_STATUS_X_FLIP;
        self->render_flags ^= SST_RENDER_X_FLIP;
        self->routine -= 2;
        own->flame_ends = 0;
        own->charging = 0;
        SstOnScreenTest(self);
        break;
    case SnailRoutine_Head: {
        bool alive;
        Sst *parent = Parent(self, own->parent, &alive);
        if (!alive) {
            Sst_Delete(self);
            break;
        }
        self->x_pos.f.u = parent->x_pos.f.u;
        self->y_pos.f.u = parent->y_pos.f.u;
        self->status = parent->status;
        self->render_flags = parent->render_flags;
        SstOnScreenTest(self);
        break;
    }
    case SnailRoutine_Flame: {
        bool alive;
        Sst *parent = Parent(self, own->parent, &alive);
        if (!alive || ((Own_Snail *)parent->own.u8)->flame_ends) {
            Sst_Delete(self);
            break;
        }
        self->x_pos.f.u = parent->x_pos.f.u;
        self->y_pos.f.u = parent->y_pos.f.u + 7;
        self->x_pos.f.u += ((self->status & SST_STATUS_X_FLIP) != 0) ? -0xD : 0xD;
        Sst_Animate(self, Animation_Buzzer);
        SstOnScreenTest(self);
        break;
    }
    }
}
