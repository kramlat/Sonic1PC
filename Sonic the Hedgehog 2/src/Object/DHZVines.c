// Dust Hill's roots and bridge for Sonic 2, as the alpha has them (objects 7F, 80 and 81: the prototype has none of them): the roots that a character hangs from and that are switches for the bridges (7F: a short
// one; 80: a long one that rises and falls, hung from by pressing against it), and the bridge that swings from upright to flat (81) when its switch is down. A character that hangs has his own controls off
// and the animation $14; a jump lets go. All carry Sonic and Tails.
#include "Object/DHZVines.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/CharControl.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/DHZVerticalBridge.h"
#include "Resource/Mappings/DHZVines.h"
#include "Resource/Mappings/DHZVinesSwitch.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

#define VINE_ANIM 0x14 // the animation of a character hanging from a root

// What each character's pad says: the buttons held (high byte of the alpha's word) and just pressed (low byte)
typedef struct {
    uint8_t held, pressed;
} Pad;

static Pad PadOf(int who) {
    Pad p;
    if (who == SolidChar_Sonic) {
        p.held = jpad1_hold1;
        p.pressed = jpad1_press1;
    } else {
        p.held = jpad2_hold;
        p.pressed = jpad2_press;
    }
    return p;
}

// The roots' state: a byte for each character (hanging or not) and one for each of how long he must wait to hang again
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[7];     // 0x29-0x2F
    uint8_t hanging[2];  // 0x30, 0x31
    uint8_t cooldown[2]; // 0x32, 0x33
    uint8_t linked;      // 0x34 (7F: unused): the roots' bit 7 of the subtype: letting go puts the switch back
    uint8_t extended;    // 0x36: (80) starts hanging down
    uint8_t pad1;
    int16_t length;      // 0x38: (80) how far down it is, 0 to $B0
    int16_t step;        // 0x3A: (80) how fast it moves
    int16_t home_y;      // 0x3C: (80)
} Scratch_Roots;

static void SwitchSet(const Scratch_Roots *scratch, bool down) {
    uint8_t *flag = &f_switch[scratch->subtype & 0xF];
    if (down)
        *flag |= 1;
    else
        *flag &= (uint8_t)~1;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 7F: roots that are switches. A character that comes to them from below (within 12 pixels each side and $28 under) takes hold, and his weight is the switch; a jump lets go (up: and the
// way he is holding, or the fall straight down). The picture is the one with two sides' roots while someone hangs
// ---------------------------------------------------------------------------------------------------------------------------------------
static void Roots_Frame(Object *obj, const Scratch_Roots *scratch) {
    obj->frame = (scratch->hanging[0] || scratch->hanging[1]) ? 1 : 0;
}

static void Roots_Character(Object *obj, Scratch_Roots *scratch, Object *chr, int who) {
    const Pad pad = PadOf(who);
    if (scratch->hanging[who]) {
        if (!(pad.pressed & 0x70))
            return;
        OBJ_CONTROL(chr) = 0;
        scratch->hanging[who] = 0;
        scratch->cooldown[who] = (pad.held & 0x0F) ? 0x3C : 0x12;
        chr->ysp = (int16_t)0xFD00;
        SwitchSet(scratch, false);
        Roots_Frame(obj, scratch);
        return;
    }
    if (scratch->cooldown[who] != 0 && --scratch->cooldown[who] != 0)
        return;
    if ((uint16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u + 0x0C) >= 0x18)
        return;
    if ((uint16_t)(chr->pos.l.y.f.u - obj->pos.l.y.f.u - 0x28) >= 0x10)
        return;
    if (chr->routine >= 4)
        return;
    chr->xsp = 0;
    chr->ysp = 0;
    chr->inertia = 0;
    chr->pos.l.x.f.u = obj->pos.l.x.f.u;
    chr->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x30);
    chr->anim = VINE_ANIM;
    OBJ_CONTROL(chr) = 1;
    scratch->hanging[who] = 1;
    SwitchSet(scratch, true);
    PlaySound(sfx_Switch);
    obj->frame = 1;
}

void Obj_DHZVinesSwitch(Object *obj) {
    Scratch_Roots *scratch = (Scratch_Roots *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_DHZVinesSwitch;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x40E);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->width_pixels = 8;
        obj->priority = 4;
    }
    FOR_EACH_CHARACTER(chr, who)
        Roots_Character(obj, scratch, chr, who);
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 80: roots that hang down and carry. They are as long as the frame says (up to $B0 pixels, in steps of $20) and grow while someone hangs from them (shrink when nobody does; or the other way round,
// with bit 4-6 of the subtype), a character takes hold from below as with 7F, and a jump lets go, to either side if the pad says so; with bit 7 of the subtype they are a switch as well
// ---------------------------------------------------------------------------------------------------------------------------------------
static void Vines_Character(Object *obj, Scratch_Roots *scratch, Object *chr, int who) {
    const Pad pad = PadOf(who);
    if (scratch->hanging[who]) {
        if (!(pad.pressed & 0x70)) { // hangs on: he goes with the roots
            chr->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x94);
            return;
        }
        OBJ_CONTROL(chr) = 0;
        scratch->hanging[who] = 0;
        scratch->cooldown[who] = (pad.held & 0x0F) ? 0x3C : 0x12;
        if (pad.held & JPAD_LEFT)
            chr->xsp = -0x200;
        if (pad.held & JPAD_RIGHT)
            chr->xsp = 0x200;
        chr->ysp = (int16_t)0xFC80;
        chr->status.p.f.in_air = true;
        if (scratch->linked)
            SwitchSet(scratch, false);
        return;
    }
    if (scratch->cooldown[who] != 0 && --scratch->cooldown[who] != 0)
        return;
    if ((uint16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u + 0x10) >= 0x20)
        return;
    if ((uint16_t)(chr->pos.l.y.f.u - obj->pos.l.y.f.u - 0x88) >= 0x18)
        return;
    if (chr->routine >= 4)
        return;
    chr->xsp = 0;
    chr->ysp = 0;
    chr->inertia = 0;
    chr->pos.l.x.f.u = obj->pos.l.x.f.u;
    chr->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x94);
    chr->anim = VINE_ANIM;
    OBJ_CONTROL(chr) = 1;
    scratch->hanging[who] = 1;
    SwitchSet(scratch, true);
    PlaySound(sfx_Switch);
}

