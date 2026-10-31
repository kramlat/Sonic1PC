// Tails (Nick Arcade's objects 02 and 05), made as Sonic Team made him: a copy of Sonic's object (Sonic.c) with the changes Tails needs, and the same copy's quirks. His control is pad 2,
// which a stub of a "CPU" fills with Sonic's own pad from 16 frames ago; he has no water code, no debug mode and no flight yet; hurting him loses SONIC'S rings (the copy of the hurt
// code works on the shared ring count); he does not die from enemies, only from falling below the level, and then he comes back above Sonic.
#include "Standing.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/Tails.h"
#include "Object/DustSplash.h"
#include "Object/Countdown.h"
#include "SplitScreen.h"
#include "Backend/VDP.h"
#include "Object/BossCommon.h"
#include "Object/HTZBoss.h"
#include "Object/Sonic.h"
#include "Constants.h"

#include "Game.h"
#include "GM_Level.h"
#include "Level.h"
#include "Object/CharControl.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "PLC.h"
#include "SpecialStage.h"
#include "Sound.h"

#include <string.h>

#include "Resource/Mappings/Tails.h"
#include "Resource/Art/Tails.h"
#include "Resource/Mappings/TailsDPLC.h"
#include "Object/Ring.h"
#include "Resource/Animation/Tails.h"
#include "Resource/Animation/TailsTails.h"

// Tails' copy of Sonic's code writes the camera's delay and look-up/down shift (cam_x_delay, cam_y_delay, look_shift): Nick Arcade's does too (its spin dash sets the scroll delay of Sonic's camera),
// and 16 frames after Sonic's own spin dash release (Tails copies his pad) the camera would stop following him and then zip to catch up. Here they are Tails' own: the camera follows Sonic only.
static uint16_t tails_cam_x_delay;
static uint8_t tails_cam_y_delay;
static int16_t tails_look_shift;
#define cam_x_delay tails_cam_x_delay
#define cam_y_delay tails_cam_y_delay
#define look_shift  tails_look_shift

#define TAILS_HEIGHT       0xF
#define TAILS_WIDTH        9
#define TAILS_BALL_HEIGHT  0xE
#define TAILS_BALL_WIDTH   7
#define TAILS_BALL_SHIFT   5

#define ArtTile_Tails      0x7A0
#define ArtTile_TailsTails 0x7B0
#define SST_TAILS_TAILS   (&sst[2]) // his tails: the reserved slot of the status table (Reserved_object_3)
// His animations, as the alpha's table has them (ids 0-8 are Sonic 1's names; its 9 is the spin dash, where the port's Sonic has had 31, and its entry 31 the script of his fastest running)
#define TAILS_ANIM_SPINDASH 9
#define TAILS_ANIM_PEELOUT  31

// Spin Dash state
static uint8_t spindash_flag;
// Tails' own top speed, acceleration and deceleration (the alpha's Miles_Max_Speed and the others at $FFFFFEC0): Sonic's are his own, so the water and the shoes of the one do not slow the other
static int16_t tails_speed_max = 0x600, tails_speed_acc = 0xC, tails_speed_dec = 0x80;
static uint16_t spindash_count;

// Sonic's pad for the last 64 frames (Sonic_Stat_Record_Buf), and how long a real pad 2 has had Tails (Tails_control_counter)
static uint8_t pad_hold[0x40], pad_press[0x40];
static uint8_t pad_head;
static uint16_t control_counter;

// The last frame loaded of each of Tails' art windows
static uint8_t last_frame_tails = 0xFF, last_frame_tail = 0xFF;

static signed int HurtTails(Sst *self, int16_t src_x, bool spikes);

// General Sonic state stuff
static inline uint8_t scratch_air(const Sst *self) {
    return ((const Scratch_Sonic *)self->own.u8)->air;
}

// Water (the alpha's Miles_Water): Tails slows in it as Sonic does, with the speeds of his own, the water splashes where he goes in and out, and his air is counted (Countdown.c) so that he drowns too.
static void Tails_Water(Sst *self) {
    if (!Level_HasWater())
        return;

    if (self->y_pos.f.u < wtr_pos1) {
        // Above water
        if (!((self->status & SST_PSTATUS_UNDERWATER) != 0))
            return;
        self->status &= (uint8_t)~SST_PSTATUS_UNDERWATER;
        Countdown_ResumeMusic(self);

        tails_speed_max = 0x600;
        tails_speed_acc = 0xC;
        tails_speed_dec = 0x80;

        if (self->routine != 4)
            self->y_vel = (int16_t)(self->y_vel << 1); // double Y-speed while leaving the water (not when he is hurt)
        if (self->y_vel == 0)
            return;
        DustSplash_Show(&sst[DUST_TAILS_SLOT], DUST_SPLASH);
        if (self->y_vel < -0x1000)
            self->y_vel = -0x1000;
        PlaySound(sfx_Splash);
    } else {
        // Underwater
        if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
            return;
        self->status |= SST_PSTATUS_UNDERWATER;
        Countdown_ResumeMusic(self);
        Countdown_Make(COUNTDOWN_TAILS_SLOT, true); // the counting of his air

        tails_speed_max = 0x300;
        tails_speed_acc = 0x6;
        tails_speed_dec = 0x40;

        self->x_vel >>= 1;
        self->y_vel >>= 2;
        if (self->y_vel == 0)
            return;
        DustSplash_Show(&sst[DUST_TAILS_SLOT], DUST_SPLASH);
        PlaySound(sfx_Splash);
    }
}

static void Tails_Display(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Handle invulnerability blinking
    uint16_t blink;
    if ((blink = scratch->flash_time)) {
        scratch->flash_time--;
        if (blink & 4)
            Sst_Draw(self);
    } else {
        Sst_Draw(self);
    }

    // Handle invincibility timer
    if (scratch->invincibility_time) {
        if (--scratch->invincibility_time == 0) {
            // Restore music
            if (!lock_screen) {
                ResumeLevelMusic();
            }

            // Clear flag
            invincibility = false;
        }
    }

    // Handle speed shoes timer
    if (scratch->shoes_time) {
        if (--scratch->shoes_time == 0) {
            // Restore Sonic's speed
            tails_speed_max = 0x600; // BUG: Water isn't checked
            tails_speed_acc = 0xC;
            tails_speed_dec = 0x80;

            // Clear flag and restore music
            shoes = false;
            SlowDownMusic();
        }
    }
}

// Tails' art: the frame's tiles from his art to its place in VRAM (the DPLC: word count, then a word per run: tiles-1 in the top nibble, first tile in the rest)
static void Tails_LoadDPLC(uint8_t frame, uint8_t *last, uint16_t tile) {
    if (frame == *last)
        return;
    *last = frame;

    const uint8_t *dplc = Mappings_TailsDPLC;
    dplc += (dplc[frame << 1] << 8) | dplc[(frame << 1) + 1];
    int16_t entries = (int16_t)((dplc[0] << 8) | dplc[1]);
    dplc += 2;

    size_t vram = (size_t)tile * 0x20;
    while (entries-- > 0) {
        uint16_t run = (uint16_t)((dplc[0] << 8) | dplc[1]);
        dplc += 2;
        size_t tiles = (size_t)((run >> 12) & 0xF) + 1;
        VDP_SeekVRAM(vram);
        VDP_WriteVRAM(Art_Tails + (size_t)(run & 0xFFF) * 0x20, tiles * 0x20);
        vram += tiles * 0x20;
    }
}

static void Tails_LoadGfx(Sst *self) {
    Tails_LoadDPLC(self->mapping_frame, &last_frame_tails, ArtTile_Tails);
}

// Tails_Control and TailsCPU_Control: a pad 2 that is being used keeps Tails for 300 frames; otherwise he copies Sonic's pad from 16 frames back (the CPU's other states
// only pass on to this one)
static void Tails_Control(void) {
    // (The prototype's TailsCPU_Control runs in the split screen too, with no check of the mode: while pad 2 is idle Tails copies Sonic's pad, and the second player's pad takes him over for 5 seconds)
    // (Sonic_RecordPos: Sonic's pad this frame)
    pad_head = (uint8_t)((pad_head + 1) & 0x3F);
    pad_hold[pad_head] = jpad1_hold1;
    pad_press[pad_head] = jpad1_press1;

    if (jpad2_hold & 0x7F) {
        control_counter = 0x12C;
        return;
    }
    if (control_counter) {
        control_counter--;
        return;
    }
    uint8_t at = (uint8_t)((pad_head - 16) & 0x3F);
    jpad2_hold = pad_hold[at];
    jpad2_press = pad_press[at];
}

