#include "FloatingBlock.h"

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

static void FBlock_MoveLR(Object *obj, Scratch_FloatingBlock *scratch, uint8_t osc_byte, int16_t keep_range) {
    int16_t d0 = osc_byte;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + keep_range);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x - d0);
}

static void FBlock_MoveUD(Object *obj, Scratch_FloatingBlock *scratch, uint8_t osc_byte, int16_t keep_range) {
    int16_t d0 = osc_byte;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + keep_range);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - d0);
}

static void FBlock_LZSmallDoor_Open(Object *obj, Scratch_FloatingBlock *scratch) {
    bool is_lz1 = LEVEL_ZONE(level_id) == ZoneId_LZ && LEVEL_ACT(level_id) == 0;

    if (!scratch->moving) {
        if (is_lz1 && scratch->switch_id == 3) {
            f_wtunneldisallow = false;
            if (player->pos.l.x.f.u < obj->pos.l.x.f.u)
                f_wtunneldisallow = true;
        }

        if (!(f_switch[scratch->switch_id] & 1)) {
            int16_t d0 = scratch->distance;
            if (obj->status.o.f.x_flip)
                d0 = (int16_t)-d0;
            obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
            return;
        }

        if (is_lz1 && scratch->switch_id == 3)
            f_wtunneldisallow = false;

        scratch->moving = true;
    }

    if (scratch->distance == 0) {
        obj->scratch.u8[0]++; // advance to FBlock_LZSmallDoor_Close
        scratch->moving = false;
        if (obj->respawn_index)
            objstate[obj->respawn_index] |= 1;
    } else {
        scratch->distance -= 2;
    }

    int16_t d0 = scratch->distance;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)-d0;
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
}

static void FBlock_LZSmallDoor_Close(Object *obj, Scratch_FloatingBlock *scratch) {
    if (!scratch->moving) {
        // Switches with the "alternate flag" (bit 7) don't exist anywhere
        // in the game -- kept faithfully, effectively dead code except for
        // one hardcoded exception elsewhere (DynWater_LZ1_Routine0, not
        // ported).
        if (!((int8_t)f_switch[scratch->switch_id] < 0)) {
            int16_t d0 = scratch->distance;
            if (obj->status.o.f.x_flip)
                d0 = (int16_t)-d0;
            obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
            return;
        }
        scratch->moving = true;
    }

    int16_t full_height = (int16_t)(obj->y_rad * 2);
    if (full_height == scratch->distance) {
        obj->scratch.u8[0]--; // back to FBlock_LZSmallDoor_Open
        scratch->moving = false;
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= ~1;
    } else {
        scratch->distance += 2;
    }

    int16_t d0 = scratch->distance;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)-d0;
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
}

static void FBlock_HorizontalSYZ3(Object *obj, Scratch_FloatingBlock *scratch) {
    if (!scratch->moving) {
        if (f_switch[0xF] == 0)
            return;
        scratch->moving = true;
        scratch->distance = 0;
    }

    obj->pos.l.x.f.u++;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->distance++;
    if (scratch->distance != 0x380)
        return;

    f_obj56 = true;
    scratch->moving = false;
    obj->scratch.u8[0] = 0; // FBlock_Stationary
}

static void FBlock_LZHorizDoor_Open(Object *obj, Scratch_FloatingBlock *scratch) {
    if (!scratch->moving) {
        if (!(f_switch[scratch->switch_id] & 1)) {
            int16_t d0 = scratch->distance;
            if (obj->status.o.f.x_flip)
                d0 = (int16_t)(-d0 + 128);
            obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
            return;
        }
        scratch->moving = true;
    }

    if (scratch->distance == 0) {
        obj->scratch.u8[0]++; // advance to FBlock_LZHorizDoor_Close
        scratch->moving = false;
        if (obj->respawn_index)
            objstate[obj->respawn_index] |= 1;
    } else {
        scratch->distance -= 2;
    }

    int16_t d0 = scratch->distance;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + 128);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
}

static void FBlock_LZHorizDoor_Close(Object *obj, Scratch_FloatingBlock *scratch) {
    if (!scratch->moving) {
        if (!((int8_t)f_switch[scratch->switch_id] < 0)) {
            int16_t d0 = scratch->distance;
            if (obj->status.o.f.x_flip)
                d0 = (int16_t)(-d0 + 128);
            obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
            return;
        }
        scratch->moving = true;
    }

    if (scratch->distance == 128) {
        obj->scratch.u8[0]--; // back to FBlock_LZHorizDoor_Open
        scratch->moving = false;
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= ~1;
    } else {
        scratch->distance += 2;
    }

    int16_t d0 = scratch->distance;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + 128);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
}

// Shared by the SLZ rotating-stairway block types 8-B: moves the block
// around the four edges of a square, switching which edge it's on every
// time the driving oscillation value hits a corner (rate == 0).
static void FBlock_SLZStair_MoveSquare(Object *obj, Scratch_FloatingBlock *scratch, uint8_t osc_byte, int16_t half_range, bool at_corner) {
    if (at_corner)
        obj->status.o.b = (uint8_t)((obj->status.o.b + 1) & 3);

    int16_t d0 = osc_byte;
    int16_t d1 = half_range;

    switch (obj->status.o.b & 3) {
    case 0: // top edge, moving right
        d0 -= d1;
        obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
        obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - d1);
        break;
    case 1: // x-flipped: right edge, moving down
        d1 -= 1;
        d0 = (int16_t)-(d0 - d1);
        obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
        d1 += 1;
        obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d1);
        break;
    case 2: // y-flipped: bottom edge, moving left
        d1 -= 1;
        d0 = (int16_t)-(d0 - d1);
        obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
        d1 += 1;
        obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d1);
        break;
    case 3: // xy-flipped: left edge, moving up
        d0 -= d1;
        obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
        obj->pos.l.x.f.u = (int16_t)(scratch->orig_x - d1);
        break;
    }
}

