#include "SidewaysStomper.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/SidewaysStomper.h"

// Object 45 - unused sideways spiked metal stomper from a beta version
// (MZ). Never placed in any real level layout -- structurally a sideways
// mirror of the shipped ChainStomp (object 31), spawning a main block,
// spikes, a wall bracket, and an extending pole, all stomping horizontally
// instead of vertically. Ported for disassembly parity only.

static const uint16_t sstom_lengths[3] = { 0x3800, 0xA000, 0x5000 };

typedef struct {
    uint8_t routine;
    int8_t xoff;
    uint8_t frame;
} SStomVar;

static const SStomVar sstom_vars[4] = {
    { 2, 4, 0 },              // main block
    { 4, (int8_t)0xE4, 1 },   // spikes (-0x1C)
    { 8, 0x34, 3 },           // pole
    { 6, 0x28, 2 },           // wall bracket
};

static void SStom_Spikes(Object *obj, Scratch_SideStomp *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_SideStomp *pscratch = (Scratch_SideStomp *)&parent->scratch;
    int16_t d0 = (int16_t)-(uint8_t)(pscratch->current_x >> 8);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
}

static void SStom_Pole(Object *obj, Scratch_SideStomp *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_SideStomp *pscratch = (Scratch_SideStomp *)&parent->scratch;
    uint8_t d0 = (uint8_t)(pscratch->current_x >> 8);
    d0 = (uint8_t)(d0 + 0x10);
    obj->frame = (uint8_t)((d0 >> 5) + 3);
    SStom_Spikes(obj, scratch); // shared X-alignment
}

static void SStom_Move(Object *obj, Scratch_SideStomp *scratch) {
    if (scratch->retract) {
        if (scratch->delay != 0) {
            scratch->delay--;
        } else {
            bool underflow = scratch->current_x < 0x80;
            scratch->current_x = (uint16_t)(scratch->current_x - 0x80);
            if (underflow) {
                scratch->current_x = 0;
                obj->xsp = 0;
                scratch->retract = false;
            }
        }
    } else if (scratch->current_x != scratch->length) {
        int16_t speed = obj->xsp;
        obj->xsp += 0x70;
        scratch->current_x = (uint16_t)(scratch->current_x + (uint16_t)speed);

        if (scratch->current_x >= scratch->length) {
            scratch->current_x = scratch->length;
            obj->xsp = 0;
            scratch->retract = true;
            scratch->delay = 60;
        }
    }

    int16_t d0 = (int16_t)-(uint8_t)(scratch->current_x >> 8);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d0);
}

static void SStom_ChkDel(Object *obj, Scratch_SideStomp *scratch) {
    if (IS_OFFSCREEN(scratch->orig_x2)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

static void SStom_MainBlock(Object *obj, Scratch_SideStomp *scratch) {
    SStom_Move(obj, scratch);

    int16_t x_rad = (int16_t)(24 / 2 + 11 /* sonic_solid_width */);
    SolidObject(obj, (uint16_t)x_rad, 64 / 2, 64 / 2, obj->pos.l.x.f.u, NULL, NULL);

    SStom_ChkDel(obj, scratch);
}

static void SStom_Main(Object *obj, Scratch_SideStomp *scratch) {
    uint8_t subtype = obj->scratch.u8[0];
    uint16_t length = sstom_lengths[subtype % 3];

    Object *last = obj;
    for (int i = 0; i < 4; i++) {
        Object *seg = obj;
        if (i > 0) {
            seg = FindNextFreeObj(obj);
            if (seg == NULL)
                break;
        }

        seg->routine = sstom_vars[i].routine;
        seg->type = ObjId_SidewaysStomper;
        seg->pos.l.y.f.u = obj->pos.l.y.f.u;
        seg->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + sstom_vars[i].xoff);
        seg->mappings = Mappings_SidewaysStomper;
        seg->tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Spike_Stomper);
        seg->render.b = 0;
        seg->render.f.align_fg = true;

        Scratch_SideStomp *sscratch = (Scratch_SideStomp *)&seg->scratch;
        sscratch->orig_x = seg->pos.l.x.f.u;
        sscratch->orig_x2 = obj->pos.l.x.f.u;
        seg->scratch.u8[0] = subtype;
        seg->width_pixels = 64 / 2;
        sscratch->length = length;
        seg->priority = 4;

        if (sstom_vars[i].frame == 1) // spikes
            seg->col_type = 0x11 | 0x80; // col_32x48 | col_hurt

        seg->frame = sstom_vars[i].frame;
        sscratch->parent_index = (uint8_t)(obj - objects);
        last = seg;
    }

    last->priority = 3;
    obj->width_pixels = 32 / 2;
}

void Obj_SidewaysStomper(Object *obj) {
    Scratch_SideStomp *scratch = (Scratch_SideStomp *)&obj->scratch;
    switch (obj->routine) {
    case 0: SStom_Main(obj, scratch); __attribute__((fallthrough));
    case 2: SStom_MainBlock(obj, scratch); break;
    case 4: SStom_Spikes(obj, scratch); SStom_ChkDel(obj, scratch); break;
    case 6: SStom_ChkDel(obj, scratch); break;
    case 8: SStom_Pole(obj, scratch); SStom_ChkDel(obj, scratch); break;
    }
}