// The animation of Tails and of his tails. Tails' (Tails_Animate): like Sonic's, but his walking sets are 8 frames apart and his running ones 6, and he tumbles from frame $75 (a spring flips him); the command
// $FC (only the tails' animation uses it) turns the tails with the direction Tails moves in.
static void Tails_AnimateReadFrame(Sst *self, const uint8_t *anim_script) {
    uint8_t cmd = anim_script[1 + self->anim_frame];
    if (cmd < 0xF0) {
    Anim_Next:
        self->mapping_frame = cmd;
        self->anim_frame++;
    } else {
        if (++cmd == 0) { // restart
            self->anim_frame = 0;
            cmd = anim_script[1];
            goto Anim_Next;
        }
        if (++cmd == 0) { // go back (next byte) frames
            self->anim_frame -= anim_script[2 + self->anim_frame];
            cmd = anim_script[1 + self->anim_frame];
            goto Anim_Next;
        }
        if (++cmd == 0) // change animation
            self->anim = anim_script[2 + self->anim_frame];
    }
}

static void Tails_AnimateTable(Sst *self, const uint8_t *table, const Sst *tails) {
    uint8_t anim = self->anim;
    if (anim != self->prev_anim) {
        self->prev_anim = anim;
        self->anim_frame = 0;
        self->anim_frame_timer = 0;
    }
#define TAILS_SCRIPT(id) (table + ((table[(id) << 1] << 8) | table[((id) << 1) + 1]))
    const uint8_t *script = TAILS_SCRIPT(anim);

    int8_t wait = (int8_t)script[0];
    if (wait >= 0) { // a regular animation
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_PSTATUS_X_FLIP) != 0));
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);
        if (--self->anim_frame_timer >= 0)
            return;
        self->anim_frame_timer = wait;
        Tails_AnimateReadFrame(self, script);
        return;
    }

    if (--self->anim_frame_timer >= 0)
        return;
    uint16_t abs_spd = (self->ground_vel < 0) ? -self->ground_vel : self->ground_vel;
    if (++wait == 0) { // $FF: walking or running
        uint8_t angle = self->angle;
        if (angle != 0 && !(angle & 0x80))
            angle--; // (the alpha: a slope that rises counts one step less)
        uint8_t flip = ((self->status & SST_PSTATUS_X_FLIP) != 0);
        if (!flip)
            angle ^= ~0;
        if ((angle += 0x10) & 0x80)
            flip ^= 3;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, (flip & 1) != 0);
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, (flip & 2) != 0);
        // Tumbling (flung by a spring): the frame comes from the flip angle alone
        uint8_t fa = ((Scratch_Sonic *)self->own.u8)->flip_angle;
        if (fa) {
            if (!((self->status & SST_PSTATUS_X_FLIP) != 0)) {
                SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
                SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);
                self->mapping_frame = (uint8_t)(((uint8_t)(fa + 0xB)) / 0x16 + 0x75);
            } else {
                SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
                SST_RENDER_SET(self, SST_RENDER_Y_FLIP, true);
                self->mapping_frame = (uint8_t)(((uint8_t)((uint8_t)-fa + 0x8F)) / 0x16 + 0x75);
            }
            self->anim_frame_timer = 0;
            return;
        }
        if (((self->status & SST_PSTATUS_PUSHING) != 0))
            goto Anim_Pushing;
        angle = (angle >> 4) & 6;
        const uint8_t *s = TAILS_SCRIPT(SonAnimId_Walk);
        uint8_t offset = (uint8_t)(angle << 2);
        if (abs_spd >= 0x600) {
            s = TAILS_SCRIPT(SonAnimId_Run);
            offset = (uint8_t)((angle + (angle >> 1)) << 1);
            if (abs_spd >= 0x700)
                s = TAILS_SCRIPT(TAILS_ANIM_PEELOUT); // (the alpha: a third set for his fastest running)
        }
        int16_t spd = 0x800 - abs_spd;
        if (spd < 0)
            spd = 0;
        self->anim_frame_timer = spd >> 8;
        Tails_AnimateReadFrame(self, s);
        self->mapping_frame += offset;
    } else if (++wait == 0) { // $FE: rolling
        const uint8_t *s = TAILS_SCRIPT(SonAnimId_Roll2);
        if (abs_spd < 0x600)
            s = TAILS_SCRIPT(SonAnimId_Roll);
        int16_t spd = 0x400 - abs_spd;
        if (spd < 0)
            spd = 0;
        self->anim_frame_timer = spd >> 8;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_PSTATUS_X_FLIP) != 0));
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);
        Tails_AnimateReadFrame(self, s);
    } else if (++wait == 0) { // $FD: pushing
    Anim_Pushing:;
        int16_t spd = 0x800 - abs_spd;
        if (spd < 0)
            spd = 0;
        self->anim_frame_timer = spd >> 6;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_PSTATUS_X_FLIP) != 0));
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);
        Tails_AnimateReadFrame(self, TAILS_SCRIPT(SonAnimId_Push));
    } else { // $FC: the tails, turned the way Tails moves
        uint8_t angle = CalcAngle(tails->x_vel, tails->y_vel);
        uint8_t flip = 0;
        if (!((self->status & SST_PSTATUS_X_FLIP) != 0))
            angle = (uint8_t)~angle;
        else
            angle = (uint8_t)(angle + 0x80);
        angle = (uint8_t)(angle + 0x10);
        if (angle & 0x80)
            flip = 3;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((((self->status & SST_PSTATUS_X_FLIP) != 0) ^ flip) & 1) != 0);
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, ((((self->status & SST_PSTATUS_X_FLIP) != 0) ^ flip) & 2) != 0);
        uint8_t offset = (uint8_t)((angle >> 3) & 0xC);
        self->anim_frame_timer = 3;
        Tails_AnimateReadFrame(self, script);
        self->mapping_frame += offset;
    }
#undef TAILS_SCRIPT
}

static void Tails_Animate(Sst *self) {
    Tails_AnimateTable(self, Animation_Tails, self);
}

// Falling below the level (the only way he dies): Tails_GameOver brings him back above Sonic
void KillTails(Sst *self) {
    self->routine = 6;
    Tails_ResetOnFloor(self);
    self->status |= SST_PSTATUS_IN_AIR;
    self->y_vel = -0x700;
    self->x_vel = 0;
    self->ground_vel = 0;
    self->anim = SonAnimId_Death;
    self->art_tile |= TILE_PRIORITY_AND;
    PlaySound(sfx_Death);
}

// Sonic collision functions
void Tails_ResetOnFloor(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Set state
    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    self->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    self->status &= (uint8_t)~SST_PSTATUS_ROLL_JUMP;

    if (((self->status & SST_PSTATUS_IN_BALL) != 0)) {
        self->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
        self->y_radius = TAILS_HEIGHT;
        self->x_radius = TAILS_WIDTH;
        self->anim = SonAnimId_Walk;
        self->y_pos.f.u -= 1; // (one, not Sonic's five)
    }

    scratch->flip_angle = 0;
    scratch->jumping = false;
    item_bonus = 0;
    scratch->flips_remaining = 0;
    if (self->anim == 0x14)
        self->anim = SonAnimId_Walk; // (the alpha: a landing ends a flip's animation)
}

static int16_t Tails_Angle(Sst *self, int16_t dist0, int16_t dist1) {
    // Get angle and distance to use (use closest one)
    uint8_t res_angle = angle_buffer1;
    int16_t res_dist = dist1;
    if (dist1 > dist0) {
        res_angle = angle_buffer0;
        res_dist = dist0;
    }

    // Use adjusted angle if hit angle is odd (special angle, run on all sides)
    if (res_angle & 1)
        self->angle = (self->angle + 0x20) & 0xC0;
    else
        self->angle = res_angle;
    return res_dist;
}

