#include "SmashBlock.h"
#include "ObjectSst.h"
#include "Object/Points.h"
#include "Object.h"
#include "Resource/Mappings/SmashableGreenBlock.h"

#include "Level.h"
#include "SstLoad.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 51 - smashable green block (MZ)

static const int16_t smab_speeds[4][2] = {
    { -0x200, -0x200 },
    { -0x100, -0x100 },
    { 0x200, -0x200 },
    { 0x100, -0x100 },
};

// Points per smashed block (combo chain), in tens (AddPoints' unit) -- 16th and later blocks
// are hardcoded to 10000 regardless of this table.
static const uint16_t smab_scores[4] = { 10, 20, 50, 100 };

void Obj_SmashBlock(Sst *self) {
    Own_SmashBlock *own = (Own_SmashBlock *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_SmashableGreenBlock;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 4 * 0x80;
        break;
    case 2: { // Solid
        own->combo = item_bonus;
        own->sonic_anim = player->anim;

        SstSolidObject(self, 32 / 2 + 11 /* sonic_solid_width */, 32 / 2, 34 / 2, self->x_pos.f.u, NULL, NULL);
        if (!((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
            SstOnScreenTest(self);
            break;
        }
        if (own->sonic_anim != SonAnimId_Roll) {
            SstOnScreenTest(self);
            break;
        }

        item_bonus = own->combo;

        player->status |= SST_PSTATUS_IN_BALL;
        player->y_radius = 28 / 2; // sonic_roll_height
        player->x_radius = 14 / 2; // sonic_roll_width
        player->anim = SonAnimId_Roll;
        player->y_vel = -0x300;
        player->status |= SST_PSTATUS_IN_AIR;
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        player->routine = 2; // force Sonic to Sonic_Control
        self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;

        // Two mapping frames exist -- 2-piece (normal) and 4-piece
        // (fragmentation effect) -- swap to the 4-piece one right before
        // smashing.
        self->mapping_frame = 1;

        SstSmashObject(self, 4, &smab_speeds[0][0]);

        Sst *points = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
        if (points != NULL) {

            uint16_t combo = item_bonus;
            item_bonus += 2; // word-based combo table below, so +2 per hit
            if (combo >= 3 * 2)
                combo = 3 * 2;

            uint16_t points_tens = smab_scores[combo / 2];
            uint8_t frame = (uint8_t)(combo / 2);
            if (item_bonus >= 16 * 2) {
                points_tens = 1000;
                frame = 5;
            }
            AddPoints(points_tens);
            points->mapping_frame = frame;
        }
        __attribute__((fallthrough)); // parent object is now the first fragment (routine 4)
    }
    case 4: // Fragment
        Sst_MoveSprite2(self);
        self->y_vel += 0x38; // gravity

        SstOnScreenTest(self);
        break;
    }
}
