#include "SpinConveyor.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "LZConveyor.h"
#include "Resource/Animation/SpinConveyor.h"

extern const uint8_t Mappings_SpinningPlatforms[]; // From Object/SpinPlatform.c
extern const uint8_t ObjectLayout_SBZ1PF1[]; // From Level.c
extern const uint8_t ObjectLayout_SBZ1PF2[]; // From Level.c
extern const uint8_t ObjectLayout_SBZ1PF3[]; // From Level.c
extern const uint8_t ObjectLayout_SBZ1PF4[]; // From Level.c
extern const uint8_t ObjectLayout_SBZ1PF5[]; // From Level.c
extern const uint8_t ObjectLayout_SBZ1PF6[]; // From Level.c

// Object 6F - spinning platforms that move around a conveyor belt (SBZ).
// This is pretty much an edited copy-paste of Object 63 (LZ conveyor) --
// reuses that object's own Own_LCon layout and its LCon_ChangeDir
// subroutine. Unlike the LZ version, these platforms are solid from all
// sides (not just from above) while upright, and become fully non-solid
// (rather than walk-off-able) while spinning.

static const uint8_t *const SpinC_ObjPos[8] = {
    ObjectLayout_SBZ1PF1, ObjectLayout_SBZ1PF2, ObjectLayout_SBZ1PF3,
    ObjectLayout_SBZ1PF4, ObjectLayout_SBZ1PF5, ObjectLayout_SBZ1PF6,
    ObjectLayout_SBZ1PF1, ObjectLayout_SBZ1PF2,
};

// Conveyor belt corner target coordinates -- one group per lower nybble of
// a (non-spawner) platform's subtype. base_x is only used for the
// out-of-range check, matching LZConveyor's own base_x field.
static const struct { int16_t base_x; LCon_Point points[4]; } SpinC_Data[6] = {
    { 0xE80,  { { 0xE14, 0x370 }, { 0xEEF, 0x302 }, { 0xEEF, 0x340 }, { 0xE14, 0x3AE } } },
    { 0xF80,  { { 0xF14, 0x2E0 }, { 0xFEF, 0x272 }, { 0xFEF, 0x2B0 }, { 0xF14, 0x31E } } },
    { 0x1080, { { 0x1014,0x270 }, { 0x10EF,0x202 }, { 0x10EF,0x240 }, { 0x1014,0x2AE } } },
    { 0xF80,  { { 0xF14, 0x570 }, { 0xFEF, 0x502 }, { 0xFEF, 0x540 }, { 0xF14, 0x5AE } } },
    { 0x1B80, { { 0x1B14,0x670 }, { 0x1BEF,0x602 }, { 0x1BEF,0x640 }, { 0x1B14,0x6AE } } },
    { 0x1C80, { { 0x1C14,0x5E0 }, { 0x1CEF,0x572 }, { 0x1CEF,0x5B0 }, { 0x1C14,0x61E } } },
};

static uint8_t SpinC_WrapIndex(uint8_t count, int8_t new_idx) {
    if ((uint8_t)new_idx < count)
        return (uint8_t)new_idx;
    if (new_idx >= 0)
        return 0;
    return (uint8_t)(count - 1);
}

static void SpinC_Platform_Update(Sst *self, Own_LCon *own) {
    bool at_corner = (self->x_pos.f.u == own->next_x && self->y_pos.f.u == own->next_y);

    if (at_corner) {
        own->posindex = SpinC_WrapIndex(own->count, (int8_t)(own->posindex + own->increment));
        own->next_x = own->points[own->posindex].x;
        own->next_y = own->points[own->posindex].y;

        if (own->posindex == 0)
            self->anim = 1; // "still"
        if (own->posindex == 2)
            self->anim = 0; // "spinning"

        LCon_ChangeDir(self, own);
    }

    Sst_MoveSprite2(self);
}