static void Tails_AnglePos(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Don't do floor collision if standing on an object
    if (((self->status & SST_PSTATUS_OBJECT_STAND) != 0)) {
        angle_buffer0 = 0;
        angle_buffer1 = 0;
        return;
    }

    // Set 'no floor' angle
    angle_buffer0 = 3;
    angle_buffer1 = 3;

    // Get symmetrical angle
    uint8_t angle = self->angle;
    if ((angle + 0x20) & 0x80) {
        if (angle & 0x80)
            angle--;
        angle += 0x20;
    } else {
        if (angle & 0x80)
            angle++;
        angle += 0x1F;
    }

    int16_t dist0, dist1, dist;
    switch (angle & 0xC0) {
    case 0x00:
        dist0 = FindFloor(self, self->x_pos.f.u + self->x_radius, self->y_pos.f.u + self->y_radius, META_SOLID_TOP, 0, 0x10, &angle_buffer0);
        dist1 = FindFloor(self, self->x_pos.f.u - self->x_radius, self->y_pos.f.u + self->y_radius, META_SOLID_TOP, 0, 0x10, &angle_buffer1);
        if ((dist = Tails_Angle(self, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    self->y_pos.f.u += dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    self->y_pos.f.u += dist;
                } else {
                    self->status |= SST_PSTATUS_IN_AIR;
                    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    self->prev_anim = 1;
                }
            }
        }
        break;
    case 0x40:
        dist0 = FindWall(self, (self->x_pos.f.u - self->y_radius) ^ 0xF, self->y_pos.f.u - self->x_radius, META_SOLID_TOP, META_X_FLIP, -0x10, &angle_buffer0);
        dist1 = FindWall(self, (self->x_pos.f.u - self->y_radius) ^ 0xF, self->y_pos.f.u + self->x_radius, META_SOLID_TOP, META_X_FLIP, -0x10, &angle_buffer1);
        if ((dist = Tails_Angle(self, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    self->x_pos.f.u -= dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    self->x_pos.f.u -= dist;
                } else {
                    self->status |= SST_PSTATUS_IN_AIR;
                    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    self->prev_anim = 1;
                }
            }
        }
        break;
    case 0x80:
        dist0 = FindFloor(self, self->x_pos.f.u - self->x_radius, (self->y_pos.f.u - self->y_radius) ^ 0xF, META_SOLID_TOP, META_Y_FLIP, -0x10, &angle_buffer0);
        dist1 = FindFloor(self, self->x_pos.f.u + self->x_radius, (self->y_pos.f.u - self->y_radius) ^ 0xF, META_SOLID_TOP, META_Y_FLIP, -0x10, &angle_buffer1);
        if ((dist = Tails_Angle(self, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    self->y_pos.f.u -= dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    self->y_pos.f.u -= dist;
                } else {
                    self->status |= SST_PSTATUS_IN_AIR;
                    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    self->prev_anim = 1;
                }
            }
        }
        break;
    case 0xC0:
        dist0 = FindWall(self, self->x_pos.f.u + self->y_radius, self->y_pos.f.u + self->x_radius, META_SOLID_TOP, 0, 0x10, &angle_buffer0);
        dist1 = FindWall(self, self->x_pos.f.u + self->y_radius, self->y_pos.f.u - self->x_radius, META_SOLID_TOP, 0, 0x10, &angle_buffer1);
        if ((dist = Tails_Angle(self, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    self->x_pos.f.u += dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    self->x_pos.f.u += dist;
                } else if (!scratch->x38.floor_clip) {
                    self->status |= SST_PSTATUS_IN_AIR;
                    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    self->prev_anim = 1;
                }
            }
        }
        break;
    }
}

static void Tails_Floor(Sst *self) {
    // Get angle we're moving in
    // There's some weird logging stuff done here
    // Maybe testing if the CalcAngle is yielding desirable results?
    uint8_t angle = CalcAngle(self->x_vel, self->y_vel);
    angle -= 0x20;
    angle &= 0xC0;

    int16_t dist0, dist1, clip;
    uint8_t hit_angle;
    switch (angle) {
    case 0x00: { // Moving down
        // Collide with walls
        int16_t wall_left = GetDistance2_Left(self, self->x_pos.f.u, self->y_pos.f.u, NULL);
        int16_t wall_right = GetDistance2_Right(self, self->x_pos.f.u, self->y_pos.f.u, NULL);
        if ((dist0 = wall_left) < 0) {
            self->x_pos.f.u -= dist0;
            self->x_vel = 0;
        }
        if ((dist0 = wall_right) < 0) {
            self->x_pos.f.u += dist0;
            self->x_vel = 0;
        }

        // Collide with floor
        GetDistance_Down(self, &dist0, &dist1, &hit_angle);

        clip = -((self->y_vel >> 8) + 8);
        if (dist1 < 0 && (dist0 >= clip || dist1 >= clip)) {
            // Set state
            self->y_pos.f.u += dist1;
            self->angle = hit_angle;
            Tails_ResetOnFloor(self);
            self->anim = SonAnimId_Walk;

            // Get inertia
            if ((hit_angle + 0x20) & 0x40) {
                // If floor is greater than 45 degrees, use our full vertical velocity (capped at 0xFC0)
                self->x_vel = 0;
                if (self->y_vel > 0xFC0)
                    self->y_vel = 0xFC0;
                self->ground_vel = (hit_angle & 0x80) ? -self->y_vel : self->y_vel;
            } else if ((hit_angle + 0x10) & 0x20) {
                // If floor is greater than 22.5 degrees, use our halved vertical velocity
                self->y_vel /= 2;
                self->ground_vel = (hit_angle & 0x80) ? -self->y_vel : self->y_vel;
            } else {
                // If floor is less than 22.5 degrees, use our horizontal velocity
                self->y_vel = 0;
                self->ground_vel = self->x_vel;
            }
        }
        break;
    }
    case 0x40: // Moving left
        // Collide with walls
        if ((dist0 = GetDistance2_Left(self, self->x_pos.f.u, self->y_pos.f.u, NULL)) < 0) {
            self->x_pos.f.u -= dist0;
            self->x_vel = 0;
            self->ground_vel = self->y_vel;
            break; // Original returns here?
        }

        // Collide with ceiling
        GetDistance_Up(self, NULL, &dist1, &hit_angle);
        if (dist1 < 0) {
            self->y_pos.f.u -= dist1;
            if (self->y_vel < 0)
                self->y_vel = 0;
            break; // Again...
        }

        // Collide with floor
        if (self->y_vel >= 0) {
            GetDistance_Down(self, NULL, &dist1, &hit_angle);
            if (dist1 < 0) {
                // Set state
                self->y_pos.f.u += dist1;
                self->angle = hit_angle;
                Tails_ResetOnFloor(self);
                self->anim = SonAnimId_Walk;

                // Get inertia
                self->y_vel = 0;
                self->ground_vel = self->x_vel;
            }
        }
        break;
    case 0x80: // Moving up
        // Collide with walls
        if ((dist0 = GetDistance2_Left(self, self->x_pos.f.u, self->y_pos.f.u, NULL)) < 0) {
            self->x_pos.f.u -= dist0;
            self->x_vel = 0;
        }
        if ((dist0 = GetDistance2_Right(self, self->x_pos.f.u, self->y_pos.f.u, NULL)) < 0) {
            self->x_pos.f.u += dist0;
            self->x_vel = 0;
        }

        // Collide with ceiling
        GetDistance_Up(self, NULL, &dist1, &hit_angle);
        if (dist1 < 0) {
            self->y_pos.f.u -= dist1;
            if (!((hit_angle + 0x20) & 0x40)) {
                // Kill speed
                if (self->y_vel < 0)
                    self->y_vel = 0;
            } else {
                // Land on ceiling
                self->angle = hit_angle;
                Tails_ResetOnFloor(self);
                self->ground_vel = (hit_angle & 0x80) ? -self->y_vel : self->y_vel;
            }
        }
        break;
    case 0xC0: // Moving right
        // Collide with walls
        if ((dist0 = GetDistance2_Right(self, self->x_pos.f.u, self->y_pos.f.u, NULL)) < 0) {
            self->x_pos.f.u += dist0;
            self->x_vel = 0;
            self->ground_vel = self->y_vel;
        }

        // Collide with ceiling
        GetDistance_Up(self, NULL, &dist1, &hit_angle);
        if (dist1 < 0) {
            self->y_pos.f.u -= dist1;
            if (self->y_vel < 0)
                self->y_vel = 0;
            break; // Again...
        }

        // Collide with floor
        if (self->y_vel >= 0) {
            GetDistance_Down(self, NULL, &dist1, &hit_angle);
            if (dist1 < 0) {
                // Set state
                self->y_pos.f.u += dist1;
                self->angle = hit_angle;
                Tails_ResetOnFloor(self);
                self->anim = SonAnimId_Walk;

                // Get inertia
                self->y_vel = 0;
                self->ground_vel = self->x_vel;
            }
        }
        break;
    }
}

