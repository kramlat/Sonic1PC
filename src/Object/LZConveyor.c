#include "LZConveyor.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "PaletteCycle.h"
#include "Resource/Mappings/LZConveyor.h"

extern const uint8_t ObjectLayout_LZ1PF1[]; // From Level.c
extern const uint8_t ObjectLayout_LZ1PF2[]; // From Level.c
extern const uint8_t ObjectLayout_LZ2PF1[]; // From Level.c
extern const uint8_t ObjectLayout_LZ2PF2[]; // From Level.c
extern const uint8_t ObjectLayout_LZ3PF1[]; // From Level.c
extern const uint8_t ObjectLayout_LZ3PF2[]; // From Level.c

// Object 63 - platforms on a conveyor belt (LZ). One "spawner" object (a
// level-placed instance with subtype bit 7 set) reads a group's worth of
// platform placements from a level-specific ObjPos-format table (X, Y,
// subtype triples) and spawns them all -- the spawner itself becomes the
// first platform. Every spawned platform then independently decodes its
// own subtype into a corner-path group (see LCon_Data) and a starting
// corner within it, and follows that path around indefinitely, optionally
// reversing direction when switch $E is pressed. Decorative wheel objects
// (subtype $7F) are also placed directly and just animate in place.

static const uint8_t *const LCon_ObjPos[8] = {
    ObjectLayout_LZ1PF1, ObjectLayout_LZ1PF2,
    ObjectLayout_LZ2PF1, ObjectLayout_LZ2PF2,
    ObjectLayout_LZ3PF1, ObjectLayout_LZ3PF2,
    ObjectLayout_LZ1PF1, ObjectLayout_LZ1PF2,
};

// Conveyor belt corner target coordinate definitions -- one group per
// lower nybble of a (non-wheel, non-spawner) platform's subtype.
static const LCon_Point lcon_points0[] = { { 0x1078, 0x21A }, { 0x10BE, 0x260 }, { 0x10BE, 0x393 }, { 0x108C, 0x3C5 }, { 0x1022, 0x390 }, { 0x1022, 0x244 } };
static const LCon_Point lcon_points1[] = { { 0x127E, 0x280 }, { 0x12CE, 0x2D0 }, { 0x12CE, 0x46E }, { 0x1232, 0x420 }, { 0x1232, 0x2CC } };
static const LCon_Point lcon_points2[] = { { 0xD22, 0x482 }, { 0xD22, 0x5DE }, { 0xDAE, 0x5DE }, { 0xDAE, 0x482 } };
static const LCon_Point lcon_points3[] = { { 0xD62, 0x3A2 }, { 0xDEE, 0x3A2 }, { 0xDEE, 0x4DE }, { 0xD62, 0x4DE } };
static const LCon_Point lcon_points4[] = { { 0xCAC, 0x242 }, { 0xDDE, 0x242 }, { 0xDDE, 0x3DE }, { 0xC52, 0x3DE }, { 0xC52, 0x29C } };
static const LCon_Point lcon_points5[] = { { 0x1252, 0x20A }, { 0x13DE, 0x20A }, { 0x13DE, 0x2BE }, { 0x1252, 0x2BE } };

static const struct { uint8_t count; int16_t base_x; const LCon_Point *points; } LCon_Data[6] = {
    { 6, 0x1070, lcon_points0 },
    { 5, 0x1280, lcon_points1 },
    { 4, 0xD68, lcon_points2 },
    { 4, 0xDA0, lcon_points3 },
    { 5, 0xD00, lcon_points4 },
    { 4, 0x1300, lcon_points5 },
};

static uint8_t LCon_WrapIndex(Scratch_LCon *scratch, int8_t new_idx) {
    if ((uint8_t)new_idx < scratch->count)
        return (uint8_t)new_idx;
    if (new_idx >= 0)
        return 0;
    return (uint8_t)(scratch->count - 1);
}

