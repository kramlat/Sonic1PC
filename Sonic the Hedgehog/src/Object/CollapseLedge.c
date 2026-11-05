#include "CollapseLedge.h"
#include "Object.h"
#include "Resource/Mappings/CollapsingLedge.h"
#include "SstLoad.h"
#include "ObjectSst.h"

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

static void Ledge_WalkOff(Sst *self) {
    SstExitPlatform(self, 96 / 2, 96 / 2, NULL);
    // Real ASM calls SlopeObject_AssumeStoodOn here, not plain SlopeObject --
    // this just repositions Sonic to match the slope, it does NOT re-run
    // collision detection or re-trigger Platform3's routine advance. Ledge_WalkOff
    // runs every frame from routines 4, 6, and 10 (WalkOff itself), so calling
    // the collision-detecting SlopeObject here made Platform3 re-fire and bump
    // self->routine +2 on every single call, marching the object from 4 straight
    // through 6 to 8 (delete) in a couple of frames instead of actually crumbling.
    SstSlopeObject_AssumeStoodOn(self, 96 / 2, ledge_slope_data, self->x_pos.f.u);
    SstOnScreenTest(self);
}

void Obj_CollapseLedge(Sst *self) {
    Own_CollapseLedge *own = (Own_CollapseLedge *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_CollapsingLedge;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->timedelay = 7;
        self->width_pixels = 96 / 2;
        self->mapping_frame = self->subtype;
        self->y_radius = 112 / 2;
        self->height_pixels = self->y_radius;
        break;

    case 2: // ChkTouch
        if (own->flag) {
            if (own->timedelay == 0) {
                own->flag = 0;
                self->mapping_frame += 2;
                 SstFragmentatePlatform(self, 25, ledge_collapse_data);
                break;  // Fragmentate handles the rest
            }
            own->timedelay--;
        }
        SstSlopeObject(self, 96 / 2, ledge_slope_data);
        SstOnScreenTest(self);
        break;

case 4: // OnPlatform
    if (own->timedelay == 0) {
        self->mapping_frame += 2;
        SstFragmentatePlatform(self, 25, ledge_collapse_data);
        break;  // ← CRITICAL: don't fall through to WalkOff
    }
    own->flag = 1;
    own->timedelay--;
    // Fallthrough to WalkOff (only when timedelay > 0)

case 10: // WalkOff
        Ledge_WalkOff(self);
        break;

case 6: { // FragmentPiece — matches ASM's Ledge_FragmentPiece
    if (own->timedelay != 0) {
        if (!own->flag) {
            own->timedelay--;
            Sst_Draw(self);
            break;
        }

        // .delayCollapse:
        own->timedelay--;
        Ledge_WalkOff(self);

        if (((player->status & SST_PSTATUS_OBJECT_STAND) != 0)) {
            if (own->timedelay == 0) {
                player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                player->prev_anim = SonAnimId_Run;
            } else {
                break;
            }
        }

        // .startCollapse:
        own->flag = 0;
        self->routine = 6;
        break;
    }

    // .fragmentFall:
    Sst_MoveSprite(self);
    SstOnScreenTest2(self, self->x_pos.f.u);
    break;
}
    case 8: // Delete
        Sst_Delete(self);
        break;
    }
}
