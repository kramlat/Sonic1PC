// Object 28 of the alpha (Aug 21st 1992): the animals that jump out of a destroyed badnik. It is Sonic 1's (Sonic 1's Animal.c) with twelve kinds of animal, a pair for each zone (the alpha's Offset_0x00A42A),
// the speeds and frames of each, and the routine of each kind of movement by number as the alpha numbers them: its own table, not Sonic 1's (where the routines of the end of a level are 18 and up).
#include "Object/Animals.h"
void Obj_Points(Sst *self);
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "SstLoad.h"
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
    return Mappings_Animals + (size_t)index * 0x1E; // (five sets of three frames, $1E bytes each in Sonic 3's format: the build converts the alpha's $24 of Sonic 2's, see mappings_SonicTheHedgehog2)
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

static bool IsOnScreen(const Sst *self) {
    return (self->render_flags & 0x80) != 0;
}

// Offset_0x00A734: the one set free goes when it is off the screen and in the stretch ahead of the player
static void Gone(Sst *self) {
    const uint16_t ahead = (uint16_t)(self->x_pos.f.u - player->x_pos.f.u);
    if (self->x_pos.f.u >= player->x_pos.f.u && ahead < 0x180 && !IsOnScreen(self)) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// Offset_0x00A914: how far the player is, less $B8; the alpha tests it by its sign (Reach) and the flickies by the carry (PlayerClose)
static int16_t Reach(const Sst *self) {
    return (int16_t)(player->x_pos.f.u - self->x_pos.f.u - 0xB8);
}

static bool PlayerClose(const Sst *self) {
    return (uint16_t)(player->x_pos.f.u - self->x_pos.f.u) < 0xB8;
}

// Offset_0x00A8D4: the frame, and the landing, of a jump
static void JumpFrame(Sst *self, const Own_Flicky *a) {
    self->mapping_frame = 1;
    if (self->y_vel < 0)
        return;
    self->mapping_frame = 0;
    const int16_t d = SstFloorDist(self, self->x_pos.f.u);
    if (d < 0) {
        self->y_pos.f.u += d;
        self->y_vel = a->ysp;
    }
}

// Offset_0x00A8FC: it turns to the player
static void FacePlayer(Sst *self) {
    self->render_flags |= 1;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u)
        self->render_flags &= (uint8_t)~1;
}

static void Flap(Sst *self) {
    if (--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 1;
        self->mapping_frame = (self->mapping_frame + 1) & 1;
    }
}

static void Turn(Sst *self) {
    self->x_vel = -self->x_vel;
    self->render_flags ^= 1;
}

// A bounce that lands (A7E6, A880): it turns round every other time
static void BounceTurn(Sst *self, Own_Flicky *a, int16_t d) {
    a->flip_flag = (uint8_t)~a->flip_flag;
    if (!a->flip_flag)
        Turn(self);
    self->y_pos.f.u += d;
    self->y_vel = a->ysp;
}

static void Init(Sst *self, Own_Flicky *a) {
    self->x_radius = 0xC;
    self->render_flags = 4;
    self->render_flags |= 1;
    self->priority = 6 * 0x80;
    self->width_pixels = 8;
    self->anim_frame_timer = 7;

    if (self->subtype != 0) { // the end of a level: the kind of movement is the subtype
        const unsigned i = (unsigned)self->subtype - 10;
        self->routine = (uint8_t)(self->subtype * 2);
        if (i >= 11) {
            Sst_Delete(self);
            return;
        }
        self->art_tile = TILE_MAP(0, 0, 0, 0, end_animals[i].tile);
        self->mappings = FrameSet(end_animals[i].frames);
        a->xsp = self->x_vel = end_animals[i].xsp;
        a->ysp = self->y_vel = end_animals[i].ysp;
        Sst_Draw(self);
        return;
    }

    self->routine += 2; // from a badnik: one of the zone's two
    const unsigned pick = RandomNumber() & 1;
    const unsigned zone = LEVEL_ZONE(level_id);
    self->art_tile = TILE_MAP(0, 0, 0, 0, pick ? ANIMAL_TILE_2 : ANIMAL_TILE_1);
    a->kind = zone_animals[zone < 17 ? zone : 0][pick];
    a->xsp = kinds[a->kind].xsp;
    a->ysp = kinds[a->kind].ysp;
    self->mappings = FrameSet(kinds[a->kind].frames);
    self->mapping_frame = 2;
    self->y_vel = -0x400;

    if (boss_status) { // after a boss they wait where they are
        self->routine = 0x1C;
        self->x_vel = 0;
    } else {
        Sst *points = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
        if (points != NULL) {
            points->mapping_frame = (uint8_t)(a->points >> 1);
        }
    }
    Sst_Draw(self);
}

