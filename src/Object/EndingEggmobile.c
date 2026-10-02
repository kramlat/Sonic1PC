#include "Ending.h"

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

#define fx   scratch.s32[0] // 0x28: x, 16.16 (VDP screen coordinates)
#define fy   scratch.s32[1] // 0x2C: y, 16.16
#define vx   scratch.s16[4] // 0x30: x speed, 8.8
#define vy   scratch.s16[5] // 0x32: y speed, 8.8
#define timer scratch.u16[6] // 0x34: frames in this part

#define ESHIP_GRAVITY   1                       // 8.8 per frame: it sinks slowly
#define ESHIP_START_X   (0x80 + 0x110)
#define ESHIP_START_Y   (0x80 + 0x1E)
#define ESHIP_BLAST_Y   (0x80 + 0x4A)           // the hills on the horizon: it goes down behind them here
#define ESHIP_BREAK_AT  70                      // frames before the debris comes off

static void Place(Object *obj, int32_t x, int32_t y) {
    obj->fx = x * 0x10000;
    obj->fy = y * 0x10000;
    obj->pos.s.x = (int16_t)x;
    obj->pos.s.y = (int16_t)y;
}

static void Move(Object *obj) {
    obj->fx += (int32_t)obj->vx * 0x100;
    obj->fy += (int32_t)obj->vy * 0x100;
    obj->pos.s.x = (int16_t)(obj->fx >> 16);
    obj->pos.s.y = (int16_t)(obj->fy >> 16);
}

// A part of the effect (smoke, debris, spark) at the given spot.
static Object *Spawn(uint8_t routine, uint8_t frame, int32_t x, int32_t y, int16_t xspeed, int16_t yspeed) {
    Object *child = FindFreeObj();
    if (child == NULL)
        return NULL;
    child->type = ObjId_8D;
    child->routine = routine;
    child->mappings = Mappings_EndingEggmobile;
    child->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Eggman);
    child->render.b = 0; // screen-positioned
    child->priority = 6; // behind Sonic and the animals
    child->frame = frame;
    Place(child, x, y);
    child->vx = xspeed;
    child->vy = yspeed;
    child->timer = 0;
    return child;
}

static void Fly(Object *obj) {
    obj->timer++;
    obj->vy += ESHIP_GRAVITY;
    Move(obj);

    if (obj->timer % 6 == 0) // a trail of smoke
        Spawn(ESHIP_SMOKE, FRAME_SMOKE, obj->pos.s.x + 8, obj->pos.s.y - 4, 0x10, -0x18);
    if (obj->timer % 22 == 0) // and sparks
        Spawn(ESHIP_SPARK, FRAME_SPARK, obj->pos.s.x - 4, obj->pos.s.y + 4, 0, 0);

    if (obj->timer == ESHIP_BREAK_AT) { // pieces come off and the ship catches fire
        obj->frame = FRAME_BURNING;
        Spawn(ESHIP_DEBRIS, FRAME_DEBRIS1, obj->pos.s.x, obj->pos.s.y, -0x50, -0x70);
        Spawn(ESHIP_DEBRIS, FRAME_DEBRIS2, obj->pos.s.x, obj->pos.s.y, 0x38, -0x48);
        Spawn(ESHIP_DEBRIS, FRAME_DEBRIS3, obj->pos.s.x, obj->pos.s.y, 0x08, -0x20);
    }

    if (obj->pos.s.y >= ESHIP_BLAST_Y) { // it has sunk to the horizon: it blows up
        obj->routine = ESHIP_BLAST;
        obj->timer = 0;
        obj->frame = FRAME_EXPLOSION;
        for (int i = 0; i < 4; i++)
            Spawn(ESHIP_SPARK, FRAME_EMBER, obj->pos.s.x, obj->pos.s.y, (i & 1) ? 0x60 : -0x60, (i & 2) ? 0x30 : -0x50);
    }
}

static void Blast(Object *obj) {
    obj->timer++;
    if (obj->timer == 8)
        obj->frame = FRAME_FIREBALL;
    if (obj->timer == 16) { // all that is left is smoke
        for (int i = 0; i < 3; i++)
            Spawn(ESHIP_SMOKE, FRAME_SMOKE, obj->pos.s.x + (i - 1) * 10, obj->pos.s.y - i * 2, (int16_t)((i - 1) * 0x20), -0x20);
        ObjectDelete(obj);
    }
}

static void Smoke(Object *obj) {
    obj->timer++;
    Move(obj);
    if (obj->timer == 10)
        obj->frame = FRAME_SMOKEFADE; // thins out
    if (obj->timer >= 24)
        ObjectDelete(obj);
}

static void Debris(Object *obj) {
    obj->timer++;
    obj->vy += 0x0A; // falls faster than the wreck
    Move(obj);
    if (obj->timer % 5 == 0)
        Spawn(ESHIP_SPARK, FRAME_EMBER, obj->pos.s.x, obj->pos.s.y, 0, 0);
    if (obj->pos.s.y >= ESHIP_BLAST_Y + 0x14)
        ObjectDelete(obj);
}

static void Spark(Object *obj) {
    obj->timer++;
    Move(obj);
    if (obj->vx != 0 || obj->vy != 0)
        obj->vy += 0x08;
    if (obj->timer == 4 && obj->frame == FRAME_SPARK)
        obj->frame = FRAME_EMBER;
    if (obj->timer >= 8)
        ObjectDelete(obj);
}

void Obj_EndEggmobile(Object *obj) {
    switch (obj->routine) {
    case ESHIP_INIT:
        obj->routine = ESHIP_FLY;
        obj->mappings = Mappings_EndingEggmobile;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Eggman);
        obj->render.b = 0;
        obj->priority = 6;
        obj->frame = FRAME_POD;
        Place(obj, ESHIP_START_X + SCREEN_WIDEADD2, ESHIP_START_Y + SCREEN_TALLADD2);
        obj->vx = -0x88; // drifts left...
        obj->vy = 0;     // ...and sinks
        obj->timer = 0;
        break;
    case ESHIP_FLY: Fly(obj); break;
    case ESHIP_BLAST: Blast(obj); break;
    case ESHIP_SMOKE: Smoke(obj); break;
    case ESHIP_DEBRIS: Debris(obj); break;
    case ESHIP_SPARK: Spark(obj); break;
    }
    if (obj->type != 0) // (an object that deleted itself above has nothing left to draw)
        DisplaySprite(obj);
}