void Obj_DHZVines(Object *obj) {
    Scratch_Roots *scratch = (Scratch_Roots *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_DHZVines;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x41E);
        obj->render.b = SPRITE_CAM_FIELD | 0x10; // (bit 4: the height is its own: y_rad)
        obj->render.f.explicit_height = true;
        obj->width_pixels = 0x10;
        obj->priority = 4;
        obj->y_rad = (int8_t)0x80;
        scratch->home_y = obj->pos.l.y.f.u;
        if (scratch->subtype & 0x80)
            scratch->linked = 1;
        scratch->step = 2;
        if (scratch->subtype & 0x70) { // it starts hanging down
            scratch->length = 0xB0;
            scratch->extended = 1;
            obj->pos.l.y.f.u += 0xB0;
            obj->frame = 6;
        }
    }

    const bool anyone = scratch->hanging[0] || scratch->hanging[1];
    bool grow = scratch->extended ? !anyone : anyone;
    if (grow) {
        if (scratch->length != 0xB0) {
            scratch->length += scratch->step;
            obj->pos.l.y.f.u = (int16_t)(scratch->home_y + scratch->length);
            obj->frame = (uint8_t)((scratch->length >> 5) + (scratch->length ? 1 : 0));
        }
    } else if (scratch->length != 0) {
        scratch->length -= scratch->step;
        obj->pos.l.y.f.u = (int16_t)(scratch->home_y + scratch->length);
        obj->frame = (uint8_t)(scratch->length ? ((scratch->length >> 5) + 1) : 0);
    }
    FOR_EACH_CHARACTER(chr, who)
        Vines_Character(obj, scratch, chr, who);
    RememberState(obj);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 81: the bridge that swings. Eight links on a chain of its own (a second object that only draws them) swing round the foot, from upright to flat, when the switch (the subtype) goes down; upright it is a wall,
// flat a bridge
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[7];     // 0x29-0x2F
    int16_t foot_x;      // 0x30
    int16_t foot_y;      // 0x32
    int16_t speed;       // 0x34: +-$100: which way its angle goes
    uint8_t opening;     // 0x36: it is swinging
    uint8_t pad1[5];
    uint8_t chain;       // 0x3C: the slot of the chain
} Scratch_Bridge;

enum { BridgeRoutine_Init = 0, BridgeRoutine_Swing = 2, BridgeRoutine_Done = 4 };