static void Tails_HurtStop(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Die when falling below the level
    if ((limit_btm2 + SCREEN_HEIGHT) < self->y_pos.f.u) {
        KillTails(self);
        return;
    }

    // Do collision
    Tails_Floor(self);
    if (!((self->status & SST_PSTATUS_IN_AIR) != 0)) {
        self->y_vel = 0;
        self->x_vel = 0;
        self->ground_vel = 0;
        self->anim = SonAnimId_Walk;
        self->routine -= 2;
        scratch->flash_time = 120;
    }
}

// Object collision
static const uint8_t obj_sizes[][2] = {
    { 0x0, 0x0 },
    { 0x14, 0x14 },
    { 0xC, 0x14 },
    { 0x14, 0xC },
    { 0x4, 0x10 },
    { 0xC, 0x12 },
    { 0x10, 0x10 },
    { 0x6, 0x6 },
    { 0x18, 0xC },
    { 0xC, 0x10 },
    { 0x10, 0xC },
    { 0x8, 0x8 },
    { 0x14, 0x10 },
    { 0x14, 0x8 },
    { 0xE, 0xE },
    { 0x18, 0x18 },
    { 0x28, 0x10 },
    { 0x10, 0x18 },
    { 0x8, 0x10 },
    { 0x20, 0x70 },
    { 0x40, 0x20 },
    { 0x80, 0x20 },
    { 0x20, 0x20 },
    { 0x8, 0x8 },
    { 0x4, 0x4 },
    { 0x20, 0x8 },
    { 0xC, 0xC },
    { 0x8, 0x4 },
    { 0x18, 0x4 },
    { 0x28, 0x4 },
    { 0x4, 0x8 },
    { 0x4, 0x18 },
    { 0x4, 0x28 },
    { 0x4, 0x20 },
    { 0x18, 0x18 },
    { 0xC, 0x18 },
    { 0x48, 0x8 },
};

static signed int React_ChkHurt(Sst *self, Sst *hit) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Check for invincibility or invulnerability
    if (invincibility)
        return -1;
    if (scratch->flash_time)
        return -1;

    // Hurt Sonic
    return HurtTails(self, hit->x_pos.f.u, hit->code == sst_object_codes[ObjId_Spikes]);
}

static signed int React_Enemy(Sst *self, Sst *hit)
{
    // Check if we can hurt the enemy
    if (!(invincibility || self->anim == SonAnimId_Roll))
        return React_ChkHurt(self, hit);

    // The alpha's bosses are multi-sprite objects whose hit counter Sonic's touch counts down (the touch code of the alpha)
    if ((hit->render_flags & SST_RENDER_MULTI_DRAW)) {
        if (hit->collision_property) {
            self->x_vel = -self->x_vel;
            self->y_vel = -self->y_vel;
            if (boss_flag != 3) { // (Hill Top's does not take the speed from him)
                self->x_vel >>= 1;
                self->y_vel >>= 1;
            }
            hit->collision_flags = 0;
            hit->collision_property--;
        }
        return 0;
    }

    // Check if enemy is a boss
    if (hit->collision_property) {
        // Hit boss
        self->x_vel = -self->x_vel >> 1;
        self->y_vel = -self->y_vel >> 1;
        hit->collision_flags = 0;
        if (!(--hit->collision_property))
            hit->status |= 0x80;
    } else {
        // Mark enemy touch flag
        hit->status |= 0x80;

        // Increment bonus
        uint8_t bonus = item_bonus;
        item_bonus += 2;

        // Handle score bonus
        if (bonus >= 6)
            bonus = bonus;
        hit->own.u16[0xB] = bonus;

        static const uint16_t points[] = { 10, 20, 50, 100 };
        uint16_t point_bonus = points[bonus >> 1];
        if (item_bonus >= 32) {
            point_bonus = 1000;
            hit->own.u16[0xB] = 10;
        }

        AddPoints(point_bonus);

        // Explode object and bounce
        hit->code = sst_object_codes[ObjId_Explosion];
        hit->routine = 0;

        if (self->y_vel >= 0) {
            if (self->y_pos.f.u < hit->y_pos.f.u)
                self->y_vel = -self->y_vel;
            else
                self->y_vel -= 0x100;
        } else {
            self->y_vel += 0x100;
        }
    }
    return 0; // d0 not set
}

static signed int React_Monitor(Sst *self, Sst *hit) {
    if (self->y_vel < 0) {
        // Check if we're below the monitor
        uint16_t chky = self->y_pos.f.u - 0x10;
        if ((uint16_t)hit->y_pos.f.u < chky) {
            // Bump monitor
            self->y_vel = -self->y_vel;
            hit->y_vel = -0x180;
            if (!hit->routine_secondary)
                hit->routine_secondary += 4;
        }
    } else if (self->anim == SonAnimId_Roll) {
        // Break monitor
        self->y_vel = -self->y_vel;
        hit->routine += 2;
    }
    return 0; // d0 not set
}

static signed int ReactToItem(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Test_Ring_Collisions: the rings of the level's layout (a hurt player is blinking and cannot collect them for a while)
    if (scratch->flash_time < 90)
        Ring_TouchLayout(self);

    // Get collision area
    int16_t width, height;
    int16_t x = self->x_pos.f.u - 8;
    int16_t y = self->y_pos.f.u - (height = (uint8_t)(self->y_radius - 3));

    if (self->mapping_frame == 0x39) {
        // Smaller hitbox when ducking
        y += 12;
        height = 10;
    }
    width = 16;
    height <<= 1;

    // Iterate through level objects
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
        Sst *hit = &sst[i];
        // Check if object is collidable
        if (hit->code == NULL || !((hit->render_flags & SST_RENDER_ON_SCREEN) && hit->collision_flags))
            continue;

        // The alpha's Hill Top boss has a second hitbox, its open cannon
        int16_t cannon_x, cannon_y, cannon_w;
        if (HTZBoss_CannonBox(hit, &cannon_x, &cannon_y, &cannon_w)) {
            const int16_t cx = x - (cannon_x - cannon_w);
            const int16_t cy = y - (cannon_y - 4);
            if (cx >= -width && cx <= cannon_w * 2 && cy >= -height && cy <= 8)
                return React_Enemy(self, hit);
        }

        // Get object's size
        const uint8_t *sizep = obj_sizes[hit->collision_flags & 0x3F];
        uint8_t hit_width = *sizep++;
        uint8_t hit_height = *sizep++;

        // Check if we're touching (same overlap test as the original's React_CheckHitboxOverlap)
        int16_t x_diff = x - (hit->x_pos.f.u - hit_width);
        int16_t y_diff = y - (hit->y_pos.f.u - hit_height);

        if (x_diff >= -width && x_diff <= hit_width * 2 && y_diff >= -height && y_diff <= hit_height * 2) {
            // Made contact
            switch (hit->collision_flags & 0xC0) {
            case 0x00: // Enemy
                return React_Enemy(self, hit);
            case 0xC0: // Special
                if ((hit->collision_flags & 0x3F) == 0x0B) { // Caterkiller spiked body segment
                    hit->status |= 0x80; // mark segment touched -- read by Obj_Caterkiller to fragmentate
                    return React_ChkHurt(self, hit);
                }
                if ((hit->collision_flags & 0x3F) == 0x0C) { // Yadrin -- narrow "face" hitbox at the top only
                    int16_t penetration = (int16_t)((y + height) - (hit->y_pos.f.u - hit_height));
                    if (penetration < 8) {
                        int16_t face_left = (int16_t)(hit->x_pos.f.u - 4);
                        if ((hit->status & SST_STATUS_X_FLIP))
                            face_left -= 16;
                        if (x < (int16_t)(face_left + 24) && (int16_t)(x + width) > face_left)
                            return React_ChkHurt(self, hit);
                    }
                    return React_Enemy(self, hit);
                }
                switch (hit->collision_flags & 0x3F) { // the alpha's Touch_Special (the final's too): the object's own routine reads and clears col_property each frame, a bit for each character
                case 6: case 0x14: case 0x15: case 0x16: case 0x17: // (the round bumper, the auto spin: bit 0 is Sonic's, bit 1 Tails')
                    hit->collision_property += 2;
                    return 0;
                case 0x21: // (a pole of Labyrinth's: counts the touches)
                    hit->collision_property++;
                    return 0;
                default:
                    break;
                }
                break;
            case 0x80: // Hurt
                return React_ChkHurt(self, hit);
            case 0x40: // Other
                if ((hit->collision_flags & 0x3F) == 6)
                    return React_Monitor(self, hit);
                if (scratch->flash_time < 90)
                    hit->routine = 4;
                break;
            }
        }
    }
    return 0;
}