// Sets a platform's movement speeds towards its next X/Y target -- moves
// at a constant 1px/frame along whichever axis has the larger distance to
// cover, and a calculated (possibly fractional, tracked via subpixel)
// speed along the shorter axis so both arrive together.
void LCon_ChangeDir(Object *obj, Scratch_LCon *scratch) {
    int16_t dx = (int16_t)(obj->pos.l.x.f.u - scratch->next_x);
    int16_t velx_dir = -0x100;
    if (dx < 0) {
        dx = (int16_t)-dx;
        velx_dir = 0x100;
    }

    int16_t dy = (int16_t)(obj->pos.l.y.f.u - scratch->next_y);
    int16_t vely_dir = -0x100;
    if (dy < 0) {
        dy = (int16_t)-dy;
        vely_dir = 0x100;
    }

    if (dy < dx) {
        int16_t signed_dy = (int16_t)(obj->pos.l.y.f.u - scratch->next_y);
        if (signed_dy == 0) {
            obj->ysp = 0;
            obj->pos.l.y.f.l = 0;
        } else {
            int32_t num = (int32_t)signed_dy << 8;
            obj->ysp = (int16_t)-(int16_t)(num / dx);
            obj->pos.l.y.f.l = (uint16_t)(int16_t)(num % dx);
        }
        obj->xsp = velx_dir;
        obj->pos.l.x.f.l = 0;
        return;
    }

    int16_t signed_dx = (int16_t)(obj->pos.l.x.f.u - scratch->next_x);
    if (signed_dx == 0) {
        obj->xsp = 0;
        obj->pos.l.x.f.l = 0;
    } else {
        int32_t num = (int32_t)signed_dx << 8;
        obj->xsp = (int16_t)-(int16_t)(num / dy);
        obj->pos.l.x.f.l = (uint16_t)(int16_t)(num % dy);
    }
    obj->ysp = vely_dir;
    obj->pos.l.y.f.l = 0;
}

static void LCon_Platform_Update(Object *obj, Scratch_LCon *scratch) {
    bool at_corner;
    if (f_switch[0xE] && !scratch->reversed) {
        scratch->reversed = true;
        f_conveyrev = true;
        scratch->increment = -1;
        at_corner = true;
    } else {
        at_corner = (obj->pos.l.x.f.u == scratch->next_x && obj->pos.l.y.f.u == scratch->next_y);
    }

    if (at_corner) {
        scratch->posindex = LCon_WrapIndex(scratch, (int8_t)(scratch->posindex + scratch->increment));
        scratch->next_x = scratch->points[scratch->posindex].x;
        scratch->next_y = scratch->points[scratch->posindex].y;
        LCon_ChangeDir(obj, scratch);
    }

    SpeedToPos(obj);
}

static void LCon_Platform(Object *obj, Scratch_LCon *scratch) {
    PlatformObject(obj, obj->width_pixels); // sets obj->routine = 4 on enter
    LCon_Platform_Update(obj, scratch);
}

static void LCon_OnPlatform(Object *obj, Scratch_LCon *scratch) {
    ExitPlatform(obj, obj->width_pixels, obj->width_pixels, NULL); // sets obj->routine = 2 on exit

    int16_t prev_x = obj->pos.l.x.f.u;
    LCon_Platform_Update(obj, scratch);
    MvSonicOnPtfm(obj, (int16_t)(obj->pos.l.y.f.u - 9), prev_x);
}

static void LCon_Wheel(Object *obj) {
    if (((uint8_t)frame_count & 3) == 0) {
        int8_t step = f_conveyrev ? -1 : 1;
        obj->frame = (uint8_t)((obj->frame + step) & 3);
    }
    RememberState(obj);
}

