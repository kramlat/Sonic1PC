// The path swapper for Sonic 2 (Nick Arcade's object 03, "colichg"): an invisible line that sends Sonic or Tails onto the other collision path (the other 16x16 collision index and the other solid bits of the
// level's blocks) and behind or in front of the foreground as they leave the strip it watches. Each character is tracked on his own (a bit of `inside` says he was inside last frame), and the switch happens on
// the frame he is found outside again: on the object's far side from the line's start (to the right or below) it is the "forward" crossing, the other side the "backward" one.
// Subtype: bits 0-1 half the strip's length ($20, $40, $80 or $100), bit 2 a horizontal line (the strip runs across, for characters crossing it up or down), bit 3 the path after a forward crossing
// (1 the second), bit 4 the same for a backward one, bits 5 and 6 whether the character is in front (1) after a forward / backward crossing, bit 7 only when on the ground.
#include "Object/PathSwapper.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include "Resource/Mappings/PathSwapper.h"

typedef struct {
    uint8_t inside; // bit 7 Sonic, bit 6 Tails: inside the strip last frame
    int16_t size;   // half the strip's length
} Own_Swapper;

static const int16_t swapper_sizes[4] = { 0x20, 0x40, 0x80, 0x100 };

// The character has left the strip: switch his path and priority by which side he left on
static void Swapper_Cross(const Sst *self, Sst *chr, bool forward, uint8_t subtype) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;
    bool second = (subtype & (forward ? 0x08 : 0x10)) != 0;
    bool front = (subtype & (forward ? 0x20 : 0x40)) != 0;
    (void)self;

    sscratch->top_solid_bit = second ? 0xE : 0xC;
    sscratch->lrb_solid_bit = second ? 0xF : 0xD;
    chr->art_tile &= ~0x8000;
    if (front)
        chr->art_tile |= 0x8000;
    if (debug_cheat)
        PlaySound(sfx_Lamppost);
}

// Both of the characters against the strip (Pathswapper_MainX / _MainY: the strip is 16 across and `size` each way along the line)
static void Swapper_Watch(Sst *self, bool horizontal) {
    Own_Swapper *own = (Own_Swapper *)self->own.u8;
    if (debug_use)
        return;

    int16_t x0, x1, y0, y1;
    if (!horizontal) {
        x0 = self->x_pos.f.u - 8;
        x1 = self->x_pos.f.u + 8;
        y0 = self->y_pos.f.u - own->size;
        y1 = self->y_pos.f.u + own->size;
    } else {
        x0 = self->x_pos.f.u - own->size;
        x1 = self->x_pos.f.u + own->size;
        y0 = self->y_pos.f.u - 8;
        y1 = self->y_pos.f.u + 8;
    }

    for (int who = 0; who < 2; who++) {
        Sst *chr = (who == 0) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr == NULL)
            continue;
        const uint8_t bit = (uint8_t)(0x80 >> who);

        uint16_t cx = (uint16_t)chr->x_pos.f.u, cy = (uint16_t)chr->y_pos.f.u;
        if (cx >= (uint16_t)x0 && cx < (uint16_t)x1 && cy >= (uint16_t)y0 && cy < (uint16_t)y1) {
            own->inside |= bit;
            continue;
        }
        if (!(own->inside & bit))
            continue;

        // He was inside and is outside now (unless on the ground is asked for and he is in the air: then nothing, and he is let go)
        own->inside &= (uint8_t)~bit;
        if ((self->subtype & 0x80) && ((chr->status & SST_PSTATUS_IN_AIR) != 0))
            continue;
        bool forward = !horizontal ? !((uint16_t)chr->x_pos.f.u < (uint16_t)self->x_pos.f.u)
                                   : !((uint16_t)chr->y_pos.f.u < (uint16_t)self->y_pos.f.u);
        Swapper_Cross(self, chr, forward, self->subtype);
    }
}

void Obj_PathSwapper(Sst *self) {
    Own_Swapper *own = (Own_Swapper *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        self->routine += 2;
        self->mappings = Mappings_PathSwapper;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 5 * 0x80;
        self->mapping_frame = self->subtype & 7;
        own->size = swapper_sizes[self->subtype & 3];
        if (self->subtype & 4) {
            self->routine += 2;
            Swapper_Watch(self, true);
        } else {
            Swapper_Watch(self, false);
        }
        break;
    case 2:
        Swapper_Watch(self, false);
        break;
    case 4:
        Swapper_Watch(self, true);
        break;
    }

    // Only shown in debug mode; deleted once off-screen
    if (debug_cheat)
        Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
