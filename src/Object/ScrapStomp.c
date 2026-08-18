#include "ScrapStomp.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Mappings/StomperDoor.h"

// Object 6B - stomper and sliding door (SBZ), and ancient lift at the start
// of SBZ3/LZ4
//
// Subtype bitfield: bit 7 set = extending sliding platform, triggered by a
// switch (lower nibble = switch ID). Bit 7 clear = stomper/lift, where the
// upper nibble selects the Sto_Var preset (size/behavior) and the lower
// nibble is used directly as the action type.

// width/2, height/2, max distance, action type
static const struct { uint8_t width, height; int16_t max_dist; uint8_t action_type; } Sto_Var[5] = {
    { 128 / 2, 24 / 2, 128, 1 }, // $8x - sliding platform extending on switch press
    {  56 / 2, 64 / 2,  56, 3 }, // $1x - stomper (stomps down and slowly goes back up)
    {  56 / 2, 64 / 2,  64, 4 }, // $2x - stomper (stomps up and down)
    {  56 / 2, 64 / 2,  96, 4 }, // $3x - stomper (stomps up and down, larger distance)
    { 256 / 2, 128 / 2,  0, 5 }, // $4x - ancient lift at the start of SBZ3/LZ4
};

static void Sto_Action(Object *obj, Scratch_ScrapStomp *scratch);

static bool Sto_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Sto_UpdateX(Object *obj, Scratch_ScrapStomp *scratch) {
    int16_t d0 = scratch->offset_now;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + 128); // keep in the same general X-range (platform is 128px wide)
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x - d0);
}

static void Sto_UpdateY(Object *obj, Scratch_ScrapStomp *scratch) {
    int16_t d0 = scratch->offset_now;
    if (obj->status.o.f.x_flip)
        d0 = (int16_t)(-d0 + 56); // keep in the same general Y-range (stomper is 56px tall)
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d0);
}

// Type 1 - extending sliding platform
static void Sto_SlidingPlatform_Extend(Object *obj, Scratch_ScrapStomp *scratch) {
    if (!scratch->active) {
        if (!(f_switch[scratch->switch_id] & 1)) {
            Sto_UpdateX(obj, scratch);
            return;
        }
        scratch->active = 1;
    }

    if (scratch->offset_max != scratch->offset_now) {
        scratch->offset_now += 2;
        Sto_UpdateX(obj, scratch);
        return;
    }

    // stopPlatform
    obj->scratch.u8[0]++; // -> Sto_SlidingPlatform_Retract
    scratch->delay = 3 * 60;
    scratch->active = 0;
    if (obj->respawn_index != 0)
        objstate[obj->respawn_index] |= 1; // seems to be unused elsewhere, kept faithfully
    Sto_UpdateX(obj, scratch);
}

// Type 2 (set from Type 1) - retract sliding platform again after a delay
static void Sto_SlidingPlatform_Retract(Object *obj, Scratch_ScrapStomp *scratch) {
    if (!scratch->active) {
        if (--scratch->delay != 0) {
            Sto_UpdateX(obj, scratch);
            return;
        }
        scratch->active = 1;
    }

    if (scratch->offset_now != 0) {
        scratch->offset_now -= 2;
        Sto_UpdateX(obj, scratch);
        return;
    }

    // stopPlatform
    obj->scratch.u8[0]--; // -> Sto_SlidingPlatform_Extend
    scratch->active = 0;
    if (obj->respawn_index != 0)
        objstate[obj->respawn_index] &= ~1;
    Sto_UpdateX(obj, scratch);
}

// Type 3 - stomper (stomps down, slowly retracts, waits, stomps down again)
static void Sto_Stomper_DownAndRetract(Object *obj, Scratch_ScrapStomp *scratch) {
    if (!scratch->active) {
        if (scratch->offset_now != 0) {
            scratch->offset_now--;
            Sto_UpdateY(obj, scratch);
            return;
        }
        if (--scratch->delay >= 0) {
            Sto_UpdateY(obj, scratch);
            return;
        }
        scratch->delay = 1 * 60;
        scratch->active = 1;
    }

    scratch->offset_now += 8;
    if (scratch->offset_now == scratch->offset_max)
        scratch->active = 0;
    Sto_UpdateY(obj, scratch);
}

// Type 4 - stomper (stomps up and down, waits 1 second between each stomp)
static void Sto_Stomper_UpAndDown(Object *obj, Scratch_ScrapStomp *scratch) {
    if (!scratch->active) {
        if (scratch->offset_now != 0) {
            scratch->offset_now -= 8;
            Sto_UpdateY(obj, scratch);
            return;
        }
        if (--scratch->delay >= 0) {
            Sto_UpdateY(obj, scratch);
            return;
        }
        scratch->delay = 1 * 60;
        scratch->active = 1;
    }

    if (scratch->offset_now == scratch->offset_max) {
        if (--scratch->delay >= 0) {
            Sto_UpdateY(obj, scratch);
            return;
        }
        scratch->delay = 1 * 60;
        scratch->active = 0;
        Sto_UpdateY(obj, scratch);
        return;
    }

    scratch->offset_now += 8;
    Sto_UpdateY(obj, scratch);
}

