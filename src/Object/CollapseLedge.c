#include "CollapseLedge.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Sloped collision heightmap for the ledge's own 96px width
static const uint8_t ledge_slope_data[48] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x24,
    0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28,
    0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C,
    0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};

// Per-fragment fall delays (25 pieces, matching sprite order)
static const uint8_t ledge_collapse_data[25] = {
    0x1C, 0x18, 0x14, 0x10,
    0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x06,
    0x18, 0x14, 0x10, 0x0C, 0x08, 0x04,
    0x16, 0x12, 0x0E, 0x0A, 0x06, 0x02,
    0x14, 0x10, 0x0C,
};

static void Ledge_WalkOff(Object *obj) {
    ExitPlatform(obj, 96 / 2, 96 / 2, NULL);
    // Real ASM calls SlopeObject_AssumeStoodOn here, not plain SlopeObject --
    // this just repositions Sonic to match the slope, it does NOT re-run
    // collision detection or re-trigger Platform3's routine advance. Ledge_WalkOff
    // runs every frame from routines 4, 6, and 10 (WalkOff itself), so calling
    // the collision-detecting SlopeObject here made Platform3 re-fire and bump
    // obj->routine +2 on every single call, marching the object from 4 straight
    // through 6 to 8 (delete) in a couple of frames instead of actually crumbling.
    SlopeObject_AssumeStoodOn(obj, 96 / 2, ledge_slope_data, obj->pos.l.x.f.u);
    RememberState(obj);
}

void Obj_CollapseLedge(Object *obj) {
    Scratch_CollapseLedge *scratch = (Scratch_CollapseLedge *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_CollapsingLedge;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level);
        obj->render.b &= 0x03;
        obj->render.f.align_fg = true;
        obj->priority = 4;
        scratch->timedelay = 7;
        obj->width_pixels = 96 / 2;
        obj->frame = obj->scratch.u8[0];
        obj->y_rad = 112 / 2;
        obj->render.f.yrad_height = true;
        break;

    case 2: // ChkTouch
        if (scratch->flag) {
            if (scratch->timedelay == 0) {
                scratch->flag = 0;
                obj->frame += 2;
                 FragmentatePlatform(obj, 25, ledge_collapse_data);
                break;  // Fragmentate handles the rest
            }
            scratch->timedelay--;
        }
        SlopeObject(obj, 96 / 2, ledge_slope_data);
        RememberState(obj);
        break;

case 4: // OnPlatform
    if (scratch->timedelay == 0) {
        obj->frame += 2;
        FragmentatePlatform(obj, 25, ledge_collapse_data);
        break;  // ← CRITICAL: don't fall through to WalkOff
    }
    scratch->flag = 1;
    scratch->timedelay--;
    // Fallthrough to WalkOff (only when timedelay > 0)

case 10: // WalkOff
        Ledge_WalkOff(obj);
        break;

case 6: { // FragmentPiece — matches ASM's Ledge_FragmentPiece
    if (scratch->timedelay != 0) {
        if (!scratch->flag) {
            scratch->timedelay--;
            DisplaySprite(obj);
            break;
        }

        // .delayCollapse:
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

        // .startCollapse:
        scratch->flag = 0;
        obj->routine = 6;
        break;
    }

    // .fragmentFall:
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
