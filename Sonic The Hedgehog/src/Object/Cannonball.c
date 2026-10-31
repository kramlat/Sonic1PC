#include "Cannonball.h"
#include "Object.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

extern const uint8_t Mappings_BallHog[]; // From Object/BallHog.c

// Object 20 - cannonball that Ball Hog throws (SBZ)

static void CBal_Main(Sst *self, Own_Cannonball *own) {
    self->routine = 2; // advance to CBal_Bounce
    self->y_radius = 14 / 2;
    self->mappings = Mappings_BallHog;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ball_Hog); // | Tile_Pal2
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 3 * 0x80; // above Ball Hog
    self->collision_flags = 0x07 | 0x80; // col_12x12 | col_hurt
    self->width_pixels = 16 / 2;

    own->time = (int16_t)(self->subtype * 60); // subtype, in seconds
    self->mapping_frame = 4; // ball frame
}

// Returns true once the explosion timer has expired and the cannonball has
// already turned into (and executed one frame of) a plain explosion.
static bool CBal_ChkExplode(Sst *self, Own_Cannonball *own) {
    if (--own->time >= 0)
        return false;

    self->code = Obj_ExplosionBomb; // real Object 3F "Explosion" (Map_ExplodeBomb + sfx_Bomb)
    self->routine = 0;
    self->code(self); // real falls straight into the explosion object's own code this same frame
    return true;
}

static void CBal_Animate(Sst *self) {
    if (--self->anim_frame_timer >= 0)
        return;
    self->anim_frame_timer = 6 - 1;
    self->mapping_frame ^= 1; // alternate between black and red ball
}

// Not in the original (the real cannonball just flies through walls): bounces back off a real wall in its way
// and keeps following gravity. A ball is thrown from the Ball Hog's own ledge, so it starts inside or flush against
// solid tiles -- it must come out of those (and fall off the edge) rather than count its own ledge as a wall.
// So walls only count once the ball has been in open air, and then only when one is ahead of it.
static void CBal_BounceOffWalls(Sst *self, Own_Cannonball *own) {
    if (!own->clear) {
        bool inside = (self->x_vel < 0) ? SstHitWallLeft(self, 0) < 0 : SstHitWallRight(self, 0) < 0;
        if (!inside)
            own->clear = 1;
        return;
    }
    if (self->x_vel < 0) {
        int16_t dist = SstHitWallLeft(self, -8);
        if (dist < 0) {
            self->x_pos.f.u = (int16_t)(self->x_pos.f.u - dist); // back out of the wall
            self->x_vel = (int16_t)-self->x_vel;
        }
    } else if (self->x_vel > 0) {
        int16_t dist = SstHitWallRight(self, 8);
        if (dist < 0) {
            self->x_pos.f.u = (int16_t)(self->x_pos.f.u + dist);
            self->x_vel = (int16_t)-self->x_vel;
        }
    }
}

static void CBal_Bounce(Sst *self, Own_Cannonball *own) {
    Sst_MoveSprite(self);
    CBal_BounceOffWalls(self, own);

    if (self->y_vel >= 0) { // not still going up
        angle_buffer0 = 0; // real ObjFloorDist starts with a blank angle
        int16_t dist = SstFloorDist(self, self->x_pos.f.u);
        if (dist < 0) {
            self->y_pos.f.u = (int16_t)(self->y_pos.f.u + dist);
            self->y_vel = (int16_t)-0x300; // bounce upwards

            // Real ObjFloorDist returns the angle with the "snap to flat" bit (bit 0) already applied: an odd angle
            // byte means a flat floor, not a slope. Reading the raw byte made ledge tiles with such an angle look
            // like slopes, turning the ball back as if it had hit an invisible wall.
            int8_t hit_angle = (angle_buffer0 & 1) ? 0 : (int8_t)angle_buffer0;
            if (hit_angle < 0) {
                // landed on an ascending (to the right) surface -- move left
                if (self->x_vel >= 0)
                    self->x_vel = (int16_t)-self->x_vel;
            } else if (hit_angle > 0) {
                // landed on a descending (to the right) surface -- move right
                if (self->x_vel < 0)
                    self->x_vel = (int16_t)-self->x_vel;
            }
            // flat surface: keep old direction
        }
    }

    if (CBal_ChkExplode(self, own))
        return;

    CBal_Animate(self);

    if ((uint16_t)self->y_pos.f.u > (uint16_t)(limit_btm2 + 224))
        Sst_Delete(self);
    else
        Sst_Draw(self);
}

void Obj_Cannonball(Sst *self) {
    Own_Cannonball *own = (Own_Cannonball *)self->own.u8;

    if (self->routine == 0)
        CBal_Main(self, own);

    CBal_Bounce(self, own);
}