static void FBlock_Action(Object *obj, Scratch_FloatingBlock *scratch) {
    int16_t prev_x = obj->pos.l.x.f.u;

    switch (obj->scratch.u8[0] & 0xF) {
    case 0: break; // stationary
    case 1: FBlock_MoveLR(obj, scratch, (uint8_t)(oscillatory.state[2][0] >> 8), 0x20 * 2); break;
    case 2: FBlock_MoveLR(obj, scratch, (uint8_t)(oscillatory.state[7][0] >> 8), 0x40 * 2); break;
    case 3: FBlock_MoveUD(obj, scratch, (uint8_t)(oscillatory.state[2][0] >> 8), 0x20 * 2); break;
    case 4: FBlock_MoveUD(obj, scratch, (uint8_t)(oscillatory.state[7][0] >> 8), 0x40 * 2); break;
    case 5: FBlock_LZSmallDoor_Open(obj, scratch); break;
    case 6: FBlock_LZSmallDoor_Close(obj, scratch); break;
    case 7: FBlock_HorizontalSYZ3(obj, scratch); break;
    case 8: FBlock_SLZStair_MoveSquare(obj, scratch, (uint8_t)((oscillatory.state[10][0] >> 8) >> 1), 0x10, oscillatory.state[10][1] == 0); break;
    case 9: FBlock_SLZStair_MoveSquare(obj, scratch, (uint8_t)(oscillatory.state[11][0] >> 8), 0x30, oscillatory.state[11][1] == 0); break;
    case 0xA: FBlock_SLZStair_MoveSquare(obj, scratch, (uint8_t)(oscillatory.state[12][0] >> 8), 0x50, oscillatory.state[12][1] == 0); break;
    case 0xB: FBlock_SLZStair_MoveSquare(obj, scratch, (uint8_t)(oscillatory.state[13][0] >> 8), 0x70, oscillatory.state[13][1] == 0); break;
    case 0xC: FBlock_LZHorizDoor_Open(obj, scratch); break;
    case 0xD: FBlock_LZHorizDoor_Close(obj, scratch); break;
    }

    if (obj->render.f.on_screen) {
        int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
        SolidObject(obj, (uint16_t)x_rad, obj->y_rad, (uint16_t)(obj->y_rad + 1), prev_x, NULL, NULL);
    }

    if (IS_OFFSCREEN(scratch->orig_x)) {
        if (obj->scratch.u8[0] == 0x37 && scratch->moving) {
            DisplaySprite(obj);
            return;
        }
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

static bool FBlock_Main(Object *obj, Scratch_FloatingBlock *scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_FloatingBlock;
    obj->tile = TILE_MAP(0, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3

    bool is_lz = LEVEL_ZONE(level_id) == ZoneId_LZ;
    if (is_lz)
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x3C4); // ArtTile_LZ_Door | Tile_Pal3

    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;

    uint8_t subtype = obj->scratch.u8[0];
    uint8_t index = (subtype >> 4) & 7;
    obj->width_pixels = FBlock_Var[index].width;
    obj->y_rad = (int8_t)FBlock_Var[index].height;
    obj->frame = index;

    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;
    scratch->distance = (int16_t)(FBlock_Var[index].height * 2);

    // REV01 addition: SYZ3's horizontally moving block, see f_obj56 comment.
    if (subtype == 0x37) {
        if (obj->pos.l.x.f.u == 0x1BB8) { // the "real" block
            if (f_obj56) {
                ObjectDelete(obj);
                return false;
            }
        } else { // the "fake" stationary block at the tunnel exit
            obj->scratch.u8[0] = 0; // force FBlock_Stationary
            if (!f_obj56) {
                ObjectDelete(obj);
                return false;
            }
        }
    }

    if (!is_lz) {
        uint8_t d0 = (uint8_t)(obj->scratch.u8[0] & 0xF);
        if (d0 >= 8) {
            int oi = 10 + (d0 - 8);
            if ((int16_t)oscillatory.state[oi][1] < 0)
                obj->status.o.f.x_flip ^= 1;
        }
    }

    // Shared "switch-activated LZ door" check -- subtype bit 7 set
    uint8_t d0 = obj->scratch.u8[0];
    if ((int8_t)d0 >= 0)
        return true; // not switch-activated

    scratch->switch_id = d0 & 0xF;
    obj->scratch.u8[0] = 5; // FBlock_LZSmallDoor_Open
    if (index == 7) { // large sideways 4x1 door
        obj->scratch.u8[0] = 0xC; // FBlock_LZHorizDoor_Open
        scratch->distance = 128;
    }

    if (obj->respawn_index) {
        objstate[obj->respawn_index] &= 0x7F;
        if (objstate[obj->respawn_index] & 1) {
            obj->scratch.u8[0]++; // already opened -- start already at the "close" type
            scratch->distance = 0;
        }
    }

    return true;
}

void Obj_FloatingBlock(Object *obj) {
    Scratch_FloatingBlock *scratch = (Scratch_FloatingBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        if (!FBlock_Main(obj, scratch))
            return;
        __attribute__((fallthrough));
    case 2:
        FBlock_Action(obj, scratch);
        break;
    }
}
