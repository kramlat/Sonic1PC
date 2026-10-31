// Object 0A of the alpha (Aug 21st 1992): the breathing bubbles of a player and the counting of his air, with the numbers that come out of the bubbles at 5, 4, 3, 2, 1 and 0. It is Sonic 1's object (Sonic 1's
// DrownCount.c) as Sonic 2 has it, with the air in each player's own object so that Tails counts too, mappings for each of them, and the numbers' art brought into VRAM a frame at a time instead of being
// there. See Countdown.h for why it is not in the object table.
#include "Object/Countdown.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "GM_Level.h"
#include "MathUtil.h"
#include "Backend/VDP.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include <string.h>

#include "Resource/Animation/Countdown.h"
#include "Resource/Animation/CountdownWobble.h"
#include "Resource/Mappings/Countdown.h"
#include "Resource/Art/CountdownNumbers.h"

#define COUNTDOWN_TILE 0x55B // the bubbles' art in the alpha's lists
#define MAPPINGS_TAILS 0x22  // where Tails' table of frames begins in the mappings (Sonic's is first)

typedef struct {
    uint8_t subtype;       // 0x28: the bubble it is (or, with the top bit, the counter); a counter's low bits are its pause between numbers
    uint8_t pad0[3];       // 0x29-0x2B
    int16_t drown_timer;   // 0x2C (var 00): frames left of the player's drowning
    uint8_t last_number;   // 0x2E (var 02): the number whose art is in VRAM
    uint8_t pad1;          // 0x2F
    int16_t base_x;        // 0x30 (var 04): where the bubble's wobble is about
    int8_t pause;          // 0x32 (var 06): numbers wait this many ticks
    uint8_t pause_reload;  // 0x33 (var 07)
    int8_t bubbles_left;   // 0x34 (var 08): bubbles still to come out of this breath
    uint8_t pad2;          // 0x35
    uint16_t flags;        // 0x36 (var 0A): any bit: bubbles are coming; $8000 a number's turn, $4000 a number is out
    int16_t timer;         // 0x38 (var 0C): ticks to the next breath (the counter), or to a number becoming fixed on the screen
    int16_t gap;           // 0x3A (var 0E): frames to the next bubble
    uint8_t owner;         // 0x3C (var 10): the slot of the player it counts for
    uint8_t pad3;          // 0x3D
    uint8_t pad4;          // 0x3E
    uint8_t tails;         // 0x3F (var 13): the player is Tails
} Own_Countdown;

const uint8_t *Countdown_BubbleMappings(void) {
    return Mappings_Countdown;
}

const uint8_t *Countdown_WobbleTable(void) {
    return Animation_CountdownWobble;
}

static inline Scratch_Sonic *PlayerScratch(Sst *player) {
    return (Scratch_Sonic *)player->own.u8;
}

void Countdown_ResumeMusic(Sst *player) {
    Scratch_Sonic *p = PlayerScratch(player);
    if (p->air <= 12) {
        // (the alpha plays the S1 music ids $82, $87 and $8C here: the level's, the invincibility's and the boss's)
        if (invincibility)
            PlayMusic(bgm_Invincible);
        else if (lock_screen)
            PlayMusic(bgm_Boss);
        else
            ResumeLevelMusic();
    }
    p->air = 30;
}

void Countdown_Make(uint8_t slot, bool tails) {
    Sst *counter = &sst[slot];
    memset(counter, 0, sizeof(*counter));
    counter->code = sst_object_codes[ObjId_DrownCount];
    Own_Countdown *c = (Own_Countdown *)counter->own.u8;
    c->subtype = COUNTDOWN_MASTER_BIT | 1;
    c->owner = tails ? 1 : 0; // (the character: 0 is Sonic, 1 Tails)
    c->tails = tails;
}

// The art of a number frame (frames 8 to 13 of the mappings) into the window of the player's dust (loaded a frame at a time: Load_Oxygen_Numbers_Dynamic_PLC)
static void LoadNumber(Sst *self, Own_Countdown *c) {
    const uint8_t frame = self->mapping_frame;
    if (frame < 8 || frame >= 14 || frame == c->last_number)
        return;
    c->last_number = frame;
    VDP_SeekVRAM(c->tails ? 0x9180 : 0x9380);
    VDP_WriteVRAM(Art_CountdownNumbers + (size_t)(frame - 8) * 0xC0, 0xC0);
}

