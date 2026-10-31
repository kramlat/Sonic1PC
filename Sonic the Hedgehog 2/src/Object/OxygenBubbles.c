// Object 24 of the alpha (Aug 21st 1992): the vents of air bubbles. A vent (a subtype with its top bit: the rest is how many breaths pass between the big bubbles) waits until it is under the water and on the screen, and
// then every now and then lets out a run of bubbles of the sizes of its table, the last of which can be big; a bubble rises with the wobble of the counting object's, and a big one bursts when a player touches it
// (his air is back to 30, he stops and takes the breath: animation 21).
#include "Object/OxygenBubbles.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/CharControl.h"
#include "Object/Countdown.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include <string.h>

#include "Resource/Animation/OxygenBubbles.h"

#define BUBBLE_TILE 0x55B

typedef struct {
    uint8_t subtype;     // 0x28: a bubble's animation, or (top bit) a vent
    uint8_t pad0[5];     // 0x29-0x2D
    uint8_t big;         // 0x2E (var 02): the bubble is a big one that a player can breathe from
    uint8_t pad1;        // 0x2F
    int16_t base_x;      // 0x30 (var 04): where the wobble is about
    int8_t breaths;      // 0x32 (var 06): breaths left before the next big bubble (a vent)
    uint8_t breaths_reload; // 0x33 (var 07)
    int8_t run_left;     // 0x34 (var 08): bubbles left of this run
    uint8_t pad2;        // 0x35
    uint16_t flags;      // 0x36 (var 0A): any: a run is going; $8000: the next of it may be big; $4000: the big one is out
    int16_t timer;       // 0x38 (var 0C): frames to the next bubble
    uint8_t sizes;       // 0x3A: which row of the sizes' table this run takes (a vent)
} Own_Vent;

// What the bubbles of a run are (Offset_0x014C14: nine words read as bytes, four rows of six sizes)
static const uint8_t run_sizes[18] = { 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0 };

static void Show(Sst *self) {
    Sst_Animate(self, Animation_OxygenBubbles);
}

// A player in a big bubble breathes (Offset_0x014C26/0x014C30: both players are tried)
static void Breathe(Sst *self, Own_Vent *vent, Sst *player) {
    if (OBJ_CONTROL(player) & 0x80)
        return;
    if (player->x_pos.f.u <= self->x_pos.f.u - 0x10 || player->x_pos.f.u > self->x_pos.f.u + 0x10)
        return;
    if (player->y_pos.f.u <= self->y_pos.f.u || player->y_pos.f.u > self->y_pos.f.u + 0x10)
        return;
    Countdown_ResumeMusic(player);
    PlaySound(sfx_Bubble);
    player->x_vel = 0;
    player->y_vel = 0;
    player->ground_vel = 0;
    player->anim = SonAnimId_GetAir;
    ((Scratch_Sonic *)player->own.u8)->control_lock = 0x23;
    ((Scratch_Sonic *)player->own.u8)->jumping = false;
    player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    player->status &= (uint8_t)~SST_PSTATUS_ROLL_JUMP;
    if (((player->status & SST_PSTATUS_IN_BALL) != 0)) {
        player->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
        player->y_radius = SONIC_HEIGHT;
        player->x_radius = SONIC_WIDTH;
        player->y_pos.f.u -= 5;
    }
    if (self->routine != 6) {
        self->routine = 6;
        self->anim += 3; // it bursts
    }
    (void)vent;
}

void Obj_OxygenBubbles(Sst *self) {
    Own_Vent *vent = (Own_Vent *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Countdown_BubbleMappings();
        self->art_tile = TILE_MAP(1, 0, 0, 0, BUBBLE_TILE);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 1 * 0x80;
        if (vent->subtype & 0x80) { // a vent
            self->routine += 8;
            vent->breaths = (int8_t)(vent->subtype & 0x7F);
            vent->breaths_reload = vent->subtype & 0x7F;
            self->anim = 6;
            goto vent;
        }
        self->anim = vent->subtype;
        vent->base_x = self->x_pos.f.u;
        self->y_vel = -0x88;
        self->angle = (uint8_t)RandomNumber();
        // Fallthrough
    case 2:
        Show(self);
        if (self->mapping_frame == 6)
            vent->big = 1;
        // Fallthrough
    case 4:
        if ((uint16_t)wtr_pos1 >= (uint16_t)self->y_pos.f.u) { // up at the surface
            self->routine = 6;
            self->anim += 3;
            goto burst;
        }
        {
            const uint8_t a = self->angle++ & 0x7F;
            self->x_pos.f.u = (int16_t)(vent->base_x + (int8_t)Countdown_WobbleTable()[a]);
        }
        if (vent->big) {
            Breathe(self, vent, player);
            Breathe(self, vent, TAILS_OBJ);
            if (self->routine == 6)
                goto burst;
        }
        Sst_MoveSprite2(self);
        SstOnScreenTest2(self, self->x_pos.f.u);
        return;
    burst:
    case 6:
        Show(self);
        SstOnScreenTest2(self, self->x_pos.f.u);
        return;
    case 8:
        Sst_Delete(self);
        return;
    case 0xA:
    vent:
        if (!vent->flags) {
            // Waiting: under the water and on the screen, and the time is up
            if ((uint16_t)wtr_pos1 >= (uint16_t)self->y_pos.f.u || SstOutOfRange(self->x_pos.f.u))
                break;
            if (--vent->timer >= 0)
                goto animate;
            vent->flags = 1;
            uint8_t pick;
            do {
                pick = (uint8_t)RandomNumber();
            } while ((pick & 7) >= 6);
            vent->run_left = (int8_t)(pick & 7);
            vent->sizes = (uint8_t)(pick & 0xC);
            if (--vent->breaths < 0) {
                vent->breaths = (int8_t)vent->breaths_reload;
                vent->flags |= 0x8000;
            }
        } else if (--vent->timer >= 0) {
            goto animate;
        }
        vent->timer = (int16_t)(RandomNumber() & 0x1F);
        {
            Sst *bubble = Sst_Allocate();
            if (bubble != NULL) {
                memset(bubble, 0, sizeof(*bubble));
                bubble->code = self->code;
                bubble->x_pos.f.u = (int16_t)(self->x_pos.f.u + (int16_t)(RandomNumber() & 0xF) - 8);
                bubble->y_pos.f.u = self->y_pos.f.u;
                Own_Vent *b = (Own_Vent *)bubble->own.u8;
                b->subtype = run_sizes[vent->sizes + vent->run_left];
                if (vent->flags & 0x8000) {
                    if ((RandomNumber() & 3) == 0) {
                        if (!(vent->flags & 0x4000)) {
                            vent->flags |= 0x4000;
                            b->subtype = 2;
                        }
                    } else if (!vent->run_left) {
                        if (!(vent->flags & 0x4000)) {
                            vent->flags |= 0x4000;
                            b->subtype = 2;
                        }
                    }
                }
            }
        }
        if (--vent->run_left < 0) {
            vent->timer = (int16_t)(vent->timer + 0x80 + (int16_t)(RandomNumber() & 0x7F));
            vent->flags = 0;
        }
    animate:
        Show(self);
        break;
    }

    // A vent that has gone far off the screen goes, one that is under the water and on it shows
    if (self->routine == 0xA) {
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        if ((uint16_t)wtr_pos1 < (uint16_t)self->y_pos.f.u)
            Sst_Draw(self);
    }
}
