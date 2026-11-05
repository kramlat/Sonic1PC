#include "SmashWall.h"
#include "Resource/Mappings/SmashableWalls.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 3C - smashable wall (GHZ, SLZ)

// Fragment X/Y speeds, indexed [piece][0]=xsp [piece][1]=ysp
static const int16_t frag_speed_right[8][2] = {
    { 0x400, -0x500 }, { 0x600, -0x100 }, { 0x600, 0x100 }, { 0x400, 0x500 },
    { 0x600, -0x600 }, { 0x800, -0x200 }, { 0x800, 0x200 }, { 0x600, 0x600 },
};
static const int16_t frag_speed_left[8][2] = {
    { -0x600, -0x600 }, { -0x800, -0x200 }, { -0x800, 0x200 }, { -0x600, 0x600 },
    { -0x400, -0x500 }, { -0x600, -0x100 }, { -0x600, 0x100 }, { -0x400, 0x500 },
};

void Obj_SmashWall(Sst *self) {
    Own_SmashWall *own = (Own_SmashWall *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_SmashableWalls;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_SLZ_Smashable_Wall); // | Tile_Pal3
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 4 * 0x80;
        self->mapping_frame = self->subtype; // subtype -- 0=left, 1=middle, 2=right
        // Fallthrough
    case 2: { // Solid
        own->speed = player->x_vel;

        SstSolidObject(self, 32 / 2 + 11 /* sonic_solid_width */, 64 / 2, 64 / 2, self->x_pos.f.u, NULL, NULL);
        if (!((self->status & SST_STATUS_PLAYER_PUSH) != 0)) {
            SstOnScreenTest(self);
            break;
        }

        if (player->anim != SonAnimId_Roll) {
            SstOnScreenTest(self);
            break;
        }

        int16_t impact_speed = own->speed < 0 ? (int16_t)-own->speed : own->speed;
        if (impact_speed < 0x480) {
            SstOnScreenTest(self);
            break;
        }

        player->x_vel = own->speed; // restore Sonic's speed from before SolidObject changed it
        const int16_t(*frag_speeds)[2];
        if (self->x_pos.f.u < player->x_pos.f.u) {
            player->x_pos.f.u += 4; // push Sonic right a bit for pseudo-seamless movement
            frag_speeds = frag_speed_right;
        } else {
            player->x_pos.f.u -= 4 * 2; // push Sonic left a bit (and undo the +4 above)
            frag_speeds = frag_speed_left;
        }

        player->ground_vel = player->x_vel;
        self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;

        SstSmashObject(self, 8, &frag_speeds[0][0]);
        __attribute__((fallthrough)); // parent object is now the first fragment -- routine 4
    }
    case 4: // Fragment
        Sst_MoveSprite2(self);
        self->y_vel += 0x38 * 2; // double gravity

        SstOnScreenTest(self);
        break;
    }
}
