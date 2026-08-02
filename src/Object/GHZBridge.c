#include "GHZBridge.h"

#include "Level.h"
#include "LevelScroll.h"

#include "MathUtil.h"

#include "Macros.h"

static void Obj_GHZBridge_Bend(Object* obj) {
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    uint16_t d0;
    uint16_t d3;
    uint8_t d2;
    uint8_t d5;
    const uint8_t* a3;
    uint8_t* a2;

    // Get push factor
    int16_t d4 = GetSin(scratch->push);

    // Get bend state
    d0 = scratch->subtype << 4;
    d3 = scratch->push_seg;
    d2 = d3;
    d3 += d0;

    d5 = ghz_bridge_bend1[d3];
    a3 = &ghz_bridge_bend2[(d3 & 0xF) << 4];
    a2 = scratch->seg;

    // Apply bend up to standing segment
    do {
        // Get segment
        Object* seg = objects + *a2++;
        Scratch_GHZBridge* segscratch = (Scratch_GHZBridge*)&obj->scratch;

        // Move segment
        uint32_t fac = (uint16_t)(*a3++ + 1) * d5 * d4;
        seg->pos.l.y.f.u = segscratch->base_y + (fac >> 16);
    } while (d2-- > 0);

    // Get bend state
    d0 = scratch->subtype;
    d3 = -(scratch->push_seg + 1 - d0);
    if (d3 & 0x80)
        return;
    d2 = d3;

    a3 = &ghz_bridge_bend2[(d3 << 4) + d2];
    if (--d2 == 0xFF)
        return;

    // Apply bend up to standing segment
    do {
        // Get segment
        Object* seg = objects + *a2++;
        Scratch_GHZBridge* segscratch = (Scratch_GHZBridge*)&obj->scratch;

        // Move segment
        uint32_t fac = (uint16_t)(*--a3 + 1) * d5 * d4;
        seg->pos.l.y.f.u = segscratch->base_y + (fac >> 16);
    } while (d2-- > 0);
}

static void Obj_GHZBridge_Solid(Object* obj) {
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    // Get bridge size
    uint16_t x_rad = (scratch->subtype << 3) + 8;
    uint16_t x_dia = scratch->subtype << 4;

    // Check if player is colliding with bridge
    if (player->ysp < 0)
        return;

    int16_t off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
    if (off < 0 || off >= x_dia)
        return;

    // Collide with bridge
    Platform3(obj, obj->pos.l.y.f.u - 8);
}

static void Obj_GHZBridge_MoveSonic(Object* obj) {
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    // Clip Sonic to the top of the bridge segment being stood on
    Object* seg = objects + scratch->seg[scratch->push_seg];
    player->pos.l.y.f.u = seg->pos.l.y.f.u - 8 - player->y_rad;
}

static void Obj_GHZBridge_WalkOff(Object* obj) {
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    // Check if we've walked off the platform
    int16_t x_off;
    uint16_t x_rad = (scratch->subtype << 3) + 8;

    if (!ExitPlatform(obj, x_rad, scratch->subtype << 3, &x_off)) {
        scratch->push_seg = x_off >> 4;
        if (scratch->push != 0x40)
            scratch->push += 4;
        Obj_GHZBridge_Bend(obj);
        Obj_GHZBridge_MoveSonic(obj);
    }
}

static void Obj_GHZBridge_ChkDel(Object* obj) {
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        // Off-screen
        uint8_t* segp = &scratch->subtype; // seg immediately follows
        uint8_t segs = *segp++ - 1;

        do {
            Object* seg = objects + *segp++;
            if (seg != obj)
                ObjectDelete(seg);
        } while (segs-- > 0);
        ObjectDelete(obj);
    }
}

void Obj_GHZBridge(Object* obj) {
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine
        obj->routine += 2;

        // Set object drawing information
        obj->mappings = Mappings_GHZBridge;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x38E);
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 3;
        obj->width_pixels = 128;

        // Create bridge segments
        int16_t y = obj->pos.l.y.f.u;
        int16_t x = obj->pos.l.x.f.u;
        uint8_t type = obj->type;
        uint8_t* segp = &scratch->subtype; // seg immediately follows
        uint8_t segs = *segp;

        // Handle segment count
        *segp++ = 0;
        x -= ((segs >> 1) << 4);

        if (!((segs -= 2) >= 0xFE)) {
            do {
                // Get segment object
                Object* seg = FindFreeObj();
                if (seg == NULL)
                    break;

                // Update controller state
                scratch->subtype++;
                if (x == obj->pos.l.x.f.u) {
                    x += 16;
                    obj->pos.l.y.f.u = y;
                    scratch->base_y = y;
                    *segp++ = obj - objects;
                    scratch->subtype++;
                }
                *segp++ = seg - objects;

                // Set segment object
                Scratch_GHZBridge* segscratch = (Scratch_GHZBridge*)&seg->scratch;
                seg->routine = 10;
                seg->type = type;
                seg->pos.l.y.f.u = y;
                segscratch->base_y = y;
                seg->pos.l.x.f.u = x;
                seg->mappings = Mappings_GHZBridge;
                seg->tile = TILE_MAP(0, 2, 0, 0, 0x38E);
                seg->render.b = 0;
                seg->render.f.align_fg = true;
                seg->priority = 3;
                seg->width_pixels = 8;

                x += 16;
            } while (segs-- > 0);
        }
        // Fallthrough
    case 2: // Controller not stood on
        Obj_GHZBridge_Solid(obj);
        if (scratch->push)
            scratch->push -= 4;
        Obj_GHZBridge_Bend(obj);
        DisplaySprite(obj);
        Obj_GHZBridge_ChkDel(obj);
        break;
    case 4: // Controller stood on
        Obj_GHZBridge_WalkOff(obj);
        DisplaySprite(obj);
        Obj_GHZBridge_ChkDel(obj);
        break;
    case 6: // Delete
    case 8:
        ObjectDelete(obj);
        break;
    case 10: // Segment draw
        DisplaySprite(obj);
        break;
    }
}
