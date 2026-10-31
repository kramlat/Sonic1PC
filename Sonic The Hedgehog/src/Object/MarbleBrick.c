#include "MarbleBrick.h"
#include "Resource/Mappings/MZBricks.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 46 - solid blocks and blocks that fall from the ceiling (MZ)

// fast=true: Type01 (wobble fast, in ceiling, v_oscillate+$16/state[5]).
// fast=false: Type04 (wobble slow, on lava, v_oscillate+$12/state[4],
// further divided by 8).
static void Brick_Wobble(Sst *self, Own_MarbleBrick *own, bool fast) {
    uint8_t subtype = self->subtype;
    int16_t d0 = (int16_t)(oscillatory.state[fast ? 5 : 4][0] >> 8);
    if (!fast)
        d0 >>= 3;
    if (subtype & 8)
        d0 = (int16_t)(-d0 + 0x10);
    self->y_pos.f.u = (int16_t)(own->orig_y - d0);
}

void Obj_MarbleBrick(Sst *self) {
    Own_MarbleBrick *own = (Own_MarbleBrick *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_MZBricks;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 32 / 2;
        self->y_radius = 30 / 2; // obHeight -- ObjFloorDist measures from here
        self->x_radius = 30 / 2; // obWidth
        own->orig_y = self->y_pos.f.u;
        break;
    case 2: { // Action
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            uint8_t subtype = self->subtype & 7;
            switch (subtype) {
            case 0: // static
                break;
            case 2: { // fall when Sonic gets close
                int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
                if (d0 < 0)
                    d0 = -d0;
                if (d0 < 0x90) {
                    self->subtype = 3;
                    break;
                }
                // Not close enough yet -- resume wobbling exactly like
                // Type01, WITHOUT actually becoming Type01 (subtype stays
                // 2 until Sonic gets close enough to trigger the fall).
                Brick_Wobble(self, own, true);
                break;
            }
            case 1: // wobble fast (in ceiling)
                Brick_Wobble(self, own, true);
                break;
            case 3: // fall until floor is hit
                Sst_MoveSprite2(self);
                self->y_vel += 0x18;
                {
                    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
                    if (floor_dist < 0) {
                        self->y_pos.f.u += floor_dist;
                        self->y_vel = 0;
                        own->orig_y = self->y_pos.f.u;

                        // Real hardware (REV01) checks whether the block
                        // landed on a lava-tile block (ID $16A and above,
                        // same Map16 collision index space this project
                        // preserves verbatim from the real ROM's conversion --
                        // only the outer chunk grouping changed) to decide
                        // whether to keep wobbling (Type04) or go fully
                        // static (Type00).
                        const uint8_t *tile = FindNearestTile(NULL, self->x_pos.f.u, self->y_pos.f.u + self->y_radius);
                        uint16_t block_id = ((tile[0] << 8) | tile[1]) & META_TILE;
                        self->subtype = (block_id >= 0x16A) ? 4 : 0;
                    }
                }
                break;
            case 4: // wobble slow (on lava)
                Brick_Wobble(self, own, false);
                break;
            }

            SstSolidObject(self, 32 / 2 + 11 /* sonic_solid_width */, 32 / 2, 34 / 2, self->x_pos.f.u, NULL, NULL);
        }

        SstOnScreenTest(self);
        break;
    }
    }
}
