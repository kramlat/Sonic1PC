#include "Gargoyle.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

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

static void Gar_Main(Sst *self, Own_Gargoyle *own) {
    self->routine += 2;
    self->mappings = Mappings_Gargoyle;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Gargoyle); // | Tile_Pal3
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;
    self->width_pixels = 32 / 2;

    uint8_t subtype = self->subtype & 0xF;
    own->spit_rate = Gar_SpitRate[subtype];
    self->anim_frame_timer = (int8_t)own->spit_rate;
    self->subtype = subtype;
}

static void Gar_MakeFire(Sst *self, Own_Gargoyle *own) {
    if (--self->anim_frame_timer != 0)
        return;
    self->anim_frame_timer = (int8_t)own->spit_rate;

    if (!SstChkObjectVisible(self))
        return;

    Sst *fire = Sst_SpawnCode(Obj_Gargoyle, self->x_pos.f.u, self->y_pos.f.u);
    if (fire == NULL)
        return;

    fire->routine = 4; // Gar_FireBall
    fire->render_flags = self->render_flags;
    fire->status = self->status;
}

// Returns false if the fireball hit a wall and was deleted (caller must
// not touch it again).
static bool Gar_AniFire(Sst *self) {
    if (((uint8_t)frame_count & 7) == 0) // alternate fireball frame every 8th frame
        self->mapping_frame ^= 1;

    Sst_MoveSprite2(self);

    int16_t hit;
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        hit = SstHitWallLeft(self, -8);
    else
        hit = SstHitWallRight(self, 8);

    if (hit < 0) {
        Sst_Delete(self);
        return false;
    }
    return true;
}

static bool Gar_FireBall(Sst *self) {
    self->routine += 2; // -> Gar_AniFire
    self->y_radius = 16 / 2;
    self->x_radius = 16 / 2;
    self->mappings = Mappings_Gargoyle;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Gargoyle); // (different palette line than head)
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x18 | 0x80; // col_8x8 | col_hurt
    self->width_pixels = 16 / 2;
    self->mapping_frame = 2;
    self->y_pos.f.u += 8;

    self->x_vel = 0x200;
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        self->x_vel = (int16_t)-self->x_vel;

    QueueSound2(sfx_Fireball);

    return Gar_AniFire(self); // falls straight into Gar_AniFire the same frame
}

void Obj_Gargoyle(Sst *self) {
    Own_Gargoyle *own = (Own_Gargoyle *)self->own.u8;
    bool alive = true;

    switch (self->routine) {
    case 0:
        Gar_Main(self, own);
        break;
    case 2:
        Gar_MakeFire(self, own);
        break;
    case 4:
        alive = Gar_FireBall(self);
        break;
    case 6:
        alive = Gar_AniFire(self);
        break;
    }

    if (alive)
        SstOnScreenTest(self);
}
