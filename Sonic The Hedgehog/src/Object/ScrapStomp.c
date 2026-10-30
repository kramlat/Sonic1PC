#include "ScrapStomp.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Constants.h"

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

static void Sto_Action(Sst *self, Own_ScrapStomp *own);

static void Sto_UpdateX(Sst *self, Own_ScrapStomp *own) {
    int16_t d0 = own->offset_now;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + 128); // keep in the same general X-range (platform is 128px wide)
    self->x_pos.f.u = (int16_t)(own->orig_x - d0);
}

static void Sto_UpdateY(Sst *self, Own_ScrapStomp *own) {
    int16_t d0 = own->offset_now;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(-d0 + 56); // keep in the same general Y-range (stomper is 56px tall)
    self->y_pos.f.u = (int16_t)(own->orig_y + d0);
}

// Type 1 - extending sliding platform
static void Sto_SlidingPlatform_Extend(Sst *self, Own_ScrapStomp *own) {
    if (!own->active) {
        if (!(f_switch[own->switch_id] & 1)) {
            Sto_UpdateX(self, own);
            return;
        }
        own->active = 1;
    }

    if (own->offset_max != own->offset_now) {
        own->offset_now += 2;
        Sto_UpdateX(self, own);
        return;
    }

    // stopPlatform
    self->subtype++; // -> Sto_SlidingPlatform_Retract
    own->delay = 3 * 60;
    own->active = 0;
    if (self->respawn != 0)
        (*SstLoad_State(self)) |= 1; // seems to be unused elsewhere, kept faithfully
    Sto_UpdateX(self, own);
}

// Type 2 (set from Type 1) - retract sliding platform again after a delay
static void Sto_SlidingPlatform_Retract(Sst *self, Own_ScrapStomp *own) {
    if (!own->active) {
        if (--own->delay != 0) {
            Sto_UpdateX(self, own);
            return;
        }
        own->active = 1;
    }

    if (own->offset_now != 0) {
        own->offset_now -= 2;
        Sto_UpdateX(self, own);
        return;
    }

    // stopPlatform
    self->subtype--; // -> Sto_SlidingPlatform_Extend
    own->active = 0;
    if (self->respawn != 0)
        (*SstLoad_State(self)) &= ~1;
    Sto_UpdateX(self, own);
}

// Type 3 - stomper (stomps down, slowly retracts, waits, stomps down again)
static void Sto_Stomper_DownAndRetract(Sst *self, Own_ScrapStomp *own) {
    if (!own->active) {
        if (own->offset_now != 0) {
            own->offset_now--;
            Sto_UpdateY(self, own);
            return;
        }
        if (--own->delay >= 0) {
            Sto_UpdateY(self, own);
            return;
        }
        own->delay = 1 * 60;
        own->active = 1;
    }

    own->offset_now += 8;
    if (own->offset_now == own->offset_max)
        own->active = 0;
    Sto_UpdateY(self, own);
}

// Type 4 - stomper (stomps up and down, waits 1 second between each stomp)
static void Sto_Stomper_UpAndDown(Sst *self, Own_ScrapStomp *own) {
    if (!own->active) {
        if (own->offset_now != 0) {
            own->offset_now -= 8;
            Sto_UpdateY(self, own);
            return;
        }
        if (--own->delay >= 0) {
            Sto_UpdateY(self, own);
            return;
        }
        own->delay = 1 * 60;
        own->active = 1;
    }

    if (own->offset_now == own->offset_max) {
        if (--own->delay >= 0) {
            Sto_UpdateY(self, own);
            return;
        }
        own->delay = 1 * 60;
        own->active = 0;
        Sto_UpdateY(self, own);
        return;
    }

    own->offset_now += 8;
    Sto_UpdateY(self, own);
}

