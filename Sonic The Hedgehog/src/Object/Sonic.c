#include "Standing.h"
#include "ObjectSst.h"
#include "Object/Spikes.h"
#include "Object/GameOverCard.h"
#include "Object.h"
#include "Sonic.h"
#include "SstLoad.h"
#include "Constants.h"
#include "DebugLog.h"

#include "Game.h"
#include "GM_Level.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/DebugList.h"
#include "Object/DrownCount.h"
#include "Object/Splash.h"
#include "PLC.h"
#include "SpecialStage.h"
#include "Sound.h"

#include <string.h>

#define DEMO_WARP

// Sonic mappings
#include "Resource/Mappings/Sonic.h"

// Sonic globals
int16_t sonspeed_max, sonspeed_acc, sonspeed_dec;

uint8_t sonframe_num, sonframe_chg;
uint8_t sgfx_buffer[SONIC_DPLC_SIZE];

int16_t track_sonic[0x40][2];
word_u track_pos;

// Spin Dash state (no room left in Scratch_Sonic, so kept here like track_sonic/track_pos)
static uint8_t spindash_flag;
static uint16_t spindash_count;

// Skid dust spawn throttle (matches Sonic 2's Obj08's own obj08_dust_timer)
static uint8_t skid_dust_timer;

uint8_t dbg_ang0, dbg_ang1, dbg_ang2, dbg_ang3; // 0xFFEC-0xFFEF

// General Sonic state stuff
static void Sonic_Display(Sst *self) {
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
            if (!(lock_screen || air < 12)) {
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
            sonspeed_max = 0x600; // BUG: Water isn't checked
            sonspeed_acc = 0xC;
            sonspeed_dec = 0x80;

            // Clear flag and restore music
            shoes = false;
            SlowDownMusic();
        }
    }
}

static void Sonic_RecordPosition(Sst *self) {
    // Track current position
    int16_t* write = &track_sonic[0][0] + (track_pos.v >> 1);
    *write++ = self->x_pos.f.u;
    *write++ = self->y_pos.f.u;
    track_pos.f.l += 4;
}

// Sonic animation
#include "Resource/Animation/Sonic.h"

#define GET_SONIC_ANISCR(x) (Animation_Sonic + ((Animation_Sonic[((x) << 1)] << 8) | (Animation_Sonic[((x) << 1) + 1] << 0)))

static void Sonic_AnimateReadFrame(Sst *self, const uint8_t *anim_script) {
    // Read current animation command
    uint8_t cmd = anim_script[1 + self->anim_frame];

    if (!(cmd & 0x80)) {
    Anim_Next:
        // Set animation frame
        self->mapping_frame = cmd;
        self->anim_frame++;
    } else {
        if (++cmd == 0) {
            // Restart animation
            self->anim_frame = 0;
            cmd = anim_script[1];
            goto Anim_Next;
        }
        if (++cmd == 0) {
            // Go back (next byte) frames
            self->anim_frame -= anim_script[2 + self->anim_frame];
            cmd = anim_script[1 + self->anim_frame];
            goto Anim_Next;
        }
        if (++cmd == 0) {
            // Change animation (falls through to the routine increment below)
            self->anim = anim_script[2 + self->anim_frame];
        }
    }
}

void Sonic_Animate(Sst *self) {
    // Get animation script to use
    const uint8_t* anim_script = Animation_Sonic;

    // Check if animation changed
    uint8_t anim = self->anim;
    if (anim != self->prev_anim) {
        // Reset animation state
        self->prev_anim = anim;
        self->anim_frame = 0;
        self->anim_frame_timer = 0;
    }

    // Get animation script to use
    anim <<= 1;
    anim_script += (anim_script[anim] << 8) | (anim_script[anim + 1] << 0);

    int8_t anim_wait = anim_script[0];
    if (anim_wait >= 0) {
        // Regular animation
        // Set render flip
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_PSTATUS_X_FLIP) != 0));
        SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);

        // Wait for current animation frame to end
        if (--self->anim_frame_timer >= 0)
            return;
        self->anim_frame_timer = anim_wait;

        // Read animation
        Sonic_AnimateReadFrame(self, anim_script);
    } else {
        // Wait for current animation frame to end
        if (--self->anim_frame_timer >= 0)
            return;

        // Special animation (walking, rolling, running, etc.)
        if (++anim_wait == 0) {
            // Walking or running
            // Get orienatation
            uint8_t angle = self->angle;
            uint8_t flip = ((self->status & SST_PSTATUS_X_FLIP) != 0);
            if (!flip)
                angle ^= ~0;
            if ((angle += 0x10) & 0x80)
                flip ^= 3;

            // Set render flip
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, (flip & 1) != 0);
            SST_RENDER_SET(self, SST_RENDER_Y_FLIP, (flip & 2) != 0);

            // Branch to pushing if the pushing flag is set.. but not in the animation
            if (((self->status & SST_PSTATUS_PUSHING) != 0))
                goto Anim_Pushing;

            // Get rotation
            angle = (angle >> 4) & 6;

            // Get absolute speed
            uint16_t abs_spd = (self->ground_vel < 0) ? -self->ground_vel : self->ground_vel;

            // Get script to use
            anim_script = GET_SONIC_ANISCR(SonAnimId_Run);
            if (abs_spd < 0x600) {
                anim_script = GET_SONIC_ANISCR(SonAnimId_Walk);
                angle += (angle >> 1);
            }
            angle <<= 1;

            // Get animation delay
            int16_t anim_spd = 0x800 - abs_spd;
            if (anim_spd < 0)
                anim_spd = 0;
            self->anim_frame_timer = anim_spd >> 8;

            // Read animation
            Sonic_AnimateReadFrame(self, anim_script);
            self->mapping_frame += angle;
        } else if (++anim_wait == 0) {
            // Rolling
            // Get absolute speed
            uint16_t abs_spd = (self->ground_vel < 0) ? -self->ground_vel : self->ground_vel;

            // Get script to use
            anim_script = GET_SONIC_ANISCR(SonAnimId_Roll2);
            if (abs_spd < 0x600)
                anim_script = GET_SONIC_ANISCR(SonAnimId_Roll);

            // Get animation delay
            int16_t anim_spd = 0x400 - abs_spd;
            if (anim_spd < 0)
                anim_spd = 0;
            self->anim_frame_timer = anim_spd >> 8;

            // Set render flip
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_PSTATUS_X_FLIP) != 0));
            SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);

            // Read animation
            Sonic_AnimateReadFrame(self, anim_script);
        } else {
        Anim_Pushing:;
            // Pushing
            // Get absolute speed
            uint16_t abs_spd = (self->ground_vel < 0) ? -self->ground_vel : self->ground_vel;

            // Get animation delay
            int16_t anim_spd = 0x800 - abs_spd;
            if (anim_spd < 0)
                anim_spd = 0;
            self->anim_frame_timer = anim_spd >> 6;

            // Get script to use
            anim_script = GET_SONIC_ANISCR(SonAnimId_Push);

            // Set render flip
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_PSTATUS_X_FLIP) != 0));
            SST_RENDER_SET(self, SST_RENDER_Y_FLIP, false);

            // Read animation
            Sonic_AnimateReadFrame(self, anim_script);
        }
    }
}

// Sonic DPLCs
#include "Ring.h"
#include "Resource/Art/Sonic.h"

