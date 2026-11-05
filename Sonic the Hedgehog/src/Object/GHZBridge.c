#include "GHZBridge.h"
#include "Object.h"
#include "Resource/Mappings/GHZBridge.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"

#include "MathUtil.h"

#include "Macros.h"

static void Obj_GHZBridge_Bend(Sst *self) {
    Own_GHZBridge *own = (Own_GHZBridge *)self->own.u8;

    uint16_t d0;
    uint16_t d3;
    uint8_t d2;
    uint8_t d5;
    const uint8_t* a3;
    uint8_t* a2;

    // Get push factor
    int16_t d4 = GetSin(own->push);

    // Get bend state
    d0 = self->subtype << 4;
    d3 = own->push_seg;
    d2 = d3;
    d3 += d0;

    d5 = ghz_bridge_bend1[d3];
    a3 = &ghz_bridge_bend2[(d3 & 0xF) << 4];
    a2 = own->seg;

    // Apply bend up to standing segment
    do {
        // Get segment
        Sst *seg = &sst[*a2++];
        Own_GHZBridge *segscratch = (Own_GHZBridge *)seg->own.u8;

        // Move segment
        uint32_t fac = (uint16_t)(*a3++ + 1) * d5 * d4;
        seg->y_pos.f.u = segscratch->base_y + (fac >> 16);
    } while (d2-- > 0);

    // Get bend state
    d0 = self->subtype;
    d3 = -(own->push_seg + 1 - d0);
    if (d3 & 0x80)
        return;
    d2 = d3;

    a3 = &ghz_bridge_bend2[(d3 << 4) + d2];
    if (--d2 == 0xFF)
        return;

    // Apply bend up to standing segment
    do {
        // Get segment
        Sst *seg = &sst[*a2++];
        Own_GHZBridge *segscratch = (Own_GHZBridge *)seg->own.u8;

        // Move segment
        uint32_t fac = (uint16_t)(*--a3 + 1) * d5 * d4;
        seg->y_pos.f.u = segscratch->base_y + (fac >> 16);
    } while (d2-- > 0);
}

static void Obj_GHZBridge_Solid(Sst *self) {
    Own_GHZBridge *own = (Own_GHZBridge *)self->own.u8;

    // Get bridge size
    uint16_t x_rad = (self->subtype << 3) + 8;
    uint16_t x_dia = self->subtype << 4;

    // Check if player is colliding with bridge
    if (player->y_vel < 0)
        return;

    int16_t off = player->x_pos.f.u - self->x_pos.f.u + x_rad;
    if (off < 0 || off >= x_dia)
        return;

    // Collide with bridge
    SstPlatform3(self, self->y_pos.f.u - 8);
}

static void Obj_GHZBridge_MoveSonic(Sst *self) {
    Own_GHZBridge *own = (Own_GHZBridge *)self->own.u8;

    // Clip Sonic to the top of the bridge segment being stood on
    Sst *seg = &sst[own->seg[own->push_seg]];
    player->y_pos.f.u = seg->y_pos.f.u - 8 - player->y_radius;
}

static void Obj_GHZBridge_WalkOff(Sst *self) {
    Own_GHZBridge *own = (Own_GHZBridge *)self->own.u8;

    // Check if we've walked off the platform
    int16_t x_off;
    uint16_t x_rad = (self->subtype << 3) + 8;

    if (!SstExitPlatform(self, x_rad, self->subtype << 3, &x_off)) {
        own->push_seg = x_off >> 4;
        if (own->push != 0x40)
            own->push += 4;
        Obj_GHZBridge_Bend(self);
        Obj_GHZBridge_MoveSonic(self);
    }
}

static void Obj_GHZBridge_ChkDel(Sst *self) {
    Own_GHZBridge *own = (Own_GHZBridge *)self->own.u8;

    if (SstOutOfRange(self->x_pos.f.u)) {
        // Off-screen
        uint8_t* segp = own->seg;
        uint8_t segs = self->subtype - 1;

        do {
            Sst *seg = &sst[*segp++];
            if (seg != self)
                Sst_Delete(seg);
        } while (segs-- > 0);
        SstDeleteIfNotInRange(self);
    }
}

void Obj_GHZBridge(Sst *self) {
    Own_GHZBridge *own = (Own_GHZBridge *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_GHZBridge;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Bridge);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 128;

        // Create bridge segments
        int16_t y = self->y_pos.f.u;
        int16_t x = self->x_pos.f.u;
        SstCode code = self->code;
        uint8_t* segp = own->seg;
        uint8_t segs = self->subtype;

        // Handle segment count
        self->subtype = 0;
        x -= ((segs >> 1) << 4);

        if (!((segs -= 2) >= 0xFE)) {
            do {
                // Get segment object
                Sst *seg = Sst_Allocate();
                if (seg == NULL)
                    break;

                // Update controller state
                self->subtype++;
                if (x == self->x_pos.f.u) {
                    x += 16;
                    self->y_pos.f.u = y;
                    own->base_y = y;
                    *segp++ = (uint8_t)(self - sst);
                    self->subtype++;
                }
                *segp++ = (uint8_t)(seg - sst);

                // Set segment object
                Own_GHZBridge *segscratch = (Own_GHZBridge *)seg->own.u8;
                seg->routine = 10;
                seg->code = code;
                seg->y_pos.f.u = y;
                segscratch->base_y = y;
                seg->x_pos.f.u = x;
                seg->mappings = Mappings_GHZBridge;
                seg->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Bridge);
                seg->render_flags = SST_RENDER_LEVEL;
                seg->priority = 3 * 0x80;
                seg->width_pixels = 8;

                x += 16;
            } while (segs-- > 0);
        }
        // Fallthrough
    case 2: // Controller not stood on
        Obj_GHZBridge_Solid(self);
        if (own->push)
            own->push -= 4;
        Obj_GHZBridge_Bend(self);
        Sst_Draw(self);
        Obj_GHZBridge_ChkDel(self);
        break;
    case 4: // Controller stood on
        Obj_GHZBridge_WalkOff(self);
        Sst_Draw(self);
        Obj_GHZBridge_ChkDel(self);
        break;
    case 6: // Delete
    case 8:
        Sst_Delete(self);
        break;
    case 10: // Segment draw
        Sst_Draw(self);
        break;
    }
}
