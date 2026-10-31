#include "AirBubbles.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

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
} Own_Bubble;

static void Bubble_Construct(Sst *self) {
    self->mappings = Mappings_Bubbles;
    self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_LZ_Bubbles);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 1 * 0x80;
    self->width_pixels = 32 / 2;
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
static bool Bubble_ChkSonic(Sst *self) {
    if (lock_multi & 0x80)
        return false;

    int16_t x = player->x_pos.f.u;
    int16_t left = (int16_t)(self->x_pos.f.u - 16);
    if (left >= x)
        return false;
    if ((int16_t)(left + 32) < x)
        return false;

    int16_t y = player->y_pos.f.u;
    int16_t top = self->y_pos.f.u;
    if (top >= y)
        return false;
    if ((int16_t)(top + 16) < y)
        return false;

    return true;
}

void Obj_Bubble(Sst *self) {
    Own_Bubble *own = (Own_Bubble *)self->own.u8;

    switch (self->routine) {
        case 0: // Bub_Main
            self->routine = 2;
            Bubble_Construct(self);

            if (self->subtype & BUBBLE_MAKER_BIT) {
                self->routine = 0xA;
                own->time = self->subtype & 0x7F;
                own->timebase = own->time;
                self->anim = 6;
                goto bubbleMaker;
            }

            self->anim = self->subtype;
            own->orig_x = self->x_pos.f.u;
            self->y_vel = -0x88;
            self->angle = (uint8_t)RandomNumber();
            // Fallthrough
        case 2: // Bub_Inflate
            // Always falls straight into Bub_ChkWater below every frame it
            // runs, regardless of whether AnimateSprite's afRoutine has
            // advanced obRoutine to 4 yet -- matches the real ASM, which has
            // no branch back after this and just runs both routines in the
            // same tick until the jump table starts dispatching straight to
            // Bub_ChkWater on its own once obRoutine actually reaches 4.
            Sst_Animate(self, Animation_Bubbles);
            if (self->mapping_frame == 6)
                own->inhalable = 1;
            // Fallthrough
        case 4: { // Bub_ChkWater
            if (self->y_pos.f.u <= wtr_pos1) {
                // Reached (or passed) the surface -- burst.
                self->routine = 6;
                self->anim += 3;
                goto bursting;
            }

            self->x_pos.f.u = Bubble_Wobble(own->orig_x, &self->angle);

            if (own->inhalable) {
                // FixBugs: large bubbles shouldn't be inhalable while debug
                // mode is active (real hardware bug -- debug-spawned objects
                // could still be "collected").
                if (!debug_use && Bubble_ChkSonic(self)) {
                    ResumeMusic();
                    PlaySound(sfx_Bubble);

                    player->x_vel = 0;
                    player->y_vel = 0;
                    player->ground_vel = 0;
                    player->anim = SonAnimId_GetAir;
                    ((Scratch_Sonic *)player->own.u8)->control_lock = 35;
                    ((Scratch_Sonic *)player->own.u8)->jumping = false;
                    player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    player->status &= (uint8_t)~SST_PSTATUS_ROLL_JUMP;

                    if (((player->status & SST_PSTATUS_IN_BALL) != 0)) {
                        player->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
                        player->y_radius = SONIC_HEIGHT;
                        player->x_radius = SONIC_WIDTH;
                        player->y_pos.f.u -= (SONIC_HEIGHT - SONIC_BALL_HEIGHT);
                    }

                    self->routine = 6;
                    self->anim += 3;
                    goto bursting;
                }
            }

            Sst_MoveSprite2(self);
            // Delete gate uses IS_OFFSCREEN (a direct position test) rather
            // than on_screen (a cached flag set by BuildSprites only AFTER an
            // object is drawn). On a freshly-spawned bubble, on_screen is
            // false -- checking it here would delete the bubble before it
            // was ever displayed, and since it was never displayed,
            // on_screen would never become true, making the deletion
            // permanent. IS_OFFSCREEN is valid on the object's very first
            // frame, which is exactly what the delete gate needs. Matches
            // Spikes.c's own use of IS_OFFSCREEN for its delete gate.
            SstOnScreenTest2(self, self->x_pos.f.u);
            break;
        }

        bursting:
        case 6: // Bub_Bursting
            Sst_Animate(self, Animation_Bubbles);
            // Same on_screen -> IS_OFFSCREEN fix as case 4 above.
            SstOnScreenTest2(self, self->x_pos.f.u);
            break;

        case 8: // Bub_BurstDelete
            Sst_Delete(self);
            break;

        bubbleMaker:
        case 0xA: { // Bub_BubbleMaker
            if (!own->bubbleflag) {
                if (self->y_pos.f.u > wtr_pos1 && (self->render_flags & SST_RENDER_ON_SCREEN)) {
                    if (--own->randomtime < 0) {
                        own->bubbleflag = 1;

                        uint16_t rnd;
                        do {
                            rnd = (uint16_t)RandomNumber();
                        } while ((rnd & 7) >= 6);
                        own->minicount = (uint8_t)(rnd & 7);

                        own->typelist_start = (uint8_t)(rnd & 0xC);

                        if (--own->time < 0) {
                            own->time = own->timebase;
                            own->bubbleflag |= 0x8000;
                        }
                        goto spawnBubble;
                    }
                    goto bmAnimate;
                }
                goto bmDisplay;
            }

            if (--own->randomtime >= 0)
                goto bmAnimate;

        spawnBubble: {
            own->randomtime = (int16_t)(RandomNumber() & 0x1F);

            Sst *bub = Sst_Allocate();
            if (bub != NULL) {
                bub->code = Obj_Bubble;
                bub->x_pos.f.u = (int16_t)(self->x_pos.f.u + (int16_t)((RandomNumber() & 0xF) - 8));
                bub->y_pos.f.u = self->y_pos.f.u;

                uint8_t idx = own->minicount;
                Own_Bubble *bubScratch = (Own_Bubble *)bub->own.u8;
                bubScratch->subtype = Bub_BblTypes[own->typelist_start + idx];

                if (own->bubbleflag & 0x8000) {
                    if ((RandomNumber() & 3) == 0) {
                        if (!(own->bubbleflag & 0x4000)) {
                            own->bubbleflag |= 0x4000;
                            bubScratch->subtype = 2;
                        }
                    } else if (own->minicount == 0) {
                        if (!(own->bubbleflag & 0x4000)) {
                            own->bubbleflag |= 0x4000;
                            bubScratch->subtype = 2;
                        }
                    }
                }
            }

            if (--own->minicount == 0xFF) {
                own->randomtime = (int16_t)(0x80 + (RandomNumber() & 0x7F));
                own->bubbleflag = 0;
            }
            goto bmAnimate;
        }

        bmAnimate:
            Sst_Animate(self, Animation_Bubbles);

        bmDisplay:
            // Same on_screen -> IS_OFFSCREEN fix as case 4/6 above.
            if (SstOutOfRange(self->x_pos.f.u)) {
                SstLoad_Forget(self);
                Sst_Delete(self);
                return;
            }
            if (self->y_pos.f.u > wtr_pos1)
                Sst_Draw(self);
            break;
        }
    }
}
