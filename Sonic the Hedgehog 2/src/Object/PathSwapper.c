// The path swapper for Sonic 2 (Nick Arcade's object 03, "colichg"): an invisible line that sends Sonic or Tails onto the other collision path (the other 16x16 collision index and the other solid bits of the
// level's blocks) and behind or in front of the foreground as they leave the strip it watches. Each character is tracked on his own (a bit of byte $30 says he was inside last frame), and the switch happens on
// the frame he is found outside again: on the object's far side from the line's start (to the right or below) it is the "forward" crossing, the other side the "backward" one.
// Subtype: bits 0-1 half the strip's length ($20, $40, $80 or $100), bit 2 a horizontal line (the strip runs across, for characters crossing it up or down), bit 3 the path after a forward crossing
// (1 the second), bit 4 the same for a backward one, bits 5 and 6 whether the character is in front (1) after a forward / backward crossing, bit 7 only when on the ground.
#include "Object/PathSwapper.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include "Resource/Mappings/PathSwapper.h"

typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad0[7]; // 0x29-0x2F
    uint8_t inside;  // 0x30: bit 7 Sonic, bit 6 Tails: inside the strip last frame
    uint8_t pad1;    // 0x31
    int16_t size;    // 0x32: half the strip's length
} Scratch_Swapper;

static const int16_t swapper_sizes[4] = { 0x20, 0x40, 0x80, 0x100 };

// The character has left the strip: switch his path and priority by which side he left on
static void Swapper_Cross(const Object *obj, Object *chr, bool forward, uint8_t subtype) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;
    bool second = (subtype & (forward ? 0x08 : 0x10)) != 0;
    bool front = (subtype & (forward ? 0x20 : 0x40)) != 0;
    (void)obj;

    sscratch->top_solid_bit = second ? 0xE : 0xC;
    sscratch->lrb_solid_bit = second ? 0xF : 0xD;
    chr->tile &= ~0x8000;
    if (front)
        chr->tile |= 0x8000;
    if (debug_cheat)
        PlaySound(sfx_Lamppost);
}

// Both of the characters against the strip (Pathswapper_MainX / _MainY: the strip is 16 across and `size` each way along the line)
static void Swapper_Watch(Object *obj, bool horizontal) {
    Scratch_Swapper *scratch = (Scratch_Swapper *)&obj->scratch;
    if (debug_use)
        return;

    int16_t x0, x1, y0, y1;
    if (!horizontal) {
        x0 = obj->pos.l.x.f.u - 8;
        x1 = obj->pos.l.x.f.u + 8;
        y0 = obj->pos.l.y.f.u - scratch->size;
        y1 = obj->pos.l.y.f.u + scratch->size;
    } else {
        x0 = obj->pos.l.x.f.u - scratch->size;
        x1 = obj->pos.l.x.f.u + scratch->size;
        y0 = obj->pos.l.y.f.u - 8;
        y1 = obj->pos.l.y.f.u + 8;
    }

    for (int who = 0; who < 2; who++) {
        Object *chr = (who == 0) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
        if (chr == NULL)
            continue;
        const uint8_t bit = (uint8_t)(0x80 >> who);

        uint16_t cx = (uint16_t)chr->pos.l.x.f.u, cy = (uint16_t)chr->pos.l.y.f.u;
        if (cx >= (uint16_t)x0 && cx < (uint16_t)x1 && cy >= (uint16_t)y0 && cy < (uint16_t)y1) {
            scratch->inside |= bit;
            continue;
        }
        if (!(scratch->inside & bit))
            continue;

        // He was inside and is outside now (unless on the ground is asked for and he is in the air: then nothing, and he is let go)
        scratch->inside &= (uint8_t)~bit;
        if ((scratch->subtype & 0x80) && chr->status.p.f.in_air)
            continue;
        bool forward = !horizontal ? !((uint16_t)chr->pos.l.x.f.u < (uint16_t)obj->pos.l.x.f.u)
                                   : !((uint16_t)chr->pos.l.y.f.u < (uint16_t)obj->pos.l.y.f.u);
        Swapper_Cross(obj, chr, forward, scratch->subtype);
    }
}

void Obj_PathSwapper(Object *obj) {
    Scratch_Swapper *scratch = (Scratch_Swapper *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Initialization
        obj->routine += 2;
        obj->mappings = Mappings_PathSwapper;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 5;
        obj->frame = scratch->subtype & 7;
        scratch->size = swapper_sizes[scratch->subtype & 3];
        if (scratch->subtype & 4) {
            obj->routine += 2;
            Swapper_Watch(obj, true);
        } else {
            Swapper_Watch(obj, false);
        }
        break;
    case 2:
        Swapper_Watch(obj, false);
        break;
    case 4:
        Swapper_Watch(obj, true);
        break;
    }

    // Only shown in debug mode; deleted once off-screen
    if (debug_cheat)
        DisplaySprite(obj);
    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
