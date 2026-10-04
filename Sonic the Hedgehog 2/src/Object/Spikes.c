// Spikes for Sonic 2 (the Simon Wai prototype's object 36): Sonic 1's spikes made solid for both characters, on palette line 1. The subtype's high nibble is the kind: 0-3 upright (16, 32, 48 or 64 wide), 4-7 sideways
// (16, 32, 48 or 64 tall), upside down when the object is flipped; its low nibble how they move (0 not, 1 up and down, 2 left and right).
#include "Object/Spikes.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

// The size of each kind: half its width and its height (the mapping's frame is the kind)
static const uint8_t spike_sizes[8][2] = { { 0x10, 0x10 }, { 0x20, 0x10 }, { 0x30, 0x10 }, { 0x40, 0x10 }, { 0x10, 0x10 }, { 0x10, 0x20 }, { 0x10, 0x30 }, { 0x10, 0x40 } };

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

// Touch_ChkHurt2: the character is pushed back out of the spikes and hurt (unless he is invincible, already hurt, or still flashing from a hit)
static void Spike_Hurt(Object *obj, Object *chr, int who) {
    if (invincibility)
        return;
    Scratch_Sonic *scratch = (Scratch_Sonic *)&chr->scratch;
    if (scratch->flash_time) // ("Proper Spike Bug Fix")
        return;
    if (chr->routine >= 4)
        return;
    chr->pos.l.y.v -= chr->ysp << 8;
    if (who == SolidChar_Sonic)
        HurtSonic(chr, obj);
    else
        Tails_Hurt(chr, obj);
}

static void Spike_Wait(Object *obj) {
    Scratch_Spikes *scratch = (Scratch_Spikes*)&obj->scratch;

    // Wait for direction switch
    if (scratch->timer) {
        if (--scratch->timer)
            return;
        if (obj->render.f.on_screen) {
            ;
            PlaySound(sfx_SpikesMove);
        }
        return;
    }

    // Switch direction
    if (scratch->dir) {
        // Move to target position
        if ((scratch->move.v -= 0x800) >= 0x8000) {
            scratch->move.v = 0x0000;
            scratch->dir = 0;
            scratch->timer = 60;
        }
    } else {
        // Move to target position
        if ((scratch->move.v += 0x800) >= 0x2000) {
            scratch->move.v = 0x2000;
            scratch->dir = 1;
            scratch->timer = 60;
        }
    }
}

void Obj_Spikes(Object *obj) {
    Scratch_Spikes* scratch = (Scratch_Spikes*)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_Spikes;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Spikes);
        obj->render.f.level_fg = true;
        obj->priority = 4;

        unsigned kind = scratch->subtype >> 4;
        scratch->subtype &= 0xF;
        obj->width_pixels = spike_sizes[kind][0];
        obj->y_rad = (int8_t)spike_sizes[kind][1];
        obj->frame = (uint8_t)kind;
        if (kind >= 4)
            obj->routine += 2; // sideways
        if (obj->status.o.f.y_flip)
            obj->routine = 6;  // upside down
        scratch->orig_x = obj->pos.l.x.f.u;
        scratch->orig_y = obj->pos.l.y.f.u;
    }

    int16_t old_x = obj->pos.l.x.f.u; // (the sideways ones are solid at where they were before they moved)
    switch (scratch->subtype) {
    case 1: // up and down
        Spike_Wait(obj);
        obj->pos.l.y.f.u = scratch->orig_y + scratch->move.f.u;
        break;
    case 2: // left and right
        Spike_Wait(obj);
        obj->pos.l.x.f.u = scratch->orig_x + scratch->move.f.u;
        break;
    }
    int16_t x = obj->routine == 4 ? old_x : obj->pos.l.x.f.u;

    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr == NULL)
            continue;
        int32_t solid = Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), obj->y_rad, (int16_t)(obj->y_rad + 1), x, NULL);
        switch (obj->routine) {
        case 2: // upright: hurts whoever stands on it
            if (obj->status.b & (1 << (3 + who)))
                Spike_Hurt(obj, chr, who);
            break;
        case 4: // sideways: whoever runs into it
            if (solid == 1) {
                Spike_Hurt(obj, chr, who);
                obj->status.b &= (uint8_t)~(1 << (5 + who));
            }
            break;
        case 6: // upside down: whoever hits it from below
            if (solid == -2)
                Spike_Hurt(obj, chr, who);
            break;
        }
    }

    // Draw and unload once offscreen
    DisplaySprite(obj);
    if (IS_OFFSCREEN(scratch->orig_x))
        ObjectDelete(obj);
}
