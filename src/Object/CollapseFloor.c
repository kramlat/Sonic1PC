#include "CollapseFloor.h"

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

static void CFlo_WalkOff(Object *obj) {
    int16_t prev_x = obj->pos.l.x.f.u;
    ExitPlatform(obj, 64 / 2, 64 / 2, NULL);
    MvSonicOnPtfm(obj, obj->pos.l.y.f.u - 8, prev_x);
    RememberState(obj); // real hardware's own WalkOff always tail-calls RememberState
}

void Obj_CollapseFloor(Object *obj) {
    Scratch_CollapseFloor *scratch = (Scratch_CollapseFloor *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_CollapsingFloors;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3
        if (LEVEL_ZONE(level_id) == ZoneId_SLZ) {
            obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Collapsing_Floor); // | Tile_Pal3
            obj->frame += 2;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
            obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Collapsing_Floor); // | Tile_Pal3
        }
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 4;
        scratch->timedelay = 7;
        obj->width_pixels = 136 / 2;
        break;
    case 2: { // ChkTouch
        if (scratch->flag) {
            if (scratch->timedelay == 0) {
                scratch->flag = 0;
                obj->frame++; // advance to next frame, 8 sprite pieces
                FragmentatePlatform(obj, 8, floor_collapse_data);
                break;
            }
            scratch->timedelay--;
        }

        PlatformObject(obj, 64 / 2); // sets routine to 4 on touch

        // SLZ-only (subtype's own top bit set): mirror the collapse
        // pattern to match which side of the platform Sonic is currently
        // standing on.
        if (obj->scratch.s8[0] < 0 && player->status.p.f.object_stand)
            obj->render.f.x_flip = (player->pos.l.x.f.u < obj->pos.l.x.f.u);
        RememberState(obj);
        break;
    }
    case 4: // OnPlatform
        if (scratch->timedelay == 0) {
            obj->frame++;
            FragmentatePlatform(obj, 8, floor_collapse_data);
            break;
        }
        scratch->flag = 1;
        scratch->timedelay--;
        // Fallthrough
    case 10: // WalkOff
        CFlo_WalkOff(obj);
        break;
    case 6: { // FragmentPiece
        if (scratch->timedelay != 0) {
            if (!scratch->flag) {
                scratch->timedelay--;
                DisplaySprite(obj);
                break;
            }

            scratch->timedelay--;
            CFlo_WalkOff(obj);

            if (player->status.p.f.object_stand) {
                if (scratch->timedelay == 0) {
                    player->status.p.f.object_stand = false;
                    player->status.p.f.pushing = false;
                    player->prev_anim = SonAnimId_Run;
                } else {
                    break;
                }
            }

            scratch->flag = 0;
            obj->routine = 6;
            break;
        }

        ObjectFall(obj);
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
    case 8: // Delete
        ObjectDelete(obj);
        break;
    }
}
