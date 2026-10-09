// Hidden Palace's own objects for Sonic 2 (Nick Arcade's 12, 13 and 1A): the emerald that is a block to stand on (12), the waterfall that follows the water's height (13), and the collapsing ledge that
// Green Hill's Sonic 1 one became (1A: Green Hill's and Hidden Palace's). All carry Sonic and Tails.
#include "Object/HPZObjects.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/HPZEmerald.h"
#include "Resource/Mappings/HPZWaterfall.h"
#include "Resource/Mappings/LedgeGHZ.h"
#include "Resource/Mappings/LedgeHPZ.h"

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 12: the emerald (a solid block)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_HPZEmerald(Object *obj) {
    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_HPZEmerald;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x392);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->width_pixels = 0x20;
        obj->priority = 4;
    }
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr != NULL)
            Solid_Character(obj, chr, who, 0x20, 0x10, 0x10, obj->pos.l.x.f.u, NULL);
    }
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 13: the waterfall, one or two streams (child objects) that grow and shrink with the water's height
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: its length (a subtype of $10 or more has a second stream)
    uint8_t pad0[0xB]; // 0x29-0x33
    int16_t top_y;     // 0x34: where the first stream begins
    int16_t second_y;  // 0x36: where the second would
    uint8_t child1;    // 0x38: the slot of the first stream (an index, checked before it is used)
    uint8_t pad1[3];   // 0x39-0x3B
    uint8_t child2;    // 0x3C: ... of the second
} Scratch_HPZWaterfall;

enum { WaterfallRoutine_Init = 0, WaterfallRoutine_Main = 2, WaterfallRoutine_Stream = 4 };

static Object *WaterfallStream(Object *obj) {
    Object *stream = FindNextFreeObj(obj + 1);
    if (stream == NULL)
        return NULL;
    stream->type = obj->type;
    stream->routine = WaterfallRoutine_Stream;
    stream->pos.l.x = obj->pos.l.x;
    stream->pos.l.y = obj->pos.l.y;
    stream->mappings = Mappings_HPZWaterfall;
    stream->tile = TILE_MAP(1, 3, 0, 0, 0x315);
    stream->render.b = 0;
    stream->render.f.level_fg = true;
    stream->width_pixels = 0x10;
    stream->priority = 1;
    return stream;
}

// A stream the parent still owns: in its slot, of its id, and not the parent itself
static Object *Stream(const Object *obj, uint8_t slot) {
    Object *stream = &objects[slot];
    return (slot != 0 && stream != obj && stream->type == obj->type && stream->routine == WaterfallRoutine_Stream) ? stream : NULL;
}

