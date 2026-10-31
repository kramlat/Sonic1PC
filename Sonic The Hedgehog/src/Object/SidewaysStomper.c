#include "SidewaysStomper.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

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

static void SStom_Spikes(Sst *self, Own_SideStomp *own) {
    Sst *parent = &sst[own->parent_index];
    Own_SideStomp *pscratch = (Own_SideStomp *)parent->own.u8;
    int16_t d0 = (int16_t)-(uint8_t)(pscratch->current_x >> 8);
    self->x_pos.f.u = (int16_t)(own->orig_x + d0);
}

static void SStom_Pole(Sst *self, Own_SideStomp *own) {
    Sst *parent = &sst[own->parent_index];
    Own_SideStomp *pscratch = (Own_SideStomp *)parent->own.u8;
    uint8_t d0 = (uint8_t)(pscratch->current_x >> 8);
    d0 = (uint8_t)(d0 + 0x10);
    self->mapping_frame = (uint8_t)((d0 >> 5) + 3);
    SStom_Spikes(self, own); // shared X-alignment
}

static void SStom_Move(Sst *self, Own_SideStomp *own) {
    if (own->retract) {
        if (own->delay != 0) {
            own->delay--;
        } else {
            bool underflow = own->current_x < 0x80;
            own->current_x = (uint16_t)(own->current_x - 0x80);
            if (underflow) {
                own->current_x = 0;
                self->x_vel = 0;
                own->retract = false;
            }
        }
    } else if (own->current_x != own->length) {
        int16_t speed = self->x_vel;
        self->x_vel += 0x70;
        own->current_x = (uint16_t)(own->current_x + (uint16_t)speed);

        if (own->current_x >= own->length) {
            own->current_x = own->length;
            self->x_vel = 0;
            own->retract = true;
            own->delay = 60;
        }
    }

    int16_t d0 = (int16_t)-(uint8_t)(own->current_x >> 8);
    self->x_pos.f.u = (int16_t)(own->orig_x + d0);
}

static void SStom_ChkDel(Sst *self, Own_SideStomp *own) {
    SstOnScreenTest2(self, own->orig_x2);
}

static void SStom_MainBlock(Sst *self, Own_SideStomp *own) {
    SStom_Move(self, own);

    int16_t x_rad = (int16_t)(24 / 2 + 11 /* sonic_solid_width */);
    SstSolidObject(self, (uint16_t)x_rad, 64 / 2, 64 / 2, self->x_pos.f.u, NULL, NULL);

    SStom_ChkDel(self, own);
}

static void SStom_Main(Sst *self, Own_SideStomp *own) {
    uint8_t subtype = self->subtype;
    uint16_t length = sstom_lengths[subtype % 3];

    Sst *last = self;
    for (int i = 0; i < 4; i++) {
        Sst *seg = self;
        if (i > 0) {
            seg = Sst_AllocateAfter(self);
            if (seg == NULL)
                break;
        }

        seg->routine = sstom_vars[i].routine;
        seg->code = Obj_SidewaysStomper;
        seg->y_pos.f.u = self->y_pos.f.u;
        seg->x_pos.f.u = (int16_t)(self->x_pos.f.u + sstom_vars[i].xoff);
        seg->mappings = Mappings_SidewaysStomper;
        seg->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Spike_Stomper);
        seg->render_flags = SST_RENDER_LEVEL;

        Own_SideStomp *sscratch = (Own_SideStomp *)seg->own.u8;
        sscratch->orig_x = seg->x_pos.f.u;
        sscratch->orig_x2 = self->x_pos.f.u;
        seg->subtype = subtype;
        seg->width_pixels = 64 / 2;
        sscratch->length = length;
        seg->priority = 4 * 0x80;

        if (sstom_vars[i].frame == 1) // spikes
            seg->collision_flags = 0x11 | 0x80; // col_32x48 | col_hurt

        seg->mapping_frame = sstom_vars[i].frame;
        sscratch->parent_index = (uint8_t)(self - sst);
        last = seg;
    }

    last->priority = 3 * 0x80;
    self->width_pixels = 32 / 2;
}

void Obj_SidewaysStomper(Sst *self) {
    Own_SideStomp *own = (Own_SideStomp *)self->own.u8;
    switch (self->routine) {
    case 0: SStom_Main(self, own); __attribute__((fallthrough));
    case 2: SStom_MainBlock(self, own); break;
    case 4: SStom_Spikes(self, own); SStom_ChkDel(self, own); break;
    case 6: SStom_ChkDel(self, own); break;
    case 8: SStom_Pole(self, own); SStom_ChkDel(self, own); break;
    }
}
