#include "Cannonball.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

extern const uint8_t Mappings_BallHog[]; // From Object/BallHog.c
void Obj_ExplosionBomb(Object *obj); // From Object/Explosion.c -- avoid Explosion.h's own #include of the Mappings_Explosion resource (ODR)

// Object 20 - cannonball that Ball Hog throws (SBZ)

static void CBal_Main(Object *obj, Scratch_Cannonball *scratch) {
    obj->routine = 2; // advance to CBal_Bounce
    obj->y_rad = 14 / 2;
    obj->mappings = Mappings_BallHog;
    obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ball_Hog); // | Tile_Pal2
    obj->render.f.level_fg = true;
    obj->priority = 3; // above Ball Hog
    obj->col_type = 0x07 | 0x80; // col_12x12 | col_hurt
    obj->width_pixels = 16 / 2;

    scratch->time = (int16_t)(obj->scratch.u8[0] * 60); // subtype, in seconds
    obj->frame = 4; // ball frame
}

// Returns true once the explosion timer has expired and the cannonball has
// already turned into (and executed one frame of) a plain explosion.
static bool CBal_ChkExplode(Object *obj, Scratch_Cannonball *scratch) {
    if (--scratch->time >= 0)
        return false;

    obj->type = ObjId_ExplosionBomb; // real Object 3F "Explosion" (Map_ExplodeBomb + sfx_Bomb)
    obj->routine = 0;
    Obj_ExplosionBomb(obj); // real falls straight into the explosion object's own code this same frame
    return true;
}

static void CBal_Animate(Object *obj) {
    if (--obj->frame_time.b >= 0)
        return;
    obj->frame_time.b = 6 - 1;
    obj->frame ^= 1; // alternate between black and red ball
}

// Not in the original (the real cannonball just flies through walls): bounces back off a real wall in its way
// and keeps following gravity. A ball is thrown from the Ball Hog's own ledge, so it starts inside or flush against
// solid tiles -- it must come out of those (and fall off the edge) rather than count its own ledge as a wall.
// So walls only count once the ball has been in open air, and then only when one is ahead of it.
static void CBal_BounceOffWalls(Object *obj, Scratch_Cannonball *scratch) {
    if (!scratch->clear) {
        bool inside = (obj->xsp < 0) ? ObjHitWallLeft(obj, 0) < 0 : ObjHitWallRight(obj, 0) < 0;
        if (!inside)
            scratch->clear = 1;
        return;
    }
    if (obj->xsp < 0) {
        int16_t dist = ObjHitWallLeft(obj, -8);
        if (dist < 0) {
            obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u - dist); // back out of the wall
            obj->xsp = (int16_t)-obj->xsp;
        }
    } else if (obj->xsp > 0) {
        int16_t dist = ObjHitWallRight(obj, 8);
        if (dist < 0) {
            obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + dist);
            obj->xsp = (int16_t)-obj->xsp;
        }
    }
}

static void CBal_Bounce(Object *obj, Scratch_Cannonball *scratch) {
    ObjectFall(obj);
    CBal_BounceOffWalls(obj, scratch);

    if (obj->ysp >= 0) { // not still going up
        angle_buffer0 = 0; // real ObjFloorDist starts with a blank angle
        int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (dist < 0) {
            obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + dist);
            obj->ysp = (int16_t)-0x300; // bounce upwards

            // Real ObjFloorDist returns the angle with the "snap to flat" bit (bit 0) already applied: an odd angle
            // byte means a flat floor, not a slope. Reading the raw byte made ledge tiles with such an angle look
            // like slopes, turning the ball back as if it had hit an invisible wall.
            int8_t hit_angle = (angle_buffer0 & 1) ? 0 : (int8_t)angle_buffer0;
            if (hit_angle < 0) {
                // landed on an ascending (to the right) surface -- move left
                if (obj->xsp >= 0)
                    obj->xsp = (int16_t)-obj->xsp;
            } else if (hit_angle > 0) {
                // landed on a descending (to the right) surface -- move right
                if (obj->xsp < 0)
                    obj->xsp = (int16_t)-obj->xsp;
            }
            // flat surface: keep old direction
        }
    }

    if (CBal_ChkExplode(obj, scratch))
        return;

    CBal_Animate(obj);

    if ((uint16_t)obj->pos.l.y.f.u > (uint16_t)(limit_btm2 + 224))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}

void Obj_Cannonball(Object *obj) {
    Scratch_Cannonball *scratch = (Scratch_Cannonball *)&obj->scratch;

    if (obj->routine == 0)
        CBal_Main(obj, scratch);

    CBal_Bounce(obj, scratch);
}
