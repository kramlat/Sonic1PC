// Dust Hill's roots and bridge for Sonic 2, as the alpha has them (objects 7F, 80 and 81: the prototype has none of them): the roots that a character hangs from and that are switches for the bridges (7F: a short
// one; 80: a long one that rises and falls, hung from by pressing against it), and the bridge that swings from upright to flat (81) when its switch is down. A character that hangs has his own controls off
// and the animation $14; a jump lets go. All carry Sonic and Tails.
#include "Object/DHZVines.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

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
} Own_Roots;

static void SwitchSet(const Sst *self, bool down) {
    uint8_t *flag = &f_switch[self->subtype & 0xF];
    if (down)
        *flag |= 1;
    else
        *flag &= (uint8_t)~1;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 7F: roots that are switches. A character that comes to them from below (within 12 pixels each side and $28 under) takes hold, and his weight is the switch; a jump lets go (up: and the
// way he is holding, or the fall straight down). The picture is the one with two sides' roots while someone hangs
// ---------------------------------------------------------------------------------------------------------------------------------------
static void Roots_Frame(Sst *self, const Own_Roots *own) {
    self->mapping_frame = (own->hanging[0] || own->hanging[1]) ? 1 : 0;
}

static void Roots_Character(Sst *self, Own_Roots *own, Sst *chr, int who) {
    const Pad pad = PadOf(who);
    if (own->hanging[who]) {
        if (!(pad.pressed & 0x70))
            return;
        OBJ_CONTROL(chr) = 0;
        own->hanging[who] = 0;
        own->cooldown[who] = (pad.held & 0x0F) ? 0x3C : 0x12;
        chr->y_vel = (int16_t)0xFD00;
        SwitchSet(self, false);
        Roots_Frame(self, own);
        return;
    }
    if (own->cooldown[who] != 0 && --own->cooldown[who] != 0)
        return;
    if ((uint16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x0C) >= 0x18)
        return;
    if ((uint16_t)(chr->y_pos.f.u - self->y_pos.f.u - 0x28) >= 0x10)
        return;
    if (chr->routine >= 4)
        return;
    chr->x_vel = 0;
    chr->y_vel = 0;
    chr->ground_vel = 0;
    chr->x_pos.f.u = self->x_pos.f.u;
    chr->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x30);
    chr->anim = VINE_ANIM;
    OBJ_CONTROL(chr) = 1;
    own->hanging[who] = 1;
    SwitchSet(self, true);
    PlaySound(sfx_Switch);
    self->mapping_frame = 1;
}

void Obj_DHZVinesSwitch(Sst *self) {
    Own_Roots *own = (Own_Roots *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_DHZVinesSwitch;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x40E);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 8;
        self->priority = 4 * 0x80;
    }
    FOR_EACH_CHARACTER(chr, who)
        Roots_Character(self, own, chr, who);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 80: roots that hang down and carry. They are as long as the frame says (up to $B0 pixels, in steps of $20) and grow while someone hangs from them (shrink when nobody does; or the other way round,
// with bit 4-6 of the subtype), a character takes hold from below as with 7F, and a jump lets go, to either side if the pad says so; with bit 7 of the subtype they are a switch as well
// ---------------------------------------------------------------------------------------------------------------------------------------
static void Vines_Character(Sst *self, Own_Roots *own, Sst *chr, int who) {
    const Pad pad = PadOf(who);
    if (own->hanging[who]) {
        if (!(pad.pressed & 0x70)) { // hangs on: he goes with the roots
            chr->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x94);
            return;
        }
        OBJ_CONTROL(chr) = 0;
        own->hanging[who] = 0;
        own->cooldown[who] = (pad.held & 0x0F) ? 0x3C : 0x12;
        if (pad.held & JPAD_LEFT)
            chr->x_vel = -0x200;
        if (pad.held & JPAD_RIGHT)
            chr->x_vel = 0x200;
        chr->y_vel = (int16_t)0xFC80;
        chr->status |= SST_PSTATUS_IN_AIR;
        if (own->linked)
            SwitchSet(self, false);
        return;
    }
    if (own->cooldown[who] != 0 && --own->cooldown[who] != 0)
        return;
    if ((uint16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x10) >= 0x20)
        return;
    if ((uint16_t)(chr->y_pos.f.u - self->y_pos.f.u - 0x88) >= 0x18)
        return;
    if (chr->routine >= 4)
        return;
    chr->x_vel = 0;
    chr->y_vel = 0;
    chr->ground_vel = 0;
    chr->x_pos.f.u = self->x_pos.f.u;
    chr->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x94);
    chr->anim = VINE_ANIM;
    OBJ_CONTROL(chr) = 1;
    own->hanging[who] = 1;
    SwitchSet(self, true);
    PlaySound(sfx_Switch);
}

