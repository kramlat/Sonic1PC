// Neo Green Hill's pillars of the Sonic 2 pre-beta (September 14, 1992), drawn from the level's own tiles: object 23 (a pillar that drops its lower part when a character comes near), 2B (a pillar that
// rises when a character comes near and breaks into pieces when he stands on it) and 82 (a pillar that falls, rises or follows the water, by its subtype). Each is a port of its routine in the
// disassembly (Data/Objects/Obj_0x23.asm, Obj_0x2B.asm, Obj_0x82.asm).
#include "Object/NGHZPillars.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Mappings.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/NGHZFallingPillar.h"
#include "Resource/Mappings/NGHZPillar.h"
#include "Resource/Mappings/NGHZRisingPillar.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// Within the distance along x of a character (the pre-beta's debug mode switches the test off)
static bool Pillar_CharacterNear(const Sst *self, int16_t distance) {
    if (debug_use)
        return false;
    FOR_EACH_CHARACTER(chr, who) {
        int16_t d0 = (int16_t)(self->x_pos.f.u - chr->x_pos.f.u);
        if (d0 < 0)
            d0 = (int16_t)-d0;
        if (d0 < distance)
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 23: the pillar. The upper part stands (frame 0), the lower part (a second object, frame 1) shakes for 8 frames when a character comes within $80, falls to the floor and stays there (frame 2)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad0[7];
    int16_t base_x; // 0x30: where it shakes about
    int16_t rest_y; // 0x32: where it landed
    int16_t timer;  // 0x34: the shake's frames left
} Own_Pillar;

enum { PillarState_Still = 0, PillarState_Wait = 2, PillarState_Shake = 4, PillarState_Fall = 6 };

// Offset_0x0203B8
static const int8_t pillar_shake[8] = { 0, 1, -1, 1, 0, -1, 0, 1 };

static void Pillar_Lower(Sst *self, Own_Pillar *own) {
    switch (self->routine_secondary) {
    case PillarState_Wait:
        if (Pillar_CharacterNear(self, 0x80)) {
            self->routine_secondary = PillarState_Shake;
            own->timer = 8;
        }
        break;

    case PillarState_Shake:
        if (--own->timer < 0) {
            own->timer = 0;
            self->routine_secondary += 2;
        } else {
            self->x_pos.f.u = (int16_t)(own->base_x + pillar_shake[own->timer]);
        }
        break;

    case PillarState_Fall: {
        Sst_MoveSprite2(self);
        self->y_vel += 0x38;
        const int16_t dist = SstFloorDist(self, self->x_pos.f.u);
        if (dist < 0) {
            self->y_pos.f.u += dist;
            self->y_vel = 0;
            own->rest_y = self->y_pos.f.u;
            self->mapping_frame = 2;
            self->routine_secondary = PillarState_Still;
        }
        break;
    }

    default:
        break;
    }
}

void Obj_NGHZPillar(Sst *self) {
    Own_Pillar *own = (Own_Pillar *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_NGHZPillar;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->y_radius = 0x20;
        self->priority = 4 * 0x80;
        Sst *lower = Sst_AllocateAfter(self);
        if (lower != NULL) {
            Own_Pillar *lower_own = (Own_Pillar *)lower->own.u8;
            lower->code = self->code;
            lower->routine = 2; // (it does not set itself up: this has)
            lower->routine_secondary = PillarState_Wait;
            lower->mappings = self->mappings;
            lower->art_tile = self->art_tile;
            lower->x_pos.f.u = self->x_pos.f.u;
            lower_own->base_x = self->x_pos.f.u;
            lower->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x30);
            lower->render_flags = self->render_flags;
            lower->width_pixels = 0x10;
            lower->y_radius = 0x10;
            lower->priority = 4 * 0x80;
            lower->mapping_frame = 1;
        }
    }

    const int16_t old_x = self->x_pos.f.u;
    Pillar_Lower(self, own);
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2B: the rising pillar. A character within $40 makes it rise six steps of 4 pixels (one every 4 frames: its frames 0 to 6 are taller each); if a character stands on it, it breaks into the
// pieces of its frame 7 to 13 (up to 14 of them, thrown out by the speeds of a table and held a few frames each) and lets him go, rolling, into the air
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad0[11];
    int16_t timer; // 0x34: the frames left of the step
    uint8_t pad1[9];
    uint8_t delay; // 0x3F: a piece's frames before it falls
} Own_RisingPillar;