// Offset_0x0126A0: a number that has risen long enough stops where it is on the screen and counts its last moments there
static void FixNumber(Sst *self, Own_Countdown *c) {
    if (c->timer == 0)
        return;
    if (--c->timer != 0)
        return;
    if (self->anim >= 7)
        return;
    c->timer = 15;
    self->y_vel = 0;
    const int16_t x = (int16_t)(self->x_pos.f.u - scrpos_x.f.u + 0x80);
    const int16_t y = (int16_t)(self->y_pos.f.u - scrpos_y.f.u + 0x80);
    self->render_flags &= (uint8_t)~SST_RENDER_LEVEL; // (on the screen: x and y are places in it now)
    self->x_pos.f.u = x;
    self->y_pos.f.u = y;
    self->routine = 0xC;
}

static void ShowBubble(Sst *self) {
    Sst_Animate(self, Animation_Countdown);
    Sst_Draw(self);
}

// The counter (routine $A): every second of a player's underwater air it counts one, warns at 25, 20 and 15, plays the drowning music at 12, makes the bubbles (and, from 12 down, the numbers), and drowns him at 0
static void Count(Sst *self, Own_Countdown *c) {
    Sst *chr = c->owner ? SST_PLAYER_2 : player;
    Scratch_Sonic *p = PlayerScratch(chr);
    bool spawn_now = false; // (the alpha's branch past the wait for the next bubble)
    bool check_gap = false;

    if (c->drown_timer) {
        if (--c->drown_timer == 0) {
            chr->routine = 6; // dead
            return;
        }
        Sst_MoveSprite2(chr);
        chr->y_vel += 0x10; // he sinks faster
        check_gap = true;
    } else {
        if (chr->routine >= 6 || !((chr->status & SST_PSTATUS_UNDERWATER) != 0))
            return;
        if (--c->timer >= 0) {
            check_gap = true;
        } else {
            c->timer = 0x3B;
            c->flags = 1;
            c->bubbles_left = (int8_t)(RandomNumber() & 1);

            const uint8_t air = p->air;
            if (air == 25 || air == 20 || air == 15) {
                PlaySound(sfx_Warning);
            } else if (air <= 12) {
                if (air == 12)
                    PlayMusic(mus_DEZ); // (the alpha's $8A: the drowning music of the final game is $9F)
                if (--c->pause < 0) {
                    c->pause = (int8_t)c->pause_reload;
                    c->flags |= 0x8000;
                }
            }

            if (p->air-- != 0) {
                spawn_now = true;
            } else {
                // He drowns
                OBJ_CONTROL(chr) = 0x81;
                PlaySound(sfx_Drown);
                c->bubbles_left = 10;
                c->flags = 1;
                c->drown_timer = 0x78;
                Countdown_ResumeMusic(chr); // (his air was $FF: no music, only the air back to 30)
                if (c->tails)
                    Tails_ResetOnFloor(chr);
                else
                    Sonic_ResetOnFloor(chr);
                chr->anim = SonAnimId_Drown;
                chr->status |= SST_PSTATUS_IN_AIR;
                chr->art_tile |= TILE_PRIORITY_AND;
                chr->y_vel = 0;
                chr->x_vel = 0;
                chr->ground_vel = 0;
                if (!c->tails)
                    nobgscroll = true; // (the alpha stops the scrolling for either player)
                check_gap = true;
            }
        }
    }

    if (check_gap) {
        if (!c->flags)
            return;
        if (--c->gap >= 0)
            return;
    } else if (!spawn_now) {
        return;
    }

    // A bubble comes out of the player
    c->gap = (int16_t)(RandomNumber() & 0xF);
    Sst *bubble = Sst_Allocate();
    if (bubble == NULL)
        return;
    memset(bubble, 0, sizeof(*bubble));
    bubble->code = self->code;
    Own_Countdown *b = (Own_Countdown *)bubble->own.u8;
    b->owner = c->owner;
    b->tails = c->tails; // (the alpha does not hand this on: its Tails' bubbles would use Sonic's mappings and window)
    int16_t side = 6;
    if (((chr->status & SST_PSTATUS_X_FLIP) != 0)) {
        side = -6;
        bubble->angle = 0x40;
    }
    bubble->x_pos.f.u = (int16_t)(chr->x_pos.f.u + side);
    bubble->y_pos.f.u = chr->y_pos.f.u;
    b->subtype = 6; // a small bubble

    if (c->drown_timer) {
        c->gap &= 7;
        bubble->y_pos.f.u = (int16_t)(chr->y_pos.f.u - 12);
        bubble->angle = (uint8_t)RandomNumber();
        if (!(frame_count & 3))
            b->subtype = 0xE; // a medium one
    } else if (c->flags & 0x8000) {
        const uint8_t number = (uint8_t)(p->air >> 1); // (the number for his air: 5 for 11 and 10, ... 0 for 1 and 0)
        bool give = false;
        if ((RandomNumber() & 3) == 0) {
            if (!(c->flags & 0x4000)) {
                c->flags |= 0x4000;
                give = true;
            }
        } else if (!c->bubbles_left) {
            if (!(c->flags & 0x4000)) {
                c->flags |= 0x4000;
                give = true;
            }
        }
        if (give) {
            b->subtype = number;
            b->timer = 0x1C;
        }
    }

    if (--c->bubbles_left < 0)
        c->flags = 0;
}

