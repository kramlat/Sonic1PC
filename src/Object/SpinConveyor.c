#include "SpinConveyor.h"

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
// reuses that object's own Scratch_LCon layout and its LCon_ChangeDir
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

static bool SpinC_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void SpinC_Platform_Update(Object *obj, Scratch_LCon *scratch) {
    bool at_corner = (obj->pos.l.x.f.u == scratch->next_x && obj->pos.l.y.f.u == scratch->next_y);

    if (at_corner) {
        scratch->posindex = SpinC_WrapIndex(scratch->count, (int8_t)(scratch->posindex + scratch->increment));
        scratch->next_x = scratch->points[scratch->posindex].x;
        scratch->next_y = scratch->points[scratch->posindex].y;

        if (scratch->posindex == 0)
            obj->anim = 1; // "still"
        if (scratch->posindex == 2)
            obj->anim = 0; // "spinning"

        LCon_ChangeDir(obj, scratch);
    }

    SpeedToPos(obj);
}

static void SpinC_Solid(Object *obj, Scratch_LCon *scratch) {
    AnimateSprite(obj, Animation_SpinConveyor);

    if (obj->frame == 0) { // upright -- solid from all sides
        int16_t prev_x = obj->pos.l.x.f.u;
        SpinC_Platform_Update(obj, scratch);
        int16_t x_rad = (int16_t)(32 / 2 + 11 /* sonic_solid_width */);
        SolidObject(obj, (uint16_t)x_rad, 14 / 2, (uint16_t)(14 / 2 + 1), prev_x, NULL, NULL);
        return;
    }

    // Spinning -- not solid
    if (obj->status.o.f.player_stand) {
        player->status.p.f.object_stand = false;
        obj->status.o.f.player_stand = false;
    }
    SpinC_Platform_Update(obj, scratch);
}

static void SpinC_Main_Platform(Object *obj, Scratch_LCon *scratch, uint8_t subtype) {
    obj->routine = 2; // advance to SpinC_Solid
    obj->mappings = Mappings_SpinningPlatforms;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x4DF); // ArtTile_SBZ_Spinning_Platform
    obj->width_pixels = 32 / 2;
    obj->render.f.align_fg = true;
    obj->priority = 4;

    uint8_t group_idx = (subtype >> 4) & 0xF;
    scratch->count = 4;
    scratch->base_x = SpinC_Data[group_idx].base_x;
    scratch->points = SpinC_Data[group_idx].points;

    scratch->posindex = subtype & 0xF;
    scratch->increment = 1;

    if (convey_rev) {
        scratch->reversed = true;
        scratch->increment = -1;
        scratch->posindex = SpinC_WrapIndex(scratch->count, (int8_t)(scratch->posindex + scratch->increment));
    }

    scratch->next_x = scratch->points[scratch->posindex].x;
    scratch->next_y = scratch->points[scratch->posindex].y;

    // FixBugs: ensure platforms on the upper path never start in the
    // spinning state.
    obj->anim = (scratch->posindex < 2) ? 1 : 0;

    LCon_ChangeDir(obj, scratch);
    SpinC_Solid(obj, scratch);
}

static void SpinC_Main_Spawner(Object *obj, uint8_t subtype) {
    Scratch_LCon *scratch = (Scratch_LCon *)&obj->scratch;
    scratch->groupid = (int8_t)subtype;
    uint8_t group = subtype & 0x7F;

    if (obj63_loaded[group] & 1) {
        ObjectDelete(obj);
        return;
    }
    obj63_loaded[group] |= 1;

    const uint8_t *data = SpinC_ObjPos[group & 7];
    uint16_t count = (uint16_t)((data[0] << 8) | data[1]);
    data += 2;

    for (uint16_t i = 0; i <= count; i++) {
        Object *plat = (i == 0) ? obj : FindNextFreeObj(obj);
        if (plat == NULL)
            break;

        plat->type = ObjId_SpinConvey;
        plat->pos.l.x.f.u = (int16_t)((data[0] << 8) | data[1]);
        plat->pos.l.y.f.u = (int16_t)((data[2] << 8) | data[3]);
        plat->scratch.u8[0] = data[5]; // subtype (stored as a word; low byte only is ever nonzero)
        data += 6;
    }
}

void Obj_SpinConveyor(Object *obj) {
    Scratch_LCon *scratch = (Scratch_LCon *)&obj->scratch;

    if (obj->routine == 0) {
        uint8_t subtype = obj->scratch.u8[0];
        if ((int8_t)subtype < 0)
            SpinC_Main_Spawner(obj, subtype);
        else
            SpinC_Main_Platform(obj, scratch, subtype);
        return;
    }

    SpinC_Solid(obj, scratch);

    if (!SpinC_OutOfRange(scratch->base_x)) {
        DisplaySprite(obj);
        return;
    }

    // Real hardware's out-of-range branch also checks for act 3 here -- a
    // dead, pointless leftover from copy-pasting Object 63 (SBZ act 3 is
    // Final Zone internally and never has these platforms), so it's
    // omitted here.
    if (scratch->groupid >= 0) {
        ObjectDelete(obj);
        return;
    }
    obj63_loaded[scratch->groupid & 0x7F] &= (uint8_t)~1;
    ObjectDelete(obj);
}