enum { RisingState_Wait = 0, RisingState_Rise = 2, RisingState_Done = 4 };

// Offset_0x0205B6: the frames each piece waits before it falls
static const uint8_t rising_delays[14] = { 0, 0, 0, 0, 4, 4, 8, 8, 12, 12, 16, 16, 20, 20 };
// Offset_0x0205C4: the speed of each piece
static const int16_t rising_speeds[14][2] = {
    { -0x200, -0x200 }, { 0x200, -0x200 }, { -0x1C0, -0x1C0 }, { 0x1C0, -0x1C0 }, { -0x180, -0x180 }, { 0x180, -0x180 }, { -0x140, -0x140 },
    { 0x140, -0x140 },  { -0x100, -0x100 }, { 0x100, -0x100 },  { -0xC0, -0xC0 },  { 0xC0, -0xC0 },   { -0x80, -0x80 },  { 0x80, -0x80 },
};

static void RisingPillar_Rise(Sst *self, Own_RisingPillar *own) {
    switch (self->routine_secondary) {
    case RisingState_Wait:
        if (Pillar_CharacterNear(self, 0x40))
            self->routine_secondary = RisingState_Rise;
        break;

    case RisingState_Rise:
        if (--own->timer >= 0)
            break;
        own->timer = 3;
        self->y_pos.f.u -= 4;
        self->y_radius += 4;
        self->mapping_frame++;
        if (self->mapping_frame == 6)
            self->routine_secondary = RisingState_Done;
        break;

    default:
        break;
    }
}

// Offset_0x0205FC: the object and 13 more are the pieces of its frame
static void RisingPillar_Break(Sst *self) {
    const SstCode code = self->code;
    const uint8_t render = self->render_flags | SST_RENDER_STATIC;
    const uint16_t tile = self->art_tile;
    const uint16_t priority = self->priority;
    const uint8_t width = self->width_pixels;
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;

    const uint8_t *piece;
    const uint16_t count = Mappings_FramePieces((const uint8_t *)self->mappings, self->mapping_frame, &piece);

    for (int i = 0; i < count && i < 14; i++) {
        Sst *frag = self;
        if (i > 0) {
            frag = Sst_AllocateAfter(self);
            if (frag == NULL)
                break;
            piece += SPRITE_PIECE_SIZE_S3;
        }
        Own_RisingPillar *frag_own = (Own_RisingPillar *)frag->own.u8;
        frag->routine = 4;
        frag->code = code;
        frag->mappings = piece;
        frag->render_flags = render;
        frag->x_pos.f.u = x;
        frag->y_pos.f.u = y;
        frag->art_tile = tile;
        frag->priority = priority;
        frag->width_pixels = width;
        frag->x_vel = rising_speeds[i][0];
        frag->y_vel = rising_speeds[i][1];
        frag_own->delay = rising_delays[i];
    }
    PlaySound(sfx_WallSmash);
}

