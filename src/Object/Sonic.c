#include "Sonic.h"

#include "Game.h"
#include "GM_Level.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object.h"
#include "Object/DebugList.h"
#include "Object/DrownCount.h"
#include "Object/Splash.h"
#include "PLC.h"
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

// Temporary alias until the driver hybridization work lands the real SFX.
#define sfx_SpinDash sfx_Roll

uint8_t dbg_ang0, dbg_ang1, dbg_ang2, dbg_ang3; // 0xFFEC-0xFFEF

// General Sonic state stuff
static void Sonic_Display(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Handle invulnerability blinking
    uint16_t blink;
    if ((blink = scratch->flash_time)) {
        scratch->flash_time--;
        if (blink & 4)
            DisplaySprite(obj);
    } else {
        DisplaySprite(obj);
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

static void Sonic_RecordPosition(Object *obj) {
    // Track current position
    int16_t* write = &track_sonic[0][0] + (track_pos.v >> 1);
    *write++ = obj->pos.l.x.f.u;
    *write++ = obj->pos.l.y.f.u;
    track_pos.f.l += 4;
}

// Sonic animation
#include "Resource/Animation/Sonic.h"

#define GET_SONIC_ANISCR(x) (Animation_Sonic + ((Animation_Sonic[((x) << 1)] << 8) | (Animation_Sonic[((x) << 1) + 1] << 0)))

static void Sonic_AnimateReadFrame(Object *obj, const uint8_t *anim_script) {
    // Read current animation command
    uint8_t cmd = anim_script[1 + obj->anim_frame];

    if (!(cmd & 0x80)) {
    Anim_Next:
        // Set animation frame
        obj->frame = cmd;
        obj->anim_frame++;
    } else {
        if (++cmd == 0) {
            // Restart animation
            obj->anim_frame = 0;
            cmd = anim_script[1];
            goto Anim_Next;
        }
        if (++cmd == 0) {
            // Go back (next byte) frames
            obj->anim_frame -= anim_script[2 + obj->anim_frame];
            cmd = anim_script[1 + obj->anim_frame];
            goto Anim_Next;
        }
        if (++cmd == 0) {
            // Change animation (falls through to the routine increment below)
            obj->anim = anim_script[2 + obj->anim_frame];
        }
    }
}

void Sonic_Animate(Object *obj) {
    // Get animation script to use
    const uint8_t* anim_script = Animation_Sonic;

    // Check if animation changed
    uint8_t anim = obj->anim;
    if (anim != obj->prev_anim) {
        // Reset animation state
        obj->prev_anim = anim;
        obj->anim_frame = 0;
        obj->frame_time.b = 0;
    }

    // Get animation script to use
    anim <<= 1;
    anim_script += (anim_script[anim] << 8) | (anim_script[anim + 1] << 0);

    int8_t anim_wait = anim_script[0];
    if (anim_wait >= 0) {
        // Regular animation
        // Set render flip
        obj->render.f.x_flip = obj->status.p.f.x_flip;
        obj->render.f.y_flip = false;

        // Wait for current animation frame to end
        if (--obj->frame_time.b >= 0)
            return;
        obj->frame_time.b = anim_wait;

        // Read animation
        Sonic_AnimateReadFrame(obj, anim_script);
    } else {
        // Wait for current animation frame to end
        if (--obj->frame_time.b >= 0)
            return;

        // Special animation (walking, rolling, running, etc.)
        if (++anim_wait == 0) {
            // Walking or running
            // Get orienatation
            uint8_t angle = obj->angle;
            uint8_t flip = obj->status.p.f.x_flip;
            if (!flip)
                angle ^= ~0;
            if ((angle += 0x10) & 0x80)
                flip ^= 3;

            // Set render flip
            obj->render.f.x_flip = (flip & 1) != 0;
            obj->render.f.y_flip = (flip & 2) != 0;

            // Branch to pushing if the pushing flag is set.. but not in the animation
            if (obj->status.p.f.pushing)
                goto Anim_Pushing;

            // Get rotation
            angle = (angle >> 4) & 6;

            // Get absolute speed
            uint16_t abs_spd = (obj->inertia < 0) ? -obj->inertia : obj->inertia;

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
            obj->frame_time.b = anim_spd >> 8;

            // Read animation
            Sonic_AnimateReadFrame(obj, anim_script);
            obj->frame += angle;
        } else if (++anim_wait == 0) {
            // Rolling
            // Get absolute speed
            uint16_t abs_spd = (obj->inertia < 0) ? -obj->inertia : obj->inertia;

            // Get script to use
            anim_script = GET_SONIC_ANISCR(SonAnimId_Roll2);
            if (abs_spd < 0x600)
                anim_script = GET_SONIC_ANISCR(SonAnimId_Roll);

            // Get animation delay
            int16_t anim_spd = 0x400 - abs_spd;
            if (anim_spd < 0)
                anim_spd = 0;
            obj->frame_time.b = anim_spd >> 8;

            // Set render flip
            obj->render.f.x_flip = obj->status.p.f.x_flip;
            obj->render.f.y_flip = false;

            // Read animation
            Sonic_AnimateReadFrame(obj, anim_script);
        } else {
        Anim_Pushing:;
            // Pushing
            // Get absolute speed
            uint16_t abs_spd = (obj->inertia < 0) ? -obj->inertia : obj->inertia;

            // Get animation delay
            int16_t anim_spd = 0x800 - abs_spd;
            if (anim_spd < 0)
                anim_spd = 0;
            obj->frame_time.b = anim_spd >> 6;

            // Get script to use
            anim_script = GET_SONIC_ANISCR(SonAnimId_Push);

            // Set render flip
            obj->render.f.x_flip = obj->status.p.f.x_flip;
            obj->render.f.y_flip = false;

            // Read animation
            Sonic_AnimateReadFrame(obj, anim_script);
        }
    }
}

// Sonic DPLCs
#include "Resource/Art/Sonic.h"

#include "Resource/Mappings/SonicDPLC.h"

void Sonic_LoadGfx(Object *obj) {
    // Check if we're loading a new frame
    uint8_t frame = obj->frame;
    if (frame == sonframe_num)
        return;
    sonframe_num = frame;

    // Get DPLC script
    const uint8_t* dplc_script = Mappings_SonicDPLC;
    frame <<= 1;
    dplc_script += (dplc_script[frame] << 8) | (dplc_script[frame + 1] << 0);

    // Read number of entries
    int8_t entries = (*dplc_script++) - 1;
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
void Sonic_ResetOnFloor(Object *obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Set state
    obj->status.p.f.pushing = false;
    obj->status.p.f.in_air = false;
    obj->status.p.f.roll_jump = false;

    if (obj->status.p.f.in_ball) {
        obj->status.p.f.in_ball = false;
        obj->y_rad = SONIC_HEIGHT;
        obj->x_rad = SONIC_WIDTH;
        obj->anim = SonAnimId_Walk;
        obj->pos.l.y.f.u -= SONIC_BALL_SHIFT;
    }

    scratch->jumping = false;
    item_bonus = 0;
}

static int16_t Sonic_Angle(Object *obj, int16_t dist0, int16_t dist1) {
    // Get angle and distance to use (use closest one)
    uint8_t res_angle = angle_buffer1;
    int16_t res_dist = dist1;
    if (dist1 > dist0) {
        res_angle = angle_buffer0;
        res_dist = dist0;
    }

    // Use adjusted angle if hit angle is odd (special angle, run on all sides)
    if (res_angle & 1)
        obj->angle = (obj->angle + 0x20) & 0xC0;
    else
        obj->angle = res_angle;
    return res_dist;
}

static void Sonic_AnglePos(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Don't do floor collision if standing on an object
    if (obj->status.p.f.object_stand) {
        angle_buffer0 = 0;
        angle_buffer1 = 0;
        return;
    }

    // Set 'no floor' angle
    angle_buffer0 = 3;
    angle_buffer1 = 3;

    // Get symmetrical angle
    uint8_t angle = obj->angle;
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
        dist0 = FindFloor(obj, obj->pos.l.x.f.u + obj->x_rad, obj->pos.l.y.f.u + obj->y_rad, META_SOLID_TOP, 0, 0x10, &angle_buffer0);
        dist1 = FindFloor(obj, obj->pos.l.x.f.u - obj->x_rad, obj->pos.l.y.f.u + obj->y_rad, META_SOLID_TOP, 0, 0x10, &angle_buffer1);
        if ((dist = Sonic_Angle(obj, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    obj->pos.l.y.f.u += dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    obj->pos.l.y.f.u += dist;
                } else {
                    obj->status.p.f.in_air = true;
                    obj->status.p.f.pushing = false;
                    obj->prev_anim = 1;
                }
            }
        }
        break;
    case 0x40:
        dist0 = FindWall(obj, (obj->pos.l.x.f.u - obj->y_rad) ^ 0xF, obj->pos.l.y.f.u - obj->x_rad, META_SOLID_TOP, META_X_FLIP, -0x10, &angle_buffer0);
        dist1 = FindWall(obj, (obj->pos.l.x.f.u - obj->y_rad) ^ 0xF, obj->pos.l.y.f.u + obj->x_rad, META_SOLID_TOP, META_X_FLIP, -0x10, &angle_buffer1);
        if ((dist = Sonic_Angle(obj, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    obj->pos.l.x.f.u -= dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    obj->pos.l.x.f.u -= dist;
                } else {
                    obj->status.p.f.in_air = true;
                    obj->status.p.f.pushing = false;
                    obj->prev_anim = 1;
                }
            }
        }
        break;
    case 0x80:
        dist0 = FindFloor(obj, obj->pos.l.x.f.u - obj->x_rad, (obj->pos.l.y.f.u - obj->y_rad) ^ 0xF, META_SOLID_TOP, META_Y_FLIP, -0x10, &angle_buffer0);
        dist1 = FindFloor(obj, obj->pos.l.x.f.u + obj->x_rad, (obj->pos.l.y.f.u - obj->y_rad) ^ 0xF, META_SOLID_TOP, META_Y_FLIP, -0x10, &angle_buffer1);
        if ((dist = Sonic_Angle(obj, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    obj->pos.l.y.f.u -= dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    obj->pos.l.y.f.u -= dist;
                } else {
                    obj->status.p.f.in_air = true;
                    obj->status.p.f.pushing = false;
                    obj->prev_anim = 1;
                }
            }
        }
        break;
    case 0xC0:
        dist0 = FindWall(obj, obj->pos.l.x.f.u + obj->y_rad, obj->pos.l.y.f.u + obj->x_rad, META_SOLID_TOP, 0, 0x10, &angle_buffer0);
        dist1 = FindWall(obj, obj->pos.l.x.f.u + obj->y_rad, obj->pos.l.y.f.u - obj->x_rad, META_SOLID_TOP, 0, 0x10, &angle_buffer1);
        if ((dist = Sonic_Angle(obj, dist0, dist1)) != 0) {
            if (dist < 0) {
                if (dist >= -14)
                    obj->pos.l.x.f.u += dist;
            } else {
                if (dist <= 14 || scratch->x38.floor_clip) {
                    obj->pos.l.x.f.u += dist;
                } else if (!scratch->x38.floor_clip) {
                    obj->status.p.f.in_air = true;
                    obj->status.p.f.pushing = false;
                    obj->prev_anim = 1;
                }
            }
        }
        break;
    }
}

static void Sonic_Floor(Object *obj) {
    // Get angle we're moving in
    // There's some weird logging stuff done here
    // Maybe testing if the CalcAngle is yielding desirable results?
    uint8_t angle = CalcAngle(obj->xsp, obj->ysp);
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
        int16_t wall_left = GetDistance2_Left(obj, obj->pos.l.x.f.u, obj->pos.l.y.f.u, NULL);
        int16_t wall_right = GetDistance2_Right(obj, obj->pos.l.x.f.u, obj->pos.l.y.f.u, NULL);
        if ((dist0 = wall_left) < 0) {
            obj->pos.l.x.f.u -= dist0;
            obj->xsp = 0;
        }
        if ((dist0 = wall_right) < 0) {
            obj->pos.l.x.f.u += dist0;
            obj->xsp = 0;
        }

        // Collide with floor
        GetDistance_Down(obj, &dist0, &dist1, &hit_angle);
        dbg_ang3 = dist1; //...What?

        clip = -((obj->ysp >> 8) + 8);
        if (dist1 < 0 && (dist0 >= clip || dist1 >= clip)) {
            // Set state
            obj->pos.l.y.f.u += dist1;
            obj->angle = hit_angle;
            Sonic_ResetOnFloor(obj);
            obj->anim = SonAnimId_Walk;

            // Get inertia
            if ((hit_angle + 0x20) & 0x40) {
                // If floor is greater than 45 degrees, use our full vertical velocity (capped at 0xFC0)
                obj->xsp = 0;
                if (obj->ysp > 0xFC0)
                    obj->ysp = 0xFC0;
                obj->inertia = (hit_angle & 0x80) ? -obj->ysp : obj->ysp;
            } else if ((hit_angle + 0x10) & 0x20) {
                // If floor is greater than 22.5 degrees, use our halved vertical velocity
                obj->ysp /= 2;
                obj->inertia = (hit_angle & 0x80) ? -obj->ysp : obj->ysp;
            } else {
                // If floor is less than 22.5 degrees, use our horizontal velocity
                obj->ysp = 0;
                obj->inertia = obj->xsp;
            }
        }
        break;
    }
    case 0x40: // Moving left
        // Collide with walls
        if ((dist0 = GetDistance2_Left(obj, obj->pos.l.x.f.u, obj->pos.l.y.f.u, NULL)) < 0) {
            obj->pos.l.x.f.u -= dist0;
            obj->xsp = 0;
            obj->inertia = obj->ysp;
            break; // Original returns here?
        }

        // Collide with ceiling
        GetDistance_Up(obj, NULL, &dist1, &hit_angle);
        if (dist1 < 0) {
            obj->pos.l.y.f.u -= dist1;
            if (obj->ysp < 0)
                obj->ysp = 0;
            break; // Again...
        }

        // Collide with floor
        if (obj->ysp >= 0) {
            GetDistance_Down(obj, NULL, &dist1, &hit_angle);
            if (dist1 < 0) {
                // Set state
                obj->pos.l.y.f.u += dist1;
                obj->angle = hit_angle;
                Sonic_ResetOnFloor(obj);
                obj->anim = SonAnimId_Walk;

                // Get inertia
                obj->ysp = 0;
                obj->inertia = obj->xsp;
            }
        }
        break;
    case 0x80: // Moving up
        // Collide with walls
        if ((dist0 = GetDistance2_Left(obj, obj->pos.l.x.f.u, obj->pos.l.y.f.u, NULL)) < 0) {
            obj->pos.l.x.f.u -= dist0;
            obj->xsp = 0;
        }
        if ((dist0 = GetDistance2_Right(obj, obj->pos.l.x.f.u, obj->pos.l.y.f.u, NULL)) < 0) {
            obj->pos.l.x.f.u += dist0;
            obj->xsp = 0;
        }

        // Collide with ceiling
        GetDistance_Up(obj, NULL, &dist1, &hit_angle);
        if (dist1 < 0) {
            obj->pos.l.y.f.u -= dist1;
            if (!((hit_angle + 0x20) & 0x40)) {
                // Kill speed
                if (obj->ysp < 0)
                    obj->ysp = 0;
            } else {
                // Land on ceiling
                obj->angle = hit_angle;
                Sonic_ResetOnFloor(obj);
                obj->inertia = (hit_angle & 0x80) ? -obj->ysp : obj->ysp;
            }
        }
        break;
    case 0xC0: // Moving right
        // Collide with walls
        if ((dist0 = GetDistance2_Right(obj, obj->pos.l.x.f.u, obj->pos.l.y.f.u, NULL)) < 0) {
            obj->pos.l.x.f.u += dist0;
            obj->xsp = 0;
            obj->inertia = obj->ysp;
        }

        // Collide with ceiling
        GetDistance_Up(obj, NULL, &dist1, &hit_angle);
        if (dist1 < 0) {
            obj->pos.l.y.f.u -= dist1;
            if (obj->ysp < 0)
                obj->ysp = 0;
            break; // Again...
        }

        // Collide with floor
        if (obj->ysp >= 0) {
            GetDistance_Down(obj, NULL, &dist1, &hit_angle);
            if (dist1 < 0) {
                // Set state
                obj->pos.l.y.f.u += dist1;
                obj->angle = hit_angle;
                Sonic_ResetOnFloor(obj);
                obj->anim = SonAnimId_Walk;

                // Get inertia
                obj->ysp = 0;
                obj->inertia = obj->xsp;
            }
        }
        break;
    }
}

