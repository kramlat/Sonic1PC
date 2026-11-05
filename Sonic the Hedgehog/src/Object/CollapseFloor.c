#include "CollapseFloor.h"
#include "Object.h"
#include "Resource/Mappings/CollapsingFloors.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 53 - collapsing floors (MZ, SLZ, SBZ)

// Per-fragment fall delays, 8 pieces. Real hardware also has an unused
// "swipe" (left-to-right) variant -- per its own comment, never actually
// used by any real level, since the shuffled order looked better and the
// devs used it everywhere -- so only the shuffled table is ported.
static const uint8_t floor_collapse_data[8] = {
    0x16, 0x1E, 0x1A, 0x12, 0x06, 0x0E, 0x0A, 0x02,
};

static void CFlo_WalkOff(Sst *self) {
    int16_t prev_x = self->x_pos.f.u;
    SstExitPlatform(self, 64 / 2, 64 / 2, NULL);
    SstMvSonicOnPtfm(self, self->y_pos.f.u - 8, prev_x);
    SstOnScreenTest(self); // real hardware's own WalkOff always tail-calls RememberState
}

void Obj_CollapseFloor(Sst *self) {
    Own_CollapseFloor *own = (Own_CollapseFloor *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_CollapsingFloors;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3
        if (LEVEL_ZONE(level_id) == ZoneId_SLZ) {
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Collapsing_Floor); // | Tile_Pal3
            self->mapping_frame += 2;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Collapsing_Floor); // | Tile_Pal3
        }
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->timedelay = 7;
        self->width_pixels = 136 / 2;
        break;
    case 2: { // ChkTouch
        if (own->flag) {
            if (own->timedelay == 0) {
                own->flag = 0;
                self->mapping_frame++; // advance to next frame, 8 sprite pieces
                SstFragmentatePlatform(self, 8, floor_collapse_data);
                break;
            }
            own->timedelay--;
        }

        SstPlatformObject(self, 64 / 2); // sets routine to 4 on touch

        // SLZ-only (subtype's own top bit set): mirror the collapse
        // pattern to match which side of the platform Sonic is currently
        // standing on.
        if (self->own.s8[0] < 0 && ((player->status & SST_PSTATUS_OBJECT_STAND) != 0))
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, (player->x_pos.f.u < self->x_pos.f.u));
        SstOnScreenTest(self);
        break;
    }
    case 4: // OnPlatform
        if (own->timedelay == 0) {
            self->mapping_frame++;
            SstFragmentatePlatform(self, 8, floor_collapse_data);
            break;
        }
        own->flag = 1;
        own->timedelay--;
        // Fallthrough
    case 10: // WalkOff
        CFlo_WalkOff(self);
        break;
    case 6: { // FragmentPiece
        if (own->timedelay != 0) {
            if (!own->flag) {
                own->timedelay--;
                Sst_Draw(self);
                break;
            }

            own->timedelay--;
            CFlo_WalkOff(self);

            if (((player->status & SST_PSTATUS_OBJECT_STAND) != 0)) {
                if (own->timedelay == 0) {
                    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                    player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    player->prev_anim = SonAnimId_Run;
                } else {
                    break;
                }
            }

            own->flag = 0;
            self->routine = 6;
            break;
        }

        Sst_MoveSprite(self);
        SstOnScreenTest2(self, self->x_pos.f.u);
        break;
    }
    case 8: // Delete
        Sst_Delete(self);
        break;
    }
}
