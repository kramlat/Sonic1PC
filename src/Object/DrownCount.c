#include "DrownCount.h"

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
} Scratch_DrownCount;

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
    ((Scratch_DrownCount*)&objects[DROWNCOUNT_SLOT].scratch)->display_time = 0;
}

static void DrownCount_Construct(Object *obj) {
    obj->mappings = Mappings_Bubbles;
    obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_LZ_Bubbles);
    obj->render.f.align_fg = true;
    obj->priority = 1;
    obj->width_pixels = 32 / 2;
}

// Handles a countdown number bubble's screen-fixed display once it's close
// enough to flashing to matter -- switches it from playfield-positioned to
// screen-fixed (HUD-style) so it stays put once Drown_AirLeft takes over.
static void Drown_ShowNumber(Object *obj, Scratch_DrownCount *scratch) {
    if (scratch->num_time == 0)
        return;
    if (--scratch->num_time != 0)
        return;
    if (obj->anim >= 7)
        return; // already flashing

    scratch->num_time = 15;
    obj->ysp = 0;

    int16_t x = (int16_t)(obj->pos.l.x.f.u - scrpos_x.f.u + 0x80);
    int16_t y = (int16_t)(obj->pos.l.y.f.u - scrpos_y.f.u + 0x80);
    obj->render.f.align_fg = false;
    obj->pos.s.x = x;
    obj->pos.s.y = y;

    obj->routine = 0xC; // Drown_AirLeft next
}

void Obj_DrownCount(Object *obj) {
    Scratch_DrownCount *scratch = (Scratch_DrownCount*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Drown_Main
        obj->routine = 2;
        DrownCount_Construct(obj);

        if (scratch->subtype & DROWNCOUNT_MASTER_BIT) {
            obj->routine = 0xA;
            scratch->type = scratch->subtype & 0x7F;
            goto countdown;
        }

        obj->anim = scratch->subtype;
        scratch->orig_x = obj->pos.l.x.f.u;
        obj->ysp = -0x88;
        // Fallthrough
    case 2: // Drown_Animate -- always falls into Drown_ChkWater, see AirBubbles.c
        AnimateSprite(obj, Animation_DrowningCountdown);
        // Fallthrough
    case 4: // Drown_ChkWater
        if (obj->pos.l.y.f.u <= wtr_pos1) {
            // Reached the surface -- switch to the flashing animation.
            obj->routine = 6;
            obj->anim += 7;
#ifdef SCP_FIX_BUGS
            // Fixes an out-of-bounds animation read when a medium bubble
            // (anim 14) reaches the surface: 14+7=21 is past the table's
            // last entry (13, blank) -- clamp back to blank instead.
            if (obj->anim >= 14)
                obj->anim = 13;
#endif
            goto display;
        }

        {
            // TODO: wind tunnels aren't ported yet (LZWaterFeatures.c), so
            // the real .notunnel branch (nudging drown_origX rightwards
            // while Sonic's in one) is skipped for now.
            uint8_t a = obj->angle;
            obj->angle++;
            a &= 0x7F;
            obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + Drown_WobbleData[a]);
        }

        Drown_ShowNumber(obj, scratch);

        SpeedToPos(obj);
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;

    display:
    case 6: // Drown_Display
    case 0xE:
        Drown_ShowNumber(obj, scratch);
        AnimateSprite(obj, Animation_DrowningCountdown);
        DisplaySprite(obj);
        break;

    case 8: // Drown_Delete
    case 0x10:
        ObjectDelete(obj);
        break;

    case 0xC: // Drown_AirLeft
        if (air > 12) {
            ObjectDelete(obj);
            return;
        }

        if (--scratch->num_time != 0) {
            AnimateSprite(obj, Animation_DrowningCountdown);
            if (!obj->render.f.on_screen) {
                ObjectDelete(obj);
                return;
            }
            DisplaySprite(obj);
            break;
        }

        obj->routine = 0xE;
        obj->anim += 7;
        goto display;

    countdown:
    case 0xA: { // Drown_Countdown -- the master tracker, never displays
        if (scratch->restart_time) {
            // Sonic is already drowning -- count down to the death state.
            if (--scratch->restart_time != 0) {
                SpeedToPos(player);
                player->ysp += 0x10; // sink faster
                goto checkSpawnExtraBubbles;
            }
            player->routine = 6; // Sonic_Death
            return;
        }

        if (player->routine >= 6 || !player->status.p.f.underwater)
            return;

        if (--scratch->num_time >= 0)
            goto checkSpawnExtraBubbles;
        scratch->num_time = 59;

        scratch->extra_bub_flag = 1;
        scratch->extra_bubbles = (uint8_t)(RandomNumber() & 1);

        if (air == 25 || air == 20 || air == 15) {
            PlaySound(sfx_Warning);
        } else if (air <= 12) {
            if (air == 12)
                QueueSound1(bgm_Drowning);

            if (--scratch->display_time < 0) {
                scratch->display_time = scratch->type;
                scratch->extra_bub_flag |= 0x80;
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
            scratch->extra_bubbles = 10;
            scratch->extra_bub_flag = 1;
            scratch->restart_time = 2 * 60;

            Sonic_ResetOnFloor(player);
            player->anim = SonAnimId_Drown;
            player->status.p.f.in_air = true;
            player->tile |= TILE_PRIORITY_AND;
            player->ysp = 0;
            player->xsp = 0;
            player->inertia = 0;
            nobgscroll = true;
            player->routine = 2; // Sonic_Control
            time_count = false;
        }

    checkSpawnExtraBubbles:
        if (!scratch->extra_bub_flag)
            return;
        if (scratch->restart_time == 0) {
            if (--scratch->delay_time >= 0)
                return;
        }

    {
        scratch->delay_time = (int16_t)(RandomNumber() & 0xF);

        Object *bub = FindFreeObj();
        if (bub != NULL) {
            bub->type = ObjId_DrownCount;
            Scratch_DrownCount *bubScratch = (Scratch_DrownCount*)&bub->scratch;

            int16_t off = 6;
            if (player->status.p.f.x_flip) {
                off = -6;
                bub->angle = 0x40;
            }
            bub->pos.l.x.f.u = (int16_t)(player->pos.l.x.f.u + off);
            bub->pos.l.y.f.u = player->pos.l.y.f.u;
            bubScratch->subtype = 6; // small bubble

            if (scratch->restart_time) {
                scratch->delay_time &= 7;
                bub->pos.l.y.f.u = (int16_t)(player->pos.l.y.f.u - 12);
                bub->angle = (uint8_t)RandomNumber();
                if ((frame_count & 3) == 0)
                    bubScratch->subtype = 0xE; // medium bubble
            } else if (scratch->extra_bub_flag & 0x80) {
                uint16_t air_anim = air >> 1;
                bool spawnNumber = false;
                if ((RandomNumber() & 3) == 0) {
                    if (!(scratch->extra_bub_flag & 0x40)) {
                        scratch->extra_bub_flag |= 0x40;
                        spawnNumber = true;
                    }
                } else if (!scratch->extra_bubbles) {
                    if (!(scratch->extra_bub_flag & 0x40)) {
                        scratch->extra_bub_flag |= 0x40;
                        spawnNumber = true;
                    }
                }
                if (spawnNumber) {
                    bubScratch->subtype = (uint8_t)air_anim;
                    bubScratch->num_time = 28;
                }
            }
        }

        if (--scratch->extra_bubbles == 0xFF)
            scratch->extra_bub_flag = 0;
        return;
    }
    }
    }
}