#include "Resource/Mappings/SonicDPLC.h"

void Sonic_LoadGfx(Sst *self) {
    // Check if we're loading a new frame
    uint8_t frame = self->mapping_frame;
    if (frame == sonframe_num)
        return;
    sonframe_num = frame;

    // Get DPLC script
    const uint8_t* dplc_script = Mappings_SonicDPLC;
    frame <<= 1;
    dplc_script += (dplc_script[frame] << 8) | (dplc_script[frame + 1] << 0);

    // Read number of entries
    int16_t entries = (int16_t)((dplc_script[0] << 8) | dplc_script[1]) - 1; // the count is a word (Sonic 2 format)
    dplc_script += 2;
    if (entries < 0)
        return;

    // Start reading data
    uint8_t* top = sgfx_buffer;
    sonframe_chg = true;

    do {
        // Read entry
        uint16_t tile = *dplc_script++;
        uint8_t tiles = tile >> 4;
        tile = ((tile << 8) | (*dplc_script++)) << 5;

        const uint8_t* fromp = Art_Sonic + tile;
        do {
            memcpy(top, fromp, 0x20);
            fromp += 0x20;
            top += 0x20;
        } while (tiles-- > 0);
    } while (entries-- > 0);
}

// Sonic collision functions
void Sonic_ResetOnFloor(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Set state
    self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    self->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    self->status &= (uint8_t)~SST_PSTATUS_ROLL_JUMP;

    if (((self->status & SST_PSTATUS_IN_BALL) != 0)) {
        self->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
        self->y_radius = SONIC_HEIGHT;
        self->x_radius = SONIC_WIDTH;
        self->anim = SonAnimId_Walk;
        self->y_pos.f.u -= SONIC_BALL_SHIFT;
    }

    scratch->jumping = false;
    item_bonus = 0;
}

