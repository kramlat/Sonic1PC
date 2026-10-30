#include "MovingBlock.h"
#include "Object.h"
#include "Resource/Mappings/LZMovingBlocks.h"
#include "Resource/Mappings/MovingBlocks.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 52 - moving platform blocks (MZ, LZ, SBZ)

// width, frame -- indexed by subtype's own upper nibble
static const struct {
    uint8_t width;
    uint8_t frame;
} mblock_var[5] = {
    { 32 / 2, 0 },  // $0x - MZ single block / LZ small raft
    { 64 / 2, 1 },  // $1x - MZ double block (unused)
    { 64 / 2, 2 },  // $2x - SBZ short (yellow/black striped)
    { 128 / 2, 3 }, // $3x - SBZ long (red sliding floors)
    { 96 / 2, 4 },  // $4x - MZ triple block
};

// Returns true for Type 7 (MBlock_SecretLZ1Raft) specifically -- the
// caller must then skip both platform interaction and display for this
// frame entirely, matching real hardware's own early-return-past-the-
// caller behavior (still reached even on the same frame the switch
// press advances it to Type 4, since the dispatch below is on the
// subtype value AT CALL TIME, before this function's own mutation).
static bool MBlock_Move(Sst *self, Own_MovingBlock *own) {
    uint8_t subtype = self->subtype & 0xF;
    switch (subtype) {
    case 0: // stationary
        break;
    case 1: { // moves left and right continuously
        int16_t d0 = (int16_t)(oscillatory.state[3][0] >> 8); // v_oscillate+$E, frequency 2, middle $30
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(-d0 + 0x60);
        self->x_pos.f.u = (int16_t)(own->orig_x - d0);
        break;
    }
    case 2: // stationary, advances to next subtype (3) when stood on
    case 4: // stationary, advances to next subtype (5) when stood on
    case 9: // stationary, advances to next subtype (A) when stood on
        if (self->routine == 4)
            self->subtype++;
        break;
    case 3: { // moves right, stops (type 0) on wall hit
        int16_t d1 = SstHitWallRight(self, self->width_pixels);
        if (d1 < 0) {
            self->subtype = 0;
            break;
        }
        self->x_pos.f.u += 1;
        own->orig_x = self->x_pos.f.u;
        break;
    }
    case 5: { // moves right, falls (type 6) on wall hit
        int16_t d1 = SstHitWallRight(self, self->width_pixels);
        if (d1 < 0) {
            self->subtype = 6;
            break;
        }
        self->x_pos.f.u += 1;
        own->orig_x = self->x_pos.f.u;
        break;
    }
    case 6: { // falls, stops (type 0) on floor hit
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist < 0) {
            self->y_pos.f.u += floor_dist;
            self->y_vel = 0;
            self->subtype = 0;
        }
        break;
    }
    case 7: // appears when switch 2 is pressed (secret LZ1 raft shortcut)
        if (f_switch[2])
            self->subtype = 4;
        return true;
    case 8: { // moves up and down continuously
        int16_t d0 = (int16_t)(oscillatory.state[7][0] >> 8); // v_oscillate+$1E, frequency 4, middle $40
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(-d0 + 0x80);
        self->y_pos.f.u = (int16_t)(own->orig_y - d0);
        break;
    }
    case 0xA: { // SBZ red sliding floor: quickly slides out, then back after a delay
        int16_t width2 = self->width_pixels * 2;
        int16_t step = 8;
        if (((self->status & SST_STATUS_X_FLIP) != 0)) {
            step = -8;
            width2 = -width2;
        }
        if (own->slide_goback == 0) {
            int16_t d0 = (int16_t)(self->x_pos.f.u - own->orig_x);
            if (d0 == width2) {
                if (--own->slide_wait == 0)
                    own->slide_goback = 1;
                break;
            }
            self->x_pos.f.u += step;
            own->slide_wait = 5 * 60;
            break;
        }
        {
            int16_t d0 = (int16_t)(self->x_pos.f.u - own->orig_x);
            if (d0 == 0) {
                own->slide_goback = 0;
                self->subtype = 9; // wait for Sonic to step on it again
                break;
            }
            self->x_pos.f.u -= step;
        }
        break;
    }
    }
    return false;
}

void Obj_MovingBlock(Sst *self) {
    Own_MovingBlock *own = (Own_MovingBlock *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main
        self->routine += 2;
        self->mappings = Mappings_MovingBlocks;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3

        if (LEVEL_ZONE(level_id) == ZoneId_LZ) {
            self->mappings = Mappings_LZMovingBlocks;
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Moving_Block); // | Tile_Pal3
            self->y_radius = 14 / 2;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
            self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Moving_Block_Short); // | Tile_Pal2
            if (self->subtype != 0x28)
                self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Moving_Block_Long); // | Tile_Pal3
        }

        self->render_flags = SST_RENDER_LEVEL;

        uint8_t index = self->subtype >> 4;
        if (index >= 5)
            index = 0;
        self->width_pixels = mblock_var[index].width;
        self->mapping_frame = mblock_var[index].frame;

        self->priority = 4 * 0x80;
        own->orig_x = self->x_pos.f.u;
        own->orig_y = self->y_pos.f.u;
        self->subtype &= 0xF;
        break;
    }
    case 2: // Platform
        if (!MBlock_Move(self, own)) {
            SstPlatformObject(self, self->width_pixels);
            SstOnScreenTest2(self, own->orig_x);
        } else SstDeleteIfNotInRange2(self, own->orig_x);
        break;
    case 4: { // StandOn
        SstExitPlatform(self, self->width_pixels, self->width_pixels, NULL);
        int16_t prev_x = self->x_pos.f.u;
        if (!MBlock_Move(self, own)) {
            SstMvSonicOnPtfm(self, self->y_pos.f.u - 8, prev_x);
            SstOnScreenTest2(self, own->orig_x);
        } else SstDeleteIfNotInRange2(self, own->orig_x);
        break;
    }
    }
}
