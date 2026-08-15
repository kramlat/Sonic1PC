#include "Cannonball.h"

#include "Level.h"
#include "LevelCollision.h"

extern const uint8_t Mappings_BallHog[]; // From Object/BallHog.c
void Obj_Explosion(Object *obj); // From Object/Explosion.c -- avoid Explosion.h's own #include of the Mappings_Explosion resource (ODR)

// Object 20 - cannonball that Ball Hog throws (SBZ)

static void CBal_Main(Object *obj, Scratch_Cannonball *scratch) {
    obj->routine = 2; // advance to CBal_Bounce
    obj->y_rad = 14 / 2;
    obj->mappings = Mappings_BallHog;
    obj->tile = TILE_MAP(0, 1, 0, 0, 0x302); // ArtTile_Ball_Hog | Tile_Pal2
    obj->render.f.align_fg = true;
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

    obj->type = ObjId_Explosion;
    obj->routine = 2; // plain explosion (real id_Explosion, not id_ExplosionItem) -- no animal, no points
    Obj_Explosion(obj); // real falls straight into the explosion object's own code this same frame
    return true;
}

static void CBal_Animate(Object *obj) {
    if (--obj->frame_time.b >= 0)
        return;
    obj->frame_time.b = 6 - 1;
    obj->frame ^= 1; // alternate between black and red ball
}

static void CBal_Bounce(Object *obj, Scratch_Cannonball *scratch) {
    ObjectFall(obj);

    if (obj->ysp >= 0) { // not still going up
        int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (dist < 0) {
            obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + dist);
            obj->ysp = (int16_t)-0x300; // bounce upwards

            int8_t hit_angle = (int8_t)angle_buffer0;
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
