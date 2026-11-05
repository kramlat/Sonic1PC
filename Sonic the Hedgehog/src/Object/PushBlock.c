#include "PushBlock.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "GeyserMaker.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/PushableBlocks.h"
#include "Sound.h"

// Object 33 - pushable blocks (MZ, available but unused in LZ). A plain
// solid platform that Sonic can also push horizontally, PLUS (MZ-only)
// the ability to fall onto lava and float/sink in it, with two entirely
// hardcoded behaviors layered on top: MZ1's own stomper-lifting button
// (coordinates with ChainStomp via the shared obj31_ypos global) and
// MZ2/MZ3's lava geysers, spawned as the block drifts past specific fixed
// X-positions while floating.

static const struct {
    uint8_t width, frame;
} pushb_vars[2] = {
    { 32 / 2, 0 },  // 1x1 block
    { 128 / 2, 1 }, // 4x1 block
};

static void PushB_ChkVisible(Sst *self, Own_PushBlock *own);
static void PushB_Display(Sst *self, Own_PushBlock *own);
static void PushB_SolidAction(Sst *self, Own_PushBlock *own, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x);

static void PushB_ChkWithinOrigin(Sst *self, Own_PushBlock *own) {
    if (SstOutOfRange(own->orig_x)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= (uint8_t)~0x01;
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }

    self->x_pos.f.u = own->orig_x;
    self->y_pos.f.u = own->orig_y;
    self->routine = 4;
    PushB_ChkVisible(self, own);
}

static void PushB_Display(Sst *self, Own_PushBlock *own) {
    if (SstOutOfRange(self->x_pos.f.u)) {
        PushB_ChkWithinOrigin(self, own);
        return;
    }
    Sst_Draw(self);
}

// Hardcoded to specific X-positions in MZ2 and MZ3 -- spawns a lava
// geyser maker just ahead of the block as it drifts along on lava.
static void PushB_SpawnLavaGeysers(Sst *self) {
    int16_t offset;
    int16_t x = self->x_pos.f.u;

    if (level_id == LEVEL_ID(ZoneId_MZ, 1)) { // act2 -- block moves left
        offset = -32;
        if (x != 0xDD0 && x != 0xCC0 && x != 0xBA0)
            return;
    } else if (level_id == LEVEL_ID(ZoneId_MZ, 2)) { // act3 -- block moves right
        offset = 32;
        if (x != 0x560 && x != 0x5C0)
            return;
    } else {
        return;
    }

    Sst *geyser = Sst_Allocate();
    if (geyser == NULL)
        return;
    geyser->code = Obj_GeyserMaker;
    geyser->x_pos.f.u = (int16_t)(self->x_pos.f.u + offset);
    geyser->y_pos.f.u = (int16_t)(self->y_pos.f.u + 16);
    ((Own_GeyserMaker *)geyser->own.u8)->parent_index = (uint8_t)(self - sst);
}

// Is the 16x16 chunk the block just landed on a lava tile? (IDs $16A and
// above.)
static bool PushB_LandedOnLava(Sst *self) {
    const uint8_t *tile = FindNearestTile(NULL, self->x_pos.f.u, (int16_t)(self->y_pos.f.u + self->y_radius));
    uint16_t word = (uint16_t)((tile[0] << 8) | tile[1]);
    return (word & META_TILE) >= 0x16A;
}

