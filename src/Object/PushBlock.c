#include "PushBlock.h"

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

static void PushB_ChkVisible(Object *obj, Scratch_PushBlock *scratch);
static void PushB_Display(Object *obj, Scratch_PushBlock *scratch);
static void PushB_SolidAction(Object *obj, Scratch_PushBlock *scratch, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x);

static void PushB_ChkWithinOrigin(Object *obj, Scratch_PushBlock *scratch) {
    if (IS_OFFSCREEN(scratch->orig_x)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= (uint8_t)~0x01;
        ObjectDelete(obj);
        return;
    }

    obj->pos.l.x.f.u = scratch->orig_x;
    obj->pos.l.y.f.u = scratch->orig_y;
    obj->routine = 4;
    PushB_ChkVisible(obj, scratch);
}

static void PushB_Display(Object *obj, Scratch_PushBlock *scratch) {
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        PushB_ChkWithinOrigin(obj, scratch);
        return;
    }
    DisplaySprite(obj);
}

// Hardcoded to specific X-positions in MZ2 and MZ3 -- spawns a lava
// geyser maker just ahead of the block as it drifts along on lava.
static void PushB_SpawnLavaGeysers(Object *obj) {
    int16_t offset;
    int16_t x = obj->pos.l.x.f.u;

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

    Object *geyser = FindFreeObj();
    if (geyser == NULL)
        return;
    geyser->type = ObjId_GeyserMaker;
    geyser->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + offset);
    geyser->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 16);
    ((Scratch_GeyserMaker *)&geyser->scratch)->parent_index = (uint8_t)(obj - objects);
}

// Is the 16x16 chunk the block just landed on a lava tile? (IDs $16A and
// above.)
static bool PushB_LandedOnLava(Object *obj) {
    const uint8_t *tile = FindNearestTile(obj, obj->pos.l.x.f.u, (int16_t)(obj->pos.l.y.f.u + obj->y_rad));
    uint16_t word = (uint16_t)((tile[0] << 8) | tile[1]);
    return (word & META_TILE) >= 0x16A;
}

static void PushB_SolidAction_NotOnPlatform(Object *obj, Scratch_PushBlock *scratch, uint16_t x_rad, uint16_t y_rad) {
    int16_t x_off, y_off;
    int32_t collision = Solid_ChkEnter(obj, x_rad, y_rad, &x_off, &y_off);
    if (collision <= 0)
        return; // no touch, or Sonic landed on top/got squished underneath (handled inside already)
    if (scratch->on_lava)
        return;
    if (x_off == 0)
        return;

    int16_t sonic_dx, ground_speed;
    if (x_off < 0) {
        // Sonic is on the right, pushing the block left -- requires facing left.
        if (!player->status.o.f.x_flip)
            return;
        if (ObjHitWallLeft(obj, (int16_t)(~(uint16_t)obj->width_pixels)) < 0)
            return;
        obj->pos.l.x.v -= 0x10000;
        sonic_dx = -1;
        ground_speed = -0x40;
    } else {
        // Sonic is on the left, pushing the block right -- requires facing right.
        if (player->status.o.f.x_flip)
            return;
        if (ObjHitWallRight(obj, obj->width_pixels) < 0)
            return;
        obj->pos.l.x.v += 0x10000;
        sonic_dx = 1;
        ground_speed = 0x40;
    }

    player->pos.l.x.f.u = (int16_t)(player->pos.l.x.f.u + sonic_dx);
    player->inertia = ground_speed;
    player->xsp = 0;

    QueueSound2(sfx_Push); // hardcoded in the sound driver to not be interruptible by itself

    if (obj->scratch.u8[0] & 0x80)
        return; // block is resting on the MZ1 stomper -- don't let it fall off

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist <= 4) {
        obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + floor_dist);
        return;
    }

    obj->xsp = 0x400; // quickly slide off the ledge it just walked off
    if (x_off < 0)
        obj->xsp = (int16_t)-obj->xsp;
    obj->routine_sec = 6; // snap to a clean 16px grid position before falling
}

static void PushB_SolidAction(Object *obj, Scratch_PushBlock *scratch, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x) {
    switch (obj->routine_sec) {
    case 2: { // Sonic is standing on the block
        int16_t x_off;
        if (ExitPlatform(obj, x_rad, x_rad, &x_off))
            obj->routine_sec = 0; // released -- back to "not on platform" so pushing works again
        else
            MvSonicOnPtfm(obj, (int16_t)(obj->pos.l.y.f.u - y_rad2), prev_x);
        return;
    }
    case 4: { // falling
        SpeedToPos(obj);
        obj->ysp += 0x18;

        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist >= 0)
            return;
        obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + floor_dist);
        obj->ysp = 0;
        obj->routine_sec = 0;

        if (PushB_LandedOnLava(obj)) {
            obj->xsp = (int16_t)(scratch->lava_speed >> 3); // $400/8 = $80
            scratch->on_lava = true;
            obj->pos.l.y.f.l = 0; // needed for sinking in lava later
        }
        return;
    }
    case 6: { // snapping to a 16px grid position before falling
        SpeedToPos(obj);
        if ((obj->pos.l.x.f.u & 0xC) != 0)
            return;
        obj->pos.l.x.f.u &= ~0xF;
        scratch->lava_speed = obj->xsp; // remembered in case it lands on lava
        obj->xsp = 0;
        obj->routine_sec -= 2; // -> falling
        return;
    }
    default: // not on platform
        PushB_SolidAction_NotOnPlatform(obj, scratch, x_rad, y_rad1);
        return;
    }
}