void Obj_HPZWaterfall(Object *obj) {
    Scratch_HPZWaterfall *scratch = (Scratch_HPZWaterfall *)&obj->scratch;

    switch (obj->routine) {
    case WaterfallRoutine_Init: {
        obj->routine += 2;
        obj->mappings = Mappings_HPZWaterfall;
        obj->tile = TILE_MAP(1, 3, 0, 0, 0x315);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 1;
        obj->frame = 0x12;

        scratch->child1 = scratch->child2 = 0;
        Object *first = WaterfallStream(obj);
        if (first == NULL) {
            ObjectDelete(obj);
            return;
        }
        first->y_rad = (int8_t)0xA0;
        first->render.f.explicit_height = true;
        scratch->child1 = (uint8_t)(first - objects);
        scratch->top_y = scratch->second_y = obj->pos.l.y.f.u;
        if (scratch->subtype >= 0x10) {
            Object *second = WaterfallStream(obj);
            if (second != NULL) {
                scratch->child2 = (uint8_t)(second - objects);
                second->pos.l.y.f.u = obj->pos.l.y.f.u + 0x98;
            }
        }
        // its length: it starts as many blocks of 16 lower as the subtype says
        scratch->top_y = (int16_t)(scratch->top_y - 0x78 + (scratch->subtype << 4));
        obj->pos.l.y.f.u = scratch->top_y;
    }
        // Fallthrough
    case WaterfallRoutine_Main: {
        Object *first = Stream(obj, scratch->child1);
        Object *second = scratch->child2 ? Stream(obj, scratch->child2) : NULL;
        obj->frame = 0x12;
        int16_t y = scratch->top_y;
        if ((uint16_t)wtr_pos1 < (uint16_t)y)
            y = wtr_pos1; // (the water is above its top)
        obj->pos.l.y.f.u = y;
        int16_t d0 = y - scratch->second_y + 0x80;
        if (d0 < 0) { // no water in it at all
            obj->frame = 0x13;
            if (first)
                first->frame = 0x13;
            if (IS_OFFSCREEN(obj->pos.l.x.f.u))
                ObjectDelete(obj);
            return;
        }
        d0 >>= 4;
        int16_t d1 = d0;
        if (d0 >= 0xF)
            d0 = 0xF;
        if (first)
            first->frame = (uint8_t)d0;
        if (scratch->subtype >= 0x10 && second) {
            d1 -= 0xF;
            if (d1 < 0)
                d1 = 0;
            second->frame = (uint8_t)(d1 + 0x13);
        }
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
    case WaterfallRoutine_Stream:
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 1A: the collapsing ledge
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the frame
    uint8_t pad0[0xF]; // 0x29-0x37
    uint8_t timer;     // 0x38: from being touched to collapsing (and, in a piece, to falling)
    uint8_t pad1;      // 0x39
    uint8_t touched;   // 0x3A
} Scratch_Ledge;

enum { LedgeRoutine_Init = 0, LedgeRoutine_Main = 2, LedgeRoutine_Piece = 6 };

// Green Hill's slope: a height for each 2 pixel column; Hidden Palace's is flat
static const uint8_t ledge_slope_ghz[] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x21, 0x21, 0x22, 0x22, 0x23, 0x23, 0x24, 0x24, 0x25, 0x25, 0x26, 0x26, 0x27, 0x27, 0x28, 0x28,
    0x29, 0x29, 0x2A, 0x2A, 0x2B, 0x2B, 0x2C, 0x2C, 0x2D, 0x2D, 0x2E, 0x2E, 0x2F, 0x2F, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};
static const uint8_t ledge_slope_hpz[] = {
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
    0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
};
// When each piece begins to fall (byte_8EF2: Green Hill's 25 pieces, byte_8F0B: Hidden Palace's 12)
static const uint8_t ledge_delays_ghz[25] = { 0x1C, 0x18, 0x14, 0x10, 0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x06, 0x18, 0x14, 0x10, 0x0C, 0x08, 0x04, 0x16, 0x12, 0x0E, 0x0A, 0x06, 0x02, 0x14, 0x10, 0x0C };
static const uint8_t ledge_delays_hpz[12] = { 0x18, 0x1C, 0x20, 0x1E, 0x1A, 0x16, 0x06, 0x0E, 0x14, 0x12, 0x0A, 0x02 };

static bool Ledge_IsHPZ(void) {
    return LEVEL_ZONE(level_id) == ZoneId_HPZ;
}

static void Ledge_Platform(Object *obj) {
    const uint8_t *slope = Ledge_IsHPZ() ? ledge_slope_hpz : ledge_slope_ghz;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr != NULL)
            Solid_SlopedPlatform(obj, chr, who, obj->width_pixels, slope, obj->pos.l.x.f.u);
    }
}

void Obj_CollapsingLedge(Object *obj) {
    Scratch_Ledge *scratch = (Scratch_Ledge *)&obj->scratch;

    switch (obj->routine) {
    case LedgeRoutine_Init:
        obj->routine += 2;
        obj->mappings = Mappings_LedgeGHZ;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        scratch->timer = 7;
        obj->frame = scratch->subtype;
        if (Ledge_IsHPZ()) {
            obj->mappings = Mappings_LedgeHPZ;
            obj->tile = TILE_MAP(0, 2, 0, 0, 0x34A);
            obj->width_pixels = 0x30;
        } else {
            obj->width_pixels = 0x34;
            obj->y_rad = 0x38;
            obj->render.f.explicit_height = true;
        }
        // Fallthrough
    case LedgeRoutine_Main:
        if (scratch->touched) {
            if (scratch->timer == 0) { // it collapses
                obj->frame += 2;
                FragmentatePlatform(obj, Ledge_IsHPZ() ? 12 : 25, Ledge_IsHPZ() ? ledge_delays_hpz : ledge_delays_ghz);
                return;
            }
            scratch->timer--;
        }
        if (obj->status.b & 0x18)
            scratch->touched = 1;
        Ledge_Platform(obj);
        RememberState(obj);
        break;
    case LedgeRoutine_Piece:
        if (obj->render.f.explicit_height)
            obj->y_rad = 0x38; // (a piece is not given its ledge's height by the fragmenting)
        if (scratch->timer == 0) { // falling
            ObjectFall(obj);
            if (!obj->render.f.on_screen)
                ObjectDelete(obj);
            else
                DisplaySprite(obj);
            break;
        }
        if (!scratch->touched) { // a piece that waits to fall
            scratch->timer--;
            DisplaySprite(obj);
            break;
        }
        // the first piece (the ledge itself) is still a platform until it lets go
        Ledge_Platform(obj);
        RememberState(obj);
        if (--scratch->timer == 0) {
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Object *chr = Character(who);
                if (chr != NULL && chr->status.p.f.object_stand) {
                    chr->status.p.f.object_stand = false;
                    chr->status.p.f.pushing = false;
                    chr->prev_anim = 1;
                }
            }
        }
        break;
    }
}