int32_t Tails_Hurt(Sst *self, Sst *src) {
    return HurtTails(self, src->x_pos.f.u, src->code == sst_object_codes[ObjId_Spikes]);
}

// Sonic functions
static signed int HurtTails(Sst *self, int16_t src_x, bool spikes)
{
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Lose rings and shield
    if (!shield) {
        if (rings) {
            // Spawn lost rings object
            Sst *rings = Sst_Allocate();
            if (rings != NULL) {
                rings->code = sst_object_codes[ObjId_RingLoss];
                rings->x_pos.f.u = self->x_pos.f.u;
                rings->y_pos.f.u = self->y_pos.f.u;
            }
        }
    }
    shield = 0;

    // Set Sonic state
    self->routine = 4;
    spindash_flag &= ~1;
    sst[DUST_TAILS_SLOT].anim = DUST_NULL;
    Tails_ResetOnFloor(self);
    self->status |= SST_PSTATUS_IN_AIR;

    self->x_vel = ((self->status & SST_PSTATUS_UNDERWATER) != 0) ? -0x100 : -0x200;
    self->y_vel = ((self->status & SST_PSTATUS_UNDERWATER) != 0) ? -0x200 : -0x400;
    if (self->x_pos.f.u >= src_x)
        self->x_vel = -self->x_vel;
    self->ground_vel = 0;

    self->anim = SonAnimId_Hurt;
    scratch->flash_time = 120;

    PlaySound(spikes ? sfx_HitSpikes : sfx_Death);
    return -1;
}

// Sonic movement functions
static bool Tails_Jump(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Don't jump if ABC isn't pressed
    if (!(jpad2_press & (JPAD_A | JPAD_C | JPAD_B)))
        return false;

    // Check if we have enough room to jump
    int16_t dist;
    GetDistanceBelowAngle(self, self->angle + 0x80, NULL, &dist, NULL);
    if (dist < 6)
        return false;

    // Get jump speed
    int16_t speed = 0x680;
    if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
        speed = 0x380;

    int16_t sin, cos;
    CalcSine(self->angle - 0x40, &sin, &cos);
    self->x_vel += (cos * speed) >> 8;
    self->y_vel += (sin * speed) >> 8;

    // Set state
    self->status |= SST_PSTATUS_IN_AIR;
    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    scratch->jumping = true;
    scratch->x38.floor_clip = 0;

    PlaySound(sfx_Jump);

    self->y_radius = TAILS_HEIGHT; // No idea why this is here
    self->x_radius = TAILS_WIDTH;

    if (!((self->status & SST_PSTATUS_IN_BALL) != 0)) {
        self->y_radius = TAILS_BALL_HEIGHT;
        self->x_radius = TAILS_BALL_WIDTH;
        self->anim = SonAnimId_Roll;
        self->status |= SST_PSTATUS_IN_BALL;
        self->y_pos.f.u += TAILS_BALL_SHIFT;
    } else {
        self->status |= SST_PSTATUS_ROLL_JUMP;
    }
    return true;
}

static void Tails_SlopeResist(Sst *self) {
    if (((self->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(self->angle) * 0x20) >> 8;
    if (self->ground_vel != 0)
        self->ground_vel += force;
}

static void Tails_MoveLeft(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia <= 0) {
        // Turn around
        if (!((self->status & SST_PSTATUS_X_FLIP) != 0)) {
            self->status |= SST_PSTATUS_X_FLIP;
            self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
            self->prev_anim = 1; // Reset animation
        }

        // Accelerate
        if ((inertia -= tails_speed_acc) <= -tails_speed_max)
            inertia = -tails_speed_max;

        // Set speed and animation
        self->ground_vel = inertia;
        self->anim = SonAnimId_Walk;
    } else {
        // Decelerate
        if ((inertia -= tails_speed_dec) < 0)
            inertia = -0x80;
        self->ground_vel = inertia;

        // (Nick Arcade tests the masked angle against the speed where it means the speed, so its skid never happens: neither does this)
    }
}

static void Tails_MoveRight(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia >= 0) {
        // Turn around
        if (((self->status & SST_PSTATUS_X_FLIP) != 0)) {
            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
            self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
            self->prev_anim = 1; // Reset animation
        }

        // Accelerate
        if ((inertia += tails_speed_acc) >= tails_speed_max)
            inertia = tails_speed_max;

        // Set speed and animation
        self->ground_vel = inertia;
        self->anim = SonAnimId_Walk;
    } else {
        // Decelerate
        if ((inertia += tails_speed_dec) >= 0)
            inertia = 0x80;
        self->ground_vel = inertia;

        // (Nick Arcade tests the masked angle against the speed where it means the speed, so its skid never happens: neither does this)
    }
}

static void Tails_Move(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    if (f_slidemode)
        goto Tails_AngleSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad2_hold & JPAD_LEFT)
                Tails_MoveLeft(self);
            if (jpad2_hold & JPAD_RIGHT)
                Tails_MoveRight(self);

            // Do idle or balance animation
            if (((self->angle + 0x20) & 0xC0) == 0x00 && !self->ground_vel) {
                // Do idle animation
                self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                self->anim = SonAnimId_Wait;

                if (((self->status & SST_PSTATUS_OBJECT_STAND) != 0)) {
                    // Balance on an object
                    StandingInfo stand;
                    Standing_Get(scratch->standing_obj, &stand);
                    if (!stand.flag7) {
                        int16_t left_dist = stand.width;
                        int16_t right_dist = left_dist + left_dist - 4;
                        left_dist += self->x_pos.f.u - stand.x;

                        if (left_dist < 4)
                            self->status |= SST_PSTATUS_X_FLIP;
                        else if (left_dist >= right_dist)
                            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
                        else
                            goto LookUpDown;
                        self->anim = SonAnimId_Balance;
                        goto Tails_ResetScr;
                    }
                } else {
                    // Balance on level
                    if (SstFloorDist(self, self->x_pos.f.u) >= 12) {
                        if (scratch->front_angle == 3) {
                            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
                            self->anim = SonAnimId_Balance;
                            goto Tails_ResetScr;
                        }
                        if (scratch->back_angle == 3) {
                            self->status |= SST_PSTATUS_X_FLIP;
                            self->anim = SonAnimId_Balance;
                            goto Tails_ResetScr;
                        }
                    }
                }

            // Handle looking up and down
            LookUpDown:;
                if (jpad2_hold & JPAD_UP) {
                    self->anim = SonAnimId_LookUp;
                    // Wait 2 seconds (120 frames) before the camera actually
                    // starts panning up, matching the Spin Dash guide's
                    // camera-delay addition
                    if (cam_y_delay < 120) {
                        cam_y_delay++;
                        goto Tails_ResetScr_Part2;
                    }
                    cam_y_delay = 120;
                    if (look_shift != (200 + SCREEN_TALLADD2))
                        look_shift += 2;
                    goto DoFriction;
                }
                if (jpad2_hold & JPAD_DOWN) {
                    self->anim = SonAnimId_Duck;
                    if (cam_y_delay < 120) {
                        cam_y_delay++;
                        goto Tails_ResetScr_Part2;
                    }
                    cam_y_delay = 120;
                    if (look_shift != (8 + SCREEN_TALLADD2))
                        look_shift -= 2;
                    goto DoFriction;
                }
            }
        }

    // Reset camera to neutral position
    Tails_ResetScr:;
        cam_y_delay = 0;

    Tails_ResetScr_Part2:;
        if (look_shift < (96 + SCREEN_TALLADD2))
            look_shift += 2;
        else if (look_shift > (96 + SCREEN_TALLADD2))
            look_shift -= 2;

    // Friction
    DoFriction:;
        if (!(jpad2_hold & (JPAD_LEFT | JPAD_RIGHT))) {
            if (self->ground_vel > 0) {
                if ((self->ground_vel -= tails_speed_acc) < 0)
                    self->ground_vel = 0;
            } else if (self->ground_vel < 0) {
                if ((self->ground_vel += tails_speed_acc) >= 0)
                    self->ground_vel = 0;
            }
        }
    }

    // Calculate global speed from inertia