static void PushB_LavaPlatform(Object *obj, Scratch_PushBlock *scratch, int16_t prev_x) {
    int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
    PushB_SolidAction(obj, scratch, (uint16_t)x_rad, 32 / 2, 34 / 2, prev_x);
    PushB_SpawnLavaGeysers(obj);
    PushB_Display(obj, scratch);
}

static void PushB_Sunken(Object *obj, Scratch_PushBlock *scratch) {
    player->status.o.f.player_stand = false;
    obj->status.o.f.player_stand = false;
    PushB_ChkWithinOrigin(obj, scratch);
}

// Handles the block once it's fallen onto lava: getting launched by a
// geyser underneath it, drifting until it hits a wall, and slowly sinking
// once it stops moving.
static void PushB_OnLava(Object *obj, Scratch_PushBlock *scratch) {
    // Captured BEFORE SpeedToPos moves the block via its own drift velocity
    // below -- MvSonicOnPtfm (inside PushB_SolidAction) needs the block's
    // PRE-drift X to compute how far it moved this frame and carry Sonic
    // along by that same amount. Capturing it any later (e.g. inside
    // PushB_LavaPlatform, after the block has already moved) makes the
    // carry delta always zero, so Sonic never rides a drifting block --
    // it just slides out from under him every frame it's moving.
    int16_t prev_x = obj->pos.l.x.f.u;

    if (obj->routine_sec < 4)
        SpeedToPos(obj); // routine_sec 4/6 do their own SpeedToPos inside PushB_SolidAction

    if (obj->status.b & 0x02) { // getting shot up by a lava geyser (set by GeyserMaker)
        obj->ysp += 0x18;

        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist < 0) {
            obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + floor_dist);
            obj->ysp = 0;
            obj->status.b &= (uint8_t)~0x02;

            if (PushB_LandedOnLava(obj)) {
                obj->xsp = (int16_t)(scratch->lava_speed >> 3);
                scratch->on_lava = true;
                obj->pos.l.y.f.l = 0;
            }
        }
        PushB_LavaPlatform(obj, scratch, prev_x);
        return;
    }

    if (obj->xsp == 0) {
        // Sunk further into the lava -- the sink depth is tracked in the
        // low byte of the Y-position's own subpixel fraction, so this
        // sinks impossibly slowly (a fraction of a pixel per frame).
        obj->pos.l.y.v += 0x2000 + 1;
        if ((uint8_t)obj->pos.l.y.f.l < 160) {
            PushB_LavaPlatform(obj, scratch, prev_x);
            return;
        }
        PushB_Sunken(obj, scratch);
        return;
    }

    int16_t hit = (obj->xsp < 0) ? ObjHitWallLeft(obj, (int16_t)(~(uint16_t)obj->width_pixels))
                                  : ObjHitWallRight(obj, obj->width_pixels);
    if (hit < 0)
        obj->xsp = 0; // hit a wall -- stop moving, don't start sinking until next frame

    PushB_LavaPlatform(obj, scratch, prev_x);
}

static void PushB_Action(Object *obj, Scratch_PushBlock *scratch) {
    if (scratch->on_lava) {
        PushB_OnLava(obj, scratch);
        return;
    }

    int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
    PushB_SolidAction(obj, scratch, (uint16_t)x_rad, 32 / 2, 34 / 2, obj->pos.l.x.f.u);

    // Hardcoded MZ1 stuff for the block that pushes down a button to lift a spiked stomper.
    if (level_id == LEVEL_ID(ZoneId_MZ, 0)) {
        obj->scratch.u8[0] &= 0x7F; // clear "on stomper" flag every frame by default
        int16_t x = obj->pos.l.x.f.u;
        if (x >= 0xA20 && x < 0xAA1) {
            obj->pos.l.y.f.u = (int16_t)(obj31_ypos - 0x1C);
            obj31_ypos = (int16_t)(obj31_ypos | 0x8000); // tell ChainStomp not to rise into the ceiling
            obj->scratch.u8[0] |= 0x80;
        }
    }

    PushB_Display(obj, scratch);
}

static void PushB_ChkVisible(Object *obj, Scratch_PushBlock *scratch) {
    if (!ChkPartiallyVisible(obj))
        return; // block doesn't exist until it's visible

    obj->routine = 2;
    scratch->on_lava = false;
    obj->xsp = 0;
    obj->ysp = 0;
}

static void PushB_Main(Object *obj, Scratch_PushBlock *scratch) {
    obj->routine += 2;
    obj->y_rad = 30 / 2;
    obj->x_rad = 30 / 2;
    obj->mappings = Mappings_PushableBlocks;
    obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3
    if (LEVEL_ZONE(level_id) == ZoneId_LZ)
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Push_Block); // | Tile_Pal3 (unused)
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;

    uint8_t subtype = obj->scratch.u8[0];
    obj->width_pixels = pushb_vars[subtype & 1].width;
    obj->frame = pushb_vars[subtype & 1].frame;

    if (subtype != 0)
        obj->tile = TILE_MAP(1, 2, 0, 0, ArtTile_MZ_Block); // | Tile_Pal3 | Tile_Prio -- always MZ's own tile here, even in LZ (matches real hardware's own apparent oversight)

    if (obj->respawn_index) {
        objstate[obj->respawn_index] &= 0x7F;
        bool already_loaded = (objstate[obj->respawn_index] & 0x01) != 0;
        objstate[obj->respawn_index] |= 0x01;
        if (already_loaded) { // only one block can exist
            ObjectDelete(obj);
            return;
        }
    }

    PushB_Action(obj, scratch);
}

void Obj_PushBlock(Object *obj) {
    Scratch_PushBlock *scratch = (Scratch_PushBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        PushB_Main(obj, scratch);
        break;
    case 2:
        PushB_Action(obj, scratch);
        break;
    case 4:
        PushB_ChkVisible(obj, scratch);
        break;
    }
}
