#include "FloatingBlock.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Oscillatory Routines.h"
#include "Resource/Mappings/FloatingBlock.h"

// Object 56 - floating blocks (SYZ/SLZ), large doors (LZ). One object type
// covering: SYZ blocks that oscillate left-right or up-down, SLZ blocks
// that circle around a square path, and LZ doors that open on a switch
// and (rarely used) close again -- plus one hardcoded special case, the
// horizontally-moving block that blocks off SYZ3's tunnel challenge.

// REV01 addition: permanently remembers that SYZ3's tunnel-blocking block
// has finished travelling, so the "fake" stationary copy at the tunnel
// exit shows instead of the real one (and the real one, now offscreen at
// its start position, stays deleted rather than despawn-cheesing the
// challenge).
bool f_obj56 = false;

static const struct { uint8_t width, height; } FBlock_Var[8] = {
    { 32 / 2, 32 / 2 }, // $0x/$8x - SYZ 1x1 block
    { 64 / 2, 64 / 2 }, // $1x/$9x - SYZ 2x2 square up/down blocks
    { 32 / 2, 64 / 2 }, // $2x/$Ax - SYZ 1x2 door
    { 64 / 2, 52 / 2 }, // $3x/$Bx - SYZ special block moving right in SYZ3
    { 32 / 2, 78 / 2 }, // $4x/$Cx - (unused)
    { 32 / 2, 32 / 2 }, // $5x/$Dx - SLZ rotating stairway block
    { 16 / 2, 64 / 2 }, // $6x/$Ex - LZ small vertical door
    { 128 / 2, 32 / 2 }, // $7x/$Fx - LZ large sideways 4x1 block
};

static void FBlock_MoveLR(Sst *self, Own_FloatingBlock *own, uint8_t osc_byte, int16_t keep_range) {
    int16_t d0 = osc_byte;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + keep_range);
    self->x_pos.f.u = (int16_t)(own->orig_x - d0);
}

static void FBlock_MoveUD(Sst *self, Own_FloatingBlock *own, uint8_t osc_byte, int16_t keep_range) {
    int16_t d0 = osc_byte;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + keep_range);
    self->y_pos.f.u = (int16_t)(own->orig_y - d0);
}

// Port change (not in the original): LZ1's switch-3 door sits across the
// first wind tunnel. The original only disabled the tunnel while Sonic was
// left of the shut door, and re-enabled it the moment the switch was pressed
// while the door was still sliding open -- the tunnel could then push Sonic
// into the door and kill him. Publish "fully open" so LZWindTunnels keeps
// that tunnel off until the door is completely out of the way.
static void FBlock_TrackLZ1TunnelDoor(const Own_FloatingBlock *own) {
    if (LEVEL_ZONE(level_id) == ZoneId_LZ && LEVEL_ACT(level_id) == 0 && own->switch_id == 3)
        f_lz1tunnel_open = (own->distance == 0);
}

static void FBlock_LZSmallDoor_Open(Sst *self, Own_FloatingBlock *own) {
    FBlock_TrackLZ1TunnelDoor(own);
    bool is_lz1 = LEVEL_ZONE(level_id) == ZoneId_LZ && LEVEL_ACT(level_id) == 0;

    if (!own->moving) {
        if (is_lz1 && own->switch_id == 3) {
            f_wtunneldisallow = false;
            if (player->x_pos.f.u < self->x_pos.f.u)
                f_wtunneldisallow = true;
        }

        if (!(f_switch[own->switch_id] & 1)) {
            int16_t d0 = own->distance;
            if (((self->status & SST_STATUS_X_FLIP) != 0))
                d0 = (int16_t)-d0;
            self->y_pos.f.u = (int16_t)(own->orig_y + d0);
            return;
        }

        if (is_lz1 && own->switch_id == 3)
            f_wtunneldisallow = false;

        own->moving = true;
    }

    if (own->distance == 0) {
        self->subtype++; // advance to FBlock_LZSmallDoor_Close
        own->moving = false;
        if (self->respawn)
            (*SstLoad_State(self)) |= 1;
    } else {
        own->distance -= 2;
    }

    int16_t d0 = own->distance;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)-d0;
    self->y_pos.f.u = (int16_t)(own->orig_y + d0);
}

