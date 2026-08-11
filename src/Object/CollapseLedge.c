#include "CollapseLedge.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 1A - collapsing ledge (GHZ)

// Sloped collision heightmap for the ledge's own 96px width, one entry
// per 2px column (see SlopeObject's own comment on the >>1 index).
// Diagram (not to scale): flat at $20, ascending $21-$2F (each step
// shown twice), flat at $30.
static const uint8_t ledge_slope_data[48] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27,
    0x28, 0x28, 0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};

// Per-fragment fall delays, 25 pieces matching the ledge's own sprite
// piece order.
static const uint8_t ledge_collapse_data[25] = {
    0x1C, 0x18, 0x14, 0x10,             //
    0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x06, //
    0x18, 0x14, 0x10, 0x0C, 0x08, 0x04, //
    0x16, 0x12, 0x0E, 0x0A, 0x06, 0x02, //
    0x14, 0x10, 0x0C,                   //
};

static void Ledge_WalkOff(Object *obj) {
    ExitPlatform(obj, 96 / 2, 96 / 2, NULL);
    SlopeObject(obj, 96 / 2, ledge_slope_data);
    RememberState(obj); // real hardware's own WalkOff always tail-calls RememberState, even when reached via a call (not just the routine-4/10 fallthrough)
}

static void Fragmentate(Object *obj, Scratch_CollapseLedge *scratch) {
    scratch->flag = 0;
    obj->frame += 2; // advance two frames, which consist of 25 sprite pieces
    FragmentatePlatform(obj, 25, ledge_collapse_data);
}

void Obj_CollapseLedge(Object *obj) {
    Scratch_CollapseLedge *scratch = (Scratch_CollapseLedge *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_CollapsingLedge;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3
        obj->render.b &= 0x03; // keep x_flip/y_flip from level placement -- real hardware ORs in align_fg here instead of overwriting, specifically so mirrored ledges stay mirrored
        obj->render.f.align_fg = true;
        obj->priority = 4;
        scratch->timedelay = 7;
        obj->width_pixels = 96 / 2;
        obj->frame = obj->scratch.u8[0]; // subtype (0 or 1)
        obj->y_rad = 112 / 2;
        obj->render.f.yrad_height = true;
        // Fallthrough
    case 2: // ChkTouch
        if (scratch->flag) {
            if (scratch->timedelay == 0) {
                Fragmentate(obj, scratch);
                break;
            }
            scratch->timedelay--;
        }
        SlopeObject(obj, 96 / 2, ledge_slope_data); // sets routine to 4 on touch
        RememberState(obj);
        break;
    case 4: // OnPlatform
        if (scratch->timedelay == 0) {
            // begin fragmentation without resetting the flag
            obj->frame += 2;
            FragmentatePlatform(obj, 25, ledge_collapse_data);
            break;
        }
        scratch->flag = 1;
        scratch->timedelay--;
        // Fallthrough
    case 10: // WalkOff
        Ledge_WalkOff(obj);
        break;
    case 6: { // FragmentPiece
        if (scratch->timedelay != 0) {
            if (!scratch->flag) {
                scratch->timedelay--;
                DisplaySprite(obj);
                break;
            }

            scratch->timedelay--;
            Ledge_WalkOff(obj);

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
