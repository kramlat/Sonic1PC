#include "DrownCount.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Sound.h"
#include "GM_Level.h"
#include "Object/Sonic.h"
#include "Object/AirBubbles.h"

#include "Resource/Animation/DrowningCountdown.h"

typedef struct {
    uint8_t subtype;         // 0x28
    uint8_t pad0[3];         // 0x29-0x2B
    int16_t restart_time;    // drown_restarttime, 0x2C -- time to restart after Sonic drowns
    uint8_t pad1[2];         // 0x2E-0x2F
    int16_t orig_x;          // drown_origX, 0x30
    int8_t display_time;     // drown_displaytime, 0x32 -- time to display each number (signed so a decrement below 0 is detectable)
    uint8_t type;            // drown_type, 0x33 -- bubble type
    uint8_t extra_bubbles;   // drown_extrabubbles, 0x34 -- number of extra bubbles to create
    uint8_t pad2;            // 0x35
    uint16_t extra_bub_flag; // drown_extrabubflag, 0x36
    int16_t num_time;        // drown_numtime, 0x38 -- time between each number change
    int16_t delay_time;      // drown_delaytime, 0x3A -- delay between bubbles
} Own_DrownCount;

void ResumeMusic(void) {
    if (air <= 12) {
        if (invincibility)
            PlayMusic(bgm_Invincible);
        else if (lock_screen)
            PlayMusic(bgm_Boss);
        else
            ResumeLevelMusic();
    }
    air = 30;
    Sst *master = DrownCount_Master();
    if (master != NULL)
        ((Own_DrownCount *)master->own.u8)->display_time = 0;
}

// The tracker that rides along while Sonic is underwater (the one with the master bit)
Sst *DrownCount_Master(void) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_DrownCount && (sst[i].subtype & DROWNCOUNT_MASTER_BIT))
            return &sst[i];
    return NULL;
}

void DrownCount_StartMaster(void) {
    Sst *master = DrownCount_Master();
    if (master == NULL)
        master = Sst_SpawnCode(Obj_DrownCount, 0, 0);
    if (master == NULL)
        return;
    master->routine = 0;
    master->subtype = DROWNCOUNT_MASTER_BIT | 1; // selects the master tracker path
}

static void DrownCount_Construct(Sst *self) {
    self->mappings = Mappings_Bubbles;
    self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_LZ_Bubbles);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 1 * 0x80;
    self->width_pixels = 32 / 2;
}

// Handles a countdown number bubble's screen-fixed display once it's close
// enough to flashing to matter -- switches it from playfield-positioned to
// screen-fixed (HUD-style) so it stays put once Drown_AirLeft takes over.
static void Drown_ShowNumber(Sst *self, Own_DrownCount *own) {
    if (own->num_time == 0)
        return;
    if (--own->num_time != 0)
        return;
    if (self->anim >= 7)
        return; // already flashing

    own->num_time = 15;
    self->y_vel = 0;

    int16_t x = (int16_t)(self->x_pos.f.u - scrpos_x.f.u + 0x80);
    int16_t y = (int16_t)(self->y_pos.f.u - scrpos_y.f.u + 0x80);
    self->render_flags &= (uint8_t)~SST_RENDER_LEVEL;
    self->x_pos.f.u = x;
    self->y_pos.f.u = y;

    self->routine = 0xC; // Drown_AirLeft next
}

