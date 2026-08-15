#include "ChainStomp.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/ChainedStompers.h"
#include "Sound.h"

// Object 31 - stomping metal blocks on chains (MZ). Each placed stomper
// spawns up to 3 more objects alongside the main block: a spike tip, a
// chain (whose display length/frame tracks the block's own current
// extension), and a base piece fixed to the ceiling. The spikeless
// "small" size skips the spike tip. 7 movement types: one rises only
// while a matching switch is held (used once, in MZ1, and coordinates
// with the not-yet-ported PushBlock object via the shared obj31_ypos
// global), four fall and re-rise on their own on a timer, and two wait
// until Sonic wanders within range before switching into an auto-stomp
// type.

// Chain lengths (8.8 fixed point, i.e. already <<8), indexed by subtype's
// own low nibble.
static const uint16_t cstom_lengths[7] = {
    0x7000, 0xA000, 0x5000, 0x7800, 0x3800, 0x5800, 0xB800,
};

typedef struct {
    uint8_t routine;
    int8_t yoff;
    uint8_t frame;
} CStomVar;

// {routine, Y-offset from the block's own spawn position, frame}, in
// spawn order (block itself always spawns first, reusing this very
// object -- no allocation needed for it).
static const CStomVar cstom_vars[4] = {
    { 2, 0, 0 },            // block
    { 4, 0x1C, 1 },         // spikes
    { 8, (int8_t)0xCC, 3 }, // chain
    { 6, (int8_t)0xF0, 2 }, // base at ceiling
};

static void CStom_UpdateBlockY(Object *obj, Scratch_ChainStomp *scratch) {
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + (uint8_t)(scratch->current >> 8));
}

// Types 1/2/4/6 - rises and stomps down on its own on a timer.
static void CStom_AutoStomp(Object *obj, Scratch_ChainStomp *scratch) {
    if (scratch->rising) {
        if (scratch->delay != 0) {
            scratch->delay--;
        } else {
            if (((uint8_t)frame_count & 0xF) == 0 && obj->render.f.on_screen)
                QueueSound2(sfx_ChainRise);

            bool underflow = scratch->current < 0x80;
            scratch->current = (uint16_t)(scratch->current - 0x80);
            if (underflow) {
                scratch->current = 0;
                obj->ysp = 0;
                scratch->rising = false;
            }
        }
    } else if (scratch->current != scratch->length) {
        int16_t speed = obj->ysp;
        obj->ysp += 0x70;
        scratch->current = (uint16_t)(scratch->current + (uint16_t)speed);

        if (scratch->current >= scratch->length) {
            scratch->current = scratch->length;
            obj->ysp = 0;
            scratch->rising = true;
            scratch->delay = 60;

            if (obj->render.f.on_screen)
                QueueSound2(sfx_ChainStomp);
        }
    }

    CStom_UpdateBlockY(obj, scratch);
}

// Type 0 - rises only while the matching switch is held down, and stops
// short of the ceiling if a pushable block is currently sitting on top of
// it (so the block doesn't get stuck poking into the ceiling).
static void CStom_SwitchActivated(Object *obj, Scratch_ChainStomp *scratch) {
    if (!f_switch[scratch->switch_id]) {
        if (scratch->current != scratch->length) {
            int16_t speed = obj->ysp;
            obj->ysp += 0x70;
            scratch->current = (uint16_t)(scratch->current + (uint16_t)speed);

            if (scratch->current >= scratch->length) {
                scratch->current = scratch->length;
                obj->ysp = 0;
                if (obj->render.f.on_screen)
                    QueueSound2(sfx_ChainStomp);
            }
        }
        CStom_UpdateBlockY(obj, scratch);
        return;
    }

    bool blocked_by_pushblock = obj31_ypos < 0 && (uint8_t)(scratch->current >> 8) == 0x10;
    if (!blocked_by_pushblock && scratch->current != 0) {
        if (((uint8_t)frame_count & 0xF) == 0 && obj->render.f.on_screen)
            QueueSound2(sfx_ChainRise);

        bool underflow = scratch->current < 0x80;
        scratch->current = (uint16_t)(scratch->current - 0x80);
        if (underflow)
            scratch->current = 0;
    }

    obj->ysp = 0;
    CStom_UpdateBlockY(obj, scratch);
}

// Types 3/5 - waits until Sonic is horizontally in range, then switches
// its own subtype to the next one along (always an auto-stomp type).
static void CStom_WaitForSonic(Object *obj, Scratch_ChainStomp *scratch) {
    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    if (d0 < 144)
        obj->scratch.u8[0]++;
    CStom_UpdateBlockY(obj, scratch);
}

static void CStom_Types(Object *obj, Scratch_ChainStomp *scratch) {
    switch (obj->scratch.u8[0] & 0xF) {
    case 0:
        CStom_SwitchActivated(obj, scratch);
        break;
    case 1:
    case 2:
    case 4:
    case 6:
        CStom_AutoStomp(obj, scratch);
        break;
    case 3:
    case 5:
        CStom_WaitForSonic(obj, scratch);
        break;
    default: // unused/undefined on real hardware too
        CStom_UpdateBlockY(obj, scratch);
        break;
    }
}

