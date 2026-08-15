#include "MovingBlock.h"

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
static bool MBlock_Move(Object *obj, Scratch_MovingBlock *scratch) {
    uint8_t subtype = obj->scratch.u8[0] & 0xF;
    switch (subtype) {
    case 0: // stationary
        break;
    case 1: { // moves left and right continuously
        int16_t d0 = (int16_t)(oscillatory.state[3][0] >> 8); // v_oscillate+$E, frequency 2, middle $30
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(-d0 + 0x60);
        obj->pos.l.x.f.u = (int16_t)(scratch->orig_x - d0);
        break;
    }
    case 2: // stationary, advances to next subtype (3) when stood on
    case 4: // stationary, advances to next subtype (5) when stood on
    case 9: // stationary, advances to next subtype (A) when stood on
        if (obj->routine == 4)
            obj->scratch.u8[0]++;
        break;
    case 3: { // moves right, stops (type 0) on wall hit
        int16_t d1 = ObjHitWallRight(obj, obj->width_pixels);
        if (d1 < 0) {
            obj->scratch.u8[0] = 0;
            break;
        }
        obj->pos.l.x.f.u += 1;
        scratch->orig_x = obj->pos.l.x.f.u;
        break;
    }
    case 5: { // moves right, falls (type 6) on wall hit
        int16_t d1 = ObjHitWallRight(obj, obj->width_pixels);
        if (d1 < 0) {
            obj->scratch.u8[0] = 6;
            break;
        }
        obj->pos.l.x.f.u += 1;
        scratch->orig_x = obj->pos.l.x.f.u;
        break;
    }
    case 6: { // falls, stops (type 0) on floor hit
        SpeedToPos(obj);
        obj->ysp += 0x18;
        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist < 0) {
            obj->pos.l.y.f.u += floor_dist;
            obj->ysp = 0;
            obj->scratch.u8[0] = 0;
        }
        break;
    }
    case 7: // appears when switch 2 is pressed (secret LZ1 raft shortcut)
        if (f_switch[2])
            obj->scratch.u8[0] = 4;
        return true;
    case 8: { // moves up and down continuously
        int16_t d0 = (int16_t)(oscillatory.state[7][0] >> 8); // v_oscillate+$1E, frequency 4, middle $40
        if (obj->status.o.f.x_flip)
            d0 = (int16_t)(-d0 + 0x80);
        obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - d0);
        break;
    }
    case 0xA: { // SBZ red sliding floor: quickly slides out, then back after a delay
        int16_t width2 = obj->width_pixels * 2;
        int16_t step = 8;
        if (obj->status.o.f.x_flip) {
            step = -8;
            width2 = -width2;
        }
        if (scratch->slide_goback == 0) {
            int16_t d0 = (int16_t)(obj->pos.l.x.f.u - scratch->orig_x);
            if (d0 == width2) {
                if (--scratch->slide_wait == 0)
                    scratch->slide_goback = 1;
                break;
            }
            obj->pos.l.x.f.u += step;
            scratch->slide_wait = 5 * 60;
            break;
        }
        {
            int16_t d0 = (int16_t)(obj->pos.l.x.f.u - scratch->orig_x);
            if (d0 == 0) {
                scratch->slide_goback = 0;
                obj->scratch.u8[0] = 9; // wait for Sonic to step on it again
                break;
            }
            obj->pos.l.x.f.u -= step;
        }
        break;
    }
    }
    return false;
}

void Obj_MovingBlock(Object *obj) {
    Scratch_MovingBlock *scratch = (Scratch_MovingBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main
        obj->routine += 2;
        obj->mappings = Mappings_MovingBlocks;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x2B8); // ArtTile_MZ_Block | Tile_Pal3

        if (LEVEL_ZONE(level_id) == ZoneId_LZ) {
            obj->mappings = Mappings_LZMovingBlocks;
            obj->tile = TILE_MAP(0, 2, 0, 0, 0x3BC); // ArtTile_LZ_Moving_Block | Tile_Pal3
            obj->y_rad = 14 / 2;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
            obj->tile = TILE_MAP(0, 1, 0, 0, 0x2C0); // ArtTile_SBZ_Moving_Block_Short | Tile_Pal2
            if (obj->scratch.u8[0] != 0x28)
                obj->tile = TILE_MAP(0, 2, 0, 0, 0x460); // ArtTile_SBZ_Moving_Block_Long | Tile_Pal3
        }

        obj->render.b = 0;
        obj->render.f.align_fg = true;

        uint8_t index = obj->scratch.u8[0] >> 4;
        if (index >= 5)
            index = 0;
        obj->width_pixels = mblock_var[index].width;
        obj->frame = mblock_var[index].frame;

        obj->priority = 4;
        scratch->orig_x = obj->pos.l.x.f.u;
        scratch->orig_y = obj->pos.l.y.f.u;
        obj->scratch.u8[0] &= 0xF;
        break;
    }
    case 2: // Platform
        if (!MBlock_Move(obj, scratch)) {
            PlatformObject(obj, obj->width_pixels);
            if (IS_OFFSCREEN(scratch->orig_x)) {
                ObjectDelete(obj);
                return;
            }
            DisplaySprite(obj);
        } else if (IS_OFFSCREEN(scratch->orig_x)) {
            ObjectDelete(obj);
        }
        break;
    case 4: { // StandOn
        ExitPlatform(obj, obj->width_pixels, obj->width_pixels, NULL);
        int16_t prev_x = obj->pos.l.x.f.u;
        if (!MBlock_Move(obj, scratch)) {
            MvSonicOnPtfm(obj, obj->pos.l.y.f.u - 8, prev_x);
            if (IS_OFFSCREEN(scratch->orig_x)) {
                ObjectDelete(obj);
                return;
            }
            DisplaySprite(obj);
        } else if (IS_OFFSCREEN(scratch->orig_x)) {
            ObjectDelete(obj);
        }
        break;
    }
    }
}
