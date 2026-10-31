// Hidden Palace's own objects for Sonic 2 (Nick Arcade's 12, 13 and 1A): the emerald that is a block to stand on (12), the waterfall that follows the water's height (13), and the collapsing ledge that
// Green Hill's Sonic 1 one became (1A: Green Hill's and Hidden Palace's). All carry Sonic and Tails.
#include "Object/HPZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 12: the emerald (a solid block)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_HPZEmerald(Sst *self) {
    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_HPZEmerald;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x392);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->priority = 4 * 0x80;
    }
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x20, 0x10, 0x10, self->x_pos.f.u, NULL);
    }
    SstOnScreenTest2(self, self->x_pos.f.u);
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
} Own_HPZWaterfall;

enum { WaterfallRoutine_Init = 0, WaterfallRoutine_Main = 2, WaterfallRoutine_Stream = 4 };

static Sst *WaterfallStream(Sst *self) {
    Sst *stream = Sst_AllocateAfter(self);
    if (stream == NULL)
        return NULL;
    stream->code = self->code;
    stream->routine = WaterfallRoutine_Stream;
    stream->x_pos = self->x_pos;
    stream->y_pos = self->y_pos;
    stream->mappings = Mappings_HPZWaterfall;
    stream->art_tile = TILE_MAP(1, 3, 0, 0, 0x315);
    stream->render_flags = SST_RENDER_LEVEL;
    stream->width_pixels = 0x10;
    stream->priority = 1 * 0x80;
    return stream;
}

// A stream the parent still owns: in its slot, of its id, and not the parent itself
static Sst *Stream(const Sst *self, uint8_t slot) {
    Sst *stream = &sst[slot];
    return (slot != 0 && stream != self && stream->code == self->code && stream->routine == WaterfallRoutine_Stream) ? stream : NULL;
}

void Obj_HPZWaterfall(Sst *self) {
    Own_HPZWaterfall *own = (Own_HPZWaterfall *)self->own.u8;

    switch (self->routine) {
    case WaterfallRoutine_Init: {
        self->routine += 2;
        self->mappings = Mappings_HPZWaterfall;
        self->art_tile = TILE_MAP(1, 3, 0, 0, 0x315);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 1 * 0x80;
        self->mapping_frame = 0x12;

        own->child1 = own->child2 = 0;
        Sst *first = WaterfallStream(self);
        if (first == NULL) {
            Sst_Delete(self);
            return;
        }
        first->y_radius = (int8_t)0xA0;
        first->height_pixels = first->y_radius;
        own->child1 = (uint8_t)(first - sst);
        own->top_y = own->second_y = self->y_pos.f.u;
        if (self->subtype >= 0x10) {
            Sst *second = WaterfallStream(self);
            if (second != NULL) {
                own->child2 = (uint8_t)(second - sst);
                second->y_pos.f.u = self->y_pos.f.u + 0x98;
            }
        }
        // its length: it starts as many blocks of 16 lower as the subtype says
        own->top_y = (int16_t)(own->top_y - 0x78 + (self->subtype << 4));
        self->y_pos.f.u = own->top_y;
    }
        // Fallthrough
    case WaterfallRoutine_Main: {
        Sst *first = Stream(self, own->child1);
        Sst *second = own->child2 ? Stream(self, own->child2) : NULL;
        self->mapping_frame = 0x12;
        int16_t y = own->top_y;
        if ((uint16_t)wtr_pos1 < (uint16_t)y)
            y = wtr_pos1; // (the water is above its top)
        self->y_pos.f.u = y;
        int16_t d0 = y - own->second_y + 0x80;
        if (d0 < 0) { // no water in it at all
            self->mapping_frame = 0x13;
            if (first)
                first->mapping_frame = 0x13;
            SstDeleteIfNotInRange2(self, self->x_pos.f.u);
            return;
        }
        d0 >>= 4;
        int16_t d1 = d0;
        if (d0 >= 0xF)
            d0 = 0xF;
        if (first)
            first->mapping_frame = (uint8_t)d0;
        if (self->subtype >= 0x10 && second) {
            d1 -= 0xF;
            if (d1 < 0)
                d1 = 0;
            second->mapping_frame = (uint8_t)(d1 + 0x13);
        }
        SstOnScreenTest2(self, self->x_pos.f.u);
        break;
    }
    case WaterfallRoutine_Stream:
        SstOnScreenTest2(self, self->x_pos.f.u);
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
} Own_Ledge;

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

static void Ledge_Platform(Sst *self) {
    const uint8_t *slope = Ledge_IsHPZ() ? ledge_slope_hpz : ledge_slope_ghz;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr != NULL)
            Solid_SlopedPlatform(self, chr, who, self->width_pixels, slope, self->x_pos.f.u);
    }
}

void Obj_CollapsingLedge(Sst *self) {
    Own_Ledge *own = (Own_Ledge *)self->own.u8;

    switch (self->routine) {
    case LedgeRoutine_Init:
        self->routine += 2;
        self->mappings = Mappings_LedgeGHZ;
        self->art_tile = TILE_MAP(0, 2, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->timer = 7;
        self->mapping_frame = self->subtype;
        if (Ledge_IsHPZ()) {
            self->mappings = Mappings_LedgeHPZ;
            self->art_tile = TILE_MAP(0, 2, 0, 0, 0x34A);
            self->width_pixels = 0x30;
        } else {
            self->width_pixels = 0x34;
            self->y_radius = 0x38;
            self->height_pixels = self->y_radius;
        }
        // Fallthrough
    case LedgeRoutine_Main:
        if (own->touched) {
            if (own->timer == 0) { // it collapses
                self->mapping_frame += 2;
                SstFragmentatePlatform(self, Ledge_IsHPZ() ? 12 : 25, Ledge_IsHPZ() ? ledge_delays_hpz : ledge_delays_ghz);
                return;
            }
            own->timer--;
        }
        if (self->status & 0x18)
            own->touched = 1;
        Ledge_Platform(self);
        SstOnScreenTest(self);
        break;
    case LedgeRoutine_Piece:
        if (self->render_flags & SST_RENDER_MULTI_DRAW)
            self->y_radius = 0x38; // (a piece is not given its ledge's height by the fragmenting)
        if (own->timer == 0) { // falling
            Sst_MoveSprite(self);
            if (!(self->render_flags & SST_RENDER_ON_SCREEN))
                Sst_Delete(self);
            else
                Sst_Draw(self);
            break;
        }
        if (!own->touched) { // a piece that waits to fall
            own->timer--;
            Sst_Draw(self);
            break;
        }
        // the first piece (the ledge itself) is still a platform until it lets go
        Ledge_Platform(self);
        SstOnScreenTest(self);
        if (--own->timer == 0) {
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Sst *chr = Character(who);
                if (chr != NULL && ((chr->status & SST_PSTATUS_OBJECT_STAND) != 0)) {
                    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    chr->prev_anim = 1;
                }
            }
        }
        break;
    }
}