static void CStom_MainBlock(Object *obj, Scratch_ChainStomp *scratch) {
    CStom_Types(obj, scratch);
    obj31_ypos = obj->pos.l.y.f.u;

    int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
    SolidObject(obj, (uint16_t)x_rad, 24 / 2, 26 / 2, obj->pos.l.x.f.u, NULL, NULL);

    if (obj->status.o.f.player_stand && (uint8_t)(scratch->current >> 8) < 0x10)
        KillSonic(player, obj);
}

static void CStom_Spikes(Object *obj, Scratch_ChainStomp *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_ChainStomp *pscratch = (Scratch_ChainStomp *)&parent->scratch;
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + (uint8_t)(pscratch->current >> 8));
}

static void CStom_Chain(Object *obj, Scratch_ChainStomp *scratch) {
    // Real ASM sets these once every tick this routine runs (not just at
    // spawn): obHeight=256/2 + the custom-height render bit, since the
    // chain can be up to 256px tall and the default ~32px assumed cull
    // height would treat most of a long chain as off-screen.
    // 128 doesn't fit in y_rad's signed int8_t range -- stored via its
    // bit-identical uint8_t cast (-128), which BuildSprites' own
    // yrad_height cull path already reinterprets back to unsigned 128 (see
    // its own comment on exactly this pattern, added for this same reason).
    obj->y_rad = (int8_t)(uint8_t)(256 / 2);
    obj->render.f.yrad_height = true;
    Object *parent = &objects[scratch->parent_index];
    Scratch_ChainStomp *pscratch = (Scratch_ChainStomp *)&parent->scratch;
    obj->frame = (uint8_t)((((uint8_t)(pscratch->current >> 8)) >> 5) + 3);
    CStom_Spikes(obj, scratch); // shared Y-alignment
}

static void CStom_Main(Object *obj, Scratch_ChainStomp *scratch) {
    uint8_t subtype = obj->scratch.u8[0];

    if (subtype & 0x80) {
        // Switch-activated -- both real table entries just map index N to
        // switch N (the subtype override value is always 0), so no
        // lookup table is needed here.
        scratch->switch_id = subtype & 0x7F;
        subtype = 0;
        obj->scratch.u8[0] = subtype;
    }

    uint16_t length = cstom_lengths[subtype & 0xF];
    scratch->length = length;
    if ((subtype & 0xF) == 0)
        scratch->current = length; // spawn already fully extended

    bool spikeless = (subtype & 0xF0) == 0x20;
    Object *last = obj;

    for (int i = 0; i < 4; i++) {
        if (i == 1 && spikeless)
            continue;

        Object *seg = obj;
        if (i > 0) {
            seg = FindNextFreeObj(obj);
            if (seg == NULL)
                break;
        }

        seg->routine = cstom_vars[i].routine;
        seg->type = ObjId_ChainStomp;
        seg->pos.l.x.f.u = obj->pos.l.x.f.u;
        seg->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + cstom_vars[i].yoff);
        seg->mappings = Mappings_ChainedStompers;
        seg->tile = TILE_MAP(0, 0, 0, 0, 0x300); // ArtTile_MZ_Spike_Stomper
        seg->render.b = 0;
        seg->render.f.align_fg = true;

        Scratch_ChainStomp *sscratch = (Scratch_ChainStomp *)&seg->scratch;
        sscratch->orig_y = seg->pos.l.y.f.u;
        seg->scratch.u8[0] = subtype;
        seg->width_pixels = 32 / 2;
        sscratch->length = length;
        seg->priority = 4;
        seg->frame = cstom_vars[i].frame;

        if (i == 1) { // spikes
            seg->width_pixels = 112 / 2;
            seg->col_type = 0x90; // col_80x32 | col_hurt
        }

        sscratch->parent_index = (uint8_t)(obj - objects);
        last = seg;
    }

    last->priority = 3; // whichever piece was spawned last (normally the ceiling base) gets a higher priority

    static const struct {
        uint8_t width, frame;
    } block_size[3] = {
        { 0x70 / 2, 0 },  // large
        { 0x60 / 2, 9 },  // medium
        { 0x20 / 2, 0xA }, // small
    };
    uint8_t size = (subtype >> 4) & 0xF;
    if (size > 2)
        size = 2;
    obj->width_pixels = block_size[size].width;
    obj->frame = block_size[size].frame;
}

void Obj_ChainStomp(Object *obj) {
    Scratch_ChainStomp *scratch = (Scratch_ChainStomp *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        CStom_Main(obj, scratch);
        __attribute__((fallthrough));
    case 2:
        CStom_MainBlock(obj, scratch);
        break;
    case 4:
        CStom_Spikes(obj, scratch);
        break;
    case 6: // base at ceiling -- doesn't move
        break;
    case 8:
        CStom_Chain(obj, scratch);
        break;
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}
