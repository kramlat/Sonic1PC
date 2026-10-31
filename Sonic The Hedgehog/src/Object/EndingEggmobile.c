#include "Ending.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"

#include "Resource/Mappings/EndingEggmobile.h"

// Object 8D - Eggman's Eggmobile coming apart in the distance of the Green Hill ending, while Sonic runs back to the
// animals. It is a restored piece of the original: the art ("Unused - Eggman Ending") has always been loaded by the
// ending's pattern list, but no object ever drew it. This object, its mappings and its movement are new.
//
// One object type plays every part (the routine says which): the Eggmobile itself, the smoke it trails, the pieces that
// break off it and the sparks and flashes of its end. Everything is screen-positioned, so it stays in the far
// background as the camera scrolls. Positions are kept in 16.16 in the scratch memory (x, y) and speeds in 8.8.

enum {
    ESHIP_INIT = 0,
    ESHIP_FLY = 2,
    ESHIP_BLAST = 4,
    ESHIP_SMOKE = 6,
    ESHIP_DEBRIS = 8,
    ESHIP_SPARK = 0xA,
};

enum { // mapping frames
    FRAME_POD,
    FRAME_BURNING,
    FRAME_DEBRIS1,
    FRAME_DEBRIS2,
    FRAME_DEBRIS3,
    FRAME_EXPLOSION,
    FRAME_FIREBALL,
    FRAME_SPARK,
    FRAME_EMBER,
    FRAME_SMOKE,
    FRAME_SMOKEFADE,
};

#define fx   own.s32[0] // 0x28: x, 16.16 (VDP screen coordinates)
#define fy   own.s32[1] // 0x2C: y, 16.16
#define vx   own.s16[4] // 0x30: x speed, 8.8
#define vy   own.s16[5] // 0x32: y speed, 8.8
#define timer own.u16[6] // 0x34: frames in this part

#define ESHIP_GRAVITY   1                       // 8.8 per frame: it sinks slowly
#define ESHIP_START_X   (0x80 + 0x110)
#define ESHIP_START_Y   (0x80 + 0x1E)
#define ESHIP_BLAST_Y   (0x80 + 0x4A)           // the hills on the horizon: it goes down behind them here
#define ESHIP_BREAK_AT  70                      // frames before the debris comes off

static void Place(Sst *self, int32_t x, int32_t y) {
    self->fx = x * 0x10000;
    self->fy = y * 0x10000;
    self->x_pos.f.u = (int16_t)x;
    self->y_pos.f.u = (int16_t)y;
}

static void Move(Sst *self) {
    self->fx += (int32_t)self->vx * 0x100;
    self->fy += (int32_t)self->vy * 0x100;
    self->x_pos.f.u = (int16_t)(self->fx >> 16);
    self->y_pos.f.u = (int16_t)(self->fy >> 16);
}

// A part of the effect (smoke, debris, spark) at the given spot.
static Sst *Spawn(uint8_t routine, uint8_t frame, int32_t x, int32_t y, int16_t xspeed, int16_t yspeed) {
    Sst *child = Sst_Allocate();
    if (child == NULL)
        return NULL;
    child->code = Obj_EndEggmobile;
    child->routine = routine;
    child->mappings = Mappings_EndingEggmobile;
    child->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Eggman);
    child->render_flags = 0; // screen-positioned
    child->priority = 6 * 0x80; // behind Sonic and the animals
    child->mapping_frame = frame;
    Place(child, x, y);
    child->vx = xspeed;
    child->vy = yspeed;
    child->timer = 0;
    return child;
}

