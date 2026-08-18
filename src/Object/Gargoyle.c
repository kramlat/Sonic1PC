#include "Gargoyle.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Mappings/Gargoyle.h"
#include "Sound.h"

// Object 62 - gargoyle head that spits fireballs (LZ). The head itself
// never moves; it just periodically spawns a fireball object (reusing the
// same object type, routine 4+) that flies straight until it hits a wall.

static const uint8_t Gar_SpitRate[8] = { 30, 60, 90, 120, 150, 180, 210, 240 };

static void Gar_Main(Object *obj, Scratch_Gargoyle *scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_Gargoyle;
    obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Gargoyle); // | Tile_Pal3
    obj->render.f.align_fg = true;
    obj->priority = 3;
    obj->width_pixels = 32 / 2;

    uint8_t subtype = obj->scratch.u8[0] & 0xF;
    scratch->spit_rate = Gar_SpitRate[subtype];
    obj->frame_time.b = (int8_t)scratch->spit_rate;
    obj->scratch.u8[0] = subtype;
}

static void Gar_MakeFire(Object *obj, Scratch_Gargoyle *scratch) {
    if (--obj->frame_time.b != 0)
        return;
    obj->frame_time.b = (int8_t)scratch->spit_rate;

    if (!ChkObjectVisible(obj))
        return;

    Object *fire = FindFreeObj();
    if (fire == NULL)
        return;

    fire->type = ObjId_Gargoyle;
    fire->routine = 4; // Gar_FireBall
    fire->pos.l.x.f.u = obj->pos.l.x.f.u;
    fire->pos.l.y.f.u = obj->pos.l.y.f.u;
    fire->render = obj->render;
    fire->status = obj->status;
}

// Returns false if the fireball hit a wall and was deleted (caller must
// not touch it again).
static bool Gar_AniFire(Object *obj) {
    if (((uint8_t)frame_count & 7) == 0) // alternate fireball frame every 8th frame
        obj->frame ^= 1;

    SpeedToPos(obj);

    int16_t hit;
    if (!obj->status.o.f.x_flip)
        hit = ObjHitWallLeft(obj, -8);
    else
        hit = ObjHitWallRight(obj, 8);

    if (hit < 0) {
        ObjectDelete(obj);
        return false;
    }
    return true;
}

static bool Gar_FireBall(Object *obj) {
    obj->routine += 2; // -> Gar_AniFire
    obj->y_rad = 16 / 2;
    obj->x_rad = 16 / 2;
    obj->mappings = Mappings_Gargoyle;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Gargoyle); // (different palette line than head)
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->col_type = 0x18 | 0x80; // col_8x8 | col_hurt
    obj->width_pixels = 16 / 2;
    obj->frame = 2;
    obj->pos.l.y.f.u += 8;

    obj->xsp = 0x200;
    if (!obj->status.o.f.x_flip)
        obj->xsp = (int16_t)-obj->xsp;

    QueueSound2(sfx_Fireball);

    return Gar_AniFire(obj); // falls straight into Gar_AniFire the same frame
}

void Obj_Gargoyle(Object *obj) {
    Scratch_Gargoyle *scratch = (Scratch_Gargoyle *)&obj->scratch;
    bool alive = true;

    switch (obj->routine) {
    case 0:
        Gar_Main(obj, scratch);
        break;
    case 2:
        Gar_MakeFire(obj, scratch);
        break;
    case 4:
        alive = Gar_FireBall(obj);
        break;
    case 6:
        alive = Gar_AniFire(obj);
        break;
    }

    if (alive)
        RememberState(obj);
}