static void LCon_Main_Platform(Object *obj, Scratch_LCon *scratch, uint8_t subtype) {
    obj->routine += 2;
    obj->mappings = Mappings_LZConveyor;
    obj->tile = TILE_MAP(0, 2, 0, 0, 0x3F6); // ArtTile_LZ_Conveyor_Belt | Tile_Pal3
    obj->render.f.align_fg = true;
    obj->width_pixels = 32 / 2;
    obj->priority = 4;

    if (subtype == 0x7F) {
        // Decorative wheel
        obj->routine += 4; // -> LCon_Wheel
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x3F6); // palette line 1
        obj->priority = 1;
        LCon_Wheel(obj);
        return;
    }

    obj->frame = 4; // "platform" frame

    uint8_t group_idx = (subtype >> 4) & 0xF;
    scratch->count = LCon_Data[group_idx].count;
    scratch->base_x = LCon_Data[group_idx].base_x;
    scratch->points = LCon_Data[group_idx].points;

    scratch->posindex = subtype & 0xF;
    scratch->increment = 1;

    if (f_conveyrev) {
        scratch->reversed = true;
        scratch->increment = -1;
        scratch->posindex = LCon_WrapIndex(scratch, (int8_t)(scratch->posindex + scratch->increment));
    }

    scratch->next_x = scratch->points[scratch->posindex].x;
    scratch->next_y = scratch->points[scratch->posindex].y;

    LCon_ChangeDir(obj, scratch);
    LCon_Platform(obj, scratch);
}

// Returns false if the spawner deleted itself (no display this frame, no
// group of newly spawned platforms displays this frame either -- they'll
// each run through LCon_Main again, as regular platforms, next frame).
static bool LCon_Main_Spawner(Object *obj, uint8_t subtype) {
    Scratch_LCon *scratch = (Scratch_LCon *)&obj->scratch;
    scratch->groupid = (int8_t)subtype;
    uint8_t group = subtype & 0x7F;

    if (obj63_loaded[group] & 1) {
        ObjectDelete(obj);
        return false;
    }
    obj63_loaded[group] |= 1;

    const uint8_t *data = LCon_ObjPos[group & 7];
    uint16_t count = (uint16_t)((data[0] << 8) | data[1]);
    data += 2;

    for (uint16_t i = 0; i <= count; i++) {
        Object *plat = (i == 0) ? obj : FindNextFreeObj(obj);
        if (plat == NULL)
            break;

        plat->type = ObjId_LabyrinthConvey;
        plat->pos.l.x.f.u = (int16_t)((data[0] << 8) | data[1]);
        plat->pos.l.y.f.u = (int16_t)((data[2] << 8) | data[3]);
        plat->scratch.u8[0] = data[5]; // subtype (stored as a word; low byte only is ever nonzero)
        data += 6;
    }

    return false;
}

void Obj_LabyrinthConvey(Object *obj) {
    Scratch_LCon *scratch = (Scratch_LCon *)&obj->scratch;

    switch (obj->routine) {
    case 0: {
        uint8_t subtype = obj->scratch.u8[0];
        if ((int8_t)subtype < 0) {
            LCon_Main_Spawner(obj, subtype);
            return; // matches real ASM skipping the shared display/delete tail entirely
        }
        LCon_Main_Platform(obj, scratch, subtype);
        return; // LCon_Main_Platform already reached LCon_Platform/LCon_Wheel, which handle their own tail
    }
    case 2:
        LCon_Platform(obj, scratch);
        break;
    case 4:
        LCon_OnPlatform(obj, scratch);
        break;
    case 6:
        LCon_Wheel(obj); // handles its own display/delete (RememberState), skip the shared tail below
        return;
    }

    // Shared tail: offscreen check (using the group's stable base_x), with
    // one exception -- and clears the group's "loaded" flag on deletion if
    // this happens to be the (former) spawner object.
    uint16_t delta = (uint16_t)(((uint16_t)scratch->base_x & ~0x7F) - (((uint16_t)scrpos_x.f.u - 0x80) & ~0x7F));
    if (delta <= (((SCREEN_WIDTH + 0x80) & ~0x7F) + 0x100)) {
        DisplaySprite(obj);
        return;
    }

    // LZ3 pop-in fix: platforms in wide group 4 shouldn't despawn just
    // from barely crossing one coarse ($80px) step past the left edge.
    if (LEVEL_ACT(level_id) == 2 && delta >= 0xFF80) {
        DisplaySprite(obj);
        return;
    }

    if (scratch->groupid >= 0) {
        ObjectDelete(obj);
        return;
    }
    obj63_loaded[scratch->groupid & 0x7F] &= (uint8_t)~1;
    ObjectDelete(obj);
}