static void SpinC_Solid(Sst *self, Own_LCon *own) {
    Sst_AnimateFlips(self, Animation_SpinConveyor);

    if (self->mapping_frame == 0) { // upright -- solid from all sides
        int16_t prev_x = self->x_pos.f.u;
        SpinC_Platform_Update(self, own);
        int16_t x_rad = (int16_t)(32 / 2 + 11 /* sonic_solid_width */);
        SstSolidObject(self, (uint16_t)x_rad, 14 / 2, (uint16_t)(14 / 2 + 1), prev_x, NULL, NULL);
        return;
    }

    // Spinning -- not solid
    if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    }
    SpinC_Platform_Update(self, own);
}

static void SpinC_Main_Platform(Sst *self, Own_LCon *own, uint8_t subtype) {
    self->routine = 2; // advance to SpinC_Solid
    self->mappings = Mappings_SpinningPlatforms;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Spinning_Platform);
    self->width_pixels = 32 / 2;
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;

    uint8_t group_idx = (subtype >> 4) & 0xF;
    own->count = 4;
    own->base_x = SpinC_Data[group_idx].base_x;
    own->points = SpinC_Data[group_idx].points;

    own->posindex = subtype & 0xF;
    own->increment = 1;
    own->reversed = false; // the spawner's own slot isn't cleared -- don't trust a stale byte

    if (convey_rev) {
        own->reversed = true;
        own->increment = -1;
        own->posindex = SpinC_WrapIndex(own->count, (int8_t)(own->posindex + own->increment));
    }

    own->next_x = own->points[own->posindex].x;
    own->next_y = own->points[own->posindex].y;

    // FixBugs: ensure platforms on the upper path never start in the
    // spinning state.
    self->anim = (own->posindex < 2) ? 1 : 0;

    LCon_ChangeDir(self, own);
    SpinC_Solid(self, own);
}

static void SpinC_Main_Spawner(Sst *self, uint8_t subtype) {
    Own_LCon *own = (Own_LCon *)self->own.u8;
    own->groupid = (int8_t)subtype;
    uint8_t group = subtype & 0x7F;

    if (obj63_loaded[group] & 1) {
        Sst_Delete(self);
        return;
    }
    obj63_loaded[group] |= 1;

    const uint8_t *data = SpinC_ObjPos[group & 7];
    uint16_t count = (uint16_t)((data[0] << 8) | data[1]);
    data += 2;

    for (uint16_t i = 0; i <= count; i++) {
        Sst *plat = (i == 0) ? self : Sst_AllocateAfter(self);
        if (plat == NULL)
            break;

        if (plat != self) {
            // FixBugs forced on (crash-class under C): a slot can carry stale
            // bytes. A leftover nonzero routine skipped
            // platform init, so the stale host pointer in own->points
            // got dereferenced at the first corner. Start from a clean slot.
            memset(plat, 0, sizeof(Sst));
        }

        plat->code = Obj_SpinConveyor;
        plat->x_pos.f.u = (int16_t)((data[0] << 8) | data[1]);
        plat->y_pos.f.u = (int16_t)((data[2] << 8) | data[3]);
        plat->subtype = data[5]; // subtype (stored as a word; low byte only is ever nonzero)
        data += 6;
    }
}

void Obj_SpinConveyor(Sst *self) {
    Own_LCon *own = (Own_LCon *)self->own.u8;

    if (self->routine == 0) {
        uint8_t subtype = self->subtype;
        if ((int8_t)subtype < 0)
            SpinC_Main_Spawner(self, subtype);
        else
            SpinC_Main_Platform(self, own, subtype);
        return;
    }

    SpinC_Solid(self, own);

    if (!SstOutOfRange(own->base_x)) {
        Sst_Draw(self);
        return;
    }

    // Real hardware's out-of-range branch also checks for act 3 here -- a
    // dead, pointless leftover from copy-pasting Object 63 (SBZ act 3 is
    // Final Zone internally and never has these platforms), so it's
    // omitted here.
    if (own->groupid >= 0) {
        Sst_Delete(self);
        return;
    }
    obj63_loaded[own->groupid & 0x7F] &= (uint8_t)~1;
    Sst_Delete(self);
}