Tails_AngleSpeed:;
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    self->x_vel = (cos * self->ground_vel) >> 8;
    self->y_vel = (sin * self->ground_vel) >> 8;

    // Handle wall collision
    if (((self->angle + 0x40) & 0x80) || !self->ground_vel)
        return;

    uint8_t add_angle = (self->ground_vel < 0) ? 0x40 : -0x40;
    uint8_t angle = self->angle + add_angle;
    int16_t dist = GetDistanceBelowAngle2(self, angle, NULL);

    if (dist < 0) {
        dist <<= 8;
        switch ((angle + 0x20) & 0xC0) {
        case 0x00:
            self->y_vel += dist;
            break;
        case 0x40:
            self->x_vel -= dist;
            self->status |= SST_PSTATUS_PUSHING;
            self->ground_vel = 0;
            break;
        case 0x80:
            self->y_vel -= dist;
            break;
        case 0xC0:
            self->x_vel += dist;
            self->status |= SST_PSTATUS_PUSHING;
            self->ground_vel = 0;
            break;
        }
    }
}

void Tails_ChkRoll(Sst *self) {
    // Enter roll state
    if (((self->status & SST_PSTATUS_IN_BALL) != 0))
        return;
    self->status |= SST_PSTATUS_IN_BALL;
    self->y_radius = TAILS_BALL_HEIGHT;
    self->x_radius = TAILS_BALL_WIDTH;
    self->anim = SonAnimId_Roll;
    self->y_pos.f.u += TAILS_BALL_SHIFT;
    PlaySound(sfx_Roll);

    // Set speed (S-tubes)
    if (!self->ground_vel)
        self->ground_vel = 0x200;
}

static void Tails_Roll(Sst *self) {
    // Don't allow rolling while on a water slide
    if (f_slidemode)
        return;

    // Check if we can and are trying to roll
    if (jump_only || ((self->ground_vel < 0) ? -self->ground_vel : self->ground_vel) < 0x80)
        return;
    if ((jpad2_hold & (JPAD_LEFT | JPAD_RIGHT)) || !(jpad2_hold & JPAD_DOWN))
        return;
    Tails_ChkRoll(self);
}

static void Tails_LevelBound(Sst *self) {
    // Get next X position
    // This is unsigned, but it shouldn't be
    uint16_t x = (self->x_pos.v + (self->x_vel << 8)) >> 16;

    // Prevent us from going off the left or right boundaries
    int16_t bound;
    if (x < (bound = limit_left2 + 16) || x > (bound = limit_right2 + (lock_screen ? 290 : 360) + SCREEN_WIDEADD)) {
        self->x_pos.f.u = bound;
        self->x_pos.f.l = 0;
        self->x_vel = 0;
        self->ground_vel = 0;
    }

    // Fall off the bottom boundary. FixBugs: use whichever of limit_btm2
    // (the level's static bottom boundary) and limit_btm1 (the camera's
    // own, possibly-DynamicLevelEvents-extended clamp) is currently
    // deeper -- the original code only ever considers limit_btm2, which
    // doesn't account for the camera boundary being in the middle of
    // (or having already finished) lowering itself, killing Sonic in
    // legitimately-reachable deep areas a zone's own DLE has unlocked.
    uint16_t kill_plane = (limit_btm1 > limit_btm2) ? limit_btm1 : limit_btm2;
    if ((kill_plane + SCREEN_HEIGHT) < self->y_pos.f.u) {
        // (SBZ2's drop into SBZ3 is not a kill-plane exception any more: the cutscene triggers it itself, see
        // DynamicLevelEvents' SBZ act 2 case.)
        KillTails(self);
    }
}

static void Tails_SlopeRepel(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Check if we can fall off a slope
    if (scratch->x38.floor_clip)
        return;

    if (!scratch->control_lock) {
        // Check if the slope is steep enough
        if ((self->angle + 0x20) & 0xC0) {
            // Fall off if we're going too slow
            if (((self->ground_vel < 0) ? -self->ground_vel : self->ground_vel) < 0x280) {
                self->ground_vel = 0;
                self->status |= SST_PSTATUS_IN_AIR;
                scratch->control_lock = 30;
            }
        }
    } else {
        // Decrement control lock
        scratch->control_lock--;
    }
}

static void Tails_RollRepel(Sst *self) {
    if (((self->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(self->angle) * 0x50) >> 8;
    if (self->ground_vel >= 0) { // (standing still counts as moving right: the original tests only for a negative speed)
        if (force < 0)
            force >>= 2;
        self->ground_vel += force;
    } else {
        if (force >= 0)
            force >>= 2;
        self->ground_vel += force;
    }
}

static void Tails_RollLeft(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia <= 0) {
        // Set animation
        self->status |= SST_PSTATUS_X_FLIP;
        self->anim = SonAnimId_Roll;
    } else {
        // Decelerate
        if ((inertia -= tails_speed_dec >> 2) < 0)
            inertia = -0x80;
        self->ground_vel = inertia;
    }
}

static void Tails_RollRight(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia >= 0) {
        // Set animation
        self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        self->anim = SonAnimId_Roll;
    } else {
        // Decelerate
        if ((inertia += tails_speed_dec >> 2) >= 0)
            inertia = 0x80;
        self->ground_vel = inertia;
    }
}

static void Tails_RollSpeed(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    if (f_slidemode)
        goto Tails_AngledRollSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad2_hold & JPAD_LEFT)
                Tails_RollLeft(self);
            if (jpad2_hold & JPAD_RIGHT)
                Tails_RollRight(self);
        }

        // Friction
        if (self->ground_vel > 0) {
            if ((self->ground_vel -= (tails_speed_acc >> 1)) < 0)
                self->ground_vel = 0;
        } else {
            if ((self->ground_vel += (tails_speed_acc >> 1)) >= 0)
                self->ground_vel = 0;
        }

        // Uncurl when we've come to a stop -- unless must_roll (GHZTunnel)
        // is forcing us to stay rolling, in which case give an extra push
        // in whichever direction we're currently facing instead (real
        // Tails_KeepRolling: "magically gives Sonic an extra push if he's
        // going to stop rolling where it's not allowed, such as in an
        // S-curve").
        if (self->ground_vel == 0) {
            if (((self->status & SST_PSTATUS_MUST_ROLL) != 0)) {
                self->ground_vel = ((self->status & SST_PSTATUS_X_FLIP) != 0) ? (int16_t)-0x400 : (int16_t)0x400;
            } else {
                self->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
                self->y_radius = TAILS_HEIGHT;
                self->x_radius = TAILS_WIDTH;
                self->anim = SonAnimId_Wait;
                self->y_pos.f.u -= TAILS_BALL_SHIFT;
            }
        }
    }

    // Calculate global speed from inertia