static void FBlock_LZSmallDoor_Close(Sst *self, Own_FloatingBlock *own) {
    FBlock_TrackLZ1TunnelDoor(own);
    if (!own->moving) {
        // Switches with the "alternate flag" (bit 7) don't exist anywhere
        // in the game -- kept faithfully, effectively dead code except for
        // one hardcoded exception elsewhere (DynWater_LZ1_Routine0, not
        // ported).
        if (!((int8_t)f_switch[own->switch_id] < 0)) {
            int16_t d0 = own->distance;
            if (((self->status & SST_STATUS_X_FLIP) != 0))
                d0 = (int16_t)-d0;
            self->y_pos.f.u = (int16_t)(own->orig_y + d0);
            return;
        }
        own->moving = true;
    }

    int16_t full_height = (int16_t)(self->y_radius * 2);
    if (full_height == own->distance) {
        self->subtype--; // back to FBlock_LZSmallDoor_Open
        own->moving = false;
        if (self->respawn)
            (*SstLoad_State(self)) &= ~1;
    } else {
        own->distance += 2;
    }

    int16_t d0 = own->distance;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)-d0;
    self->y_pos.f.u = (int16_t)(own->orig_y + d0);
}

static void FBlock_HorizontalSYZ3(Sst *self, Own_FloatingBlock *own) {
    if (!own->moving) {
        if (f_switch[0xF] == 0)
            return;
        own->moving = true;
        own->distance = 0;
    }

    self->x_pos.f.u++;
    own->orig_x = self->x_pos.f.u;
    own->distance++;
    if (own->distance != 0x380)
        return;

    f_obj56 = true;
    own->moving = false;
    self->subtype = 0; // FBlock_Stationary
}

static void FBlock_LZHorizDoor_Open(Sst *self, Own_FloatingBlock *own) {
    if (!own->moving) {
        if (!(f_switch[own->switch_id] & 1)) {
            int16_t d0 = own->distance;
            if (((self->status & SST_STATUS_X_FLIP) != 0))
                d0 = (int16_t)(-d0 + 128);
            self->x_pos.f.u = (int16_t)(own->orig_x + d0);
            return;
        }
        own->moving = true;
    }

    if (own->distance == 0) {
        self->subtype++; // advance to FBlock_LZHorizDoor_Close
        own->moving = false;
        if (self->respawn)
            (*SstLoad_State(self)) |= 1;
    } else {
        own->distance -= 2;
    }

    int16_t d0 = own->distance;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + 128);
    self->x_pos.f.u = (int16_t)(own->orig_x + d0);
}

static void FBlock_LZHorizDoor_Close(Sst *self, Own_FloatingBlock *own) {
    if (!own->moving) {
        if (!((int8_t)f_switch[own->switch_id] < 0)) {
            int16_t d0 = own->distance;
            if (((self->status & SST_STATUS_X_FLIP) != 0))
                d0 = (int16_t)(-d0 + 128);
            self->x_pos.f.u = (int16_t)(own->orig_x + d0);
            return;
        }
        own->moving = true;
    }

    if (own->distance == 128) {
        self->subtype--; // back to FBlock_LZHorizDoor_Open
        own->moving = false;
        if (self->respawn)
            (*SstLoad_State(self)) &= ~1;
    } else {
        own->distance += 2;
    }

    int16_t d0 = own->distance;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + 128);
    self->x_pos.f.u = (int16_t)(own->orig_x + d0);
}

// Shared by the SLZ rotating-stairway block types 8-B: moves the block
// around the four edges of a square, switching which edge it's on every
// time the driving oscillation value hits a corner (rate == 0).
static void FBlock_SLZStair_MoveSquare(Sst *self, Own_FloatingBlock *own, uint8_t osc_byte, int16_t half_range, bool at_corner) {
    if (at_corner)
        self->status = (uint8_t)((self->status + 1) & 3);

    int16_t d0 = osc_byte;
    int16_t d1 = half_range;

    switch (self->status & 3) {
    case 0: // top edge, moving right
        d0 -= d1;
        self->x_pos.f.u = (int16_t)(own->orig_x + d0);
        self->y_pos.f.u = (int16_t)(own->orig_y - d1);
        break;
    case 1: // x-flipped: right edge, moving down
        d1 -= 1;
        d0 = (int16_t)-(d0 - d1);
        self->y_pos.f.u = (int16_t)(own->orig_y + d0);
        d1 += 1;
        self->x_pos.f.u = (int16_t)(own->orig_x + d1);
        break;
    case 2: // y-flipped: bottom edge, moving left
        d1 -= 1;
        d0 = (int16_t)-(d0 - d1);
        self->x_pos.f.u = (int16_t)(own->orig_x + d0);
        d1 += 1;
        self->y_pos.f.u = (int16_t)(own->orig_y + d1);
        break;
    case 3: // xy-flipped: left edge, moving up
        d0 -= d1;
        self->y_pos.f.u = (int16_t)(own->orig_y + d0);
        self->x_pos.f.u = (int16_t)(own->orig_x - d1);
        break;
    }
}

