// The log bridge for Sonic 2 (Nick Arcade's object 11, "hashi"): in Green Hill, Emerald Hill and Hidden Palace. Unlike Sonic 1's, whose every log is an object, it draws its logs as the child sprites of up to two
// extra objects of its own (8 logs each), and it carries Sonic and Tails both: each gives it a log index (which log he stands on) and it sinks under them by a sine of how long they have stood there.
// Subtype: the number of logs (8 to 16). In Hidden Palace the logs wriggle under whoever stands on them.
#include "Object/Bridge.h"
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
} Scratch_Bridge;

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
static ObjectChild *Bridge_Log(const Scratch_Bridge *scratch, int i) {
    static ObjectChild none; // (never reached: the loops stop at the bridge's own logs, but a missing child must not be some other object's)
    uint8_t slot = i < 8 ? scratch->child1 : scratch->child2;
    if (i < 0 || i >= BRIDGE_MAX_LOGS || slot == 0)
        return &none;
    return &objects[slot].children[i & 7];
}

static void Bridge_DeleteChildren(Object *obj) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;
    if (scratch->child1) {
        ObjectDelete(&objects[scratch->child1]);
        scratch->child1 = 0;
    }
    if (scratch->child2) {
        ObjectDelete(&objects[scratch->child2]);
        scratch->child2 = 0;
    }
}

// One set of up to 8 logs (Bridge_MakeSegments): an object of its own, drawn as its children. `x` is where the first log is, and the next ones follow 16 pixels apart
static Object *Bridge_MakeSegments(Object *obj, int count, int16_t y, int16_t x) {
    Object *child = FindNextFreeObj(obj + 1);
    if (child == NULL)
        return NULL;
    child->type = obj->type;
    child->pos.l.x.f.u = obj->pos.l.x.f.u;
    child->pos.l.y.f.u = obj->pos.l.y.f.u;
    child->mappings = obj->mappings;
    child->tile = obj->tile;
    child->render.b = obj->render.b;
    child->render.f.multi_sprite = true;
    child->priority = 3;
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
static void Bridge_Depress(Object *obj) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;

    int16_t sin, cos;
    CalcSine(scratch->bend, &sin, &cos);
    uint32_t d4 = (uint16_t)sin;

    int logs = scratch->subtype;
    int stood = scratch->sonic_log;
    uint32_t depth = bend_depth[logs - 8][stood & 15]; // (8 to 16 logs: the subtype is kept in that range)

    // The logs from the first up to the one stood on (the shape for that many logs, from its start) ...
    const uint8_t *shape = bend_shape[stood & 15];
    int i = 0;
    for (int n = stood; n >= 0; n--, i++) {
        uint32_t d0 = (uint16_t)(*shape++ + 1u);
        d0 *= depth;
        d0 *= d4;
        Bridge_Log(scratch, i)->y = (int16_t)((d0 >> 16) + scratch->base_y);
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
        Bridge_Log(scratch, i)->y = (int16_t)((d0 >> 16) + scratch->base_y);
    }
}

// Sonic and Tails against the bridge (sub_7DC0 / sub_7DDA): standing on it, they ride the log they stand on; otherwise they may land on it
static void Bridge_Characters(Object *obj, int16_t x_rad, int16_t width) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;

    for (int who = SolidChar_Tails; who >= SolidChar_Sonic; who--) { // (Tails first)
        Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
        if (chr == NULL)
            continue;
        uint8_t *log_of = (who == SolidChar_Sonic) ? &scratch->sonic_log : &scratch->tails_log;
        const uint8_t stand_bit = (uint8_t)(1 << (3 + who));

        if (obj->status.b & stand_bit) {
            int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
            if (chr->status.p.f.in_air || d0 < 0 || d0 >= width) {
                chr->status.p.f.object_stand = false;
                obj->status.b &= (uint8_t)~stand_bit;
                continue;
            }
            int log = d0 >> 4;
            *log_of = (uint8_t)log;
            chr->pos.l.y.f.u = Bridge_Log(scratch, log)->y - 8 - chr->y_rad;
        } else if (Solid_PlatformLand(obj, chr, who, x_rad, width, 8) || (obj->status.b & stand_bit)) {
            int16_t d0 = chr->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
            *log_of = (uint8_t)(d0 >> 4);
        }
    }
}

