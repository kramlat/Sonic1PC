// The log bridge for Sonic 2 (Nick Arcade's object 11, "hashi"): in Green Hill, Emerald Hill and Hidden Palace. Unlike Sonic 1's, whose every log is an object, it draws its logs as the child sprites of up to two
// extra objects of its own (8 logs each), and it carries Sonic and Tails both: each gives it a log index (which log he stands on) and it sinks under them by a sine of how long they have stood there.
// Subtype: the number of logs (8 to 16). In Hidden Palace the logs wriggle under whoever stands on them.
#include "Object/Bridge.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/BridgeEHZ.h"
#include "Resource/Mappings/BridgeGHZ.h"
#include "Resource/Mappings/BridgeHPZ.h"

#define BRIDGE_MAX_LOGS 16

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    uint8_t child1;    // 0x30: the object slot of the first set of logs (an index, not a pointer: it cannot dangle)
    uint8_t pad1[3];   // 0x31-0x33
    uint8_t child2;    // 0x34: ... the second set
    uint8_t pad2[6];   // 0x35-0x3A
    uint8_t tails_log; // 0x3B: the log Tails stands on
    uint16_t base_y;   // 0x3C: where the logs rest
    uint8_t bend;      // 0x3E: how far down they are pushed (a sine angle)
    uint8_t sonic_log; // 0x3F: the log Sonic stands on
} Own_Bridge;

enum { BridgeRoutine_Init = 0, BridgeRoutine_Main = 2, BridgeRoutine_Display = 4, BridgeRoutine_HPZ = 6 };

// Sink of each log for a bridge of 8 to 16 logs, with Sonic or Tails on the log named by the column (Bridge_BendData) ...
static const uint8_t bend_depth[9][16] = {
    { 2, 4, 6, 8, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 4, 6, 8, 0xA, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0, 0 },
    { 2, 4, 6, 8, 0xA, 0xA, 8, 6, 4, 2, 0, 0, 0, 0, 0, 0 },
    { 2, 4, 6, 8, 0xA, 0xC, 0xA, 8, 6, 4, 2, 0, 0, 0, 0, 0 },
    { 2, 4, 6, 8, 0xA, 0xC, 0xC, 0xA, 8, 6, 4, 2, 0, 0, 0, 0 },
    { 2, 4, 6, 8, 0xA, 0xC, 0xE, 0xC, 0xA, 8, 6, 4, 2, 0, 0, 0 },
    { 2, 4, 6, 8, 0xA, 0xC, 0xE, 0xE, 0xC, 0xA, 8, 6, 4, 2, 0, 0 },
    { 2, 4, 6, 8, 0xA, 0xC, 0xE, 0x10, 0xE, 0xC, 0xA, 8, 6, 4, 2, 0 },
    { 2, 4, 6, 8, 0xA, 0xC, 0xE, 0x10, 0x10, 0xE, 0xC, 0xA, 8, 6, 4, 2 },
};

// ... and how the sink is spread over the logs on each side of the one stood on (Bridge_BendData2), by the number of logs on that side
static const uint8_t bend_shape[16][16] = {
    { 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0xB5, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x7E, 0xDB, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x61, 0xB5, 0xEC, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x4A, 0x93, 0xCD, 0xF3, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x3E, 0x7E, 0xB0, 0xDB, 0xF6, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x38, 0x6D, 0x9D, 0xC5, 0xE4, 0xF8, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x31, 0x61, 0x8E, 0xB5, 0xD4, 0xEC, 0xFB, 0xFF, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x2B, 0x56, 0x7E, 0xA2, 0xC1, 0xDB, 0xEE, 0xFB, 0xFF, 0, 0, 0, 0, 0, 0, 0 },
    { 0x25, 0x4A, 0x73, 0x93, 0xB0, 0xCD, 0xE1, 0xF3, 0xFC, 0xFF, 0, 0, 0, 0, 0, 0 },
    { 0x1F, 0x44, 0x67, 0x88, 0xA7, 0xBD, 0xD4, 0xE7, 0xF4, 0xFD, 0xFF, 0, 0, 0, 0, 0 },
    { 0x1F, 0x3E, 0x5C, 0x7E, 0x98, 0xB0, 0xC9, 0xDB, 0xEA, 0xF6, 0xFD, 0xFF, 0, 0, 0, 0 },
    { 0x19, 0x38, 0x56, 0x73, 0x8E, 0xA7, 0xBD, 0xD1, 0xE1, 0xEE, 0xF8, 0xFE, 0xFF, 0, 0, 0 },
    { 0x19, 0x38, 0x50, 0x6D, 0x83, 0x9D, 0xB0, 0xC5, 0xD8, 0xE4, 0xF1, 0xF8, 0xFE, 0xFF, 0, 0 },
    { 0x19, 0x31, 0x4A, 0x67, 0x7E, 0x93, 0xA7, 0xBD, 0xCD, 0xDB, 0xE7, 0xF3, 0xF9, 0xFE, 0xFF, 0 },
    { 0x19, 0x31, 0x4A, 0x61, 0x78, 0x8E, 0xA2, 0xB5, 0xC5, 0xD4, 0xE1, 0xEC, 0xF4, 0xFB, 0xFE, 0xFF },
};