// Offset_0x01EF72: where each link is, from the angle: a link every 16 pixels further from the foot (a sixteenth of the sine and cosine, in 16.16)
static void Bridge_Links(Object *obj, const Scratch_Bridge *scratch) {
    if (!scratch->opening)
        return;
    Object *chain = &objects[scratch->chain];
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    const int32_t step_y = (int32_t)sin << 12, step_x = (int32_t)cos << 12;
    int32_t y = step_y, x = step_x;
    for (int i = 0; i < chain->child_count; i++) {
        chain->children[i].y = (int16_t)((y >> 16) + scratch->foot_y);
        chain->children[i].x = (int16_t)((x >> 16) + scratch->foot_x);
        y += step_y;
        x += step_x;
    }
}

void Obj_DHZVerticalBridge(Object *obj) {
    Scratch_Bridge *scratch = (Scratch_Bridge *)&obj->scratch;

    if (obj->render.f.multi_sprite) { // the chain: it is drawn, at a priority of its own
        DisplaySprite(obj);
        return;
    }

    switch (obj->routine) {
    case BridgeRoutine_Init: {
        obj->routine += 2;
        obj->mappings = Mappings_DHZVerticalBridge;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x43C);
        obj->render.b = SPRITE_CAM_FIELD;
        obj->priority = 5;
        obj->width_pixels = 8;
        obj->status.b |= 0x80;
        scratch->foot_x = obj->pos.l.x.f.u;
        scratch->foot_y = obj->pos.l.y.f.u;
        obj->pos.l.y.f.u -= 0x48;
        obj->angle = 0xC0;
        int16_t link_step = -0x10;
        if (obj->status.o.f.y_flip) {
            obj->pos.l.y.f.u += 0x90;
            obj->angle = 0x40;
            link_step = 0x10;
        }
        scratch->speed = obj->status.o.f.x_flip ? -0x100 : 0x100;
        Object *chain = FindNextFreeObj(obj + 1);
        if (chain != NULL) {
            chain->type = obj->type;
            chain->mappings = obj->mappings;
            chain->tile = obj->tile;
            chain->render.b = SPRITE_CAM_FIELD;
            chain->render.f.multi_sprite = true;
            chain->priority = 5;
            chain->width_pixels = 0xFF; // (the links reach far to each side: the object is drawn while its place is near)
            chain->pos.l.x.f.u = scratch->foot_x;
            chain->pos.l.y.f.u = scratch->foot_y;
            chain->child_count = 8;
            int16_t y = scratch->foot_y;
            for (int i = 0; i < 8; i++) {
                y = (int16_t)(y + link_step);
                chain->children[i].x = scratch->foot_x;
                chain->children[i].y = y;
                chain->children[i].frame = 1;
            }
            scratch->chain = (uint8_t)(chain - objects);
        }
        // (falls on into the swing)
    }
        // fall through
    case BridgeRoutine_Swing: {
        if (f_switch[scratch->subtype & 0xF] & 1)
            scratch->opening = 1; // (the alpha's sound for it ($E7) is behind a branch that is always taken: it is never played)
        if (scratch->opening) {
            int16_t d1 = 0x48;
            if (obj->angle != 0) {
                if (obj->angle != 0x80) {
                    obj->angle = (uint8_t)(obj->angle + (scratch->speed >> 8)); // the angle is the high byte of a word that goes on by $100 a frame
                    goto links;
                }
                d1 = -d1;
            }
            obj->pos.l.y.f.u = scratch->foot_y;
            obj->pos.l.x.f.u = (int16_t)(scratch->foot_x + d1);
            obj->width_pixels = 0x40;
            scratch->opening = 0;
            PlaySound(sfx_DHZRoots2);
            obj->routine += 2;
        }
    links:
        Bridge_Links(obj, scratch);
    }
        // fall through
    case BridgeRoutine_Done: {
        int16_t d1 = 0x13, d2 = 0x40, d3 = 0x41;
        const uint8_t angle = obj->angle;
        if (angle == 0 || !(angle == 0x40 || angle >= 0xC0)) {
            d1 = 0x4B;
            d2 = 8;
            d3 = 9;
        }
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, d1, d2, d3, obj->pos.l.x.f.u, NULL);
        if (IS_OFFSCREEN(scratch->foot_x)) {
            ObjectDelete(&objects[scratch->chain]);
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
    }
}
