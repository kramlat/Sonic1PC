#include "BallHog.h"

#include "Cannonball.h"
#include "Level.h"
#include "LevelCollision.h"
#include "Resource/Animation/BallHog.h"
#include "Resource/Mappings/BallHog.h"

// Object 1E - Ball Hog enemy (SBZ). Falls (invisibly) until it lands on the
// floor, then periodically launches an Object 20 cannonball.

static void Hog_Main(Object *obj) {
    obj->y_rad = 38 / 2;
    obj->x_rad = 16 / 2;
    obj->mappings = Mappings_BallHog;
    obj->tile = TILE_MAP(0, 1, 0, 0, 0x302); // ArtTile_Ball_Hog | Tile_Pal2
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->col_type = 0x05; // col_24x36 | col_badnik
    obj->width_pixels = 24 / 2;

    // Make the Ball Hog fall until it has collided with the floor (while invisible)
    ObjectFall(obj);
    int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (dist < 0) {
        obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + dist);
        obj->ysp = 0;
        obj->routine = 2; // advance to Hog_Action
    }

    // FixBugs: don't fall invisibly forever if there's no floor beneath it
    if ((uint16_t)obj->pos.l.y.f.u > 0x7FF)
        ObjectDelete(obj);
}

static void Hog_LaunchBall(Object *obj, Scratch_BallHog *scratch) {
    scratch->launched = 1; // cannonball already launched this animation cycle

    Object *ball = FindFreeObj();
    if (ball == NULL)
        return;
    ball->type = ObjId_Cannonball;
    ball->pos.l.x.f.u = obj->pos.l.x.f.u;
    ball->pos.l.y.f.u = obj->pos.l.y.f.u;
    ball->xsp = (int16_t)-0x100; // bounce left by default
    ball->ysp = 0;

    int16_t x_off = -4;
    if (obj->status.o.f.x_flip) {
        x_off = 4;
        ball->xsp = (int16_t)-ball->xsp; // bounce right instead
    }
    ball->pos.l.x.f.u = (int16_t)(ball->pos.l.x.f.u + x_off);
    ball->pos.l.y.f.u = (int16_t)(ball->pos.l.y.f.u + 12);
    ball->scratch.u8[0] = obj->scratch.u8[0]; // subtype (explosion timer in seconds)
}

static void Hog_Action(Object *obj, Scratch_BallHog *scratch) {
    AnimateSprite(obj, Animation_BallHog);

    // Stays on the final frame (ID 1) for 9 frames -- only launch once per
    // visit, not on every one of those 9 frames.
    if (obj->frame == 1) {
        if (!scratch->launched)
            Hog_LaunchBall(obj, scratch);
    } else {
        scratch->launched = 0;
    }

    RememberState(obj);
}

void Obj_BallHog(Object *obj) {
    Scratch_BallHog *scratch = (Scratch_BallHog *)&obj->scratch;

    switch (obj->routine) {
    case 0: Hog_Main(obj); break;
    case 2: Hog_Action(obj, scratch); break;
    }
}