static void FBlock_Action(Sst *self, Own_FloatingBlock *own) {
    int16_t prev_x = self->x_pos.f.u;

    switch (self->subtype & 0xF) {
    case 0: break; // stationary
    case 1: FBlock_MoveLR(self, own, (uint8_t)(oscillatory.state[2][0] >> 8), 0x20 * 2); break;
    case 2: FBlock_MoveLR(self, own, (uint8_t)(oscillatory.state[7][0] >> 8), 0x40 * 2); break;
    case 3: FBlock_MoveUD(self, own, (uint8_t)(oscillatory.state[2][0] >> 8), 0x20 * 2); break;
    case 4: FBlock_MoveUD(self, own, (uint8_t)(oscillatory.state[7][0] >> 8), 0x40 * 2); break;
    case 5: FBlock_LZSmallDoor_Open(self, own); break;
    case 6: FBlock_LZSmallDoor_Close(self, own); break;
    case 7: FBlock_HorizontalSYZ3(self, own); break;
    case 8: FBlock_SLZStair_MoveSquare(self, own, (uint8_t)((oscillatory.state[10][0] >> 8) >> 1), 0x10, oscillatory.state[10][1] == 0); break;
    case 9: FBlock_SLZStair_MoveSquare(self, own, (uint8_t)(oscillatory.state[11][0] >> 8), 0x30, oscillatory.state[11][1] == 0); break;
    case 0xA: FBlock_SLZStair_MoveSquare(self, own, (uint8_t)(oscillatory.state[12][0] >> 8), 0x50, oscillatory.state[12][1] == 0); break;
    case 0xB: FBlock_SLZStair_MoveSquare(self, own, (uint8_t)(oscillatory.state[13][0] >> 8), 0x70, oscillatory.state[13][1] == 0); break;
    case 0xC: FBlock_LZHorizDoor_Open(self, own); break;
    case 0xD: FBlock_LZHorizDoor_Close(self, own); break;
    }

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
        SstSolidObject(self, (uint16_t)x_rad, self->y_radius, (uint16_t)(self->y_radius + 1), prev_x, NULL, NULL);
    }

    if (SstOutOfRange(own->orig_x)) {
        if (self->subtype == 0x37 && own->moving) {
            Sst_Draw(self);
            return;
        }
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

static bool FBlock_Main(Sst *self, Own_FloatingBlock *own) {
    self->routine += 2;
    self->mappings = Mappings_FloatingBlock;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3

    bool is_lz = LEVEL_ZONE(level_id) == ZoneId_LZ;
    if (is_lz)
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Door); // | Tile_Pal3

    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;

    uint8_t subtype = self->subtype;
    uint8_t index = (subtype >> 4) & 7;
    self->width_pixels = FBlock_Var[index].width;
    self->y_radius = (int8_t)FBlock_Var[index].height;
    self->mapping_frame = index;

    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;
    own->distance = (int16_t)(FBlock_Var[index].height * 2);

    // REV01 addition: SYZ3's horizontally moving block, see f_obj56 comment.
    if (subtype == 0x37) {
        if (self->x_pos.f.u == 0x1BB8) { // the "real" block
            if (f_obj56) {
                Sst_Delete(self);
                return false;
            }
        } else { // the "fake" stationary block at the tunnel exit
            self->subtype = 0; // force FBlock_Stationary
            if (!f_obj56) {
                Sst_Delete(self);
                return false;
            }
        }
    }

    if (!is_lz) {
        uint8_t d0 = (uint8_t)(self->subtype & 0xF);
        if (d0 >= 8) {
            int oi = 10 + (d0 - 8);
            if ((int16_t)oscillatory.state[oi][1] < 0)
                self->status ^= SST_STATUS_X_FLIP;
        }
    }

    // Shared "switch-activated LZ door" check -- subtype bit 7 set
    uint8_t d0 = self->subtype;
    if ((int8_t)d0 >= 0)
        return true; // not switch-activated

    own->switch_id = d0 & 0xF;
    self->subtype = 5; // FBlock_LZSmallDoor_Open
    if (index == 7) { // large sideways 4x1 door
        self->subtype = 0xC; // FBlock_LZHorizDoor_Open
        own->distance = 128;
    }

    if (self->respawn) {
        (*SstLoad_State(self)) &= 0x7F;
        if ((*SstLoad_State(self)) & 1) {
            self->subtype++; // already opened -- start already at the "close" type
            own->distance = 0;
        }
    }

    return true;
}

void Obj_FloatingBlock(Sst *self) {
    Own_FloatingBlock *own = (Own_FloatingBlock *)self->own.u8;

    switch (self->routine) {
    case 0:
        if (!FBlock_Main(self, own))
            return;
        __attribute__((fallthrough));
    case 2:
        FBlock_Action(self, own);
        break;
    }
}