// Offset_0x00A63A: falling out of the badnik until it lands, and then moving off as its kind does
static void Land(Sst *self, Own_Flicky *a) {
    if (!IsOnScreen(self)) {
        Sst_Delete(self);
        return;
    }
    Sst_MoveSprite(self);
    if (self->y_vel >= 0) {
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->y_pos.f.u += d;
            self->x_vel = a->xsp;
            self->y_vel = a->ysp;
            self->mapping_frame = 1;
            self->routine = (uint8_t)(a->kind * 2 + 4);
            if (boss_status && (frame_count & 0x10))
                Turn(self);
        }
    }
    Sst_Draw(self);
}

// Offset_0x00A694: running along, jumping when it lands
static void Walk(Sst *self, Own_Flicky *a) {
    Sst_MoveSprite(self);
    self->mapping_frame = 1;
    if (self->y_vel >= 0) {
        self->mapping_frame = 0;
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->y_pos.f.u += d;
            self->y_vel = a->ysp;
        }
    }
    if (self->subtype != 0) {
        Gone(self);
    } else if (IsOnScreen(self)) {
        Sst_Draw(self);
    } else {
        Sst_Delete(self);
    }
}

// Offset_0x00A6D0: flying, flapping as it goes
static void Fly(Sst *self, Own_Flicky *a) {
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;
    if (self->y_vel >= 0) {
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->y_pos.f.u += d;
            self->y_vel = a->ysp;
            if (self->subtype != 0 && self->subtype != 0x0A)
                Turn(self);
        }
    }
    Flap(self);
    if (self->subtype != 0)
        Gone(self);
    else if (IsOnScreen(self))
        Sst_Draw(self);
    else
        Sst_Delete(self);
}

void Obj_FlickyAnimals(Sst *self) {
    Own_Flicky *a = (Own_Flicky *)self->own.u8;
    const unsigned index = self->routine >> 1;
    if (index >= sizeof(routine_kinds)) {
        Sst_Delete(self);
        return;
    }

    switch (routine_kinds[index]) {
    case K_INIT:
        Init(self, a);
        break;
    case K_LAND:
        Land(self, a);
        break;
    case K_WALK:
        Walk(self, a);
        break;
    case K_FLY:
        Fly(self, a);
        break;
    case K_WAIT: // (Offset_0x00A750: they wait where they are, after a boss, until the capsule counts down)
        if (!IsOnScreen(self)) {
            Sst_Delete(self);
            return;
        }
        if (--a->timer == 0) {
            self->routine = 2;
            self->priority = 3 * 0x80;
        }
        Sst_Draw(self);
        break;
    case K_FLICKY_WAIT: // (they fly off when the player is close)
        if (PlayerClose(self)) {
            self->x_vel = a->xsp;
            self->y_vel = a->ysp;
            self->routine = 0xE;
            Fly(self, a);
        } else {
            Gone(self);
        }
        break;
    case K_FLICKY_JUMP: // (it jumps on the spot, facing the player, when he is near)
        if (Reach(self) < 0) {
            self->x_vel = 0;
            a->xsp = 0;
            Sst_MoveSprite2(self);
            self->y_vel += 0x18;
            JumpFrame(self, a);
            FacePlayer(self);
            Flap(self);
        }
        Gone(self);
        break;
    case K_RABBIT_WAIT: // (it runs when the player is near)
        if (Reach(self) < 0) {
            self->x_vel = a->xsp;
            self->y_vel = a->ysp;
            self->routine = 4;
            Walk(self, a);
        } else {
            Gone(self);
        }
        break;
    case K_DOUBLE_BOUNCE: // (Offset_0x00A7E6: no check of the player's nearness)
        Sst_MoveSprite(self);
        self->mapping_frame = 1;
        if (self->y_vel >= 0) {
            self->mapping_frame = 0;
            const int16_t d = SstFloorDist(self, self->x_pos.f.u);
            if (d < 0)
                BounceTurn(self, a, d);
        }
        Gone(self);
        break;
    case K_LAND_JUMP: // (it jumps in place once the player is near)
        if (Reach(self) < 0) {
            self->x_vel = 0;
            a->xsp = 0;
            Sst_MoveSprite(self);
            JumpFrame(self, a);
            FacePlayer(self);
        }
        Gone(self);
        break;
    case K_SINGLE_BOUNCE:
        if (Reach(self) < 0) {
            Sst_MoveSprite(self);
            self->mapping_frame = 1;
            if (self->y_vel >= 0) {
                self->mapping_frame = 0;
                const int16_t d = SstFloorDist(self, self->x_pos.f.u);
                if (d < 0) {
                    Turn(self);
                    self->y_pos.f.u += d;
                    self->y_vel = a->ysp;
                }
            }
        }
        Gone(self);
        break;
    case K_FLY_BOUNCE:
        if (Reach(self) < 0) {
            Sst_MoveSprite2(self);
            self->y_vel += 0x18;
            if (self->y_vel >= 0) {
                const int16_t d = SstFloorDist(self, self->x_pos.f.u);
                if (d < 0)
                    BounceTurn(self, a, d);
            }
            Flap(self);
        }
        Gone(self);
        break;
    }
}