// How the logs next to the one stood on wriggle (frames, per beat), in Hidden Palace
static const uint8_t wriggle[16] = { 1, 2, 1, 2, 1, 2, 1, 2, 0, 1, 0, 0, 0, 0, 0, 1 };

// The log `i` of the bridge: a child sprite of one of its two child objects (or NULL if there is none)
static SstChild *Bridge_Log(const Own_Bridge *own, int i) {
    static SstChild none; // (never reached: the loops stop at the bridge's own logs, but a missing child must not be some other object's)
    uint8_t slot = i < 8 ? own->child1 : own->child2;
    if (i < 0 || i >= BRIDGE_MAX_LOGS || slot == 0)
        return &none;
    return &sst[slot].children[i & 7];
}

static void Bridge_DeleteChildren(Sst *self) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;
    if (own->child1) {
        Sst_Delete(&sst[own->child1]);
        own->child1 = 0;
    }
    if (own->child2) {
        Sst_Delete(&sst[own->child2]);
        own->child2 = 0;
    }
}

// One set of up to 8 logs (Bridge_MakeSegments): an object of its own, drawn as its children. `x` is where the first log is, and the next ones follow 16 pixels apart
static Sst *Bridge_MakeSegments(Sst *self, int count, int16_t y, int16_t x) {
    Sst *child = Sst_AllocateAfter(self);
    if (child == NULL)
        return NULL;
    child->code = self->code;
    child->x_pos.f.u = self->x_pos.f.u;
    child->y_pos.f.u = self->y_pos.f.u;
    child->mappings = self->mappings;
    child->art_tile = self->art_tile;
    child->render_flags = self->render_flags;
    child->render_flags |= SST_RENDER_MULTI_DRAW;
    child->priority = 3 * 0x80;
    child->width_pixels = 0x40;
    child->child_count = (uint8_t)count;
    for (int i = 0; i < count; i++) {
        child->children[i].x = x;
        child->children[i].y = y;
        child->children[i].frame = 0;
        x += 0x10;
    }
    return child;
}

// The bridge sinks where Sonic or Tails stand (Bridge_Depress): each log by a share (the bend shape) of the depth (by the number of logs and where the weight is) scaled by the sine of how far down it has been pushed
static void Bridge_Depress(Sst *self) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;

    int16_t sin, cos;
    CalcSine(own->bend, &sin, &cos);
    uint32_t d4 = (uint16_t)sin;

    int logs = self->subtype;
    int stood = own->sonic_log;
    uint32_t depth = bend_depth[logs - 8][stood & 15]; // (8 to 16 logs: the subtype is kept in that range)

    // The logs from the first up to the one stood on (the shape for that many logs, from its start) ...
    const uint8_t *shape = bend_shape[stood & 15];
    int i = 0;
    for (int n = stood; n >= 0; n--, i++) {
        uint32_t d0 = (uint16_t)(*shape++ + 1u);
        d0 *= depth;
        d0 *= d4;
        Bridge_Log(own, i)->y = (int16_t)((d0 >> 16) + own->base_y);
    }

    // ... and those after it (the shape for that many, from its end)
    int after = logs - (stood + 1);
    if (after < 0)
        return;
    shape = &bend_shape[after & 15][after & 15];
    for (int n = after; n > 0; n--, i++) {
        uint32_t d0 = (uint16_t)(*--shape + 1u);
        d0 *= depth;
        d0 *= d4;
        Bridge_Log(own, i)->y = (int16_t)((d0 >> 16) + own->base_y);
    }
}

// Sonic and Tails against the bridge (sub_7DC0 / sub_7DDA): standing on it, they ride the log they stand on; otherwise they may land on it
static void Bridge_Characters(Sst *self, int16_t x_rad, int16_t width) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;

    for (int who = SolidChar_Tails; who >= SolidChar_Sonic; who--) { // (Tails first)
        Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr == NULL)
            continue;
        uint8_t *log_of = (who == SolidChar_Sonic) ? &own->sonic_log : &own->tails_log;
        const uint8_t stand_bit = (uint8_t)(1 << (3 + who));

        if (self->status & stand_bit) {
            int16_t d0 = chr->x_pos.f.u - self->x_pos.f.u + x_rad;
            if (((chr->status & SST_PSTATUS_IN_AIR) != 0) || d0 < 0 || d0 >= width) {
                chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                self->status &= (uint8_t)~stand_bit;
                continue;
            }
            int log = d0 >> 4;
            *log_of = (uint8_t)log;
            chr->y_pos.f.u = Bridge_Log(own, log)->y - 8 - chr->y_radius;
        } else if (Solid_PlatformLand(self, chr, who, x_rad, width, 8) || (self->status & stand_bit)) {
            int16_t d0 = chr->x_pos.f.u - self->x_pos.f.u + x_rad;
            *log_of = (uint8_t)(d0 >> 4);
        }
    }
}

