// Object 24 of the alpha (Aug 21st 1992): the vents of air bubbles. A vent (a subtype with its top bit: the rest is how many breaths pass between the big bubbles) waits until it is under the water and on the screen, and
// then every now and then lets out a run of bubbles of the sizes of its table, the last of which can be big; a bubble rises with the wobble of the counting object's, and a big one bursts when a player touches it
// (his air is back to 30, he stops and takes the breath: animation 21).
#include "Object/OxygenBubbles.h"
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
} Scratch_Vent;

// What the bubbles of a run are (Offset_0x014C14: nine words read as bytes, four rows of six sizes)
static const uint8_t run_sizes[18] = { 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0 };

static void Show(Object *obj) {
    AnimateSprite(obj, Animation_OxygenBubbles);
}

// A player in a big bubble breathes (Offset_0x014C26/0x014C30: both players are tried)
static void Breathe(Object *obj, Scratch_Vent *vent, Object *player) {
    if (OBJ_CONTROL(player) & 0x80)
        return;
    if (player->pos.l.x.f.u <= obj->pos.l.x.f.u - 0x10 || player->pos.l.x.f.u > obj->pos.l.x.f.u + 0x10)
        return;
    if (player->pos.l.y.f.u <= obj->pos.l.y.f.u || player->pos.l.y.f.u > obj->pos.l.y.f.u + 0x10)
        return;
    Countdown_ResumeMusic(player);
    PlaySound(sfx_Bubble);
    player->xsp = 0;
    player->ysp = 0;
    player->inertia = 0;
    player->anim = SonAnimId_GetAir;
    ((Scratch_Sonic *)&player->scratch)->control_lock = 0x23;
    ((Scratch_Sonic *)&player->scratch)->jumping = false;
    player->status.p.f.pushing = false;
    player->status.p.f.roll_jump = false;
    if (player->status.p.f.in_ball) {
        player->status.p.f.in_ball = false;
        player->y_rad = SONIC_HEIGHT;
        player->x_rad = SONIC_WIDTH;
        player->pos.l.y.f.u -= 5;
    }
    if (obj->routine != 6) {
        obj->routine = 6;
        obj->anim += 3; // it bursts
    }
    (void)vent;
}

void Obj_OxygenBubbles(Object *obj) {
    Scratch_Vent *vent = (Scratch_Vent *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Countdown_BubbleMappings();
        obj->tile = TILE_MAP(1, 0, 0, 0, BUBBLE_TILE);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x10;
        obj->priority = 1;
        if (vent->subtype & 0x80) { // a vent
            obj->routine += 8;
            vent->breaths = (int8_t)(vent->subtype & 0x7F);
            vent->breaths_reload = vent->subtype & 0x7F;
            obj->anim = 6;
            goto vent;
        }
        obj->anim = vent->subtype;
        vent->base_x = obj->pos.l.x.f.u;
        obj->ysp = -0x88;
        obj->angle = (uint8_t)RandomNumber();
        // Fallthrough
    case 2:
        Show(obj);
        if (obj->frame == 6)
            vent->big = 1;
        // Fallthrough
    case 4:
        if ((uint16_t)wtr_pos1 >= (uint16_t)obj->pos.l.y.f.u) { // up at the surface
            obj->routine = 6;
            obj->anim += 3;
            goto burst;
        }
        {
            const uint8_t a = obj->angle++ & 0x7F;
            obj->pos.l.x.f.u = (int16_t)(vent->base_x + (int8_t)Countdown_WobbleTable()[a]);
        }
        if (vent->big) {
            Breathe(obj, vent, player);
            Breathe(obj, vent, TAILS_OBJ);
            if (obj->routine == 6)
                goto burst;
        }
        SpeedToPos(obj);
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        return;
    burst:
    case 6:
        Show(obj);
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        return;
    case 8:
        ObjectDelete(obj);
        return;
    case 0xA:
    vent:
        if (!vent->flags) {
            // Waiting: under the water and on the screen, and the time is up
            if ((uint16_t)wtr_pos1 >= (uint16_t)obj->pos.l.y.f.u || IS_OFFSCREEN(obj->pos.l.x.f.u))
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
            Object *bubble = FindFreeObj();
            if (bubble != NULL) {
                memset(bubble, 0, sizeof(*bubble));
                bubble->type = obj->type;
                bubble->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + (int16_t)(RandomNumber() & 0xF) - 8);
                bubble->pos.l.y.f.u = obj->pos.l.y.f.u;
                Scratch_Vent *b = (Scratch_Vent *)&bubble->scratch;
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
        Show(obj);
        break;
    }

    // A vent that has gone far off the screen goes, one that is under the water and on it shows
    if (obj->routine == 0xA) {
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        if ((uint16_t)wtr_pos1 < (uint16_t)obj->pos.l.y.f.u)
            DisplaySprite(obj);
    }
}