void Obj_NGHZRisingPillar(Sst *self) {
    Own_RisingPillar *own = (Own_RisingPillar *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_NGHZRisingPillar;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->y_radius = 0x18;
        self->priority = 4 * 0x80;
        __attribute__((fallthrough));
    case 2: {
        const int16_t old_x = self->x_pos.f.u;
        RisingPillar_Rise(self, own);
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
        if ((self->status & 0x18) == 0) {
            SstOnScreenTest(self);
            return;
        }
        // stood on: it breaks, and those on it are let go in the air, rolling
        self->mapping_frame += 7;
        RisingPillar_Break(self);
        const uint8_t stood = self->status;
        FOR_EACH_CHARACTER(chr, who) {
            if (!(stood & (1 << (3 + who))))
                continue;
            chr->status |= SST_PSTATUS_IN_BALL | SST_PSTATUS_IN_AIR;
            chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            chr->y_radius = 0xE;
            chr->x_radius = 7;
            chr->anim = SonAnimId_Roll;
            chr->routine = 2;
        }
        self->status &= (uint8_t) ~0x18;
    }
        __attribute__((fallthrough)); // (the object is now the first piece)
    default: // 4: a piece
        if (own->delay != 0) {
            own->delay--;
        } else {
            Sst_MoveSprite2(self);
            self->y_vel += 0x18;
        }
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 82: the falling pillar. Its subtype's high nibble is its size (0 a platform $20 by 8, 1 a pillar $1C by $30); the low one what it does: 0 nothing; 1 and 3 wait for a character to stand on it
// for $1E frames and then go to 2 (fall) and 4 (rise); 5 goes to 6 (fall) when touched at the side; 7 keeps at the water's height. One that is still to move dips up to 4 pixels while a character
// stands on it
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad0[7];
    int16_t base_y; // 0x30
    uint8_t pad1[2];
    int16_t base_x; // 0x34
    int16_t timer;  // 0x36: frames to wait after it was stood on
    uint8_t dips;   // 0x38: it dips under a character
    uint8_t pad2[5];
    uint8_t dip;    // 0x3E: the dip's angle
    uint8_t touched;// 0x3F: bit 0 Sonic, bit 1 Tails touched it at the side (the solid test's result)
} Own_FallingPillar;

// Offset_0x024E0E: width and height by size
static const uint8_t falling_sizes[4][2] = { { 0x20, 0x08 }, { 0x1C, 0x30 }, { 0x10, 0x10 }, { 0x10, 0x10 } };

static void FallingPillar_Move(Sst *self, Own_FallingPillar *own) {
    switch (self->subtype) {
    case 1:
    case 3:
        if (own->timer != 0) {
            if (--own->timer == 0) {
                self->subtype++;
                own->dips = 0;
            }
        } else if (self->status & 0x18) {
            own->timer = 0x1E;
        }
        break;

    case 2:
    case 6: {
        Sst_MoveSprite2(self);
        self->y_vel += 8;
        int16_t dist = SstFloorDist(self, self->x_pos.f.u);
        if (dist < 0) {
            dist++;
            self->y_pos.f.u += dist;
            self->y_vel = 0;
            self->subtype = 0;
        }
        break;
    }

    case 4: {
        Sst_MoveSprite2(self);
        self->y_vel -= 8;
        const int16_t dist = SstHitCeiling(self);
        if (dist < 0) {
            self->y_pos.f.u -= dist;
            self->y_vel = 0;
            self->subtype = 0;
        }
        break;
    }

    case 5:
        if (own->touched & 3) {
            self->subtype++;
            own->dips = 0;
        }
        break;

    case 7: {
        int16_t d0 = (int16_t)(wtr_pos1 - self->y_pos.f.u);
        if (d0 == 0)
            break;
        if (d0 < 0) { // the water is above: it rises with it (at most 2 a frame)
            if (d0 < -2)
                d0 = -2;
            self->y_pos.f.u += d0;
            const int16_t dist = SstHitCeiling(self);
            if (dist < 0)
                self->y_pos.f.u -= dist;
        } else {
            if (d0 > 2)
                d0 = 2;
            self->y_pos.f.u += d0;
            int16_t dist = SstFloorDist(self, self->x_pos.f.u);
            if (dist < 0) {
                dist++;
                self->y_pos.f.u += dist;
            }
        }
        break;
    }

    default: // 0
        break;
    }
}

// Offset_0x024F9E
static void FallingPillar_Dip(Sst *self, Own_FallingPillar *own) {
    if (!own->dips)
        return;
    if (!(self->status & 0x18)) {
        if (own->dip == 0)
            return;
        own->dip -= 4;
    } else {
        if (own->dip == 0x40)
            return;
        own->dip += 4;
    }
    int16_t sin, cos;
    CalcSine(own->dip, &sin, &cos);
    self->y_pos.f.u = (int16_t)(own->base_y + ((sin * 0x400) >> 16));
}

void Obj_NGHZFallingPillar(Sst *self) {
    Own_FallingPillar *own = (Own_FallingPillar *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_NGHZFallingPillar;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        const int size = (self->subtype >> 4) & 3; // (the pre-beta reads it from a table of 4 and the frames of 2: the last two are as broken as it is)
        self->width_pixels = falling_sizes[size][0];
        self->y_radius = falling_sizes[size][1];
        self->mapping_frame = (uint8_t)size;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        const uint8_t kind = self->subtype & 0xF;
        if (kind != 0 && kind != 7)
            own->dips = 1;
        self->subtype &= 0xF;
    }

    const int16_t old_x = self->x_pos.f.u;
    FallingPillar_Move(self, own);
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        own->touched = 0;
        FOR_EACH_CHARACTER(chr, who) {
            if (Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL) == 1)
                own->touched |= (uint8_t)(1 << who);
        }
        FallingPillar_Dip(self, own);
    }
    SstOnScreenTest2(self, own->base_x);
}
