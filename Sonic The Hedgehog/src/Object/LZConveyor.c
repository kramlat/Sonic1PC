#include "LZConveyor.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include <string.h>

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

static uint8_t LCon_WrapIndex(Own_LCon *own, int8_t new_idx) {
    if ((uint8_t)new_idx < own->count)
        return (uint8_t)new_idx;
    if (new_idx >= 0)
        return 0;
    return (uint8_t)(own->count - 1);
}

// Sets a platform's movement speeds towards its next X/Y target -- moves
// at a constant 1px/frame along whichever axis has the larger distance to
// cover, and a calculated (possibly fractional, tracked via subpixel)
// speed along the shorter axis so both arrive together.
void LCon_ChangeDir(Sst *self, Own_LCon *own) {
    int16_t dx = (int16_t)(self->x_pos.f.u - own->next_x);
    int16_t velx_dir = -0x100;
    if (dx < 0) {
        dx = (int16_t)-dx;
        velx_dir = 0x100;
    }

    int16_t dy = (int16_t)(self->y_pos.f.u - own->next_y);
    int16_t vely_dir = -0x100;
    if (dy < 0) {
        dy = (int16_t)-dy;
        vely_dir = 0x100;
    }

    if (dy < dx) {
        int16_t signed_dy = (int16_t)(self->y_pos.f.u - own->next_y);
        if (signed_dy == 0) {
            self->y_vel = 0;
            self->y_pos.f.l = 0;
        } else {
            int32_t num = (int32_t)signed_dy << 8;
            self->y_vel = (int16_t)-(int16_t)(num / dx);
            self->y_pos.f.l = (uint16_t)(int16_t)(num % dx);
        }
        self->x_vel = velx_dir;
        self->x_pos.f.l = 0;
        return;
    }

    int16_t signed_dx = (int16_t)(self->x_pos.f.u - own->next_x);
    if (signed_dx == 0) {
        self->x_vel = 0;
        self->x_pos.f.l = 0;
    } else {
        int32_t num = (int32_t)signed_dx << 8;
        self->x_vel = (int16_t)-(int16_t)(num / dy);
        self->x_pos.f.l = (uint16_t)(int16_t)(num % dy);
    }
    self->y_vel = vely_dir;
    self->y_pos.f.l = 0;
}

static void LCon_Platform_Update(Sst *self, Own_LCon *own) {
    bool at_corner;
    if (f_switch[0xE] && !own->reversed) {
        own->reversed = true;
        f_conveyrev = true;
        own->increment = -1;
        at_corner = true;
    } else {
        at_corner = (self->x_pos.f.u == own->next_x && self->y_pos.f.u == own->next_y);
    }

    if (at_corner) {
        own->posindex = LCon_WrapIndex(own, (int8_t)(own->posindex + own->increment));
        own->next_x = own->points[own->posindex].x;
        own->next_y = own->points[own->posindex].y;
        LCon_ChangeDir(self, own);
    }

    Sst_MoveSprite2(self);
}

static void LCon_Platform(Sst *self, Own_LCon *own) {
    SstPlatformObject(self, self->width_pixels); // sets self->routine = 4 on enter
    LCon_Platform_Update(self, own);
}

static void LCon_OnPlatform(Sst *self, Own_LCon *own) {
    SstExitPlatform(self, self->width_pixels, self->width_pixels, NULL); // sets self->routine = 2 on exit

    int16_t prev_x = self->x_pos.f.u;
    LCon_Platform_Update(self, own);
    SstMvSonicOnPtfm(self, (int16_t)(self->y_pos.f.u - 9), prev_x);
}

static void LCon_Wheel(Sst *self) {
    if (((uint8_t)frame_count & 3) == 0) {
        int8_t step = f_conveyrev ? -1 : 1;
        self->mapping_frame = (uint8_t)((self->mapping_frame + step) & 3);
    }
    SstOnScreenTest(self);
}

