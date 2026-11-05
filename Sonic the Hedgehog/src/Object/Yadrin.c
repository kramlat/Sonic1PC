#include "Yadrin.h"

#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Animation/Yadrin.h"
#include "Resource/Mappings/Yadrin.h"

// Object 50 - Yadrin enemy (MZ [unused in real placement data, but present
// in the debug menu], SYZ). Walks back and forth, pausing and turning
// around whenever it meets a wall, steep upward slope, or a drop ahead.
// Its own special collision (col_special, see React_Special in Sonic.c)
// exposes a narrow, 24x8px "face" hitbox right at its top edge -- only
// touching within that thin band hurts Sonic; touching it anywhere else
// (deeper, or outside that band) is a normal stompable badnik touch.

typedef struct {
    int16_t timedelay; // delay before turning around
} Own_Yadrin;

static bool Yad_ChkWall(Sst *self) {
    // Only checked every 4th frame on real hardware too (offset by this
    // object's own RAM index so multiple Yadrins don't all check on the
    // same frame) -- presumably a performance shortcut, not gameplay-critical.
    if (((uint8_t)frame_count + (uint8_t)(self - sst)) & 3)
        return false;

    int16_t hit = (self->x_vel < 0) ? SstHitWallLeft(self, (int16_t)(~(uint16_t)self->width_pixels))
                                    : SstHitWallRight(self, self->width_pixels);
    return hit < 0;
}

static void Yad_Action_Wait(Sst *self, Own_Yadrin *own) {
    if (--own->timedelay >= 0)
        return;

    self->routine_secondary += 2;
    self->x_vel = (int16_t)-0x100;
    self->anim = 1;

    bool was_left = (self->status & SST_STATUS_X_FLIP) != 0;
    self->status ^= SST_STATUS_X_FLIP;
    if (!was_left)
        self->x_vel = (int16_t)-self->x_vel;
}

static void Yad_Action_Move(Sst *self, Own_Yadrin *own) {
    Sst_MoveSprite2(self);

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    bool pause = (floor_dist < -8) || (floor_dist >= 0xC);
    if (!pause) {
        self->y_pos.f.u += floor_dist;
        pause = Yad_ChkWall(self);
    }
    if (!pause)
        return;

    self->routine_secondary -= 2;
    own->timedelay = 60 - 1;
    self->x_vel = 0;
    self->anim = 0;
}

void Obj_Yadrin(Sst *self) {
    Own_Yadrin *own = (Own_Yadrin *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main -- falls until it lands, invisible the whole time
        self->mappings = Mappings_Yadrin;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Yadrin); // | Tile_Pal2
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 40 / 2;
        self->y_radius = 34 / 2;
        self->x_radius = 16 / 2;
        self->collision_flags = 0xCC; // col_40x32 | col_special

        Sst_MoveSprite(self);
        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist >= 0) {
            if ((uint16_t)self->y_pos.f.u > 0x7FF) // fell below max level height
                Sst_Delete(self);
            return;
        }
        self->y_pos.f.u += floor_dist;
        self->y_vel = 0;
        self->routine += 2;
        self->status ^= SST_STATUS_X_FLIP; // face left on spawn
        return;
    }
    case 2: // Action
        if (self->routine_secondary == 0)
            Yad_Action_Wait(self, own);
        else
            Yad_Action_Move(self, own);

        Sst_Animate(self, Animation_Yadrin);
        SstOnScreenTest(self);
        break;
    }
}