// Type 5 - ancient lift at the start of SBZ3/LZ4
static void Sto_AncientLift(Object *obj, Scratch_ScrapStomp *scratch) {
    if (!scratch->active) {
        if (!(f_switch[scratch->switch_id] & 1))
            return;
        scratch->active = 1;
        if (obj->respawn_index != 0)
            objstate[obj->respawn_index] |= 1; // globally remember lift has already started moving
    }

    obj->pos.l.x.v -= 0x10000; // move left at 1px/frame (incl. subpixels)
    obj->pos.l.y.v += 0x8000;  // move down at 0.5px/frame (incl. subpixels)
    scratch->orig_x = obj->pos.l.x.f.u; // overwrite initial X with current X

    if (obj->pos.l.x.f.u == (int16_t)0x980) {
        obj->scratch.u8[0] = 0; // stop -- Sto_Stationary
        scratch->active = 0; // redundant at this point
    }
}

static void Sto_Action(Object *obj, Scratch_ScrapStomp *scratch) {
    int16_t prev_x = obj->pos.l.x.f.u;

    switch (obj->scratch.u8[0] & 0xF) {
    case 0: break; // stationary
    case 1: Sto_SlidingPlatform_Extend(obj, scratch); break;
    case 2: Sto_SlidingPlatform_Retract(obj, scratch); break;
    case 3: Sto_Stomper_DownAndRetract(obj, scratch); break;
    case 4: Sto_Stomper_UpAndDown(obj, scratch); break;
    case 5: Sto_AncientLift(obj, scratch); break;
    }

    if (obj->render.f.on_screen) {
        int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
        SolidObject(obj, (uint16_t)x_rad, obj->y_rad, (uint16_t)(obj->y_rad + 1), prev_x, NULL, NULL);
    }

    if (!Sto_OutOfRange(scratch->orig_x)) {
        DisplaySprite(obj);
        return;
    }

    if (LEVEL_ZONE(level_id) == ZoneId_LZ) {
        obj6B = 0;
        if (obj->respawn_index != 0)
            objstate[obj->respawn_index] &= 0x7F;
    }
    ObjectDelete(obj);
}

static void Sto_Main(Object *obj, Scratch_ScrapStomp *scratch) {
    obj->routine = 2; // advance to Sto_Action

    uint8_t subtype = obj->scratch.u8[0];
    int idx = (subtype >> 4) & 7;

    obj->width_pixels = Sto_Var[idx].width;
    obj->y_rad = (int8_t)Sto_Var[idx].height;
    obj->frame = (uint8_t)idx;

    obj->mappings = Mappings_StomperDoor;
    obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Moving_Block_Short); // | Tile_Pal2 (SBZ1/2)

    if (LEVEL_ZONE(level_id) == ZoneId_LZ) { // SBZ3
        bool already_loaded = (obj6B & 1) != 0;
        obj6B |= 1;
        if (already_loaded) {
            if (obj->respawn_index != 0)
                objstate[obj->respawn_index] &= 0x7F;
            ObjectDelete(obj);
            return;
        }

        // Ancient lift at the start of SBZ3
        obj->tile = TILE_MAP(0, 2, 0, 0, (ArtTile_Level+0x1F0)); // | Tile_Pal3
        if (obj->pos.l.x.f.u == (int16_t)0xA80 && obj->respawn_index != 0 &&
            (objstate[obj->respawn_index] & 1)) {
            // Prevent a pre-switched ancient lift from reappearing after
            // the switch was already pressed.
            obj6B = 0;
            if (obj->respawn_index != 0)
                objstate[obj->respawn_index] &= 0x7F;
            ObjectDelete(obj);
            return;
        }
    }

    obj->render.f.align_fg = true;
    obj->priority = 4;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;
    scratch->offset_max = Sto_Var[idx].max_dist;

    if (subtype & 0x80) {
        scratch->switch_id = subtype & 0xF;
        obj->scratch.u8[0] = Sto_Var[idx].action_type;

        if (Sto_Var[idx].action_type == 5)
            obj->render.f.yrad_height = true; // custom sprite render height

        if (obj->respawn_index != 0)
            objstate[obj->respawn_index] &= 0x7F;
    }

    Sto_Action(obj, scratch);
}

void Obj_ScrapStomp(Object *obj) {
    Scratch_ScrapStomp *scratch = (Scratch_ScrapStomp *)&obj->scratch;

    if (obj->routine == 0) {
        Sto_Main(obj, scratch);
        return;
    }
    Sto_Action(obj, scratch);
}