static void LCon_Main_Platform(Sst *self, Own_LCon *own, uint8_t subtype) {
    self->routine += 2;
    self->mappings = Mappings_LZConveyor;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Conveyor_Belt); // | Tile_Pal3
    self->render_flags |= SST_RENDER_LEVEL;
    self->width_pixels = 32 / 2;
    self->priority = 4 * 0x80;

    if (subtype == 0x7F) {
        // Decorative wheel
        self->routine += 4; // -> LCon_Wheel
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Conveyor_Belt); // palette line 1
        self->priority = 1 * 0x80;
        LCon_Wheel(self);
        return;
    }

    self->mapping_frame = 4; // "platform" frame

    uint8_t group_idx = (subtype >> 4) & 0xF;
    own->count = LCon_Data[group_idx].count;
    own->base_x = LCon_Data[group_idx].base_x;
    own->points = LCon_Data[group_idx].points;

    own->posindex = subtype & 0xF;
    own->increment = 1;
    own->reversed = false; // the spawner's own slot isn't cleared -- don't trust a stale byte

    if (f_conveyrev) {
        own->reversed = true;
        own->increment = -1;
        own->posindex = LCon_WrapIndex(own, (int8_t)(own->posindex + own->increment));
    }

    own->next_x = own->points[own->posindex].x;
    own->next_y = own->points[own->posindex].y;

    LCon_ChangeDir(self, own);
    LCon_Platform(self, own);
}

// Returns false if the spawner deleted itself (no display this frame, no
// group of newly spawned platforms displays this frame either -- they'll
// each run through LCon_Main again, as regular platforms, next frame).
static bool LCon_Main_Spawner(Sst *self, uint8_t subtype) {
    Own_LCon *own = (Own_LCon *)self->own.u8;
    own->groupid = (int8_t)subtype;
    uint8_t group = subtype & 0x7F;

    if (obj63_loaded[group] & 1) {
        Sst_Delete(self);
        return false;
    }
    obj63_loaded[group] |= 1;

    const uint8_t *data = LCon_ObjPos[group & 7];
    uint16_t count = (uint16_t)((data[0] << 8) | data[1]);
    data += 2;

    for (uint16_t i = 0; i <= count; i++) {
        Sst *plat = (i == 0) ? self : Sst_AllocateAfter(self);
        if (plat == NULL) // none left after the spawner: any free slot will do (the original takes the first free one anywhere), or the
            plat = Sst_Allocate(); // rest of the group is never made and a rope loop is left with one or two platforms
        if (plat == NULL)
            break;

        if (plat != self) {
            // FixBugs forced on (crash-class under C): a slot can carry stale
            // bytes. A leftover nonzero routine skipped
            // platform init, so the stale host pointer in own->points
            // got dereferenced at the first corner. Start from a clean slot.
            memset(plat, 0, sizeof(Sst));
        }

        plat->code = Obj_LabyrinthConvey;
        plat->x_pos.f.u = (int16_t)((data[0] << 8) | data[1]);
        plat->y_pos.f.u = (int16_t)((data[2] << 8) | data[3]);
        plat->subtype = data[5]; // subtype (stored as a word; low byte only is ever nonzero)
        data += 6;
    }

    return false;
}

void Obj_LabyrinthConvey(Sst *self) {
    Own_LCon *own = (Own_LCon *)self->own.u8;

    switch (self->routine) {
    case 0: {
        uint8_t subtype = self->subtype;
        if ((int8_t)subtype < 0) {
            LCon_Main_Spawner(self, subtype);
            return; // matches real ASM skipping the shared display/delete tail entirely
        }
        LCon_Main_Platform(self, own, subtype);
        return; // LCon_Main_Platform already reached LCon_Platform/LCon_Wheel, which handle their own tail
    }
    case 2:
        LCon_Platform(self, own);
        break;
    case 4:
        LCon_OnPlatform(self, own);
        break;
    case 6:
        LCon_Wheel(self); // handles its own display/delete (SstOnScreenTest), skip the shared tail below
        return;
    }

    // Shared tail: offscreen check (using the group's stable base_x), with
    // one exception -- and clears the group's "loaded" flag on deletion if
    // this happens to be the (former) spawner object.
    uint16_t delta = (uint16_t)(((uint16_t)own->base_x & ~0x7F) - (((uint16_t)scrpos_x.f.u - 0x80) & ~0x7F));
    if (delta <= (((SCREEN_WIDTH + 0x80) & ~0x7F) + 0x100)) {
        Sst_Draw(self);
        return;
    }

    // LZ3 pop-in fix: platforms in wide group 4 shouldn't despawn just
    // from barely crossing one coarse ($80px) step past the left edge.
    if (LEVEL_ACT(level_id) == 2 && delta >= 0xFF80) {
        Sst_Draw(self);
        return;
    }

    if (own->groupid >= 0) {
        Sst_Delete(self);
        return;
    }
    obj63_loaded[own->groupid & 0x7F] &= (uint8_t)~1;
    Sst_Delete(self);
}
