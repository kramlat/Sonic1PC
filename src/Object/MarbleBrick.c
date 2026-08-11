#include "MarbleBrick.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 46 - solid blocks and blocks that fall from the ceiling (MZ)

// fast=true: Type01 (wobble fast, in ceiling, v_oscillate+$16/state[5]).
// fast=false: Type04 (wobble slow, on lava, v_oscillate+$12/state[4],
// further divided by 8).
static void Brick_Wobble(Object *obj, Scratch_MarbleBrick *scratch, bool fast) {
    uint8_t subtype = obj->scratch.u8[0];
    int16_t d0 = (int16_t)(oscillatory.state[fast ? 5 : 4][0] >> 8);
    if (!fast)
        d0 >>= 3;
    if (subtype & 8)
        d0 = (int16_t)(-d0 + 0x10);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - d0);
}

void Obj_MarbleBrick(Object *obj) {
    Scratch_MarbleBrick *scratch = (Scratch_MarbleBrick *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_MZBricks;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 3;
        obj->width_pixels = 32 / 2;
        scratch->orig_y = obj->pos.l.y.f.u;
        break;
    case 2: { // Action
        if (obj->render.f.on_screen) {
            uint8_t subtype = obj->scratch.u8[0] & 7;
            switch (subtype) {
            case 0: // static
                break;
            case 2: { // fall when Sonic gets close
                int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
                if (d0 < 0)
                    d0 = -d0;
                if (d0 < 0x90) {
                    obj->scratch.u8[0] = 3;
                    break;
                }
                // Not close enough yet -- resume wobbling exactly like
                // Type01, WITHOUT actually becoming Type01 (subtype stays
                // 2 until Sonic gets close enough to trigger the fall).
                Brick_Wobble(obj, scratch, true);
                break;
            }
            case 1: // wobble fast (in ceiling)
                Brick_Wobble(obj, scratch, true);
                break;
            case 3: // fall until floor is hit
                SpeedToPos(obj);
                obj->ysp += 0x18;
                {
                    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
                    if (floor_dist < 0) {
                        obj->pos.l.y.f.u += floor_dist;
                        obj->ysp = 0;
                        scratch->orig_y = obj->pos.l.y.f.u;
                        // Real hardware also checks whether the block landed on
                        // a lava-tile chunk (REV01 only) to decide whether to
                        // keep wobbling (Type04) or go fully static (Type00) --
                        // this project's floor-collision API doesn't expose the
                        // landed chunk ID, so landed bricks always keep the
                        // slow lava-wobble here (closer to the REV01 intent
                        // than always going static).
                        obj->scratch.u8[0] = 4;
                    }
                }
                break;
            case 4: // wobble slow (on lava)
                Brick_Wobble(obj, scratch, false);
                break;
            }

            SolidObject(obj, 32 / 2 + 11 /* sonic_solid_width */, 32 / 2, 34 / 2, obj->pos.l.x.f.u, NULL, NULL);
        }

        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
    }
}