Tails_AngledRollSpeed:;
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    self->y_vel = (sin * self->ground_vel) >> 8;
    cos = (cos * self->ground_vel) >> 8;
    if (cos > 0x1000) // Global X speed is limited to 0x1000 both ways.
        cos = 0x1000; // This causes the speed to desync from inertia
    if (cos < -0x1000) // at high speeds.
        cos = -0x1000; // Is this here because of the broken camera?
    self->x_vel = cos;

    // Handle wall collision
    if (((self->angle + 0x40) & 0x80) || !self->ground_vel)
        return;

    uint8_t add_angle = (self->ground_vel < 0) ? 0x40 : -0x40;
    uint8_t angle = self->angle + add_angle;
    int16_t dist = GetDistanceBelowAngle2(self, angle, NULL);

    if (dist < 0) {
        dist <<= 8;
        switch ((angle + 0x20) & 0xC0) {
        case 0x00:
            self->y_vel += dist;
            break;
        case 0x40:
            self->x_vel -= dist;
            self->status |= SST_PSTATUS_PUSHING;
            self->ground_vel = 0;
            break;
        case 0x80:
            self->y_vel -= dist;
            break;
        case 0xC0:
            self->x_vel += dist;
            self->status |= SST_PSTATUS_PUSHING;
            self->ground_vel = 0;
            break;
        }
    }
}

static void Tails_JumpHeight(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    if (scratch->jumping) {
        // Get minimum jump speed and apply if ABC isn't held
        int16_t spd = ((self->status & SST_PSTATUS_UNDERWATER) != 0) ? -0x200 : -0x400;
        if (self->y_vel < spd && !(jpad2_hold & (JPAD_A | JPAD_C | JPAD_B)))
            self->y_vel = spd;
    } else {
        // Cap upwards speed
        if (self->y_vel < -0xFC0)
            self->y_vel = -0xFC0;
    }
}

static void Tails_JumpDirection(Sst *self) {
    // Handle acceleration
    if (!((self->status & SST_PSTATUS_ROLL_JUMP) != 0)) {
        int16_t xsp = self->x_vel;

        // Accelerate left
        if (jpad2_hold & JPAD_LEFT) {
            self->status |= SST_PSTATUS_X_FLIP;
            if ((xsp -= (tails_speed_acc << 1)) <= -tails_speed_max)
                xsp = -tails_speed_max;
        }

        // Accelerate right
        if (jpad2_hold & JPAD_RIGHT) {
            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
            if ((xsp += (tails_speed_acc << 1)) >= tails_speed_max)
                xsp = tails_speed_max;
        }

        // Apply acceleration
        self->x_vel = xsp;
    }

    // Reset screen shift
    if (look_shift < (96 + SCREEN_TALLADD2))
        look_shift += 2;
    else if (look_shift > (96 + SCREEN_TALLADD2))
        look_shift -= 2;

    // Handle air drag
    if ((uint16_t)self->y_vel >= (uint16_t)-0x400) {
        int16_t xsp = self->x_vel;
        int16_t drag = xsp >> 5;
        if (drag > 0) // Do I really have to say anything?
        {
            if ((xsp -= drag) < 0)
                xsp = 0;
            self->x_vel = xsp;
        } else if (drag < 0) {
            if ((xsp -= drag) >= 0)
                xsp = 0;
            self->x_vel = xsp;
        }
    }
}

static void Tails_JumpAngle(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Reset angle towards 0
    uint8_t angle = self->angle;
    if (angle != 0) {
        if (!(angle & 0x80)) {
            if ((angle -= 2) & 0x80)
                angle = 0;
        } else {
            if (!((angle += 2) & 0x80))
                angle = 0;
        }

        self->angle = angle;
    }

    // Tumble (a spring's flip), as Sonic's
    uint8_t flip = scratch->flip_angle;
    if (flip == 0)
        return;

    if (self->ground_vel >= 0) {
        unsigned sum = flip + scratch->flip_speed;
        flip = (uint8_t)sum;
        if (sum > 0xFF && (int8_t)--scratch->flips_remaining < 0) {
            scratch->flips_remaining = 0;
            flip = 0;
        }
    } else {
        int diff = flip - scratch->flip_speed;
        flip = (uint8_t)diff;
        if (diff < 0 && (int8_t)--scratch->flips_remaining < 0) {
            scratch->flips_remaining = 0;
            flip = 0;
        }
    }
    scratch->flip_angle = flip;
}

// Spin Dash
static void Tails_Spindash_ResetScr(Sst *self) {
    // Reset camera to neutral position (same logic as Tails_Move/Tails_JumpDirection)
    if (look_shift < (96 + SCREEN_TALLADD2))
        look_shift += 2;
    else if (look_shift > (96 + SCREEN_TALLADD2))
        look_shift -= 2;

    // Since we skip the rest of the normal-movement case, run these manually
    Tails_LevelBound(self);
    Tails_AnglePos(self);
}

static void Tails_ChargingSpindash(Sst *self) {
    self->anim = TAILS_ANIM_SPINDASH; // make sure Spin Dash animation stays

    // Charge decay
    spindash_count -= spindash_count >> 5;

    if (!(jpad2_press & (JPAD_A | JPAD_C | JPAD_B))) {
        Tails_Spindash_ResetScr(self);
        return;
    }

    // Restart Spin Dash animation
    self->anim = TAILS_ANIM_SPINDASH;
    self->anim_frame = 0;
    self->anim_frame_timer = 0;
    PlaySound(sfx_SpindashRev);

    spindash_count += 0x200;
    if (spindash_count > 0x800)
        spindash_count = 0x800;

    Tails_Spindash_ResetScr(self);
}

static void Tails_ReleaseSpindash(Sst *self) {
    spindash_flag &= ~1;
    self->y_radius = TAILS_BALL_HEIGHT;
    self->x_radius = TAILS_BALL_WIDTH;
    self->y_pos.f.u += TAILS_BALL_SHIFT;
    self->anim = SonAnimId_Roll;
    self->status |= SST_PSTATUS_IN_BALL;
    PlaySound(sfx_Teleport);
    sst[DUST_TAILS_SLOT].anim = DUST_NULL;

    // Get release speed from the number of revs (the alpha's table: $80 for each step of the counter's high byte)
    int16_t speed = (int16_t)(0x800 + (((spindash_count >> 8) & 0xFF) << 7));
    if (((self->status & SST_PSTATUS_X_FLIP) != 0))
        speed = -speed;
    self->ground_vel = speed;

    // Camera delay (the alpha's is always $2000)
    cam_x_delay = 0x2000;

    // Set new velocities immediately (same convention as Tails_Move/Tails_RollSpeed: cos->x_vel, sin->y_vel)
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    self->x_vel = (int16_t)(((int32_t)cos * self->ground_vel) >> 8);
    self->y_vel = (int16_t)(((int32_t)sin * self->ground_vel) >> 8);

    Tails_Spindash_ResetScr(self);
}

static void Tails_UpdateSpindash(Sst *self) {
    // Bug fix (part 2 of the monitor fix): guarantee the Spin Dash
    // animation is set as soon as we know we're spin dashing, before
    // Monitor's own Mon_SolidSides push-vs-break check (which now also
    // exempts this animation, see Monitor.c) gets a chance to run this
    // same frame and see a stale Push animation instead.
    self->anim = TAILS_ANIM_SPINDASH;
    if (jpad2_hold & JPAD_DOWN) {
        Tails_ChargingSpindash(self);
        return;
    }
    Tails_ReleaseSpindash(self);
}

// Abandons a spin dash that is still charging when Sonic leaves the ground some other
// way than releasing it (e.g. a seesaw launch). Clearing only the flag left the dust
// companion in its Dash animation, so it kept following Sonic through the air.
void Tails_CancelSpindash(void) {
    if (!(spindash_flag & 1))
        return;
    spindash_flag &= ~1;
    spindash_count = 0;
    if (sst[DUST_TAILS_SLOT].anim == DUST_DASH)
        sst[DUST_TAILS_SLOT].anim = DUST_NULL;
}

static bool Tails_SpinDash(Sst *self) {
    if (spindash_flag & 1) {
        Tails_UpdateSpindash(self);
        return true;
    }

    if (self->anim != SonAnimId_Duck)
        return false;
    if (!(jpad2_press & (JPAD_A | JPAD_C | JPAD_B)))
        return false;

    self->anim = TAILS_ANIM_SPINDASH;
    PlaySound(sfx_SpindashRev);
    spindash_flag |= 1;
    spindash_count = 0;
    if (scratch_air(self) >= 12)
        DustSplash_Show(&sst[DUST_TAILS_SLOT], DUST_DASH);

    // Because we're skipping the rest of the normal-movement case
    Tails_LevelBound(self);
    Tails_AnglePos(self);
    return true;
}


