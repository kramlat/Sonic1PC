#include "BallHog.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Cannonball.h"
#include "Level.h"
#include "LevelCollision.h"
#include "Resource/Animation/BallHog.h"
#include "Resource/Mappings/BallHog.h"

// Object 1E - Ball Hog enemy (SBZ). Falls (invisibly) until it lands on the
// floor, then periodically launches an Object 20 cannonball.

static void Hog_Main(Sst *self) {
    self->y_radius = 38 / 2;
    self->x_radius = 16 / 2;
    self->mappings = Mappings_BallHog;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ball_Hog); // | Tile_Pal2
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x05; // col_24x36 | col_badnik
    self->width_pixels = 24 / 2;

    // Make the Ball Hog fall until it has collided with the floor (while invisible)
    Sst_MoveSprite(self);
    int16_t dist = SstFloorDist(self, self->x_pos.f.u);
    if (dist < 0) {
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + dist);
        self->y_vel = 0;
        self->routine = 2; // advance to Hog_Action
    }

    // FixBugs: don't fall invisibly forever if there's no floor beneath it
    if ((uint16_t)self->y_pos.f.u > 0x7FF)
        Sst_Delete(self);
}

static void Hog_LaunchBall(Sst *self, Own_BallHog *own) {
    own->launched = 1; // cannonball already launched this animation cycle

    Sst *ball = Sst_SpawnCode(Obj_Cannonball, self->x_pos.f.u, self->y_pos.f.u);
    if (ball == NULL)
        return;
    ball->x_vel = (int16_t)-0x100; // bounce left by default
    ball->y_vel = 0;

    int16_t x_off = -4;
    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        x_off = 4;
        ball->x_vel = (int16_t)-ball->x_vel; // bounce right instead
    }
    ball->x_pos.f.u = (int16_t)(ball->x_pos.f.u + x_off);
    ball->y_pos.f.u = (int16_t)(ball->y_pos.f.u + 12);
    ball->subtype = self->subtype; // subtype (explosion timer in seconds)
}

static void Hog_Action(Sst *self, Own_BallHog *own) {
    Sst_Animate(self, Animation_BallHog);

    // Stays on the final frame (ID 1) for 9 frames -- only launch once per
    // visit, not on every one of those 9 frames.
    if (self->mapping_frame == 1) {
        if (!own->launched)
            Hog_LaunchBall(self, own);
    } else {
        own->launched = 0;
    }

    SstOnScreenTest(self);
}

void Obj_BallHog(Sst *self) {
    Own_BallHog *own = (Own_BallHog *)self->own.u8;

    switch (self->routine) {
    case 0: Hog_Main(self); break;
    case 2: Hog_Action(self, own); break;
    }
}