void Obj_Countdown(Sst *self) {
    Own_Countdown *c = (Own_Countdown *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = c->tails ? Mappings_Countdown + MAPPINGS_TAILS : Mappings_Countdown;
        self->art_tile = TILE_MAP(1, 0, 0, 0, COUNTDOWN_TILE);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 1 * 0x80;
        if (c->subtype & COUNTDOWN_MASTER_BIT) {
            self->routine = 0xA;
            c->pause_reload = c->subtype & 0x7F;
            Count(self, c);
            return;
        }
        self->anim = c->subtype;
        c->base_x = self->x_pos.f.u;
        self->y_vel = -0x88;
        // Fallthrough
    case 2:
        Sst_Animate(self, Animation_Countdown);
        // Fallthrough
    case 4:
        if ((uint16_t)wtr_pos1 >= (uint16_t)self->y_pos.f.u) {
            // Up at the surface: the bubble bursts (its animation 7 on)
            self->routine = 6;
            self->anim += 7;
            if (self->anim > 0xD)
                self->anim = 0xD;
            FixNumber(self, c);
            ShowBubble(self);
            return;
        }
        if (tunnel_mode)
            c->base_x += 4; // (it drifts with a player in a wind tunnel)
        {
            const uint8_t a = self->angle++ & 0x7F;
            self->x_pos.f.u = (int16_t)(c->base_x + (int8_t)Animation_CountdownWobble[a]);
        }
        FixNumber(self, c);
        Sst_MoveSprite2(self);
        if ((self->render_flags & SST_RENDER_LEVEL) && SstOutOfRange(self->x_pos.f.u)) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    case 6:
        FixNumber(self, c);
        ShowBubble(self);
        break;
    case 8:
    case 0x10:
        Sst_Delete(self);
        break;
    case 0xA:
        Count(self, c);
        break;
    case 0xC: { // a number fixed on the screen: its last moments
        Sst *chr = c->owner ? SST_PLAYER_2 : player;
        if (PlayerScratch(chr)->air > 12) {
            Sst_Delete(self);
            return;
        }
        if (--c->timer == 0) {
            self->routine = 0xE;
            self->anim += 7;
            ShowBubble(self);
            return;
        }
        Sst_Animate(self, Animation_Countdown);
        LoadNumber(self, c);
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
    case 0xE: {
        Sst *chr = c->owner ? SST_PLAYER_2 : player;
        if (PlayerScratch(chr)->air > 12) {
            Sst_Delete(self);
            return;
        }
        FixNumber(self, c);
        ShowBubble(self);
        break;
    }
    }
}