// Type 5 - ancient lift at the start of SBZ3/LZ4
static void Sto_AncientLift(Sst *self, Own_ScrapStomp *own) {
    if (!own->active) {
        if (!(f_switch[own->switch_id] & 1))
            return;
        own->active = 1;
        if (self->respawn != 0)
            (*SstLoad_State(self)) |= 1; // globally remember lift has already started moving
    }

    self->x_pos.v -= 0x10000; // move left at 1px/frame (incl. subpixels)
    self->y_pos.v += 0x8000;  // move down at 0.5px/frame (incl. subpixels)
    own->orig_x = self->x_pos.f.u; // overwrite initial X with current X

    if (self->x_pos.f.u == (int16_t)0x980) {
        self->subtype = 0; // stop -- Sto_Stationary
        own->active = 0; // redundant at this point
    }
}

static void Sto_Action(Sst *self, Own_ScrapStomp *own) {
    int16_t prev_x = self->x_pos.f.u;

    switch (self->subtype & 0xF) {
    case 0: break; // stationary
    case 1: Sto_SlidingPlatform_Extend(self, own); break;
    case 2: Sto_SlidingPlatform_Retract(self, own); break;
    case 3: Sto_Stomper_DownAndRetract(self, own); break;
    case 4: Sto_Stomper_UpAndDown(self, own); break;
    case 5: Sto_AncientLift(self, own); break;
    }

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
        SstSolidObject(self, (uint16_t)x_rad, self->y_radius, (uint16_t)(self->y_radius + 1), prev_x, NULL, NULL);
    }

    if (!SstOutOfRange(own->orig_x)) {
        Sst_Draw(self);
        return;
    }

    if (LEVEL_ZONE(level_id) == ZoneId_LZ) {
        obj6B = 0;
        if (self->respawn != 0)
            (*SstLoad_State(self)) &= 0x7F;
    }
    Sst_Delete(self);
}

static void Sto_Main(Sst *self, Own_ScrapStomp *own) {
    self->routine = 2; // advance to Sto_Action

    uint8_t subtype = self->subtype;
    int idx = (subtype >> 4) & 7;

    self->width_pixels = Sto_Var[idx].width;
    self->y_radius = (int8_t)Sto_Var[idx].height;
    self->mapping_frame = (uint8_t)idx;

    self->mappings = Mappings_StomperDoor;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Moving_Block_Short); // | Tile_Pal2 (SBZ1/2)

    if (LEVEL_ZONE(level_id) == ZoneId_LZ) { // SBZ3
        bool already_loaded = (obj6B & 1) != 0;
        obj6B |= 1;
        if (already_loaded) {
            if (self->respawn != 0)
                (*SstLoad_State(self)) &= 0x7F;
            Sst_Delete(self);
            return;
        }

        // Ancient lift at the start of SBZ3
        self->art_tile = TILE_MAP(0, 2, 0, 0, (ArtTile_Level+0x1F0)); // | Tile_Pal3
        if (self->x_pos.f.u == (int16_t)0xA80 && self->respawn != 0 &&
            ((*SstLoad_State(self)) & 1)) {
            // Prevent a pre-switched ancient lift from reappearing after
            // the switch was already pressed.
            obj6B = 0;
            if (self->respawn != 0)
                (*SstLoad_State(self)) &= 0x7F;
            Sst_Delete(self);
            return;
        }
    }

    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;
    own->offset_max = Sto_Var[idx].max_dist;

    if (subtype & 0x80) {
        own->switch_id = subtype & 0xF;
        self->subtype = Sto_Var[idx].action_type;

        if (Sto_Var[idx].action_type == 5)
            self->height_pixels = self->y_radius; // custom sprite render height

        if (self->respawn != 0)
            (*SstLoad_State(self)) &= 0x7F;
    }

    Sto_Action(self, own);
}

void Obj_ScrapStomp(Sst *self) {
    Own_ScrapStomp *own = (Own_ScrapStomp *)self->own.u8;

    if (self->routine == 0) {
        Sto_Main(self, own);
        return;
    }
    Sto_Action(self, own);
}