static void Fly(Sst *self) {
    self->timer++;
    self->vy += ESHIP_GRAVITY;
    Move(self);

    if (self->timer % 6 == 0) // a trail of smoke
        Spawn(ESHIP_SMOKE, FRAME_SMOKE, self->x_pos.f.u + 8, self->y_pos.f.u - 4, 0x10, -0x18);
    if (self->timer % 22 == 0) // and sparks
        Spawn(ESHIP_SPARK, FRAME_SPARK, self->x_pos.f.u - 4, self->y_pos.f.u + 4, 0, 0);

    if (self->timer == ESHIP_BREAK_AT) { // pieces come off and the ship catches fire
        self->mapping_frame = FRAME_BURNING;
        Spawn(ESHIP_DEBRIS, FRAME_DEBRIS1, self->x_pos.f.u, self->y_pos.f.u, -0x50, -0x70);
        Spawn(ESHIP_DEBRIS, FRAME_DEBRIS2, self->x_pos.f.u, self->y_pos.f.u, 0x38, -0x48);
        Spawn(ESHIP_DEBRIS, FRAME_DEBRIS3, self->x_pos.f.u, self->y_pos.f.u, 0x08, -0x20);
    }

    if (self->y_pos.f.u >= ESHIP_BLAST_Y) { // it has sunk to the horizon: it blows up
        self->routine = ESHIP_BLAST;
        self->timer = 0;
        self->mapping_frame = FRAME_EXPLOSION;
        for (int i = 0; i < 4; i++)
            Spawn(ESHIP_SPARK, FRAME_EMBER, self->x_pos.f.u, self->y_pos.f.u, (i & 1) ? 0x60 : -0x60, (i & 2) ? 0x30 : -0x50);
    }
}

static void Blast(Sst *self) {
    self->timer++;
    if (self->timer == 8)
        self->mapping_frame = FRAME_FIREBALL;
    if (self->timer == 16) { // all that is left is smoke
        for (int i = 0; i < 3; i++)
            Spawn(ESHIP_SMOKE, FRAME_SMOKE, self->x_pos.f.u + (i - 1) * 10, self->y_pos.f.u - i * 2, (int16_t)((i - 1) * 0x20), -0x20);
        Sst_Delete(self);
    }
}

static void Smoke(Sst *self) {
    self->timer++;
    Move(self);
    if (self->timer == 10)
        self->mapping_frame = FRAME_SMOKEFADE; // thins out
    if (self->timer >= 24)
        Sst_Delete(self);
}

static void Debris(Sst *self) {
    self->timer++;
    self->vy += 0x0A; // falls faster than the wreck
    Move(self);
    if (self->timer % 5 == 0)
        Spawn(ESHIP_SPARK, FRAME_EMBER, self->x_pos.f.u, self->y_pos.f.u, 0, 0);
    if (self->y_pos.f.u >= ESHIP_BLAST_Y + 0x14)
        Sst_Delete(self);
}

static void Spark(Sst *self) {
    self->timer++;
    Move(self);
    if (self->vx != 0 || self->vy != 0)
        self->vy += 0x08;
    if (self->timer == 4 && self->mapping_frame == FRAME_SPARK)
        self->mapping_frame = FRAME_EMBER;
    if (self->timer >= 8)
        Sst_Delete(self);
}

void Obj_EndEggmobile(Sst *self) {
    switch (self->routine) {
    case ESHIP_INIT:
        self->routine = ESHIP_FLY;
        self->mappings = Mappings_EndingEggmobile;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Eggman);
        self->render_flags = 0;
        self->priority = 6 * 0x80;
        self->mapping_frame = FRAME_POD;
        Place(self, ESHIP_START_X + SCREEN_WIDEADD2, ESHIP_START_Y + SCREEN_TALLADD2);
        self->vx = -0x88; // drifts left...
        self->vy = 0;     // ...and sinks
        self->timer = 0;
        break;
    case ESHIP_FLY: Fly(self); break;
    case ESHIP_BLAST: Blast(self); break;
    case ESHIP_SMOKE: Smoke(self); break;
    case ESHIP_DEBRIS: Debris(self); break;
    case ESHIP_SPARK: Spark(self); break;
    }
    if (self->code != NULL) // (an object that deleted itself above has nothing left to draw)
        Sst_Draw(self);
}
