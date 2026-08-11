#include "AirBubbles.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Sound.h"
#include "Object/Sonic.h"
#include "Object/DrownCount.h"

#include "Resource/Mappings/Bubbles.h"
#include "Resource/Animation/Bubbles.h"

// Non-inhalable bubbles production sequence: 0 = small, 1 = medium
static const uint8_t Bub_BblTypes[] = {0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0};

typedef struct {
    uint8_t subtype;        // 0x28
    uint8_t pad0[5];        // 0x29-0x2D
    uint8_t inhalable;      // bub_inhalable, 0x2E
    uint8_t pad1;           // 0x2F
    int16_t orig_x;         // bub_origX, 0x30
    int8_t time;            // bub_time, 0x32 -- signed so a decrement below 0 is detectable
    uint8_t timebase;       // bub_timebase, 0x33
    uint8_t minicount;      // bub_minicount, 0x34
    uint8_t pad2;           // 0x35
    uint16_t bubbleflag;    // bub_bubbleflag, 0x36 -- 1 = spawning, $4000 = large spawned, $8000 = large allowed
    int16_t randomtime;     // bub_randomtime, 0x38
    uint8_t typelist_start; // bub_typelist, 0x3C -- start index into Bub_BblTypes (0/4/8/12)
} Scratch_Bubble;

static void Bubble_Construct(Object *obj) {
    obj->mappings = Mappings_Bubbles;
    obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_LZ_Bubbles);
    obj->render.f.align_fg = true;
    obj->priority = 1;
    obj->width_pixels = 32 / 2;
}

// Shared wobble effect for a bubble drifting up towards the water surface
// (Bub_ChkWater's .wobble). Returns the new playfield X-position.
static int16_t Bubble_Wobble(int16_t orig_x, uint8_t *angle) {
    uint8_t a = *angle;
    (*angle)++;
    a &= 0x7F;
    return (int16_t)(orig_x + Drown_WobbleData[a]);
}

// Checks if Sonic is within range of a large, inhalable bubble.
static bool Bubble_ChkSonic(Object *obj) {
    if (lock_multi & 0x80)
        return false;

    int16_t x = player->pos.l.x.f.u;
    int16_t left = (int16_t)(obj->pos.l.x.f.u - 16);
    if (left >= x)
        return false;
    if ((int16_t)(left + 32) < x)
        return false;

    int16_t y = player->pos.l.y.f.u;
    int16_t top = obj->pos.l.y.f.u;
    if (top >= y)
        return false;
    if ((int16_t)(top + 16) < y)
        return false;

    return true;
}