static void PushB_SolidAction_NotOnPlatform(Sst *self, Own_PushBlock *own, uint16_t x_rad, uint16_t y_rad) {
    int16_t x_off, y_off;
    int32_t collision = SstSolid_ChkEnter(self, x_rad, y_rad, &x_off, &y_off);
    if (collision <= 0)
        return; // no touch, or Sonic landed on top/got squished underneath (handled inside already)
    if (own->on_lava)
        return;
    if (x_off == 0)
        return;

    int16_t sonic_dx, ground_speed;
    if (x_off < 0) {
        // Sonic is on the right, pushing the block left -- requires facing left.
        if (!((player->status & 1) != 0))
            return;
        if (SstHitWallLeft(self, (int16_t)(~(uint16_t)self->width_pixels)) < 0)
            return;
        self->x_pos.v -= 0x10000;
        sonic_dx = -1;
        ground_speed = -0x40;
    } else {
        // Sonic is on the left, pushing the block right -- requires facing right.
        if (((player->status & 1) != 0))
            return;
        if (SstHitWallRight(self, self->width_pixels) < 0)
            return;
        self->x_pos.v += 0x10000;
        sonic_dx = 1;
        ground_speed = 0x40;
    }

    player->x_pos.f.u = (int16_t)(player->x_pos.f.u + sonic_dx);
    player->ground_vel = ground_speed;
    player->x_vel = 0;

    QueueSound2(sfx_Push); // hardcoded in the sound driver to not be interruptible by itself

    if (self->subtype & 0x80)
        return; // block is resting on the MZ1 stomper -- don't let it fall off

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist <= 4) {
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + floor_dist);
        return;
    }

    self->x_vel = 0x400; // quickly slide off the ledge it just walked off
    if (x_off < 0)
        self->x_vel = (int16_t)-self->x_vel;
    self->routine_secondary = 6; // snap to a clean 16px grid position before falling
}

static void PushB_SolidAction(Sst *self, Own_PushBlock *own, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x) {
    switch (self->routine_secondary) {
    case 2: { // Sonic is standing on the block
        int16_t x_off;
        if (SstExitPlatform(self, x_rad, x_rad, &x_off))
            self->routine_secondary = 0; // released -- back to "not on platform" so pushing works again
        else
            SstMvSonicOnPtfm(self, (int16_t)(self->y_pos.f.u - y_rad2), prev_x);
        return;
    }
    case 4: { // falling
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;

        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist >= 0)
            return;
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + floor_dist);
        self->y_vel = 0;
        self->routine_secondary = 0;

        if (PushB_LandedOnLava(self)) {
            self->x_vel = (int16_t)(own->lava_speed >> 3); // $400/8 = $80
            own->on_lava = true;
            self->y_pos.f.l = 0; // needed for sinking in lava later
        }
        return;
    }
    case 6: { // snapping to a 16px grid position before falling
        Sst_MoveSprite2(self);
        if ((self->x_pos.f.u & 0xC) != 0)
            return;
        self->x_pos.f.u &= ~0xF;
        own->lava_speed = self->x_vel; // remembered in case it lands on lava
        self->x_vel = 0;
        self->routine_secondary -= 2; // -> falling
        return;
    }
    default: // not on platform
        PushB_SolidAction_NotOnPlatform(self, own, x_rad, y_rad1);
        return;
    }
}

static void PushB_LavaPlatform(Sst *self, Own_PushBlock *own, int16_t prev_x) {
    int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
    PushB_SolidAction(self, own, (uint16_t)x_rad, 32 / 2, 34 / 2, prev_x);
    PushB_SpawnLavaGeysers(self);
    PushB_Display(self, own);
}

static void PushB_Sunken(Sst *self, Own_PushBlock *own) {
    player->status &= (uint8_t)~8;
    self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    PushB_ChkWithinOrigin(self, own);
}