static void Sonic_HurtStop(Object *obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Die when falling below the level
    if ((limit_btm2 + SCREEN_HEIGHT) < obj->pos.l.y.f.u) {
        KillSonic(obj, obj); // a1 isn't set
        return;
    }

    // Do collision
    Sonic_Floor(obj);
    if (!obj->status.p.f.in_air) {
        obj->ysp = 0;
        obj->xsp = 0;
        obj->inertia = 0;
        obj->anim = SonAnimId_Walk;
        obj->routine -= 2;
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

static signed int React_ChkHurt(Object *obj, Object *hit) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Check for invincibility or invulnerability
    if (invincibility)
        return -1;
    if (scratch->flash_time)
        return -1;

    // Hurt Sonic
    return HurtSonic(obj, hit);
}

static signed int React_Enemy(Object *obj, Object *hit)
{
    // Check if we can hurt the enemy
    if (!(invincibility || obj->anim == SonAnimId_Roll))
        return React_ChkHurt(obj, hit);

    // Check if enemy is a boss
    if (hit->col_property) {
        // Hit boss
        obj->xsp = -obj->xsp >> 1;
        obj->ysp = -obj->ysp >> 1;
        hit->col_type = 0;
        if (!(--hit->col_property))
            hit->status.o.f.flag7 = true;
    } else {
        // Mark enemy touch flag
        hit->status.o.f.flag7 = true;

        // Increment bonus
        uint8_t bonus = item_bonus;
        item_bonus += 2;

        // Handle score bonus
        if (bonus >= 6)
            bonus = bonus;
        hit->scratch.u16[0xB] = bonus;

        static const uint16_t points[] = { 10, 20, 50, 100 };
        uint16_t point_bonus = points[bonus >> 1];
        if (item_bonus >= 32) {
            point_bonus = 1000;
            hit->scratch.u16[0xB] = 10;
        }

        AddPoints(point_bonus);

        // Explode object and bounce
        hit->type = ObjId_Explosion;
        hit->routine = 0;

        if (obj->ysp >= 0) {
            if (obj->pos.l.y.f.u < hit->pos.l.y.f.u)
                obj->ysp = -obj->ysp;
            else
                obj->ysp -= 0x100;
        } else {
            obj->ysp += 0x100;
        }
    }
    return 0; // d0 not set
}

static signed int React_Monitor(Object *obj, Object *hit) {
    if (obj->ysp < 0) {
        // Check if we're below the monitor
        uint16_t chky = obj->pos.l.y.f.u - 0x10;
        if ((uint16_t)hit->pos.l.y.f.u < chky) {
            // Bump monitor
            obj->ysp = -obj->ysp;
            hit->ysp = -0x180;
            if (!hit->routine_sec)
                hit->routine_sec += 4;
        }
    } else if (obj->anim == SonAnimId_Roll) {
        // Break monitor
        obj->ysp = -obj->ysp;
        hit->routine += 2;
    }
    return 0; // d0 not set
}

static signed int ReactToItem(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Get collision area
    int16_t width, height;
    int16_t x = obj->pos.l.x.f.u - 8;
    int16_t y = obj->pos.l.y.f.u - (height = (uint8_t)(obj->y_rad - 3));

    if (obj->frame == 0x39) {
        // Smaller hitbox when ducking
        y += 12;
        height = 10;
    }
    width = 16;
    height <<= 1;

    // Iterate through level objects
    Object *hit = level_objects;
    for (int i = 0; i < LEVEL_OBJECTS; i++, hit++) {
        // Check if object is collidable
        if (!(hit->render.f.on_screen && hit->col_type))
            continue;

        // Get object's size
        const uint8_t *sizep = obj_sizes[hit->col_type & 0x3F];
        uint8_t hit_width = *sizep++;
        uint8_t hit_height = *sizep++;

        // Check if we're touching (TODO: may be inaccurate)
        int16_t x_diff = x - (hit->pos.l.x.f.u - hit_width);
        int16_t y_diff = y - (hit->pos.l.y.f.u - hit_height);

        if (x_diff >= -width && x_diff <= hit_width * 2 && y_diff >= -height && y_diff <= hit_height * 2) {
            // Made contact
            switch (hit->col_type & 0xC0) {
            case 0x00: // Enemy
                return React_Enemy(obj, hit);
            case 0xC0: // Special
                if ((hit->col_type & 0x3F) == 0x0B) { // Caterkiller spiked body segment
                    hit->status.o.f.flag7 = true; // mark segment touched -- read by Obj_Caterkiller to fragmentate
                    return React_ChkHurt(obj, hit);
                }
                if ((hit->col_type & 0x3F) == 0x0C) { // Yadrin -- narrow "face" hitbox at the top only
                    int16_t penetration = (int16_t)((y + height) - (hit->pos.l.y.f.u - hit_height));
                    if (penetration < 8) {
                        int16_t face_left = (int16_t)(hit->pos.l.x.f.u - 4);
                        if (hit->status.o.f.x_flip)
                            face_left -= 16;
                        if (x < (int16_t)(face_left + 24) && (int16_t)(x + width) > face_left)
                            return React_ChkHurt(obj, hit);
                    }
                    return React_Enemy(obj, hit);
                }
                if ((hit->col_type & 0x3F) == 0x17 || (hit->col_type & 0x3F) == 0x21) { // SYZ bumper / LZ pole -- own routine reads/clears col_property each frame
                    hit->col_property++;
                    return 0;
                }
                break;
            case 0x80: // Hurt
                return React_ChkHurt(obj, hit);
            case 0x40: // Other
                if ((hit->col_type & 0x3F) == 6)
                    return React_Monitor(obj, hit);
                if (scratch->flash_time < 90)
                    hit->routine = 4;
                break;
            }
        }
    }
    return 0;
}

// Sonic functions
signed int HurtSonic(Object *obj, Object *src)
{
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Lose rings and shield
    if (!shield) {
        if (rings) {
            // Spawn lost rings object
            Object* rings = FindFreeObj();
            if (rings != NULL) {
                rings->type = ObjId_RingLoss;
                rings->pos.l.x.f.u = obj->pos.l.x.f.u;
                rings->pos.l.y.f.u = obj->pos.l.y.f.u;
            }
        } else if (!debug_mode) {
            // Die
            return KillSonic(obj, src);
        }
    }
    shield = 0;

    // Set Sonic state
    obj->routine = 4;
    spindash_flag &= ~1;
    objects[0x1B].anim = SplashAnim_Null;
    Sonic_ResetOnFloor(obj);
    obj->status.p.f.in_air = true;

    obj->xsp = obj->status.p.f.underwater ? -0x100 : -0x200;
    obj->ysp = obj->status.p.f.underwater ? -0x200 : -0x400;
    if (obj->pos.l.x.f.u >= src->pos.l.x.f.u)
        obj->xsp = -obj->xsp;
    obj->inertia = 0;

    obj->anim = SonAnimId_Hurt;
    scratch->flash_time = 120;

    // LZ harpoon isn't ported yet -- only checking for Spikes here.
    PlaySound((src->type == ObjId_Spikes) ? sfx_HitSpikes : sfx_Death);
    return -1;
}

int32_t KillSonic(Object *obj, Object *src) {
    (void)src;
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Check if we can be killed
    if (debug_use)
        return -1;

    // Set state
    invincibility = false;
    obj->routine = 6;
    Sonic_ResetOnFloor(obj);
    obj->status.p.f.in_air = true;
    obj->ysp = -0x700;
    obj->xsp = 0;
    obj->inertia = 0;
    scratch->x38.death_y = obj->pos.l.y.f.u;
    obj->anim = SonAnimId_Death;
    obj->tile |= TILE_PRIORITY_AND;

    // Play death sound (LZ harpoon isn't ported yet -- only Spikes here)
    PlaySound((src->type == ObjId_Spikes) ? sfx_HitSpikes : sfx_Death);
    return -1;
}

// Sonic movement functions
static bool Sonic_Jump(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Don't jump if ABC isn't pressed
    if (!(jpad1_press2 & (JPAD_A | JPAD_C | JPAD_B)))
        return false;

    // Check if we have enough room to jump
    int16_t dist;
    GetDistanceBelowAngle(obj, obj->angle + 0x80, NULL, &dist, NULL);
    if (dist < 6)
        return false;

    // Get jump speed
    int16_t speed = 0x680;
    if (obj->status.p.f.underwater)
        speed = 0x380;

    int16_t sin, cos;
    CalcSine(obj->angle - 0x40, &sin, &cos);
    obj->xsp += (cos * speed) >> 8;
    obj->ysp += (sin * speed) >> 8;

    // Set state
    obj->status.p.f.in_air = true;
    obj->status.p.f.pushing = false;
    scratch->jumping = true;
    scratch->x38.floor_clip = 0;

    PlaySound(sfx_Jump);

    obj->y_rad = SONIC_HEIGHT; // No idea why this is here
    obj->x_rad = SONIC_WIDTH;

    if (!obj->status.p.f.in_ball) {
        obj->y_rad = SONIC_BALL_HEIGHT;
        obj->x_rad = SONIC_BALL_WIDTH;
        obj->anim = SonAnimId_Roll;
        obj->status.p.f.in_ball = true;
        obj->pos.l.y.f.u += SONIC_BALL_SHIFT;
    } else {
        obj->status.p.f.roll_jump = true;
    }
    return true;
}

static void Sonic_SlopeResist(Object *obj) {
    if (((obj->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(obj->angle) * 0x20) >> 8;
    if (obj->inertia != 0)
        obj->inertia += force;
}

static void Sonic_MoveLeft(Object *obj) {
    int16_t inertia = obj->inertia;
    if (inertia <= 0) {
        // Turn around
        if (!obj->status.p.f.x_flip) {
            obj->status.p.f.x_flip = true;
            obj->status.p.f.pushing = false;
            obj->prev_anim = 1; // Reset animation
        }

        // Accelerate
        if ((inertia -= sonspeed_acc) <= -sonspeed_max)
            inertia = -sonspeed_max;

        // Set speed and animation
        obj->inertia = inertia;
        obj->anim = SonAnimId_Walk;
    } else {
        // Decelerate
        if ((inertia -= sonspeed_dec) < 0)
            inertia = -0x80;
        obj->inertia = inertia;

        // Skid
        if (((obj->angle + 0x20) & 0xC0) == 0x00 && inertia >= 0x400) {
            obj->anim = SonAnimId_Stop;
            obj->status.p.f.x_flip = false;
            PlaySound(sfx_Skid);
            if (air >= 12) {
                if (skid_dust_timer == 0) {
                    skid_dust_timer = 4;
                    Splash_SpawnSkidDust(obj);
                } else {
                    skid_dust_timer--;
                }
            }
        }
    }
}

static void Sonic_MoveRight(Object *obj) {
    int16_t inertia = obj->inertia;
    if (inertia >= 0) {
        // Turn around
        if (obj->status.p.f.x_flip) {
            obj->status.p.f.x_flip = false;
            obj->status.p.f.pushing = false;
            obj->prev_anim = 1; // Reset animation
        }

        // Accelerate
        if ((inertia += sonspeed_acc) >= sonspeed_max)
            inertia = sonspeed_max;

        // Set speed and animation
        obj->inertia = inertia;
        obj->anim = SonAnimId_Walk;
    } else {
        // Decelerate
        if ((inertia += sonspeed_dec) >= 0)
            inertia = 0x80;
        obj->inertia = inertia;

        // Skid
        if (((obj->angle + 0x20) & 0xC0) == 0x00 && inertia <= -0x400) {
            obj->anim = SonAnimId_Stop;
            obj->status.p.f.x_flip = true;
            PlaySound(sfx_Skid);
            if (air >= 12) {
                if (skid_dust_timer == 0) {
                    skid_dust_timer = 4;
                    Splash_SpawnSkidDust(obj);
                } else {
                    skid_dust_timer--;
                }
            }
        }
    }
}

static void Sonic_Move(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    if (f_slidemode)
        goto Sonic_AngleSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad1_hold2 & JPAD_LEFT)
                Sonic_MoveLeft(obj);
            if (jpad1_hold2 & JPAD_RIGHT)
                Sonic_MoveRight(obj);

            // Do idle or balance animation
            if (((obj->angle + 0x20) & 0xC0) == 0x00 && !obj->inertia) {
                // Do idle animation
                obj->status.p.f.pushing = false;
                obj->anim = SonAnimId_Wait;

                if (obj->status.p.f.object_stand) {
                    // Balance on an object
                    Object* stand = &objects[scratch->standing_obj];
                    if (!stand->status.o.f.flag7) {
                        int16_t left_dist = stand->width_pixels;
                        int16_t right_dist = left_dist + left_dist - 4;
                        left_dist += obj->pos.l.x.f.u - stand->pos.l.x.f.u;

                        if (left_dist < 4)
                            obj->status.p.f.x_flip = true;
                        else if (left_dist >= right_dist)
                            obj->status.p.f.x_flip = false;
                        else
                            goto LookUpDown;
                        obj->anim = SonAnimId_Balance;
                        goto Sonic_ResetScr;
                    }
                } else {
                    // Balance on level
                    if (ObjFloorDist(obj, obj->pos.l.x.f.u) >= 12) {
                        if (scratch->front_angle == 3) {
                            obj->status.p.f.x_flip = false;
                            obj->anim = SonAnimId_Balance;
                            goto Sonic_ResetScr;
                        }
                        if (scratch->back_angle == 3) {
                            obj->status.p.f.x_flip = true;
                            obj->anim = SonAnimId_Balance;
                            goto Sonic_ResetScr;
                        }
                    }
                }

            // Handle looking up and down
            LookUpDown:;
                if (jpad1_hold2 & JPAD_UP) {
                    obj->anim = SonAnimId_LookUp;
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
                    obj->anim = SonAnimId_Duck;
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
            if (obj->inertia > 0) {
                if ((obj->inertia -= sonspeed_acc) < 0)
                    obj->inertia = 0;
            } else if (obj->inertia < 0) {
                if ((obj->inertia += sonspeed_acc) >= 0)
                    obj->inertia = 0;
            }
        }
    }

    // Calculate global speed from inertia
Sonic_AngleSpeed:;
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    obj->xsp = (cos * obj->inertia) >> 8;
    obj->ysp = (sin * obj->inertia) >> 8;

    // Handle wall collision
    if (((obj->angle + 0x40) & 0x80) || !obj->inertia)
        return;

    uint8_t add_angle = (obj->inertia < 0) ? 0x40 : -0x40;
    uint8_t angle = obj->angle + add_angle;
    int16_t dist = GetDistanceBelowAngle2(obj, angle, NULL);

    if (dist < 0) {
        dist <<= 8;
        switch ((angle + 0x20) & 0xC0) {
        case 0x00:
            obj->ysp += dist;
            break;
        case 0x40:
            obj->xsp -= dist;
            obj->status.p.f.pushing = true;
            obj->inertia = 0;
            break;
        case 0x80:
            obj->ysp -= dist;
            break;
        case 0xC0:
            obj->xsp += dist;
            obj->status.p.f.pushing = true;
            obj->inertia = 0;
            break;
        }
    }
}

void Sonic_ChkRoll(Object *obj) {
    // Enter roll state
    if (obj->status.p.f.in_ball)
        return;
    obj->status.p.f.in_ball = true;
    obj->y_rad = SONIC_BALL_HEIGHT;
    obj->x_rad = SONIC_BALL_WIDTH;
    obj->anim = SonAnimId_Roll;
    obj->pos.l.y.f.u += SONIC_BALL_SHIFT;
    PlaySound(sfx_Roll);

    // Set speed (S-tubes)
    if (!obj->inertia)
        obj->inertia = 0x200;
}

static void Sonic_Roll(Object *obj) {
    // Don't allow rolling while on a water slide
    if (f_slidemode)
        return;

    // Check if we can and are trying to roll
    if (jump_only || ((obj->inertia < 0) ? -obj->inertia : obj->inertia) < 0x80)
        return;
    if ((jpad1_hold2 & (JPAD_LEFT | JPAD_RIGHT)) || !(jpad1_hold2 & JPAD_DOWN))
        return;
    Sonic_ChkRoll(obj);
}

static void Sonic_LevelBound(Object *obj) {
    // Get next X position
    // This is unsigned, but it shouldn't be
    uint16_t x = (obj->pos.l.x.v + (obj->xsp << 8)) >> 16;

    // Prevent us from going off the left or right boundaries
    int16_t bound;
    if (x < (bound = limit_left2 + 16) || x > (bound = limit_right2 + (lock_screen ? 290 : 360) + SCREEN_WIDEADD)) {
        obj->pos.l.x.f.u = bound;
        obj->pos.l.x.f.l = 0;
        obj->xsp = 0;
        obj->inertia = 0;
    }

    // Fall off the bottom boundary. FixBugs: use whichever of limit_btm2
    // (the level's static bottom boundary) and limit_btm1 (the camera's
    // own, possibly-DynamicLevelEvents-extended clamp) is currently
    // deeper -- the original code only ever considers limit_btm2, which
    // doesn't account for the camera boundary being in the middle of
    // (or having already finished) lowering itself, killing Sonic in
    // legitimately-reachable deep areas a zone's own DLE has unlocked.
    uint16_t kill_plane = (limit_btm1 > limit_btm2) ? limit_btm1 : limit_btm2;
    if ((kill_plane + SCREEN_HEIGHT) < obj->pos.l.y.f.u) {
        if (level_id == LEVEL_ID(ZoneId_SBZ, 1) && obj->pos.l.x.f.u >= 0x2000) {
            // Go to SBZ3 if falling off at the end of SBZ2
            last_lamp = 0;
            restart = true;
            level_id = LEVEL_ID(ZoneId_LZ, 3);
        } else {
            // Kill Sonic
            KillSonic(obj, obj);
        }
    }
}

static void Sonic_SlopeRepel(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Check if we can fall off a slope
    if (scratch->x38.floor_clip)
        return;

    if (!scratch->control_lock) {
        // Check if the slope is steep enough
        if ((obj->angle + 0x20) & 0xC0) {
            // Fall off if we're going too slow
            if (((obj->inertia < 0) ? -obj->inertia : obj->inertia) < 0x280) {
                obj->inertia = 0;
                obj->status.p.f.in_air = true;
                scratch->control_lock = 30;
            }
        }
    } else {
        // Decrement control lock
        scratch->control_lock--;
    }
}

static void Sonic_RollRepel(Object *obj) {
    if (((obj->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(obj->angle) * 0x50) >> 8;
    if (obj->inertia > 0) {
        if (force < 0)
            force >>= 2;
        obj->inertia += force;
    } else if (obj->inertia < 0) {
        if (force >= 0)
            force >>= 2;
        obj->inertia += force;
    }
}

static void Sonic_RollLeft(Object *obj) {
    int16_t inertia = obj->inertia;
    if (inertia <= 0) {
        // Set animation
        obj->status.p.f.x_flip = true;
        obj->anim = SonAnimId_Roll;
    } else {
        // Decelerate
        if ((inertia -= sonspeed_dec >> 2) < 0)
            inertia = -0x80;
        obj->inertia = inertia;
    }
}

static void Sonic_RollRight(Object *obj) {
    int16_t inertia = obj->inertia;
    if (inertia >= 0) {
        // Set animation
        obj->status.p.f.x_flip = false;
        obj->anim = SonAnimId_Roll;
    } else {
        // Decelerate
        if ((inertia += sonspeed_dec >> 2) >= 0)
            inertia = 0x80;
        obj->inertia = inertia;
    }
}

static void Sonic_RollSpeed(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    if (f_slidemode)
        goto Sonic_AngledRollSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad1_hold2 & JPAD_LEFT)
                Sonic_RollLeft(obj);
            if (jpad1_hold2 & JPAD_RIGHT)
                Sonic_RollRight(obj);
        }

        // Friction
        if (obj->inertia > 0) {
            if ((obj->inertia -= (sonspeed_acc >> 1)) < 0)
                obj->inertia = 0;
        } else {
            if ((obj->inertia += (sonspeed_acc >> 1)) >= 0)
                obj->inertia = 0;
        }

        // Uncurl when we've come to a stop -- unless must_roll (GHZTunnel)
        // is forcing us to stay rolling, in which case give an extra push
        // in whichever direction we're currently facing instead (real
        // Sonic_KeepRolling: "magically gives Sonic an extra push if he's
        // going to stop rolling where it's not allowed, such as in an
        // S-curve").
        if (obj->inertia == 0) {
            if (obj->status.p.f.must_roll) {
                obj->inertia = obj->status.p.f.x_flip ? (int16_t)-0x400 : (int16_t)0x400;
            } else {
                obj->status.p.f.in_ball = false;
                obj->y_rad = SONIC_HEIGHT;
                obj->x_rad = SONIC_WIDTH;
                obj->anim = SonAnimId_Wait;
                obj->pos.l.y.f.u -= SONIC_BALL_SHIFT;
            }
        }
    }

    // Calculate global speed from inertia
Sonic_AngledRollSpeed:;
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    obj->ysp = (sin * obj->inertia) >> 8;
    cos = (cos * obj->inertia) >> 8;
    if (cos > 0x1000) // Global X speed is limited to 0x1000 both ways.
        cos = 0x1000; // This causes the speed to desync from inertia
    if (cos < -0x1000) // at high speeds.
        cos = -0x1000; // Is this here because of the broken camera?
    obj->xsp = cos;

    // Handle wall collision
    if (((obj->angle + 0x40) & 0x80) || !obj->inertia)
        return;

    uint8_t add_angle = (obj->inertia < 0) ? 0x40 : -0x40;
    uint8_t angle = obj->angle + add_angle;
    int16_t dist = GetDistanceBelowAngle2(obj, angle, NULL);

    if (dist < 0) {
        dist <<= 8;
        switch ((angle + 0x20) & 0xC0) {
        case 0x00:
            obj->ysp += dist;
            break;
        case 0x40:
            obj->xsp -= dist;
            obj->status.p.f.pushing = true;
            obj->inertia = 0;
            break;
        case 0x80:
            obj->ysp -= dist;
            break;
        case 0xC0:
            obj->xsp += dist;
            obj->status.p.f.pushing = true;
            obj->inertia = 0;
            break;
        }
    }
}

static void Sonic_JumpHeight(Object *obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    if (scratch->jumping) {
        // Get minimum jump speed and apply if ABC isn't held
        int16_t spd = obj->status.p.f.underwater ? -0x200 : -0x400;
        if (obj->ysp < spd && !(jpad1_hold2 & (JPAD_A | JPAD_C | JPAD_B)))
            obj->ysp = spd;
    } else {
        // Cap upwards speed
        if (obj->ysp < -0xFC0)
            obj->ysp = -0xFC0;
    }
}

static void Sonic_JumpDirection(Object *obj) {
    // Handle acceleration
    if (!obj->status.p.f.roll_jump) {
        int16_t xsp = obj->xsp;

        // Accelerate left
        if (jpad1_hold2 & JPAD_LEFT) {
            obj->status.p.f.x_flip = true;
            if ((xsp -= (sonspeed_acc << 1)) <= -sonspeed_max)
                xsp = -sonspeed_max;
        }

        // Accelerate right
        if (jpad1_hold2 & JPAD_RIGHT) {
            obj->status.p.f.x_flip = false;
            if ((xsp += (sonspeed_acc << 1)) >= sonspeed_max)
                xsp = sonspeed_max;
        }

        // Apply acceleration
        obj->xsp = xsp;
    }

    // Reset screen shift
    if (look_shift < (96 + SCREEN_TALLADD2))
        look_shift += 2;
    else if (look_shift > (96 + SCREEN_TALLADD2))
        look_shift -= 2;

    // Handle air drag
    if ((uint16_t)obj->ysp >= (uint16_t)-0x400) {
        int16_t xsp = obj->xsp;
        int16_t drag = xsp >> 5;
        if (drag > 0) // Do I really have to say anything?
        {
            if ((xsp -= drag) < 0)
                xsp = 0;
            obj->xsp = xsp;
        } else if (drag < 0) {
            if ((xsp -= drag) >= 0)
                xsp = 0;
            obj->xsp = xsp;
        }
    }
}

static void Sonic_JumpAngle(Object *obj) {
    // Reset angle towards 0
    uint8_t angle = obj->angle;
    if (angle == 0)
        return;

    if (!(angle & 0x80)) {
        if ((angle -= 2) & 0x80)
            angle = 0;
    } else {
        if (!((angle += 2) & 0x80))
            angle = 0;
    }

    obj->angle = angle;
}

static void Sonic_Loops(Object *obj) {
    // Make sure we're in SLZ or GHZ
    if (LEVEL_ZONE(level_id) != ZoneId_SLZ && LEVEL_ZONE(level_id) != ZoneId_GHZ)
        return;

    // Get chunk we're on (128x128 px chunks, see FindNearestTile)
    int16_t cx = (obj->pos.l.x.f.u >> 7) & (LEVEL_LAYOUT_COLS - 1);
    int16_t cy = (obj->pos.l.y.f.u >> 7) & (LEVEL_LAYOUT_ROWS - 1);
    uint8_t chunk = LEVEL_LAYOUT_FG(cy)[cx];

    // Handle S-tubes
    if (chunk == level_schunks[1][0] || chunk == level_schunks[1][1]) {
        Sonic_ChkRoll(obj);
        return;
    }

    // Handle loops
    if (chunk == level_schunks[0][0]) {
    CheckLoop:;
        // Return to high plane if to the left
        if ((uint8_t)obj->pos.l.x.f.u < 0x2C) {
            obj->render.f.player_loop = false;
            return;
        }

        // Go to low plane if to the right
        if ((uint8_t)obj->pos.l.x.f.u >= 0xE0) {
            obj->render.f.player_loop = true;
            return;
        }

        // Check our angle
        if (!obj->render.f.player_loop) {
            if (obj->angle && obj->angle < 0x80)
                obj->render.f.player_loop = true;
        } else {
            if (obj->angle > 0x80)
                obj->render.f.player_loop = false;
        }
    } else if (chunk == level_schunks[0][1]) {
        // Return to high plane if in mid-air
        if (!obj->status.p.f.in_air)
            goto CheckLoop;
        obj->render.f.player_loop = false;
    } else {
        // Return to high plane
        obj->render.f.player_loop = false;
    }
}

// Spin Dash
static void Sonic_Spindash_ResetScr(Object *obj) {
    // Reset camera to neutral position (same logic as Sonic_Move/Sonic_JumpDirection)
    if (look_shift < (96 + SCREEN_TALLADD2))
        look_shift += 2;
    else if (look_shift > (96 + SCREEN_TALLADD2))
        look_shift -= 2;

    // Since we skip the rest of the normal-movement case, run these manually
    Sonic_LevelBound(obj);
    Sonic_AnglePos(obj);
}

static void Sonic_ChargingSpindash(Object *obj) {
    obj->anim = SonAnimId_SpinDash; // make sure Spin Dash animation stays

    // Charge decay
    spindash_count -= spindash_count >> 5;

    if (!(jpad1_press2 & (JPAD_A | JPAD_C | JPAD_B))) {
        Sonic_Spindash_ResetScr(obj);
        return;
    }

    // Restart Spin Dash animation
    obj->anim = SonAnimId_SpinDash;
    obj->anim_frame = 0;
    obj->frame_time.b = 0;
    PlaySound(sfx_SpinDash);

    spindash_count += 0x200;
    if (spindash_count > 0x800)
        spindash_count = 0x800;

    Sonic_Spindash_ResetScr(obj);
}

static void Sonic_ReleaseSpindash(Object *obj) {
    spindash_flag &= ~1;
    obj->y_rad = SONIC_BALL_HEIGHT;
    obj->x_rad = SONIC_BALL_WIDTH;
    obj->pos.l.y.f.u += SONIC_BALL_SHIFT;
    obj->anim = SonAnimId_Roll;
    obj->status.p.f.in_ball = true;
    PlaySound(sfx_Teleport);
    objects[0x1B].anim = SplashAnim_Null;

    // Get release speed from number of revs performed
    int16_t rev = (int16_t)(spindash_count >> 1);
    int16_t speed = rev + 0x800;
    if (obj->status.p.f.x_flip)
        speed = -speed;
    obj->inertia = speed;

    // Camera delay (based on rev count, before the base speed was added)
    uint16_t cam = (uint16_t)rev;
    cam <<= 1;
    cam &= 0x1F00;
    cam = (uint16_t)(-(int16_t)cam);
    cam += 0x2000;
    cam_x_delay = cam;

    // Set new velocities immediately (same convention as Sonic_Move/Sonic_RollSpeed: cos->xsp, sin->ysp)
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    obj->xsp = (int16_t)(((int32_t)cos * obj->inertia) >> 8);
    obj->ysp = (int16_t)(((int32_t)sin * obj->inertia) >> 8);

    Sonic_Spindash_ResetScr(obj);
}

static void Sonic_UpdateSpindash(Object *obj) {
    // Bug fix (part 2 of the monitor fix): guarantee the Spin Dash
    // animation is set as soon as we know we're spin dashing, before
    // Monitor's own Mon_SolidSides push-vs-break check (which now also
    // exempts this animation, see Monitor.c) gets a chance to run this
    // same frame and see a stale Push animation instead.
    obj->anim = SonAnimId_SpinDash;
    if (jpad1_hold2 & JPAD_DOWN) {
        Sonic_ChargingSpindash(obj);
        return;
    }
    Sonic_ReleaseSpindash(obj);
}

static bool Sonic_SpinDash(Object *obj) {
    if (spindash_flag & 1) {
        Sonic_UpdateSpindash(obj);
        return true;
    }

    if (obj->anim != SonAnimId_Duck)
        return false;
    if (!(jpad1_press2 & (JPAD_A | JPAD_C | JPAD_B)))
        return false;

    obj->anim = SonAnimId_SpinDash;
    PlaySound(sfx_SpinDash);
    spindash_flag |= 1;
    spindash_count = 0;
    if (air >= 12)
        objects[0x1B].anim = SplashAnim_Dash;

    // Because we're skipping the rest of the normal-movement case
    Sonic_LevelBound(obj);
    Sonic_AnglePos(obj);
    return true;
}

// Water entry/exit: adjusts speed for being underwater, spawns the splash
// effect, and (on entry) spawns the drowning countdown's master tracker
// (Object/DrownCount.h) at its fixed slot. LZWindTunnels/LZWaterSlides
// (movement inside wind tunnel/water-slide chunks) are still TODO.
static void Sonic_Water(Object *obj) {
    if (LEVEL_ZONE(level_id) != ZoneId_LZ)
        return;

    if (obj->pos.l.y.f.u < wtr_pos1) {
        // Above water
        if (!obj->status.p.f.underwater)
            return;
        obj->status.p.f.underwater = false;
        ResumeMusic(); // replenish air and resume music if necessary

        sonspeed_max = 0x600;
        sonspeed_acc = 0xC;
        sonspeed_dec = 0x80;
        if (shoes) {
            sonspeed_max = 0xC00;
            sonspeed_acc = 0x18;
        }

        obj->ysp = (int16_t)(obj->ysp << 1); // double Y-speed while exiting water
        if (obj->ysp == 0)
            return;
        if (obj->ysp < -0x1000)
            obj->ysp = -0x1000; // cap max speed on leaving water

        objects[0x1B].anim = SplashAnim_Splash;
        PlaySound(sfx_Splash);
    } else {
        // Underwater
        if (obj->status.p.f.underwater)
            return;
        obj->status.p.f.underwater = true;
        ResumeMusic(); // replenish air (music won't resume here, we've only just entered water)

        objects[DROWNCOUNT_SLOT].type = ObjId_DrownCount;
        objects[DROWNCOUNT_SLOT].routine = 0;
        objects[DROWNCOUNT_SLOT].scratch.u8[0] = DROWNCOUNT_MASTER_BIT | 1; // subtype -- selects the master tracker path

        sonspeed_max = 0x300;
        sonspeed_acc = 0x6;
        sonspeed_dec = 0x40;
        if (shoes) {
            sonspeed_max = 0x600;
            sonspeed_acc = 0xC;
            sonspeed_dec = 0x80;
        }

        obj->xsp >>= 1; // half X-speed when entering water
        obj->ysp >>= 2; // quarter Y-speed when entering water
        if (obj->ysp == 0)
            return;

        objects[0x1B].anim = SplashAnim_Splash;
        PlaySound(sfx_Splash);
    }
}

// Other functions
static void GameOver(Object* obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Have we fallen below the screen?
    if ((limit_btm2 + 256 + SCREEN_TALLADD) >= obj->pos.l.y.f.u)
        return;

    // Enter respawn state
    obj->ysp = -0x38; //???
    obj->routine += 2;
    time_count = false;
    life_count++;

    // Check if we've game over'ed
    if (--lives == 0) {
        // Set death timer
        scratch->death_timer = 0;

        // Load 'GAME OVER' objects
        objects[2].type = ObjId_GameOverCard; // GAME
        objects[3].type = ObjId_GameOverCard; // OVER
        objects[3].frame = 1;

        time_over = false;
        // music	bgm_GameOver,0,0,0	; play game over music //TODO
        AddPLC(PlcId_GameOver);
    } else {
        // Set death timer
        scratch->death_timer = 60;

        // Check if we've time over'ed
        if (time_over) {
            // Set death timer
            scratch->death_timer = 0;

            // Load 'TIME OVER' objects
            objects[2].type = ObjId_GameOverCard; // TIME
            objects[3].type = ObjId_GameOverCard; // OVER
            objects[2].frame = 2;
            objects[3].frame = 3;

            PlayMusic(bgm_GameOver);
            AddPLC(PlcId_GameOver);
        }
    }
}

#define DEBUG_MOVE_DELAY 12  // frames to wait when holding D-Pad before accelerating
#define DEBUG_START_SPEED 15 // initial movement speed when first holding D-Pad

extern const uint8_t Mappings_RingREV01[]; // From Object/Ring.c

// Looks up one frame's own piece data in a normal (non-raw) per-frame
// mapping table -- same lookup Object.c's BuildSprites does (word offset
// table, then a piece-count byte, then that many 5-byte pieces).
static const uint8_t *DebugMode_LookupFrame(const uint8_t *mappings, uint8_t frame, uint8_t *piece_count_out) {
    const uint8_t *mapping_ind = mappings + (frame << 1);
    const uint8_t *piece_data = mappings + ((mapping_ind[0] << 8) | mapping_ind[1]);
    *piece_count_out = *piece_data;
    return piece_data + 1;
}

// Scratch buffer a subtype variant's frame stack gets assembled into --
// composed once per preview refresh (item change / subtype adjust), not
// per-frame, so it doesn't need double-buffering against BuildSprites
// reading the previous frame's contents. Header shape matches a normal
// (non-raw) single-frame mapping table: 2-byte offset (always 2, pointing
// right past itself) + 1-byte piece count + that many 5-byte pieces.
#define DEBUG_STACK_MAX_PIECES 16
static uint8_t debug_stack_buffer[2 + 1 + DEBUG_STACK_MAX_PIECES * 5];

// Composites a variant's frame stack (per your direction: several of the
// object's own real frames, each independently offset, drawn together --
// e.g. a Monitor's box frame plus its content icon frame) into
// debug_stack_buffer, and points obj at it.
// A stacked layer's own piece coordinate plus its DebugFramePiece offset can
// exceed a real sprite piece's int8_t range (Ring's row/column preview in
// particular -- 6 extra rings at 0x20 apart reaches 192). Clamping instead
// of letting the (uint8_t)(int8_t) cast wrap keeps the preview visually
// sane (it just compresses toward the screen-relative edge) rather than
// producing garbage jumbled positions.
static int8_t DebugMode_ClampOffset(int value) {
    if (value < -128)
        return -128;
    if (value > 127)
        return 127;
    return (int8_t)value;
}

static void DebugMode_BuildStack(Object *obj, const uint8_t *base_mappings, const DebugFramePiece *stack, uint8_t stack_count) {
    uint8_t total = 0;
    uint8_t *out = debug_stack_buffer + 3;
    for (uint8_t i = 0; i < stack_count; i++) {
        uint8_t count;
        const uint8_t *pieces = DebugMode_LookupFrame(base_mappings, stack[i].frame, &count);
        for (uint8_t p = 0; p < count && total < DEBUG_STACK_MAX_PIECES; p++, total++, pieces += 5) {
            *out++ = (uint8_t)DebugMode_ClampOffset((int8_t)pieces[0] + stack[i].y_off); // ypos
            *out++ = pieces[1];                                                          // size
            *out++ = pieces[2];                                                          // tile hi
            *out++ = pieces[3];                                                          // tile lo
            *out++ = (uint8_t)DebugMode_ClampOffset((int8_t)pieces[4] + stack[i].x_off); // xpos
        }
    }
    debug_stack_buffer[0] = 0;
    debug_stack_buffer[1] = 2; // offset from buffer start to the piece-count byte at index 2
    debug_stack_buffer[2] = total;

    obj->mappings = debug_stack_buffer;
    obj->frame = 0;
    obj->render.f.raw_mappings = false;
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
static void DebugMode_RefreshPreview(Object *obj, const DebugListEntry *item) {
    if (item->mappings == NULL) {
        obj->mappings = Mappings_RingREV01;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
        obj->frame = 0;
        obj->render.f.raw_mappings = false;
        return;
    }

    obj->mappings = item->mappings;
    obj->tile = item->tile;
    obj->frame = item->frame;
    obj->render.f.raw_mappings = false;

    if (item->variants != NULL) {
        uint8_t key = (uint8_t)((debug_subtype >> item->variant_shift) & item->variant_mask);
        for (uint8_t i = 0; i < item->variant_count; i++) {
            if (item->variants[i].subtype_key != key)
                continue;
            const DebugSubtypeVariant *v = &item->variants[i];
            DebugMode_BuildStack(obj, (const uint8_t*)item->mappings, v->stack, v->stack_count);
            uint16_t base_tile = v->tile_override ? v->tile_override : item->tile;
            if (v->flip || v->pal_add || v->tile_override) {
                uint16_t tile = base_tile & (uint16_t)~(TILE_PALETTE_AND | TILE_X_FLIP_AND | TILE_Y_FLIP_AND);
                uint16_t pal = (uint16_t)(((base_tile & TILE_PALETTE_AND) >> TILE_PALETTE_SHIFT) + v->pal_add) & 3;
                tile |= (pal << TILE_PALETTE_SHIFT) & TILE_PALETTE_AND;
                if (v->flip & 1)
                    tile |= TILE_X_FLIP_AND;
                if (v->flip & 2)
                    tile |= TILE_Y_FLIP_AND;
                obj->tile = tile;
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
static void DebugMode_ShowItem(Object *obj) {
    int count;
    const DebugListEntry *list = DebugList_Get(&count);
    if (debug_item >= count)
        debug_item = 0;
    const DebugListEntry *item = &list[debug_item];
    debug_subtype = item->subtype;
    DebugMode_RefreshPreview(obj, item);
}

// Debug (object placement) mode -- entered by pressing B while debug_cheat
// is active (see the "Enter debug mode" check below), exited by pressing B
// again. debug_use doubles as a routine selector matching the
// disassembly's DebugMode: 1 (set by that B-press check) means "just
// entered, run one-time setup", 2 means "already active".
static void DebugMode(Object *obj) {
    if (debug_use == 1) {
        // One-time setup: temporarily unlock the level's Y boundaries (so
        // debug mode can fly anywhere), pick a valid starting item, and
        // show its placeholder sprite in place of Sonic.
        limit_top_db = limit_top2;
        limit_btm_db = limit_btm1;
        limit_top2 = 0;
        limit_btm1 = 0x800 - 224;

        obj->render.b = 0;
        obj->render.f.align_fg = true; // obj->pos tracks world/playfield space (see the free-movement code below), not raw screen coords -- BuildSprites needs this to convert it correctly
        obj->priority = 2;
        obj->width_pixels = 8;
        DebugMode_ShowItem(obj);

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
            obj->pos.l.y.v -= speed;
        if (jpad1_hold1 & JPAD_DOWN)
            obj->pos.l.y.v += speed;
        if (jpad1_hold1 & JPAD_LEFT)
            obj->pos.l.x.v -= speed;
        if (jpad1_hold1 & JPAD_RIGHT)
            obj->pos.l.x.v += speed;
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
        DebugMode_ShowItem(obj);
    } else if (jpad1_press1 & JPAD_A) {
        // Cycle forward one item.
        debug_item++;
        if (debug_item >= count)
            debug_item = 0;
        DebugMode_ShowItem(obj);
    } else if (jpad1_press1 & JPAD_C) {
        // Spawn the currently-selected item at the debug object's position.
        const DebugListEntry *item = &list[debug_item];
        if (item->type != ObjId_Null) {
            Object *spawned = FindFreeObj();
            if (spawned != NULL) {
                spawned->pos.l.x.f.u = obj->pos.l.x.f.u;
                spawned->pos.l.y.f.u = obj->pos.l.y.f.u;
                spawned->scratch.u8[0] = debug_subtype; // obSubtype -- read by each object's own Init routine
                spawned->type = item->type;
            }
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
        DebugMode_RefreshPreview(obj, &list[debug_item]);
    } else if (jpad1_press1 & JPAD_B) {
        // Exit back to normal Sonic.
        debug_use = 0;

        limit_top2 = limit_top_db;
        limit_btm1 = limit_btm_db;

        obj->mappings = Mappings_Sonic;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
        obj->frame = 0;
        obj->anim = SonAnimId_Walk;
        obj->width_pixels = 24;
        obj->render.f.align_fg = true;
    }

    DisplaySprite(obj);
}

// Sonic object
void Obj_Sonic(Object* obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Run debug mode code while in debug mode
    if (debug_use) {
        DebugMode(obj);
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
    switch (obj->routine) {
    case 0: // Initialiation
        // Increment routine
        obj->routine += 2;

        // Initialize collision size
        obj->y_rad = SONIC_HEIGHT;
        obj->x_rad = SONIC_WIDTH;

        // Set object drawing information
        obj->mappings = Mappings_Sonic;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
        obj->priority = 2;
        obj->width_pixels = 24;
        obj->render.b = 0;
        obj->render.f.align_fg = true;

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
            switch ((obj->status.p.f.in_ball << 2) | (obj->status.p.f.in_air << 1)) {
            case 0: // Not in ball, not in air
                if (Sonic_SpinDash(obj))
                    break;
                if (Sonic_Jump(obj))
                    break;
                Sonic_SlopeResist(obj);
                Sonic_Move(obj);
                Sonic_Roll(obj);
                Sonic_LevelBound(obj);
                SpeedToPos(obj);
                Sonic_AnglePos(obj);
                Sonic_SlopeRepel(obj);
                break;
            case 2: // Not in ball, in air
                spindash_flag &= ~1; // see-saw bug fix: don't get stuck charging if launched airborne
                Sonic_JumpHeight(obj);
                Sonic_JumpDirection(obj);
                Sonic_LevelBound(obj);
                ObjectFall(obj);
                if (obj->status.p.f.underwater)
                    obj->ysp -= 0x28;
                Sonic_JumpAngle(obj);
                Sonic_Floor(obj);
                break;
            case 4: // In ball, not in air
                // Real Obj01_MdRoll skips the jump check entirely while
                // pinball_mode/must_roll is set (GHZTunnel) -- can't jump
                // out of a forced roll.
                if (!obj->status.p.f.must_roll && Sonic_Jump(obj))
                    break;
                Sonic_RollRepel(obj);
                Sonic_RollSpeed(obj);
                Sonic_LevelBound(obj);
                SpeedToPos(obj);
                Sonic_AnglePos(obj);
                Sonic_SlopeRepel(obj);
                break;
            case 6: // In ball, in air
                spindash_flag &= ~1; // see-saw bug fix
                Sonic_JumpHeight(obj);
                Sonic_JumpDirection(obj);
                Sonic_LevelBound(obj);
                ObjectFall(obj);
                if (obj->status.p.f.underwater)
                    obj->ysp -= 0x28;
                Sonic_JumpAngle(obj);
                Sonic_Floor(obj);
                break;
            }
        }

        // Handle general player state stuff
        Sonic_Display(obj);
        Sonic_RecordPosition(obj);
        Sonic_Water(obj);

        // Copy angle buffers
        scratch->front_angle = angle_buffer0;
        scratch->back_angle = angle_buffer1;

        // Animate
        if (tunnel_mode) {
            if (!obj->anim)
                obj->prev_anim = obj->anim;
        }
        Sonic_Animate(obj);

        // Handle object and loop interaction
        if (!(lock_multi & 0x80))
            ReactToItem(obj);
        Sonic_Loops(obj);

        // Handle DPLCs
        Sonic_LoadGfx(obj);
        break;
    case 4: // Hurt
        // Fall
        SpeedToPos(obj);
        obj->ysp += 0x30;
        if (obj->status.p.f.underwater)
            obj->ysp -= 0x20;

        // Collision
        Sonic_HurtStop(obj);

        // Handle general player state stuff
        Sonic_RecordPosition(obj);
        Sonic_LevelBound(obj);

        // Animate
        Sonic_Animate(obj);

        // Handle DPLCs and draw sprite
        Sonic_LoadGfx(obj);
        DisplaySprite(obj);
        break;
    case 6: // Dead
        // Check for respawning and fall
        GameOver(obj);
        ObjectFall(obj);

        // Handle general player state stuff
        Sonic_RecordPosition(obj);

        // Animate
        Sonic_Animate(obj);

        // Handle DPLCs and draw sprite
        Sonic_LoadGfx(obj);
        DisplaySprite(obj);
        break;
    case 8: // Dead, respawning
        // Handle death timer
        if (scratch->death_timer && --scratch->death_timer == 0)
            restart = true;
        break;
    }
}