static int16_t Sonic_Angle(Sst *self, int16_t dist0, int16_t dist1) {
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

static void Sonic_AnglePos(Sst *self) {
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
        if ((dist = Sonic_Angle(self, dist0, dist1)) != 0) {
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
        if ((dist = Sonic_Angle(self, dist0, dist1)) != 0) {
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
        if ((dist = Sonic_Angle(self, dist0, dist1)) != 0) {
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
        if ((dist = Sonic_Angle(self, dist0, dist1)) != 0) {
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

static void Sonic_Floor(Sst *self) {
    // Get angle we're moving in
    // There's some weird logging stuff done here
    // Maybe testing if the CalcAngle is yielding desirable results?
    uint8_t angle = CalcAngle(self->x_vel, self->y_vel);
    dbg_ang0 = angle;
    angle -= 0x20;
    dbg_ang1 = angle;
    angle &= 0xC0;
    dbg_ang2 = angle;

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
        dbg_ang3 = dist1; //...What?

        clip = -((self->y_vel >> 8) + 8);
        if (dist1 < 0 && (dist0 >= clip || dist1 >= clip)) {
            // Set state
            self->y_pos.f.u += dist1;
            self->angle = hit_angle;
            Sonic_ResetOnFloor(self);
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
                Sonic_ResetOnFloor(self);
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
                Sonic_ResetOnFloor(self);
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
                Sonic_ResetOnFloor(self);
                self->anim = SonAnimId_Walk;

                // Get inertia
                self->y_vel = 0;
                self->ground_vel = self->x_vel;
            }
        }
        break;
    }
}

static void Sonic_HurtStop(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Die when falling below the level
    if ((limit_btm2 + SCREEN_HEIGHT) < self->y_pos.f.u) {
        KillSonicFrom(self, false); // a1 isn't set
        return;
    }

    // Do collision
    Sonic_Floor(self);
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

// The reactions to an object on the status table of Sonic 3 & Knuckles (Sst.h): its collision flags and property, status bit 7 (the touch flag) and own word 0xB (the points' index)
static signed int React_ChkHurt(Sst *self, Sst *hit) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;
    if (invincibility)
        return -1;
    if (scratch->flash_time)
        return -1;
    return HurtSonicFrom(self, hit->x_pos.f.u, Obj_Spikes != NULL && hit->code == Obj_Spikes);
}

// An object made into another one (a badnik into its explosion)
static void Sst_BecomeObject(Sst *hit, ObjectId id) {
    hit->code = sst_object_codes[id];
    hit->routine = 0;
}

static signed int React_Enemy(Sst *self, Sst *hit) {
    if (!(invincibility || self->anim == SonAnimId_Roll))
        return React_ChkHurt(self, hit);

    if (hit->collision_property) { // a boss
        self->x_vel = -self->x_vel >> 1;
        self->y_vel = -self->y_vel >> 1;
        hit->collision_flags = 0;
        if (!(--hit->collision_property))
            hit->status |= 0x80;
    } else {
        hit->status |= 0x80;
        uint8_t bonus = item_bonus;
        item_bonus += 2;
        hit->own.u16[0xB] = bonus;
        static const uint16_t points[] = { 10, 20, 50, 100 };
        uint16_t point_bonus = points[bonus >> 1];
        if (item_bonus >= 32) {
            point_bonus = 1000;
            hit->own.u16[0xB] = 10;
        }
        AddPoints(point_bonus);
        Sst_BecomeObject(hit, ObjId_Explosion);
        if (self->y_vel >= 0) {
            if (self->y_pos.f.u < hit->y_pos.f.u)
                self->y_vel = -self->y_vel;
            else
                self->y_vel -= 0x100;
        } else {
            self->y_vel += 0x100;
        }
    }
    return 0;
}

static signed int React_Monitor(Sst *self, Sst *hit) {
    if (self->y_vel < 0) {
        uint16_t chky = self->y_pos.f.u - 0x10;
        if ((uint16_t)hit->y_pos.f.u < chky) {
            self->y_vel = -self->y_vel;
            hit->y_vel = -0x180;
            if (!hit->routine_secondary)
                hit->routine_secondary += 4;
        }
    } else if (self->anim == SonAnimId_Roll) {
        self->y_vel = -self->y_vel;
        hit->routine += 2;
    }
    return 0;
}

signed int ReactToItem(Sst *self) {
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

    // Iterate through the objects
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
        Sst *hit = &sst[i];
        if (hit->code == NULL || !(hit->render_flags & SST_RENDER_ON_SCREEN) || !hit->collision_flags)
            continue;
        const uint8_t *sizep = obj_sizes[hit->collision_flags & 0x3F];
        uint8_t hit_width = *sizep++;
        uint8_t hit_height = *sizep++;
        int16_t x_diff = x - (hit->x_pos.f.u - hit_width);
        int16_t y_diff = y - (hit->y_pos.f.u - hit_height);
        if (x_diff >= -width && x_diff <= hit_width * 2 && y_diff >= -height && y_diff <= hit_height * 2) {
            switch (hit->collision_flags & 0xC0) {
            case 0x00: // Enemy
                return React_Enemy(self, hit);
            case 0xC0: // Special
                if ((hit->collision_flags & 0x3F) == 0x0B) { // Caterkiller spiked body segment
                    hit->status |= 0x80;
                    return React_ChkHurt(self, hit);
                }
                if ((hit->collision_flags & 0x3F) == 0x0C) { // Yadrin
                    int16_t penetration = (int16_t)((y + height) - (hit->y_pos.f.u - hit_height));
                    if (penetration < 8) {
                        int16_t face_left = (int16_t)(hit->x_pos.f.u - 4);
                        if (hit->status & SST_STATUS_X_FLIP)
                            face_left -= 16;
                        if (x < (int16_t)(face_left + 24) && (int16_t)(x + width) > face_left)
                            return React_ChkHurt(self, hit);
                    }
                    return React_Enemy(self, hit);
                }
                if ((hit->collision_flags & 0x3F) == 0x17 || (hit->collision_flags & 0x3F) == 0x21) { // SYZ bumper / LZ pole
                    hit->collision_property++;
                    return 0;
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

// Sonic functions
// HurtSonic for whatever hurt him: the X it was at (he is thrown away from it) and whether it was spikes (the sound)
signed int HurtSonicFrom(Sst *self, int16_t src_x, bool spikes)
{
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;
    DEBUG_LOG("player", "hurt at (%d,%d) rings=%u shield=%u", self->x_pos.f.u, self->y_pos.f.u, rings, shield);

    // Lose rings and shield
    if (!shield) {
        if (rings) {
            // Spawn lost rings object
            Sst *rings = Sst_Allocate();
            if (rings != NULL) {
                rings->code = Obj_RingLoss;
                rings->x_pos.f.u = self->x_pos.f.u;
                rings->y_pos.f.u = self->y_pos.f.u;
            }
        } else if (!debug_mode) {
            // Die
            return KillSonicFrom(self, spikes);
        }
    }
    shield = 0;

    // Set Sonic state
    self->routine = 4;
    spindash_flag &= ~1;
    Splash_SetAnim(SplashAnim_Null);
    Sonic_ResetOnFloor(self);
    self->status |= SST_PSTATUS_IN_AIR;

    self->x_vel = ((self->status & SST_PSTATUS_UNDERWATER) != 0) ? -0x100 : -0x200;
    self->y_vel = ((self->status & SST_PSTATUS_UNDERWATER) != 0) ? -0x200 : -0x400;
    if (self->x_pos.f.u >= src_x)
        self->x_vel = -self->x_vel;
    self->ground_vel = 0;

    self->anim = SonAnimId_Hurt;
    scratch->flash_time = 120;

    // LZ harpoon isn't ported yet -- only checking for Spikes here.
    PlaySound(spikes ? sfx_HitSpikes : sfx_Death);
    return -1;
}

int32_t KillSonicFrom(Sst *self, bool spikes) {
    DEBUG_LOG("player", "killed at (%d,%d) lives=%u rings=%u", self->x_pos.f.u, self->y_pos.f.u, lives, rings);
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Check if we can be killed
    if (debug_use)
        return -1;

    // Set state
    invincibility = false;
    self->routine = 6;
    Sonic_ResetOnFloor(self);
    self->status |= SST_PSTATUS_IN_AIR;
    self->y_vel = -0x700;
    self->x_vel = 0;
    self->ground_vel = 0;
    scratch->x38.death_y = self->y_pos.f.u;
    self->anim = SonAnimId_Death;
    self->art_tile |= TILE_PRIORITY_AND;

    // Play death sound (LZ harpoon isn't ported yet -- only Spikes here)
    PlaySound(spikes ? sfx_HitSpikes : sfx_Death);
    return -1;
}

// Sonic movement functions
static bool Sonic_Jump(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Don't jump if ABC isn't pressed
    if (!(jpad1_press2 & (JPAD_A | JPAD_C | JPAD_B)))
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

    self->y_radius = SONIC_HEIGHT; // No idea why this is here
    self->x_radius = SONIC_WIDTH;

    if (!((self->status & SST_PSTATUS_IN_BALL) != 0)) {
        self->y_radius = SONIC_BALL_HEIGHT;
        self->x_radius = SONIC_BALL_WIDTH;
        self->anim = SonAnimId_Roll;
        self->status |= SST_PSTATUS_IN_BALL;
        self->y_pos.f.u += SONIC_BALL_SHIFT;
    } else {
        self->status |= SST_PSTATUS_ROLL_JUMP;
    }
    return true;
}

static void Sonic_SlopeResist(Sst *self) {
    if (((self->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(self->angle) * 0x20) >> 8;
    if (self->ground_vel != 0)
        self->ground_vel += force;
}

static void Sonic_MoveLeft(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia <= 0) {
        // Turn around
        if (!((self->status & SST_PSTATUS_X_FLIP) != 0)) {
            self->status |= SST_PSTATUS_X_FLIP;
            self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
            self->prev_anim = 1; // Reset animation
        }

        // Accelerate
        if ((inertia -= sonspeed_acc) <= -sonspeed_max)
            inertia = -sonspeed_max;

        // Set speed and animation
        self->ground_vel = inertia;
        self->anim = SonAnimId_Walk;
    } else {
        // Decelerate
        if ((inertia -= sonspeed_dec) < 0)
            inertia = -0x80;
        self->ground_vel = inertia;

        // Skid
        if (((self->angle + 0x20) & 0xC0) == 0x00 && inertia >= 0x400) {
            self->anim = SonAnimId_Stop;
            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
            PlaySound(sfx_Skid);
            if (air >= 12) {
                if (skid_dust_timer == 0) {
                    skid_dust_timer = 4;
                    Splash_SpawnSkidDust(self);
                } else {
                    skid_dust_timer--;
                }
            }
        }
    }
}

static void Sonic_MoveRight(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia >= 0) {
        // Turn around
        if (((self->status & SST_PSTATUS_X_FLIP) != 0)) {
            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
            self->status &= (uint8_t)~SST_PSTATUS_PUSHING;
            self->prev_anim = 1; // Reset animation
        }

        // Accelerate
        if ((inertia += sonspeed_acc) >= sonspeed_max)
            inertia = sonspeed_max;

        // Set speed and animation
        self->ground_vel = inertia;
        self->anim = SonAnimId_Walk;
    } else {
        // Decelerate
        if ((inertia += sonspeed_dec) >= 0)
            inertia = 0x80;
        self->ground_vel = inertia;

        // Skid
        if (((self->angle + 0x20) & 0xC0) == 0x00 && inertia <= -0x400) {
            self->anim = SonAnimId_Stop;
            self->status |= SST_PSTATUS_X_FLIP;
            PlaySound(sfx_Skid);
            if (air >= 12) {
                if (skid_dust_timer == 0) {
                    skid_dust_timer = 4;
                    Splash_SpawnSkidDust(self);
                } else {
                    skid_dust_timer--;
                }
            }
        }
    }
}

static void Sonic_Move(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    if (f_slidemode)
        goto Sonic_AngleSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad1_hold2 & JPAD_LEFT)
                Sonic_MoveLeft(self);
            if (jpad1_hold2 & JPAD_RIGHT)
                Sonic_MoveRight(self);

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
                        goto Sonic_ResetScr;
                    }
                } else {
                    // Balance on level
                    if (SstFloorDist(self, self->x_pos.f.u) >= 12) {
                        if (scratch->front_angle == 3) {
                            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
                            self->anim = SonAnimId_Balance;
                            goto Sonic_ResetScr;
                        }
                        if (scratch->back_angle == 3) {
                            self->status |= SST_PSTATUS_X_FLIP;
                            self->anim = SonAnimId_Balance;
                            goto Sonic_ResetScr;
                        }
                    }
                }

            // Handle looking up and down
            LookUpDown:;
                if (jpad1_hold2 & JPAD_UP) {
                    self->anim = SonAnimId_LookUp;
                    // Wait 2 seconds (120 frames) before the camera actually
                    // starts panning up, matching the Spin Dash guide's
                    // camera-delay addition
                    if (cam_y_delay < 120) {
                        cam_y_delay++;
                        goto Sonic_ResetScr_Part2;
                    }
                    cam_y_delay = 120;
                    if (look_shift != (200 + SCREEN_TALLADD2))
                        look_shift += 2;
                    goto DoFriction;
                }
                if (jpad1_hold2 & JPAD_DOWN) {
                    self->anim = SonAnimId_Duck;
                    if (cam_y_delay < 120) {
                        cam_y_delay++;
                        goto Sonic_ResetScr_Part2;
                    }
                    cam_y_delay = 120;
                    if (look_shift != (8 + SCREEN_TALLADD2))
                        look_shift -= 2;
                    goto DoFriction;
                }
            }
        }

    // Reset camera to neutral position
    Sonic_ResetScr:;
        cam_y_delay = 0;

    Sonic_ResetScr_Part2:;
        if (look_shift < (96 + SCREEN_TALLADD2))
            look_shift += 2;
        else if (look_shift > (96 + SCREEN_TALLADD2))
            look_shift -= 2;

    // Friction
    DoFriction:;
        if (!(jpad1_hold2 & (JPAD_LEFT | JPAD_RIGHT))) {
            if (self->ground_vel > 0) {
                if ((self->ground_vel -= sonspeed_acc) < 0)
                    self->ground_vel = 0;
            } else if (self->ground_vel < 0) {
                if ((self->ground_vel += sonspeed_acc) >= 0)
                    self->ground_vel = 0;
            }
        }
    }

    // Calculate global speed from inertia
Sonic_AngleSpeed:;
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

void Sonic_ChkRoll(Sst *self) {
    // Enter roll state
    if (((self->status & SST_PSTATUS_IN_BALL) != 0))
        return;
    self->status |= SST_PSTATUS_IN_BALL;
    self->y_radius = SONIC_BALL_HEIGHT;
    self->x_radius = SONIC_BALL_WIDTH;
    self->anim = SonAnimId_Roll;
    self->y_pos.f.u += SONIC_BALL_SHIFT;
    PlaySound(sfx_Roll);

    // Set speed (S-tubes)
    if (!self->ground_vel)
        self->ground_vel = 0x200;
}

static void Sonic_Roll(Sst *self) {
    // Don't allow rolling while on a water slide
    if (f_slidemode)
        return;

    // Check if we can and are trying to roll
    if (jump_only || ((self->ground_vel < 0) ? -self->ground_vel : self->ground_vel) < 0x80)
        return;
    if ((jpad1_hold2 & (JPAD_LEFT | JPAD_RIGHT)) || !(jpad1_hold2 & JPAD_DOWN))
        return;
    Sonic_ChkRoll(self);
}

static void Sonic_LevelBound(Sst *self) {
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
        KillSonicFrom(self, false);
    }
}

static void Sonic_SlopeRepel(Sst *self) {
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

static void Sonic_RollRepel(Sst *self) {
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

static void Sonic_RollLeft(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia <= 0) {
        // Set animation
        self->status |= SST_PSTATUS_X_FLIP;
        self->anim = SonAnimId_Roll;
    } else {
        // Decelerate
        if ((inertia -= sonspeed_dec >> 2) < 0)
            inertia = -0x80;
        self->ground_vel = inertia;
    }
}

static void Sonic_RollRight(Sst *self) {
    int16_t inertia = self->ground_vel;
    if (inertia >= 0) {
        // Set animation
        self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        self->anim = SonAnimId_Roll;
    } else {
        // Decelerate
        if ((inertia += sonspeed_dec >> 2) >= 0)
            inertia = 0x80;
        self->ground_vel = inertia;
    }
}

static void Sonic_RollSpeed(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    if (f_slidemode)
        goto Sonic_AngledRollSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad1_hold2 & JPAD_LEFT)
                Sonic_RollLeft(self);
            if (jpad1_hold2 & JPAD_RIGHT)
                Sonic_RollRight(self);
        }

        // Friction
        if (self->ground_vel > 0) {
            if ((self->ground_vel -= (sonspeed_acc >> 1)) < 0)
                self->ground_vel = 0;
        } else {
            if ((self->ground_vel += (sonspeed_acc >> 1)) >= 0)
                self->ground_vel = 0;
        }

        // Uncurl when we've come to a stop -- unless must_roll (GHZTunnel)
        // is forcing us to stay rolling, in which case give an extra push
        // in whichever direction we're currently facing instead (real
        // Sonic_KeepRolling: "magically gives Sonic an extra push if he's
        // going to stop rolling where it's not allowed, such as in an
        // S-curve").
        if (self->ground_vel == 0) {
            if (((self->status & SST_PSTATUS_MUST_ROLL) != 0)) {
                self->ground_vel = ((self->status & SST_PSTATUS_X_FLIP) != 0) ? (int16_t)-0x400 : (int16_t)0x400;
            } else {
                self->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
                self->y_radius = SONIC_HEIGHT;
                self->x_radius = SONIC_WIDTH;
                self->anim = SonAnimId_Wait;
                self->y_pos.f.u -= SONIC_BALL_SHIFT;
            }
        }
    }

    // Calculate global speed from inertia
Sonic_AngledRollSpeed:;
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

static void Sonic_JumpHeight(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    if (scratch->jumping) {
        // Get minimum jump speed and apply if ABC isn't held
        int16_t spd = ((self->status & SST_PSTATUS_UNDERWATER) != 0) ? -0x200 : -0x400;
        if (self->y_vel < spd && !(jpad1_hold2 & (JPAD_A | JPAD_C | JPAD_B)))
            self->y_vel = spd;
    } else {
        // Cap upwards speed
        if (self->y_vel < -0xFC0)
            self->y_vel = -0xFC0;
    }
}

static void Sonic_JumpDirection(Sst *self) {
    // Handle acceleration
    if (!((self->status & SST_PSTATUS_ROLL_JUMP) != 0)) {
        int16_t xsp = self->x_vel;

        // Accelerate left
        if (jpad1_hold2 & JPAD_LEFT) {
            self->status |= SST_PSTATUS_X_FLIP;
            if ((xsp -= (sonspeed_acc << 1)) <= -sonspeed_max)
                xsp = -sonspeed_max;
        }

        // Accelerate right
        if (jpad1_hold2 & JPAD_RIGHT) {
            self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
            if ((xsp += (sonspeed_acc << 1)) >= sonspeed_max)
                xsp = sonspeed_max;
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

static void Sonic_JumpAngle(Sst *self) {
    // Reset angle towards 0
    uint8_t angle = self->angle;
    if (angle == 0)
        return;

    if (!(angle & 0x80)) {
        if ((angle -= 2) & 0x80)
            angle = 0;
    } else {
        if (!((angle += 2) & 0x80))
            angle = 0;
    }

    self->angle = angle;
}

// Spin Dash
static void Sonic_Spindash_ResetScr(Sst *self) {
    // Reset camera to neutral position (same logic as Sonic_Move/Sonic_JumpDirection)
    if (look_shift < (96 + SCREEN_TALLADD2))
        look_shift += 2;
    else if (look_shift > (96 + SCREEN_TALLADD2))
        look_shift -= 2;

    // Since we skip the rest of the normal-movement case, run these manually
    Sonic_LevelBound(self);
    Sonic_AnglePos(self);
}

static void Sonic_ChargingSpindash(Sst *self) {
    self->anim = SonAnimId_SpinDash; // make sure Spin Dash animation stays

    // Charge decay
    spindash_count -= spindash_count >> 5;

    if (!(jpad1_press2 & (JPAD_A | JPAD_C | JPAD_B))) {
        Sonic_Spindash_ResetScr(self);
        return;
    }

    // Restart Spin Dash animation
    self->anim = SonAnimId_SpinDash;
    self->anim_frame = 0;
    self->anim_frame_timer = 0;
    PlaySound(sfx_SpindashRev);

    spindash_count += 0x200;
    if (spindash_count > 0x800)
        spindash_count = 0x800;

    Sonic_Spindash_ResetScr(self);
}

static void Sonic_ReleaseSpindash(Sst *self) {
    spindash_flag &= ~1;
    self->y_radius = SONIC_BALL_HEIGHT;
    self->x_radius = SONIC_BALL_WIDTH;
    self->y_pos.f.u += SONIC_BALL_SHIFT;
    self->anim = SonAnimId_Roll;
    self->status |= SST_PSTATUS_IN_BALL;
    PlaySound(sfx_Teleport);
    Splash_SetAnim(SplashAnim_Null);

    // Get release speed from number of revs performed
    int16_t rev = (int16_t)(spindash_count >> 1);
    int16_t speed = rev + 0x800;
    if (((self->status & SST_PSTATUS_X_FLIP) != 0))
        speed = -speed;
    self->ground_vel = speed;

    // Camera delay (based on rev count, before the base speed was added)
    uint16_t cam = (uint16_t)rev;
    cam <<= 1;
    cam &= 0x1F00;
    cam = (uint16_t)(-(int16_t)cam);
    cam += 0x2000;
    cam_x_delay = cam;

    // Set new velocities immediately (same convention as Sonic_Move/Sonic_RollSpeed: cos->x_vel, sin->y_vel)
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);
    self->x_vel = (int16_t)(((int32_t)cos * self->ground_vel) >> 8);
    self->y_vel = (int16_t)(((int32_t)sin * self->ground_vel) >> 8);

    Sonic_Spindash_ResetScr(self);
}

static void Sonic_UpdateSpindash(Sst *self) {
    // Bug fix (part 2 of the monitor fix): guarantee the Spin Dash
    // animation is set as soon as we know we're spin dashing, before
    // Monitor's own Mon_SolidSides push-vs-break check (which now also
    // exempts this animation, see Monitor.c) gets a chance to run this
    // same frame and see a stale Push animation instead.
    self->anim = SonAnimId_SpinDash;
    if (jpad1_hold2 & JPAD_DOWN) {
        Sonic_ChargingSpindash(self);
        return;
    }
    Sonic_ReleaseSpindash(self);
}

// Abandons a spin dash that is still charging when Sonic leaves the ground some other
// way than releasing it (e.g. a seesaw launch). Clearing only the flag left the dust
// companion in its Dash animation, so it kept following Sonic through the air.
void Sonic_CancelSpindash(void) {
    if (!(spindash_flag & 1))
        return;
    spindash_flag &= ~1;
    spindash_count = 0;
    if (Splash_GetAnim() == SplashAnim_Dash)
        Splash_SetAnim(SplashAnim_Null);
}

static bool Sonic_SpinDash(Sst *self) {
    if (spindash_flag & 1) {
        Sonic_UpdateSpindash(self);
        return true;
    }

    if (self->anim != SonAnimId_Duck)
        return false;
    if (!(jpad1_press2 & (JPAD_A | JPAD_C | JPAD_B)))
        return false;

    self->anim = SonAnimId_SpinDash;
    PlaySound(sfx_SpindashRev);
    spindash_flag |= 1;
    spindash_count = 0;
    if (air >= 12)
        Splash_SetAnim(SplashAnim_Dash);

    // Because we're skipping the rest of the normal-movement case
    Sonic_LevelBound(self);
    Sonic_AnglePos(self);
    return true;
}

// Water entry/exit: adjusts speed for being underwater, spawns the splash
// effect, and (on entry) spawns the drowning countdown's master tracker
// (Object/DrownCount.h) at its fixed slot. LZWindTunnels/LZWaterSlides
// (movement inside wind tunnel/water-slide chunks) are in LZWaterFeatures.c.
static void Sonic_Water(Sst *self) {
    if (LEVEL_ZONE(level_id) != ZoneId_LZ)
        return;

    if (self->y_pos.f.u < wtr_pos1) {
        // Above water
        if (!((self->status & SST_PSTATUS_UNDERWATER) != 0))
            return;
        self->status &= (uint8_t)~SST_PSTATUS_UNDERWATER;
        ResumeMusic(); // replenish air and resume music if necessary

        sonspeed_max = 0x600;
        sonspeed_acc = 0xC;
        sonspeed_dec = 0x80;
        if (shoes) {
            sonspeed_max = 0xC00;
            sonspeed_acc = 0x18;
        }

        self->y_vel = (int16_t)(self->y_vel << 1); // double Y-speed while exiting water
        if (self->y_vel == 0)
            return;
        if (self->y_vel < -0x1000)
            self->y_vel = -0x1000; // cap max speed on leaving water

        Splash_SetAnim(SplashAnim_Splash);
        PlaySound(sfx_Splash);
    } else {
        // Underwater
        if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
            return;
        self->status |= SST_PSTATUS_UNDERWATER;
        ResumeMusic(); // replenish air (music won't resume here, we've only just entered water)

        DrownCount_StartMaster();

        sonspeed_max = 0x300;
        sonspeed_acc = 0x6;
        sonspeed_dec = 0x40;
        if (shoes) {
            sonspeed_max = 0x600;
            sonspeed_acc = 0xC;
            sonspeed_dec = 0x80;
        }

        self->x_vel >>= 1; // half X-speed when entering water
        self->y_vel >>= 2; // quarter Y-speed when entering water
        if (self->y_vel == 0)
            return;

        Splash_SetAnim(SplashAnim_Splash);
        PlaySound(sfx_Splash);
    }
}

// Other functions
static void GameOver(Sst *self) {
    Scratch_Sonic* scratch = (Scratch_Sonic *)self->own.u8;

    // Have we fallen below the screen?
    if ((limit_btm2 + 256 + SCREEN_TALLADD) >= self->y_pos.f.u)
        return;

    // Enter respawn state
    self->y_vel = -0x38; //???
    self->routine += 2;
    time_count = false;
    life_count++;

    // Check if we've game over'ed
    if (--lives == 0) {
        // Set death timer
        scratch->death_timer = 0;

        // Load 'GAME OVER' objects
        Sst_SpawnCode(Obj_GameOverCard, 0, 0); // GAME
        Sst *over = Sst_SpawnCode(Obj_GameOverCard, 0, 0); // OVER
        if (over != NULL)
            over->mapping_frame = 1;

        time_over = false;
        PlayMusic(bgm_GameOver);
        AddPLC(PlcId_GameOver);
    } else {
        // Set death timer
        scratch->death_timer = 60;

        // Check if we've time over'ed
        if (time_over) {
            // Set death timer
            scratch->death_timer = 0;

            // Load 'TIME OVER' objects
            Sst *time_card = Sst_SpawnCode(Obj_GameOverCard, 0, 0); // TIME
            Sst *over_card = Sst_SpawnCode(Obj_GameOverCard, 0, 0); // OVER
            if (time_card != NULL)
                time_card->mapping_frame = 2;
            if (over_card != NULL)
                over_card->mapping_frame = 3;

            PlayMusic(bgm_GameOver);
            AddPLC(PlcId_GameOver);
        }
    }
}

#define DEBUG_MOVE_DELAY 12  // frames to wait when holding D-Pad before accelerating
#define DEBUG_START_SPEED 15 // initial movement speed when first holding D-Pad

extern const uint8_t Mappings_RingREV01[]; // From Object/Ring.c

// Looks up one frame's own piece data in a normal (non-raw) per-frame
// mapping table -- same lookup BuildSprites does (Mappings.h).
static const uint8_t *DebugMode_LookupFrame(const uint8_t *mappings, uint8_t frame, uint8_t *piece_count_out) {
    const uint8_t *pieces;
    *piece_count_out = (uint8_t)Mappings_FramePieces(mappings, frame, &pieces);
    return pieces;
}

// Scratch buffer a subtype variant's frame stack gets assembled into --
// composed once per preview refresh (item change / subtype adjust), not
// per-frame, so it doesn't need double-buffering against BuildSprites
// reading the previous frame's contents. Header shape matches a normal
// (non-raw) single-frame mapping table: 2-byte offset (always 2, pointing
// right past itself) + the piece count word + that many pieces (Mappings.h).
#define DEBUG_STACK_MAX_PIECES 16
static uint8_t debug_stack_buffer[2 + SPRITE_FRAME_HEADER_SIZE + DEBUG_STACK_MAX_PIECES * SPRITE_PIECE_SIZE_S3];

// Composites a variant's frame stack (per your direction: several of the
// object's own real frames, each independently offset, drawn together --
// e.g. a Monitor's box frame plus its content icon frame) into
// debug_stack_buffer, and points self at it.
// A stacked layer's own piece coordinate plus its DebugFramePiece offset can
// exceed what the preview should show (Ring's row/column preview in
// particular -- 6 extra rings at 0x20 apart reaches 192). Clamping to the
// range a Sonic 1 sprite piece could hold keeps the preview visually
// sane (it just compresses toward the screen-relative edge) rather than
// producing garbage jumbled positions.
static int8_t DebugMode_ClampOffset(int value) {
    if (value < -128)
        return -128;
    if (value > 127)
        return 127;
    return (int8_t)value;
}

static void DebugMode_BuildStack(Sst *self, const uint8_t *base_mappings, const DebugFramePiece *stack, uint8_t stack_count) {
    uint8_t total = 0;
    uint8_t *out = debug_stack_buffer + 2 + SPRITE_FRAME_HEADER_SIZE;
    for (uint8_t i = 0; i < stack_count; i++) {
        uint8_t count;
        const uint8_t *pieces = DebugMode_LookupFrame(base_mappings, stack[i].frame, &count);
        for (uint8_t p = 0; p < count && total < DEBUG_STACK_MAX_PIECES; p++, total++, pieces += SPRITE_PIECE_SIZE_S3) {
            int8_t x = DebugMode_ClampOffset(Mappings_PieceXS3(pieces) + stack[i].x_off);
            *out++ = (uint8_t)DebugMode_ClampOffset(Mappings_PieceY(pieces) + stack[i].y_off); // ypos
            *out++ = pieces[1];                                                                // size
            *out++ = pieces[2];                                                                // tile hi
            *out++ = pieces[3];                                                                // tile lo
            *out++ = (uint8_t)(x < 0 ? 0xFF : 0x00);                                           // xpos (word)
            *out++ = (uint8_t)x;
        }
    }
    debug_stack_buffer[0] = 0;
    debug_stack_buffer[1] = 2; // offset from buffer start to the piece-count word at index 2
    debug_stack_buffer[2] = 0;
    debug_stack_buffer[3] = total;

    self->mappings = debug_stack_buffer;
    self->mapping_frame = 0;
    self->render_flags &= (uint8_t)~SST_RENDER_STATIC;
}

// Refreshes the debug object's displayed sprite for whatever's currently
// selected in the active zone's DebugList, at the current debug_subtype --
// matches Debug_ShowItem, extended (per your direction) to also update the
// preview as debug_subtype itself is adjusted -- including compositing a
// multi-frame stack when the matched variant has one -- so what you see is
// what you'll actually place. A NULL_ENTRY placeholder (or any real entry
// that forgot to set mappings) shows a Ring instead of nothing/garbage --
// purely a safety fallback, since spawning is still blocked by type ==
// ObjId_Null regardless of what the preview sprite looks like.
// The preview is drawn by the character (the status table's renderer: Sonic 3 & Knuckles' 6-byte pieces), so every entry's mappings are in that format.
static void DebugMode_RefreshPreview(Sst *self, const DebugListEntry *item) {
    const uint8_t *preview_mappings = (const uint8_t *)item->mappings;
    if (preview_mappings == NULL) {
        self->mappings = Mappings_RingREV01;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
        self->mapping_frame = 0;
        self->render_flags &= (uint8_t)~SST_RENDER_STATIC;
        return;
    }

    self->mappings = preview_mappings;
    self->art_tile = item->tile;
    self->mapping_frame = item->frame;
    self->render_flags &= (uint8_t)~SST_RENDER_STATIC;

    if (item->variants != NULL) {
        uint8_t key = (uint8_t)((debug_subtype >> item->variant_shift) & item->variant_mask);
        for (uint8_t i = 0; i < item->variant_count; i++) {
            if (item->variants[i].subtype_key != key)
                continue;
            const DebugSubtypeVariant *v = &item->variants[i];
            DebugMode_BuildStack(self, preview_mappings, v->stack, v->stack_count);
            uint16_t base_tile = v->tile_override ? v->tile_override : item->tile;
            if (v->flip || v->pal_add || v->tile_override) {
                uint16_t tile = base_tile & (uint16_t)~(TILE_PALETTE_AND | TILE_X_FLIP_AND | TILE_Y_FLIP_AND);
                uint16_t pal = (uint16_t)(((base_tile & TILE_PALETTE_AND) >> TILE_PALETTE_SHIFT) + v->pal_add) & 3;
                tile |= (pal << TILE_PALETTE_SHIFT) & TILE_PALETTE_AND;
                if (v->flip & 1)
                    tile |= TILE_X_FLIP_AND;
                if (v->flip & 2)
                    tile |= TILE_Y_FLIP_AND;
                self->art_tile = tile;
            }
            break;
        }
    }
}

// Steps debug_subtype to the next/previous value -- per your direction, an
// item with a variants table can ONLY ever land on one of that table's own
// listed subtype_key values (wrapping at each end), never anything outside
// it, so cycling can never produce an undefined subtype (e.g. a Monitor
// past Goggles, which would read past Mappings_Monitor's real table). Only
// the bits covered by variant_shift/variant_mask change -- other bits in
// the subtype byte (e.g. Spikes' movement-type nibble, which variants
// doesn't cover) are left alone. Entries with no variants table (subtype
// only affects size/position, e.g. InvisibleBarrier) keep the old
// free ++/-- behavior -- there's no unsafe value to avoid there.
static void DebugMode_StepSubtype(const DebugListEntry *item, bool dec) {
    if (item->variants == NULL || item->variant_count == 0) {
        if (dec)
            debug_subtype--;
        else
            debug_subtype++;
        return;
    }

    uint8_t key = (uint8_t)((debug_subtype >> item->variant_shift) & item->variant_mask);
    int idx = -1;
    for (uint8_t i = 0; i < item->variant_count; i++) {
        if (item->variants[i].subtype_key == key) {
            idx = i;
            break;
        }
    }

    int new_idx;
    if (idx < 0)
        new_idx = 0; // current value isn't a listed one -- snap to the first defined entry
    else if (dec)
        new_idx = (idx == 0) ? (int)item->variant_count - 1 : idx - 1;
    else
        new_idx = (idx == (int)item->variant_count - 1) ? 0 : idx + 1;

    uint8_t new_key = item->variants[new_idx].subtype_key;
    uint8_t shifted_mask = (uint8_t)(item->variant_mask << item->variant_shift);
    debug_subtype = (uint8_t)((debug_subtype & (uint8_t)~shifted_mask) | ((new_key << item->variant_shift) & shifted_mask));
}

// Selects a (possibly new) item from the active zone's DebugList, resetting
// debug_subtype to that item's own default, and refreshes the preview.
static void DebugMode_ShowItem(Sst *self) {
    int count;
    const DebugListEntry *list = DebugList_Get(&count);
    if (debug_item >= count)
        debug_item = 0;
    const DebugListEntry *item = &list[debug_item];
    debug_subtype = item->subtype;
    DebugMode_RefreshPreview(self, item);
}

// Debug (object placement) mode -- entered by pressing B while debug_cheat
// is active (see the "Enter debug mode" check below), exited by pressing B
// again. debug_use doubles as a routine selector matching the
// disassembly's DebugMode: 1 (set by that B-press check) means "just
// entered, run one-time setup", 2 means "already active".
void Sonic_DebugMode(Sst *self) {
    if (debug_use == 1) {
        // One-time setup: temporarily unlock the level's Y boundaries (so
        // debug mode can fly anywhere), pick a valid starting item, and
        // show its placeholder sprite in place of Sonic.
        if (gamemode == GameMode_Special) {
            // No level bounds to unlock here; freeze the stage rotation so the
            // view holds still while placing objects.
            ss_rotate = 0;
            ss_angle.f.u = 0;
        } else {
            limit_top_db = limit_top2;
            limit_btm_db = limit_btm1;
            limit_top2 = 0;
            limit_btm1 = 0x800 - 224;
        }

        self->render_flags = SST_RENDER_LEVEL; // self->pos tracks world/playfield space (see the free-movement code below), not raw screen coords -- BuildSprites needs this to convert it correctly
        self->priority = 2 * 0x80;
        self->width_pixels = 8;
        DebugMode_ShowItem(self);

        debug_speed_timer = DEBUG_MOVE_DELAY;
        debug_speed = DEBUG_START_SPEED;
        debug_use = 2;
    }

    // Free movement (matches Debug_Move): held D-Pad accelerates from
    // DEBUG_START_SPEED up to a max of 0xFF over time, released D-Pad
    // resets back to the slow initial speed next time it's held again.
    uint8_t held_dir = jpad1_hold1 & (JPAD_UP | JPAD_DOWN | JPAD_LEFT | JPAD_RIGHT);
    if (!(jpad1_press1 & (JPAD_UP | JPAD_DOWN | JPAD_LEFT | JPAD_RIGHT))) {
        if (held_dir) {
            if (--debug_speed_timer == 0) {
                debug_speed_timer = 1; // keeps retriggering every frame once ramped
                if (++debug_speed == 0)
                    debug_speed = 0xFF; // clamp at max once it wraps
            }
        } else {
            debug_speed_timer = DEBUG_MOVE_DELAY;
            debug_speed = DEBUG_START_SPEED;
        }
    }

    if (held_dir) {
        int32_t speed = ((int32_t)(debug_speed + 1)) << (16 - 4); // /16, matches the real >>4 subpixel scale-down
        if (jpad1_hold1 & JPAD_UP)
            self->y_pos.v -= speed;
        if (jpad1_hold1 & JPAD_DOWN)
            self->y_pos.v += speed;
        if (jpad1_hold1 & JPAD_LEFT)
            self->x_pos.v -= speed;
        if (jpad1_hold1 & JPAD_RIGHT)
            self->x_pos.v += speed;
    }

    // Item cycling / spawning / subtype adjust / exit (matches
    // Debug_ChgItem) -- mutually exclusive per frame, same as the real
    // ASM's own if/elseif chain. Every branch (including exit) still falls
    // through to DisplaySprite at the end, matching Debug_Action's own
    // "bsr Debug_Control / jmp DisplaySprite" shape: real hardware displays
    // the debug object every single frame it runs, regardless of which of
    // these branches fired that frame.
    int count;
    const DebugListEntry *list = DebugList_Get(&count);

    if (jpad1_press_ext & JPAD_EXT_Y) {
        // Cycle back one item -- / on keyboard, a physical gamepad's Y
        // button (see Backend/Joypad.h's JPAD_EXT_Y comment). Real Sonic 1
        // does this with Hold A + Press C instead, but that's an awkward
        // chord with no natural keyboard/pad equivalent once C is also
        // "spawn item" on its own; a dedicated button reads better here.
        if (debug_item == 0)
            debug_item = (uint8_t)(count - 1);
        else
            debug_item--;
        DebugMode_ShowItem(self);
    } else if (jpad1_press1 & JPAD_A) {
        // Cycle forward one item.
        debug_item++;
        if (debug_item >= count)
            debug_item = 0;
        DebugMode_ShowItem(self);
    } else if (jpad1_press1 & JPAD_C) {
        // Spawn the currently-selected item at the debug object's position.
        const DebugListEntry *item = &list[debug_item];
        if (item->type != ObjId_Null) {
            Sst *made = sst_object_codes[item->type] != NULL ? Sst_Spawn(item->type, self->x_pos.f.u, self->y_pos.f.u) : NULL;
            if (made != NULL)
                made->subtype = debug_subtype; // read by each object's own Init routine
        }
    } else if (jpad1_press_ext & (JPAD_EXT_SUBTYPE_DEC | JPAD_EXT_SUBTYPE_INC)) {
        // Adjust the selected item's subtype before spawning -- not a real
        // Sonic 1 feature (there's no per-instance subtype editing in the
        // original debug mode), added per your direction. Right stick
        // left/right, ,/. or [/] on keyboard (see JPAD_EXT_SUBTYPE_*).
        if (jpad1_press_ext & JPAD_EXT_SUBTYPE_DEC)
            DebugMode_StepSubtype(&list[debug_item], true);
        if (jpad1_press_ext & JPAD_EXT_SUBTYPE_INC)
            DebugMode_StepSubtype(&list[debug_item], false);
        DebugMode_RefreshPreview(self, &list[debug_item]);
    } else if (jpad1_press1 & JPAD_B) {
        // Exit back to normal Sonic.
        debug_use = 0;

        if (gamemode == GameMode_Special) {
            // Resume the stage as a rolling ball at the normal spin speed.
            ss_angle.f.u = 0;
            ss_rotate = 0x40;
            self->priority = 0 * 0x80;
            self->status |= SST_PSTATUS_IN_BALL;
            self->status |= SST_PSTATUS_IN_AIR;
        } else {
            limit_top2 = limit_top_db;
            limit_btm1 = limit_btm_db;
        }

        self->mappings = Mappings_Sonic;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
        self->mapping_frame = 0;
        self->anim = (gamemode == GameMode_Special) ? SonAnimId_Roll : SonAnimId_Walk;
        self->width_pixels = 24;
        self->render_flags |= SST_RENDER_LEVEL;
    }

    Sst_Draw(self);
}

// Sonic object
void Obj_Sonic(Sst *self) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)self->own.u8;

    // Run debug mode code while in debug mode
    if (debug_use) {
        Sonic_DebugMode(self);
        return;
    }

#ifdef DEMO_WARP
    static const uint16_t demo_loc[ZoneId_Num][4] = {
        {
            LEVEL_ID(ZoneId_GHZ, 1),
            LEVEL_ID(ZoneId_GHZ, 2),
            LEVEL_ID(ZoneId_MZ, 0),
            0,
        },
        {
            LEVEL_ID(ZoneId_LZ, 1),
            LEVEL_ID(ZoneId_LZ, 2),
            LEVEL_ID(ZoneId_SLZ, 0),
            LEVEL_ID(ZoneId_SBZ, 2),
        },
        {
            LEVEL_ID(ZoneId_MZ, 1),
            LEVEL_ID(ZoneId_MZ, 2),
            LEVEL_ID(ZoneId_SYZ, 0),
            0,
        },
        {
            LEVEL_ID(ZoneId_SLZ, 1),
            LEVEL_ID(ZoneId_SLZ, 2),
            LEVEL_ID(ZoneId_SBZ, 0),
            0,
        },
        {
            LEVEL_ID(ZoneId_SYZ, 1),
            LEVEL_ID(ZoneId_SYZ, 2),
            LEVEL_ID(ZoneId_LZ, 0),
            0,
        },
        {
            LEVEL_ID(ZoneId_SBZ, 1),
            LEVEL_ID(ZoneId_LZ, 3),
            LEVEL_ID(ZoneId_GHZ, 0),
            0,
        },
        {
            0,
            0,
            0,
            0,
        },
    };

    if ((jpad1_press1 & JPAD_START) && (jpad1_hold1 & JPAD_A)) {
        level_id = demo_loc[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)];
        restart = true;
    }
#endif

    // Run player routine
    switch (self->routine) {
    case 0: // Initialiation
        // Increment routine
        self->routine += 2;

        // Initialize collision size
        self->y_radius = SONIC_HEIGHT;
        self->x_radius = SONIC_WIDTH;

        // Set object drawing information
        self->mappings = Mappings_Sonic;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
        self->priority = 2 * 0x80;
        self->width_pixels = 24;
        self->render_flags = SST_RENDER_LEVEL;

        // Initialize speeds
        sonspeed_max = 0x600;
        sonspeed_acc = 0xC;
        sonspeed_dec = 0x80;
        // Fallthrough
    case 2: // Regular movement
        // Enter debug mode
        if (debug_cheat && (jpad1_press1 & JPAD_B)) {
            debug_use = true;
            lock_ctrl = false;
            break;
        }

        // Copy player controls if not locked
        if (!lock_ctrl) {
            jpad1_hold2 = jpad1_hold1;
            jpad1_press2 = jpad1_press1;
        }

        // Run player routine
        if (!(lock_multi & 1)) {
            switch ((((self->status & SST_PSTATUS_IN_BALL) != 0) << 2) | (((self->status & SST_PSTATUS_IN_AIR) != 0) << 1)) {
            case 0: // Not in ball, not in air
                if (Sonic_SpinDash(self))
                    break;
                if (Sonic_Jump(self))
                    break;
                Sonic_SlopeResist(self);
                Sonic_Move(self);
                Sonic_Roll(self);
                Sonic_LevelBound(self);
                Sst_MoveSprite2(self);
                Sonic_AnglePos(self);
                Sonic_SlopeRepel(self);
                break;
            case 2: // Not in ball, in air
                Sonic_CancelSpindash(); // see-saw bug fix: don't get stuck charging if launched airborne
                Sonic_JumpHeight(self);
                Sonic_JumpDirection(self);
                Sonic_LevelBound(self);
                Sst_MoveSprite(self);
                if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
                    self->y_vel -= 0x28;
                Sonic_JumpAngle(self);
                Sonic_Floor(self);
                break;
            case 4: // In ball, not in air
                // Real Obj01_MdRoll skips the jump check entirely while
                // pinball_mode/must_roll is set (GHZTunnel) -- can't jump
                // out of a forced roll.
                if (!((self->status & SST_PSTATUS_MUST_ROLL) != 0) && Sonic_Jump(self))
                    break;
                Sonic_RollRepel(self);
                Sonic_RollSpeed(self);
                Sonic_LevelBound(self);
                Sst_MoveSprite2(self);
                Sonic_AnglePos(self);
                Sonic_SlopeRepel(self);
                break;
            case 6: // In ball, in air
                Sonic_CancelSpindash(); // see-saw bug fix
                Sonic_JumpHeight(self);
                Sonic_JumpDirection(self);
                Sonic_LevelBound(self);
                Sst_MoveSprite(self);
                if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
                    self->y_vel -= 0x28;
                Sonic_JumpAngle(self);
                Sonic_Floor(self);
                break;
            }
        }

        // Handle general player state stuff
        Sonic_Display(self);
        Sonic_RecordPosition(self);
        Sonic_Water(self);

        // Copy angle buffers
        scratch->front_angle = angle_buffer0;
        scratch->back_angle = angle_buffer1;

        // Animate
        if (tunnel_mode) {
            if (!self->anim)
                self->prev_anim = self->anim;
        }
        Sonic_Animate(self);

        // Handle object interaction
        if (!(lock_multi & 0x80))
            ReactToItem(self);

        // Handle DPLCs
        Sonic_LoadGfx(self);
        break;
    case 4: // Hurt
        // Fall
        Sst_MoveSprite2(self);
        self->y_vel += 0x30;
        if (((self->status & SST_PSTATUS_UNDERWATER) != 0))
            self->y_vel -= 0x20;

        // Collision
        Sonic_HurtStop(self);

        // Handle general player state stuff
        Sonic_RecordPosition(self);
        Sonic_LevelBound(self);

        // Animate
        Sonic_Animate(self);

        // Handle DPLCs and draw sprite
        Sonic_LoadGfx(self);
        Sst_Draw(self);
        break;
    case 6: // Dead
        // Check for respawning and fall
        GameOver(self);
        Sst_MoveSprite(self);

        // Handle general player state stuff
        Sonic_RecordPosition(self);

        // Animate
        Sonic_Animate(self);

        // Handle DPLCs and draw sprite
        Sonic_LoadGfx(self);
        Sst_Draw(self);
        break;
    case 8: // Dead, respawning
        // Handle death timer
        if (scratch->death_timer && --scratch->death_timer == 0)
            restart = true;
        break;
    }
}