// The logs next to the ones stood on wriggle (sub_7E60): a child sprite frame for each, by how near it is to Sonic's log and to Tails'
static void Bridge_Wriggle(Sst *self) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;

    int beat = (player->x_vel != 0) ? 0 : (frame_count & 0x1C) >> 1;
    uint16_t sonic_low = wriggle[beat], sonic_high = wriggle[beat + 1];
    beat = (TAILS_OBJ->x_vel != 0) ? 0 : (frame_count & 0x1C) >> 1;
    uint16_t tails_low = wriggle[beat], tails_high = wriggle[beat + 1];

    int16_t d3 = -2, d4 = -2;
    if (self->status & 8)
        d3 = own->sonic_log;
    if (self->status & 0x10)
        d4 = own->tails_log;

    for (int log = 0; log < self->subtype; log++) {
        uint8_t d0 = 0;
        if ((uint8_t)(d3 - 1) == log || (uint8_t)(d3 + 1) == log)
            d0 = (uint8_t)sonic_low;
        if ((uint8_t)(d4 - 1) == log || (uint8_t)(d4 + 1) == log)
            d0 = (uint8_t)tails_low;
        if ((uint8_t)d3 == log)
            d0 = (uint8_t)sonic_high;
        if ((uint8_t)d4 == log)
            d0 = (uint8_t)tails_high;
        Bridge_Log(own, log)->frame = d0;
    }
}

// Run once a frame: the weight on it, the bend, the characters, and gone when it is far off screen
static void Bridge_Run(Sst *self, bool hpz) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;

    uint8_t weight = self->status & 0x18;
    bool depress = true;
    if (weight == 0) {
        if (own->bend == 0)
            depress = false;
        else
            own->bend -= 4;
    } else {
        // Tails on it: the log Sonic is taken to be on moves toward Tails'
        if (weight & 0x10) {
            uint8_t diff = (uint8_t)(own->sonic_log - own->tails_log);
            if (diff != 0) {
                if (own->sonic_log < own->tails_log)
                    own->sonic_log++;
                else
                    own->sonic_log--;
            }
        }
        if (own->bend != 0x40)
            own->bend += 4;
    }
    if (depress)
        Bridge_Depress(self);

    int16_t half = (self->subtype << 3) + 8;
    int16_t width = self->subtype << 4;
    Bridge_Characters(self, half, width);
    if (hpz)
        Bridge_Wriggle(self);

    if (SstOutOfRange(self->x_pos.f.u)) {
        Bridge_DeleteChildren(self);
        SstLoad_Forget(self);
        Sst_Delete(self);
    }
}

void Obj_Bridge(Sst *self) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;

    if ((self->render_flags & SST_RENDER_MULTI_DRAW)) { // a set of logs only has to be drawn
        Sst_Draw(self);
        return;
    }

    switch (self->routine) {
    case BridgeRoutine_Init: {
        self->routine += 2;
        self->mappings = Mappings_BridgeGHZ;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Bridge);
        self->priority = 3 * 0x80;
        if (LEVEL_ZONE(level_id) == ZoneId_EHZ) {
            self->mappings = Mappings_BridgeEHZ;
            self->art_tile = TILE_MAP(0, 2, 0, 0, 0x3B6);
        }
        if (LEVEL_ZONE(level_id) == ZoneId_HPZ) {
            self->routine += 4;
            self->mappings = Mappings_BridgeHPZ;
            self->art_tile = TILE_MAP(0, 3, 0, 0, 0x300);
        }
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x80;

        int logs = self->subtype;
        if (logs < 8)
            logs = 8; // (Sonic 2 has no bridges of fewer logs)
        if (logs > BRIDGE_MAX_LOGS)
            logs = BRIDGE_MAX_LOGS;
        self->subtype = (uint8_t)logs;
        own->base_y = (uint16_t)self->y_pos.f.u;
        own->bend = 0;
        own->sonic_log = 0;
        own->tails_log = 0;
        own->child1 = own->child2 = 0;

        int16_t y = self->y_pos.f.u;
        int16_t x = self->x_pos.f.u - ((logs / 2) << 4);
        Sst *first = Bridge_MakeSegments(self, 8, y, x);
        if (first == NULL) {
            Sst_Delete(self);
            break;
        }
        first->x_pos.f.u = first->children[3].x - 8; // (the centre of its logs, for the culling)
        own->child1 = (uint8_t)(first - sst);

        int rest = logs - 8;
        if (rest > 0) {
            Sst *second = Bridge_MakeSegments(self, rest, y, x + 8 * 0x10);
            if (second == NULL) {
                Bridge_DeleteChildren(self);
                Sst_Delete(self);
                break;
            }
            second->x_pos.f.u = second->children[rest / 2].x - 8;
            own->child2 = (uint8_t)(second - sst);
        }
    }
        // Fallthrough
    case BridgeRoutine_Main:
        Bridge_Run(self, false);
        break;
    case BridgeRoutine_Display:
        Sst_Draw(self);
        break;
    case BridgeRoutine_HPZ:
        Bridge_Run(self, true);
        break;
    }
}
