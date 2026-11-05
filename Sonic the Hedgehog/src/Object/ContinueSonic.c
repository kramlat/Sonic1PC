#include "ContinueSonic.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Object/Sonic.h"
#include "Sound.h"

#include "Resource/Animation/ContinueSonic.h"
#include "Resource/Mappings/ContinueSonicLying.h"

// ---------------------------------------------------------------------------
// Object 81 - Sonic on the continue screen
// ---------------------------------------------------------------------------

enum {
    CSON_MAIN = 0,
    CSON_CHECK_LAND = 2,
    CSON_ANIMATE = 4,
    CSON_RUN_RIGHT = 6,
};

#define CSON_FLOOR_Y (0x1A0 + SCREEN_TALLADD2)

static void CSon_Animate(Sst *self);
static void CSon_RunRight(Sst *self);

static void CSon_Main(Sst *self) {
    self->routine += 2;
    self->x_pos.f.u = 0xA0 + SCREEN_WIDEADD2;
    self->y_pos.f.u = 0xC0 + SCREEN_TALLADD2;
    self->mappings = Mappings_Sonic; // Sonic's own mappings, graphics and animations
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 2 * 0x80; // behind the other elements
    self->anim = SonAnimId_Float3;
    self->y_vel = 0x400; // falls in from above
}

static void CSon_CheckLand(Sst *self) {
    if (self->y_pos.f.u != CSON_FLOOR_Y) {
        Sst_MoveSprite2(self);
        Sonic_Animate(self);
        Sonic_LoadGfx(self);
        return;
    }
    self->routine += 2;
    self->y_vel = 0;
    self->mappings = Mappings_ContinueSonicLying; // his own lying-down frames
    self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Continue_Sonic);
    self->anim = 0;
    CSon_Animate(self);
}

static void CSon_Animate(Sst *self) {
    if (!(jpad1_press1 & JPAD_START)) {
        Sst_Animate(self, Animation_ContinueSonic);
        return;
    }
    // A continue is used: Sonic gets up and runs off to the right
    self->routine += 2;
    self->mappings = Mappings_Sonic;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
    self->anim = SonAnimId_Float4; // getting up, turning into the walk animation
    self->ground_vel = 0;
    self->y_pos.f.u -= 8;
    FadeOutMusic();
    CSon_RunRight(self);
}

static void CSon_RunRight(Sst *self) {
    if (self->ground_vel == 0x40 * 0x20) {
        self->x_vel = 0x1000; // shoots off once his ground speed is up
    } else {
        self->ground_vel += 0x20; // (only the animation looks at it until then)
    }
    Sst_MoveSprite2(self);
    Sonic_Animate(self);
    Sonic_LoadGfx(self);
}

void Obj_ContSonic(Sst *self) {
    switch (self->routine) {
    case CSON_MAIN:
        CSon_Main(self);
        // falls through
    case CSON_CHECK_LAND:
        CSon_CheckLand(self);
        break;
    case CSON_ANIMATE:
        CSon_Animate(self);
        break;
    case CSON_RUN_RIGHT:
        CSon_RunRight(self);
        break;
    }
    Sst_Draw(self);
}