// The tails (object 05): follows Tails, turning with his animation. His own art window, and the animation of the tails for each of Tails' animations (the alpha's table at $0124CA; a Tails who pushes
// has the tails of animation 4).
static const uint8_t TailsTails_Animations[34] = {
    0, 0, 3, 3, 9, 1, 0, 2, 1, 7, 0, 0, 0, 8, 0, 0,
    0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
};
static uint8_t tails_tails_prev_anim = 0xFF;

void Obj_TailsTails(Sst *self) {
    const Sst *tails = SST_PLAYER_2;
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_Tails;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_TailsTails);
        self->priority = 2 * 0x80;
        self->width_pixels = 0x18;
        self->render_flags = SST_RENDER_LEVEL;
        tails_tails_prev_anim = 0xFF;
        last_frame_tail = 0xFF;
        // Fallthrough
    case 2:
        self->angle = tails->angle;
        self->status = tails->status;
        self->x_pos.f.u = tails->x_pos.f.u;
        self->y_pos.f.u = tails->y_pos.f.u;
        const uint8_t cue = ((tails->status & SST_PSTATUS_PUSHING) != 0) ? 4 : (tails->anim < sizeof(TailsTails_Animations) ? tails->anim : 0);
        if (cue != tails_tails_prev_anim) {
            tails_tails_prev_anim = cue;
            self->anim = TailsTails_Animations[cue];
        }
        Tails_AnimateTable(self, Animation_TailsTails, tails);
        Tails_LoadDPLC(self->mapping_frame, &last_frame_tail, ArtTile_TailsTails);
        Sst_Draw(self);
        break;
    }
}

// Tails (object 02)
void Obj_Tails(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        self->routine += 2;
        self->y_radius = TAILS_HEIGHT;
        self->x_radius = TAILS_WIDTH;
        self->mappings = Mappings_Tails;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Tails);
        self->priority = 2 * 0x80;
        self->width_pixels = 0x18;
        self->render_flags = SST_RENDER_LEVEL;
        scratch->air = 30;
        tails_speed_max = 0x600;
        tails_speed_acc = 0xC;
        tails_speed_dec = 0x80;
        scratch->top_solid_bit = 0xC;
        scratch->lrb_solid_bit = 0xD;
        scratch->flips_remaining = 0;
        scratch->flip_speed = 4;
        spindash_flag = 0;
        spindash_count = 0;
        memset(pad_hold, 0, sizeof(pad_hold));
        memset(pad_press, 0, sizeof(pad_press));
        pad_head = 0;
        control_counter = 0;
        last_frame_tails = 0xFF;
        SST_TAILS_TAILS->code = sst_object_codes[ObjId_05]; // (his tails)
        // Fallthrough
    case 2: // Regular movement
        Tails_Control();

        if (!(CharObjControl(self) & 1)) {
            switch ((((self->status & SST_PSTATUS_IN_BALL) != 0) << 2) | (((self->status & SST_PSTATUS_IN_AIR) != 0) << 1)) {
            case 0: // Not in ball, not in air
                if (Tails_SpinDash(self))
                    break;
                if (Tails_Jump(self))
                    break;
                Tails_SlopeResist(self);
                Tails_Move(self);
                Tails_Roll(self);
                Tails_LevelBound(self);
                Sst_MoveSprite2(self);
                Tails_AnglePos(self);
                Tails_SlopeRepel(self);
                break;
            case 2: // Not in ball, in air
                Tails_CancelSpindash();
                Tails_JumpHeight(self);
                Tails_JumpDirection(self);
                Tails_LevelBound(self);
                Sst_MoveSprite(self);
                if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
                    self->y_vel -= 0x28;
                Tails_JumpAngle(self);
                Tails_Floor(self);
                break;
            case 4: // In ball, not in air
                if (!((self->status & SST_PSTATUS_MUST_ROLL) != 0) && Tails_Jump(self))
                    break;
                Tails_RollRepel(self);
                Tails_RollSpeed(self);
                Tails_LevelBound(self);
                Sst_MoveSprite2(self);
                Tails_AnglePos(self);
                Tails_SlopeRepel(self);
                break;
            case 6: // In ball, in air
                Tails_CancelSpindash();
                Tails_JumpHeight(self);
                Tails_JumpDirection(self);
                Tails_LevelBound(self);
                Sst_MoveSprite(self);
                if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
                    self->y_vel -= 0x28;
                Tails_JumpAngle(self);
                Tails_Floor(self);
                break;
            }
        }

        if (limit_top2 == 0xFF00) // (a level that wraps vertically: see Sonic.c)
            self->y_pos.f.u &= 0x7FF;

        Tails_Display(self);
        Tails_Water(self);
        scratch->front_angle = angle_buffer0;
        scratch->back_angle = angle_buffer1;
        Tails_Animate(self);
        if (!(CharObjControl(self) & 0x80))
            ReactToItem(self);
        Tails_LoadGfx(self);
        break;
    case 4: // Hurt
        Sst_MoveSprite2(self);
        self->y_vel += 0x30;
        if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
            self->y_vel -= 0x20;
        Tails_HurtStop(self);
        if (self->routine != 6) {
            Tails_LevelBound(self);
            Tails_Animate(self);
            Tails_LoadGfx(self);
            Sst_Draw(self);
        }
        break;
    case 6: // Dead (fell below the level): back above Sonic once he is well out of sight
        if ((uint16_t)(limit_btm2 + 0x100) < (uint16_t)self->y_pos.f.u) {
            // (Tails is not meant to wrap: with Sonic against the left boundary the word would put him past the right one, and he would fall and come back there over and over. The level's wraparound itself is
            // Sonic's, and stays.)
            int16_t back = (int16_t)(player->x_pos.f.u - 0x40);
            if (player->x_pos.f.u < 0x40 || back < (int16_t)(limit_left2 + 16))
                back = (int16_t)(limit_left2 + 16);
            self->x_pos.f.u = back;
            self->y_pos.f.u = player->y_pos.f.u - 0x80;
            self->routine = 2;
            self->art_tile &= ~TILE_PRIORITY_AND;
        } else {
            Sst_MoveSprite(self);
            Tails_Animate(self);
            Tails_LoadGfx(self);
            Sst_Draw(self);
        }
        break;
    }
}

// The level's own objects, beyond what Sonic 1's level start makes: Tails, a little behind Sonic (in every zone: Nick Arcade left him out of Emerald Hill, the Simon Wai prototype does not)
void Game_LevelObjects(void) {
    VDP_SetShadowHighlight((jpad1_hold1 & JPAD_C) != 0); // (the prototype enables the VDP's shadow/highlight mode, which darkens all but what is above the planes' priority, when C is held as a level loads)
    if (cli_start_level >= 0) // a level started from the command line skipped the title, which loads the main art (HUD, rings, ...)
        AddPLC(PlcId_Main);
    Sst *tails = SST_PLAYER_2;
    memset(tails, 0, sizeof(*tails));
    tails->code = sst_object_codes[ObjId_02];
    tails->x_pos.f.u = player->x_pos.f.u - 0x20;
    tails->y_pos.f.u = player->y_pos.f.u;
    memset(SST_TAILS_TAILS, 0, sizeof(Sst));
    if (LEVEL_ZONE(level_id) == ZoneId_OOZ)
        Sst_Spawn(7, 0, 0); // (the oil, where Hidden Palace and the others have a water surface: Oil Ocean has no water)
    DustSplash_MakeTails(); // (the alpha's dust and splash for Tails)
    SplitScreen_LoadLevel(); // (a level picked with B in the level select is a two-player one: Tails is its second player)
}

// While the level editor has the level, neither player runs or shows (the editor's cursor is where Sonic is: EditorLive.h)
bool Game_EditorSkipObject(const void *self) {
    return self == (const void *)player || self == (const void *)TAILS_OBJ;
}
