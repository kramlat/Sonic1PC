// The path swapper for Sonic 2 (the pre-beta's object $03, the final game's): an invisible line that sends Sonic or Tails onto the other collision path (the other 16x16 collision index and the other solid bits of
// the level's blocks) and behind or in front of the foreground as they cross it. Each character has a side of the line he was last found on (set when the object is made, from where he stands); when he is found
// on the other side the side turns over, and if he is in the strip's length along the line the path and the priority are set by the way he went (the Nick Arcade version it replaces watched the character leave a strip).
// Subtype: bits 0-1 half the strip's length ($20, $40, $80 or $100), bit 2 a horizontal line (it runs across, for characters crossing it up or down), bit 3 the path after a crossing to the right or down (1 the second),
// bit 4 the same for a crossing to the left or up, bits 5 and 6 whether the character is in front (1) after a crossing to the right or down / to the left or up, bit 7 only when on the ground. An object turned to the
// left (X flip) leaves the path as it is and sets only the priority.
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
    uint8_t side[2]; // Sonic's and Tails': 1 when he was found past the line (to the right of it, or below it)
    int16_t size;    // half the strip's length
} Own_Swapper;

static const int16_t swapper_sizes[4] = { 0x20, 0x40, 0x80, 0x100 };

static Sst *Swapper_Character(int who) {
    return who == 0 ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

// The character crossed the line (to the right or down, or the other way): his path and priority by the subtype
static void Swapper_Cross(const Sst *self, Sst *chr, bool forward) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;
    const uint8_t subtype = self->subtype;
    if (!(self->render_flags & SST_RENDER_X_FLIP)) {
        const bool second = (subtype & (forward ? 0x08 : 0x10)) != 0;
        sscratch->top_solid_bit = second ? 0xE : 0xC;
        sscratch->lrb_solid_bit = second ? 0xF : 0xD;
    }
    chr->art_tile &= ~0x8000;
    if (subtype & (forward ? 0x20 : 0x40))
        chr->art_tile |= 0x8000;
    if (debug_cheat)
        PlaySound(sfx_Lamppost);
}

// Both of the characters against the line (Offset_0x01A828 for the vertical one, Offset_0x01A942 for the horizontal)
static void Swapper_Watch(Sst *self, bool horizontal) {
    Own_Swapper *own = (Own_Swapper *)self->own.u8;
    if (debug_use)
        return;

    const uint16_t line = (uint16_t)(horizontal ? self->y_pos.f.u : self->x_pos.f.u);
    const int16_t centre = horizontal ? self->x_pos.f.u : self->y_pos.f.u; // (the middle of the strip, along the line)
    for (int who = 0; who < 2; who++) {
        Sst *chr = Swapper_Character(who);
        if (chr == NULL)
            continue;
        const uint16_t pos = (uint16_t)(horizontal ? chr->y_pos.f.u : chr->x_pos.f.u);
        bool forward;
        if (!own->side[who]) {
            if (line > pos)
                continue;
            own->side[who] = 1;
            forward = true;
        } else {
            if (line <= pos)
                continue;
            own->side[who] = 0;
            forward = false;
        }
        const int16_t along = horizontal ? chr->x_pos.f.u : chr->y_pos.f.u;
        if (along < centre - own->size || along >= centre + own->size)
            continue;
        if ((self->subtype & 0x80) && (chr->status & SST_PSTATUS_IN_AIR))
            continue;
        Swapper_Cross(self, chr, forward);
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
        own->size = swapper_sizes[self->subtype & 3];
        {
            const bool horizontal = (self->subtype & 4) != 0;
            self->mapping_frame = self->subtype & (horizontal ? 7 : 3);
            if (horizontal)
                self->routine += 2;
            for (int who = 0; who < 2; who++) { // (which side of the line each stands on to begin with)
                Sst *chr = Swapper_Character(who);
                if (chr != NULL)
                    own->side[who] = (uint16_t)(horizontal ? self->y_pos.f.u : self->x_pos.f.u) < (uint16_t)(horizontal ? chr->y_pos.f.u : chr->x_pos.f.u);
            }
            Swapper_Watch(self, horizontal);
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
