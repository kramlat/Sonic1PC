#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"

#include "MathUtil.h"

#include "Macros.h"

// GHZ bridge assets
#include "Resource/Mappings/GHZBridge.h"

static const uint8_t ghz_bridge_bend1[0x110] = {
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0C,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0C,
    0x0C,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0C,
    0x0E,
    0x0C,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0C,
    0x0E,
    0x0E,
    0x0C,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0C,
    0x0E,
    0x10,
    0x0E,
    0x0C,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
    0x00,
    0x02,
    0x04,
    0x06,
    0x08,
    0x0A,
    0x0C,
    0x0E,
    0x10,
    0x10,
    0x0E,
    0x0C,
    0x0A,
    0x08,
    0x06,
    0x04,
    0x02,
};

static const uint8_t ghz_bridge_bend2[256] = {
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0xB5,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x7E,
    0xDB,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x61,
    0xB5,
    0xEC,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x4A,
    0x93,
    0xCD,
    0xF3,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x3E,
    0x7E,
    0xB0,
    0xDB,
    0xF6,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x38,
    0x6D,
    0x9D,
    0xC5,
    0xE4,
    0xF8,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x31,
    0x61,
    0x8E,
    0xB5,
    0xD4,
    0xEC,
    0xFB,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x2B,
    0x56,
    0x7E,
    0xA2,
    0xC1,
    0xDB,
    0xEE,
    0xFB,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x25,
    0x4A,
    0x73,
    0x93,
    0xB0,
    0xCD,
    0xE1,
    0xF3,
    0xFC,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1F,
    0x44,
    0x67,
    0x88,
    0xA7,
    0xBD,
    0xD4,
    0xE7,
    0xF4,
    0xFD,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x1F,
    0x3E,
    0x5C,
    0x7E,
    0x98,
    0xB0,
    0xC9,
    0xDB,
    0xEA,
    0xF6,
    0xFD,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x00,
    0x19,
    0x38,
    0x56,
    0x73,
    0x8E,
    0xA7,
    0xBD,
    0xD1,
    0xE1,
    0xEE,
    0xF8,
    0xFE,
    0xFF,
    0x00,
    0x00,
    0x00,
    0x19,
    0x38,
    0x50,
    0x6D,
    0x83,
    0x9D,
    0xB0,
    0xC5,
    0xD8,
    0xE4,
    0xF1,
    0xF8,
    0xFE,
    0xFF,
    0x00,
    0x00,
    0x19,
    0x31,
    0x4A,
    0x67,
    0x7E,
    0x93,
    0xA7,
    0xBD,
    0xCD,
    0xDB,
    0xE7,
    0xF3,
    0xF9,
    0xFE,
    0xFF,
    0x00,
    0x19,
    0x31,
    0x4A,
    0x61,
    0x78,
    0x8E,
    0xA2,
    0xB5,
    0xC5,
    0xD4,
    0xE1,
    0xEC,
    0xF4,
    0xFB,
    0xFE,
    0xFF,
};

// GHZ bridge object
typedef struct
{
    uint8_t subtype; // 0x28
    uint8_t seg[0x13]; // 0x29
    int16_t base_y; // 0x3C
    uint8_t push; // 0x3E
    uint8_t push_seg; // 0x3F
} Scratch_GHZBridge;

static void Obj_GHZBridge_Bend(Object* obj)
{
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

static void Obj_GHZBridge_Solid(Object* obj)
{
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

static void Obj_GHZBridge_MoveSonic(Object* obj)
{
    Scratch_GHZBridge* scratch = (Scratch_GHZBridge*)&obj->scratch;

    // Clip Sonic to the top of the bridge segment being stood on
    Object* seg = objects + scratch->seg[scratch->push_seg];
    player->pos.l.y.f.u = seg->pos.l.y.f.u - 8 - player->y_rad;
}

static void Obj_GHZBridge_WalkOff(Object* obj)
{
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

static void Obj_GHZBridge_ChkDel(Object* obj)
{
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

void Obj_GHZBridge(Object* obj)
{
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