void Obj_DrownCount(Sst *self) {
    Own_DrownCount *own = (Own_DrownCount *)self->own.u8;

    switch (self->routine) {
    case 0: // Drown_Main
        self->routine = 2;
        DrownCount_Construct(self);

        if (self->subtype & DROWNCOUNT_MASTER_BIT) {
            self->routine = 0xA;
            own->type = self->subtype & 0x7F;
            goto countdown;
        }

        self->anim = self->subtype;
        own->orig_x = self->x_pos.f.u;
        self->y_vel = -0x88;
        // Fallthrough
    case 2: // Drown_Animate -- always falls into Drown_ChkWater, see AirBubbles.c
        Sst_Animate(self, Animation_DrowningCountdown);
        // Fallthrough
    case 4: // Drown_ChkWater
        if (self->y_pos.f.u <= wtr_pos1) {
            // Reached the surface -- switch to the flashing animation.
            self->routine = 6;
            self->anim += 7;
            goto display;
        }

        {
            if (tunnel_mode)
                own->orig_x += 4; // drift with Sonic while he's in a wind tunnel

            uint8_t a = self->angle;
            self->angle++;
            a &= 0x7F;
            self->x_pos.f.u = (int16_t)(own->orig_x + Drown_WobbleData[a]);
        }

        Drown_ShowNumber(self, own);

        Sst_MoveSprite2(self);
        // Delete gate uses IS_OFFSCREEN (direct position test), not
        // on_screen (cached flag set only AFTER the object is drawn by
        // BuildSprites). On a freshly-spawned bubble on_screen is false,
        // so checking it here would delete the object before it was ever
        // displayed -- and since it was never displayed, on_screen would
        // never become true, making the deletion permanent. Same fix as
        // AirBubbles.c; same reasoning.
        // (Only while it still has a world position: a number that has just become screen-fixed holds its screen position in those
        // bytes, see Drown_AirLeft.)
        if ((self->render_flags & SST_RENDER_LEVEL) && SstOutOfRange(self->x_pos.f.u)) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;

    display:
    case 6: // Drown_Display
    case 0xE:
        // Fixes an out-of-bounds animation read when a medium bubble
        // (anim 14) reaches the surface: 14+7=21 is past the table's
        // last entry (13, blank) -- clamp back to blank instead.
        if (self->anim >= 14)
            self->anim = 13;
        Drown_ShowNumber(self, own);
        Sst_Animate(self, Animation_DrowningCountdown);
        Sst_Draw(self);
        break;

    case 8: // Drown_Delete
    case 0x10:
        Sst_Delete(self);
        break;

    case 0xC: // Drown_AirLeft
        if (air > 12 || air == 0) {
            Sst_Delete(self);
            return;
        }

        if (--own->num_time != 0) {
            Sst_Animate(self, Animation_DrowningCountdown);
            // A number stuck to the screen keeps its screen position in the same bytes that hold a world X, so IS_OFFSCREEN would
            // read the screen Y as an X and delete it at once wherever the camera is not near the left edge. It has been drawn
            // already (this is not a fresh bubble), so the drawn flag is valid here, as in the original.
            if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
                Sst_Delete(self);
                return;
            }
            Sst_Draw(self);
            break;
        }

        self->routine = 0xE;
        self->anim += 7;
        goto display;

    countdown:
    case 0xA: { // Drown_Countdown -- the master tracker, never displays
            if (own->restart_time) {
                // Sonic is already drowning -- count down to the death state.
                if (--own->restart_time != 0) {
                    Sst_MoveSprite2(player);
                    player->y_vel += 0x10; // sink faster
                    goto checkSpawnExtraBubbles;
                }
                player->routine = 6; // Sonic_Death
                return;
            }

            if (player->routine >= 6 || !((player->status & SST_PSTATUS_UNDERWATER) != 0))
                return;

            if (--own->num_time >= 0)
                goto checkSpawnExtraBubbles;
            own->num_time = 59;

            own->extra_bub_flag = 1;
            own->extra_bubbles = (uint8_t)(RandomNumber() & 1);

            if (air == 25 || air == 20 || air == 15) {
                PlaySound(sfx_Warning);
            } else if (air <= 12) {
                if (air == 12)
                    QueueSound1(bgm_Drowning);

                if (--own->display_time < 0) {
                    own->display_time = own->type;
                    own->extra_bub_flag |= 0x80;
                }
            }

            if (--air == 0xFFFF) {
                // Sonic drowns here. Calling ResumeMusic() this way (rather
                // than ResumeLevelMusic() directly) matches the real ASM --
                // air is 0xFFFF (just wrapped) at this point, so ResumeMusic's
                // own "air <= 12" gate is false and it skips the music-restore
                // branch entirely, only resetting air to 30 and clearing the
                // countdown display timer. The actual death jingle comes from
                // Sonic_Death separately once restart_time expires below.
                ResumeMusic();
                lock_multi = 0x81;
                PlaySound(sfx_Drown);
                own->extra_bubbles = 10;
                own->extra_bub_flag = 1;
                own->restart_time = 2 * 60;

                Sonic_ResetOnFloor(player);
                player->anim = SonAnimId_Drown;
                player->status |= SST_PSTATUS_IN_AIR;
                player->art_tile |= TILE_PRIORITY_AND;
                player->y_vel = 0;
                player->x_vel = 0;
                player->ground_vel = 0;
                nobgscroll = true;
                player->routine = 2; // Sonic_Control
                time_count = false;
            }

        checkSpawnExtraBubbles:
            if (!own->extra_bub_flag)
                return;
            if (own->restart_time == 0) {
                if (--own->delay_time >= 0)
                    return;
            }

            {
                own->delay_time = (int16_t)(RandomNumber() & 0xF);

                Sst *bub = Sst_Allocate();
                if (bub != NULL) {
                    bub->code = Obj_DrownCount;
                    Own_DrownCount *bubScratch = (Own_DrownCount *)bub->own.u8;

                    int16_t off = 6;
                    if (((player->status & SST_PSTATUS_X_FLIP) != 0)) {
                        off = -6;
                        bub->angle = 0x40;
                    }
                    bub->x_pos.f.u = (int16_t)(player->x_pos.f.u + off);
                    bub->y_pos.f.u = player->y_pos.f.u;
                    bub->subtype = 6; // small bubble

                    if (own->restart_time) {
                        own->delay_time &= 7;
                        bub->y_pos.f.u = (int16_t)(player->y_pos.f.u - 12);
                        bub->angle = (uint8_t)RandomNumber();
                        if ((frame_count & 3) == 0)
                            bub->subtype = 0xE; // medium bubble
                    } else if (own->extra_bub_flag & 0x80) {
                        uint16_t air_anim = air >> 1;
                        bool spawnNumber = false;
                        if ((RandomNumber() & 3) == 0) {
                            if (!(own->extra_bub_flag & 0x40)) {
                                own->extra_bub_flag |= 0x40;
                                spawnNumber = true;
                            }
                        } else if (!own->extra_bubbles) {
                            if (!(own->extra_bub_flag & 0x40)) {
                                own->extra_bub_flag |= 0x40;
                                spawnNumber = true;
                            }
                        }
                        if (spawnNumber) {
                            bub->subtype = (uint8_t)air_anim;
                            bubScratch->num_time = 28;
                        }
                    }
                }

                if (--own->extra_bubbles == 0xFF)
                    own->extra_bub_flag = 0;
                return;
            }
        }
    }
}