void Obj_Bubble(Object *obj) {
    Scratch_Bubble *scratch = (Scratch_Bubble*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Bub_Main
        obj->routine = 2;
        Bubble_Construct(obj);

        if (scratch->subtype & BUBBLE_MAKER_BIT) {
            obj->routine = 0xA;
            scratch->time = scratch->subtype & 0x7F;
            scratch->timebase = scratch->time;
            obj->anim = 6;
            goto bubbleMaker;
        }

        obj->anim = scratch->subtype;
        scratch->orig_x = obj->pos.l.x.f.u;
        obj->ysp = -0x88;
        obj->angle = (uint8_t)RandomNumber();
        // Fallthrough
    case 2: // Bub_Inflate
        // Always falls straight into Bub_ChkWater below every frame it
        // runs, regardless of whether AnimateSprite's afRoutine has
        // advanced obRoutine to 4 yet -- matches the real ASM, which has
        // no branch back after this and just runs both routines in the
        // same tick until the jump table starts dispatching straight to
        // Bub_ChkWater on its own once obRoutine actually reaches 4.
        AnimateSprite(obj, Animation_Bubbles);
        if (obj->frame == 6)
            scratch->inhalable = 1;
        // Fallthrough
    case 4: { // Bub_ChkWater
        if (obj->pos.l.y.f.u <= wtr_pos1) {
            // Reached (or passed) the surface -- burst.
            obj->routine = 6;
            obj->anim += 3;
            goto bursting;
        }

        obj->pos.l.x.f.u = Bubble_Wobble(scratch->orig_x, &obj->angle);

        if (scratch->inhalable) {
            // FixBugs: large bubbles shouldn't be inhalable while debug
            // mode is active (real hardware bug -- debug-spawned objects
            // could still be "collected").
            if (!debug_use && Bubble_ChkSonic(obj)) {
                ResumeMusic();
                PlaySound(sfx_Bubble);

                player->xsp = 0;
                player->ysp = 0;
                player->inertia = 0;
                player->anim = SonAnimId_GetAir;
                ((Scratch_Sonic*)&player->scratch)->control_lock = 35;
                ((Scratch_Sonic*)&player->scratch)->jumping = false;
                player->status.p.f.pushing = false;
                player->status.p.f.roll_jump = false;

                if (player->status.p.f.in_ball) {
                    player->status.p.f.in_ball = false;
                    player->y_rad = SONIC_HEIGHT;
                    player->x_rad = SONIC_WIDTH;
                    player->pos.l.y.f.u -= (SONIC_HEIGHT - SONIC_BALL_HEIGHT);
                }

                obj->routine = 6;
                obj->anim += 3;
                goto bursting;
            }
        }

        SpeedToPos(obj);
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }

    bursting:
    case 6: // Bub_Bursting
        AnimateSprite(obj, Animation_Bubbles);
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;

    case 8: // Bub_BurstDelete
        ObjectDelete(obj);
        break;

    bubbleMaker:
    case 0xA: { // Bub_BubbleMaker
        if (!scratch->bubbleflag) {
            if (obj->pos.l.y.f.u > wtr_pos1 && obj->render.f.on_screen) {
                if (--scratch->randomtime < 0) {
                    scratch->bubbleflag = 1;

                    uint16_t rnd;
                    do {
                        rnd = (uint16_t)RandomNumber();
                    } while ((rnd & 7) >= 6);
                    scratch->minicount = (uint8_t)(rnd & 7);

                    scratch->typelist_start = (uint8_t)(rnd & 0xC);

                    if (--scratch->time < 0) {
                        scratch->time = scratch->timebase;
                        scratch->bubbleflag |= 0x8000;
                    }
                    goto spawnBubble;
                }
                goto bmAnimate;
            }
            goto bmDisplay;
        }

        if (--scratch->randomtime >= 0)
            goto bmAnimate;

    spawnBubble: {
        scratch->randomtime = (int16_t)(RandomNumber() & 0x1F);

        Object *bub = FindFreeObj();
        if (bub != NULL) {
            bub->type = ObjId_Bubble;
            bub->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + (int16_t)((RandomNumber() & 0xF) - 8));
            bub->pos.l.y.f.u = obj->pos.l.y.f.u;

            uint8_t idx = scratch->minicount;
            Scratch_Bubble *bubScratch = (Scratch_Bubble*)&bub->scratch;
            bubScratch->subtype = Bub_BblTypes[scratch->typelist_start + idx];

            if (scratch->bubbleflag & 0x8000) {
                if ((RandomNumber() & 3) == 0) {
                    if (!(scratch->bubbleflag & 0x4000)) {
                        scratch->bubbleflag |= 0x4000;
                        bubScratch->subtype = 2;
                    }
                } else if (scratch->minicount == 0) {
                    if (!(scratch->bubbleflag & 0x4000)) {
                        scratch->bubbleflag |= 0x4000;
                        bubScratch->subtype = 2;
                    }
                }
            }
        }

        if (--scratch->minicount == 0xFF) {
            scratch->randomtime = (int16_t)(0x80 + (RandomNumber() & 0x7F));
            scratch->bubbleflag = 0;
        }
        goto bmAnimate;
    }

    bmAnimate:
        AnimateSprite(obj, Animation_Bubbles);

    bmDisplay:
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        if (obj->pos.l.y.f.u > wtr_pos1)
            DisplaySprite(obj);
        break;
    }
    }
}