void Obj_DHZVines(Sst *self) {
    Own_Roots *own = (Own_Roots *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_DHZVines;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x41E);
        self->render_flags = SPRITE_CAM_FIELD | 0x10; // (bit 4: the height is its own: y_rad)
        self->height_pixels = self->y_radius;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        self->y_radius = (int8_t)0x80;
        own->home_y = self->y_pos.f.u;
        if (self->subtype & 0x80)
            own->linked = 1;
        own->step = 2;
        if (self->subtype & 0x70) { // it starts hanging down
            own->length = 0xB0;
            own->extended = 1;
            self->y_pos.f.u += 0xB0;
            self->mapping_frame = 6;
        }
    }

    const bool anyone = own->hanging[0] || own->hanging[1];
    bool grow = own->extended ? !anyone : anyone;
    if (grow) {
        if (own->length != 0xB0) {
            own->length += own->step;
            self->y_pos.f.u = (int16_t)(own->home_y + own->length);
            self->mapping_frame = (uint8_t)((own->length >> 5) + (own->length ? 1 : 0));
        }
    } else if (own->length != 0) {
        own->length -= own->step;
        self->y_pos.f.u = (int16_t)(own->home_y + own->length);
        self->mapping_frame = (uint8_t)(own->length ? ((own->length >> 5) + 1) : 0);
    }
    FOR_EACH_CHARACTER(chr, who)
        Vines_Character(self, own, chr, who);
    SstOnScreenTest(self);
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
} Own_Bridge;

enum { BridgeRoutine_Init = 0, BridgeRoutine_Swing = 2, BridgeRoutine_Done = 4 };

// Offset_0x01EF72: where each link is, from the angle: a link every 16 pixels further from the foot (a sixteenth of the sine and cosine, in 16.16)
static void Bridge_Links(Sst *self, const Own_Bridge *own) {
    if (!own->opening)
        return;
    Sst *chain = &sst[own->chain];
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    const int32_t step_y = (int32_t)sin << 12, step_x = (int32_t)cos << 12;
    int32_t y = step_y, x = step_x;
    for (int i = 0; i < chain->child_count; i++) {
        chain->children[i].y = (int16_t)((y >> 16) + own->foot_y);
        chain->children[i].x = (int16_t)((x >> 16) + own->foot_x);
        y += step_y;
        x += step_x;
    }
}

void Obj_DHZVerticalBridge(Sst *self) {
    Own_Bridge *own = (Own_Bridge *)self->own.u8;

    if ((self->render_flags & SST_RENDER_MULTI_DRAW)) { // the chain: it is drawn, at a priority of its own
        Sst_Draw(self);
        return;
    }

    switch (self->routine) {
    case BridgeRoutine_Init: {
        self->routine += 2;
        self->mappings = Mappings_DHZVerticalBridge;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x43C);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 8;
        self->status |= 0x80;
        own->foot_x = self->x_pos.f.u;
        own->foot_y = self->y_pos.f.u;
        self->y_pos.f.u -= 0x48;
        self->angle = 0xC0;
        int16_t link_step = -0x10;
        if (((self->status & SST_STATUS_Y_FLIP) != 0)) {
            self->y_pos.f.u += 0x90;
            self->angle = 0x40;
            link_step = 0x10;
        }
        own->speed = ((self->status & SST_STATUS_X_FLIP) != 0) ? -0x100 : 0x100;
        Sst *chain = Sst_AllocateAfter(self);
        if (chain != NULL) {
            chain->code = self->code;
            chain->mappings = self->mappings;
            chain->art_tile = self->art_tile;
            chain->render_flags = SST_RENDER_LEVEL;
            chain->render_flags |= SST_RENDER_MULTI_DRAW;
            chain->priority = 5 * 0x80;
            chain->width_pixels = 0xFF; // (the links reach far to each side: the object is drawn while its place is near)
            chain->x_pos.f.u = own->foot_x;
            chain->y_pos.f.u = own->foot_y;
            chain->child_count = 8;
            int16_t y = own->foot_y;
            for (int i = 0; i < 8; i++) {
                y = (int16_t)(y + link_step);
                chain->children[i].x = own->foot_x;
                chain->children[i].y = y;
                chain->children[i].frame = 1;
            }
            own->chain = (uint8_t)(chain - sst);
        }
        // (falls on into the swing)
    }
        // fall through
    case BridgeRoutine_Swing: {
        if (f_switch[self->subtype & 0xF] & 1)
            own->opening = 1; // (the alpha's sound for it ($E7) is behind a branch that is always taken: it is never played)
        if (own->opening) {
            int16_t d1 = 0x48;
            if (self->angle != 0) {
                if (self->angle != 0x80) {
                    self->angle = (uint8_t)(self->angle + (own->speed >> 8)); // the angle is the high byte of a word that goes on by $100 a frame
                    goto links;
                }
                d1 = -d1;
            }
            self->y_pos.f.u = own->foot_y;
            self->x_pos.f.u = (int16_t)(own->foot_x + d1);
            self->width_pixels = 0x40;
            own->opening = 0;
            PlaySound(sfx_DHZRoots2);
            self->routine += 2;
        }
    links:
        Bridge_Links(self, own);
    }
        // fall through
    case BridgeRoutine_Done: {
        int16_t d1 = 0x13, d2 = 0x40, d3 = 0x41;
        const uint8_t angle = self->angle;
        if (angle == 0 || !(angle == 0x40 || angle >= 0xC0)) {
            d1 = 0x4B;
            d2 = 8;
            d3 = 9;
        }
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, d1, d2, d3, self->x_pos.f.u, NULL);
        if (SstOutOfRange(own->foot_x)) {
            Sst_Delete(&sst[own->chain]);
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
    }
}