// The logs next to the ones stood on wriggle (sub_7E60): a child sprite frame for each, by how near it is to Sonic's log and to Tails'
static void Bridge_Wriggle(Object *obj) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;

    int beat = (player->xsp != 0) ? 0 : (frame_count & 0x1C) >> 1;
    uint16_t sonic_low = wriggle[beat], sonic_high = wriggle[beat + 1];
    beat = (TAILS_OBJ->xsp != 0) ? 0 : (frame_count & 0x1C) >> 1;
    uint16_t tails_low = wriggle[beat], tails_high = wriggle[beat + 1];

    int16_t d3 = -2, d4 = -2;
    if (obj->status.b & 8)
        d3 = scratch->sonic_log;
    if (obj->status.b & 0x10)
        d4 = scratch->tails_log;

    for (int log = 0; log < scratch->subtype; log++) {
        uint8_t d0 = 0;
        if ((uint8_t)(d3 - 1) == log || (uint8_t)(d3 + 1) == log)
            d0 = (uint8_t)sonic_low;
        if ((uint8_t)(d4 - 1) == log || (uint8_t)(d4 + 1) == log)
            d0 = (uint8_t)tails_low;
        if ((uint8_t)d3 == log)
            d0 = (uint8_t)sonic_high;
        if ((uint8_t)d4 == log)
            d0 = (uint8_t)tails_high;
        Bridge_Log(scratch, log)->frame = d0;
    }
}

// Run once a frame: the weight on it, the bend, the characters, and gone when it is far off screen
static void Bridge_Run(Object *obj, bool hpz) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;

    uint8_t weight = obj->status.b & 0x18;
    bool depress = true;
    if (weight == 0) {
        if (scratch->bend == 0)
            depress = false;
        else
            scratch->bend -= 4;
    } else {
        // Tails on it: the log Sonic is taken to be on moves toward Tails'
        if (weight & 0x10) {
            uint8_t diff = (uint8_t)(scratch->sonic_log - scratch->tails_log);
            if (diff != 0) {
                if (scratch->sonic_log < scratch->tails_log)
                    scratch->sonic_log++;
                else
                    scratch->sonic_log--;
            }
        }
        if (scratch->bend != 0x40)
            scratch->bend += 4;
    }
    if (depress)
        Bridge_Depress(obj);

    int16_t half = (scratch->subtype << 3) + 8;
    int16_t width = scratch->subtype << 4;
    Bridge_Characters(obj, half, width);
    if (hpz)
        Bridge_Wriggle(obj);

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        Bridge_DeleteChildren(obj);
        ObjectDelete(obj);
    }
}

void Obj_Bridge(Object *obj) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;

    if (obj->render.f.multi_sprite) { // a set of logs only has to be drawn
        DisplaySprite(obj);
        return;
    }

    switch (obj->routine) {
    case BridgeRoutine_Init: {
        obj->routine += 2;
        obj->mappings = Mappings_BridgeGHZ;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Bridge);
        obj->priority = 3;
        if (LEVEL_ZONE(level_id) == 3) { // Emerald Hill
            obj->mappings = Mappings_BridgeEHZ;
            obj->tile = TILE_MAP(0, 2, 0, 0, 0x3C6);
        }
        if (LEVEL_ZONE(level_id) == 4) { // Hidden Palace
            obj->routine += 4;
            obj->mappings = Mappings_BridgeHPZ;
            obj->tile = TILE_MAP(0, 3, 0, 0, 0x300);
        }
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x80;

        int logs = scratch->subtype;
        if (logs < 8)
            logs = 8; // (Sonic 2 has no bridges of fewer logs)
        if (logs > BRIDGE_MAX_LOGS)
            logs = BRIDGE_MAX_LOGS;
        scratch->subtype = (uint8_t)logs;
        scratch->base_y = (uint16_t)obj->pos.l.y.f.u;
        scratch->bend = 0;
        scratch->sonic_log = 0;
        scratch->tails_log = 0;
        scratch->child1 = scratch->child2 = 0;

        int16_t y = obj->pos.l.y.f.u;
        int16_t x = obj->pos.l.x.f.u - ((logs / 2) << 4);
        Object *first = Bridge_MakeSegments(obj, 8, y, x);
        if (first == NULL) {
            ObjectDelete(obj);
            break;
        }
        first->pos.l.x.f.u = first->children[3].x - 8; // (the centre of its logs, for the culling)
        scratch->child1 = (uint8_t)(first - objects);

        int rest = logs - 8;
        if (rest > 0) {
            Object *second = Bridge_MakeSegments(obj, rest, y, x + 8 * 0x10);
            if (second == NULL) {
                Bridge_DeleteChildren(obj);
                ObjectDelete(obj);
                break;
            }
            second->pos.l.x.f.u = second->children[rest / 2].x - 8;
            scratch->child2 = (uint8_t)(second - objects);
        }
    }
        // Fallthrough
    case BridgeRoutine_Main:
        Bridge_Run(obj, false);
        break;
    case BridgeRoutine_Display:
        DisplaySprite(obj);
        break;
    case BridgeRoutine_HPZ:
        Bridge_Run(obj, true);
        break;
    }
}