// Handles the block once it's fallen onto lava: getting launched by a
// geyser underneath it, drifting until it hits a wall, and slowly sinking
// once it stops moving.
static void PushB_OnLava(Sst *self, Own_PushBlock *own) {
    // Captured BEFORE SpeedToPos moves the block via its own drift velocity
    // below -- MvSonicOnPtfm (inside PushB_SolidAction) needs the block's
    // PRE-drift X to compute how far it moved this frame and carry Sonic
    // along by that same amount. Capturing it any later (e.g. inside
    // PushB_LavaPlatform, after the block has already moved) makes the
    // carry delta always zero, so Sonic never rides a drifting block --
    // it just slides out from under him every frame it's moving.
    int16_t prev_x = self->x_pos.f.u;

    if (self->routine_secondary < 4)
        Sst_MoveSprite2(self); // routine_sec 4/6 do their own SpeedToPos inside PushB_SolidAction

    if (self->status & 0x02) { // getting shot up by a lava geyser (set by GeyserMaker)
        self->y_vel += 0x18;

        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist < 0) {
            self->y_pos.f.u = (int16_t)(self->y_pos.f.u + floor_dist);
            self->y_vel = 0;
            self->status &= (uint8_t)~0x02;

            if (PushB_LandedOnLava(self)) {
                self->x_vel = (int16_t)(own->lava_speed >> 3);
                own->on_lava = true;
                self->y_pos.f.l = 0;
            }
        }
        PushB_LavaPlatform(self, own, prev_x);
        return;
    }

    if (self->x_vel == 0) {
        // Sunk further into the lava -- the sink depth is tracked in the
        // low byte of the Y-position's own subpixel fraction, so this
        // sinks impossibly slowly (a fraction of a pixel per frame).
        self->y_pos.v += 0x2000 + 1;
        if ((uint8_t)self->y_pos.f.l < 160) {
            PushB_LavaPlatform(self, own, prev_x);
            return;
        }
        PushB_Sunken(self, own);
        return;
    }

    int16_t hit = (self->x_vel < 0) ? SstHitWallLeft(self, (int16_t)(~(uint16_t)self->width_pixels))
                                  : SstHitWallRight(self, self->width_pixels);
    if (hit < 0)
        self->x_vel = 0; // hit a wall -- stop moving, don't start sinking until next frame

    PushB_LavaPlatform(self, own, prev_x);
}

static void PushB_Action(Sst *self, Own_PushBlock *own) {
    if (own->on_lava) {
        PushB_OnLava(self, own);
        return;
    }

    int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
    PushB_SolidAction(self, own, (uint16_t)x_rad, 32 / 2, 34 / 2, self->x_pos.f.u);

    // Hardcoded MZ1 stuff for the block that pushes down a button to lift a spiked stomper.
    if (level_id == LEVEL_ID(ZoneId_MZ, 0)) {
        self->subtype &= 0x7F; // clear "on stomper" flag every frame by default
        int16_t x = self->x_pos.f.u;
        if (x >= 0xA20 && x < 0xAA1) {
            self->y_pos.f.u = (int16_t)(obj31_ypos - 0x1C);
            obj31_ypos = (int16_t)(obj31_ypos | 0x8000); // tell ChainStomp not to rise into the ceiling
            self->subtype |= 0x80;
        }
    }

    PushB_Display(self, own);
}

static void PushB_ChkVisible(Sst *self, Own_PushBlock *own) {
    if (!SstChkPartiallyVisible(self))
        return; // block doesn't exist until it's visible

    self->routine = 2;
    own->on_lava = false;
    self->x_vel = 0;
    self->y_vel = 0;
}

static void PushB_Main(Sst *self, Own_PushBlock *own) {
    self->routine += 2;
    self->y_radius = 30 / 2;
    self->x_radius = 30 / 2;
    self->mappings = Mappings_PushableBlocks;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3
    if (LEVEL_ZONE(level_id) == ZoneId_LZ)
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Push_Block); // | Tile_Pal3 (unused)
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;
    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;

    uint8_t subtype = self->subtype;
    self->width_pixels = pushb_vars[subtype & 1].width;
    self->mapping_frame = pushb_vars[subtype & 1].frame;

    if (subtype != 0)
        self->art_tile = TILE_MAP(1, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3 | Tile_Prio -- always MZ's own tile here, even in LZ (matches real hardware's own apparent oversight)

    if (self->respawn) {
        (*SstLoad_State(self)) &= 0x7F;
        bool already_loaded = ((*SstLoad_State(self)) & 0x01) != 0;
        (*SstLoad_State(self)) |= 0x01;
        if (already_loaded) { // only one block can exist
            Sst_Delete(self);
            return;
        }
    }

    PushB_Action(self, own);
}

void Obj_PushBlock(Sst *self) {
    Own_PushBlock *own = (Own_PushBlock *)self->own.u8;

    switch (self->routine) {
    case 0:
        PushB_Main(self, own);
        break;
    case 2:
        PushB_Action(self, own);
        break;
    case 4:
        PushB_ChkVisible(self, own);
        break;
    }
}
