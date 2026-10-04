// Tails (Nick Arcade's objects 02 and 05), made as Sonic Team made him: a copy of Sonic's object (Sonic.c) with the changes Tails needs, and the same copy's quirks. His control is pad 2,
// which a stub of a "CPU" fills with Sonic's own pad from 16 frames ago; he has no water code, no debug mode and no flight yet; hurting him loses SONIC'S rings (the copy of the hurt
// code works on the shared ring count); he does not die from enemies, only from falling below the level, and then he comes back above Sonic.
#include "Object/Tails.h"
#include "SplitScreen.h"
#include "Backend/VDP.h"
#include "Object/Sonic.h"
#include "Constants.h"

#include "Game.h"
#include "GM_Level.h"
#include "Level.h"
#include "Object/CharControl.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object.h"
#include "PLC.h"
#include "SpecialStage.h"
#include "Sound.h"

#include <string.h>

#include "Resource/Mappings/Tails.h"
#include "Resource/Art/Tails.h"
#include "Resource/Mappings/TailsDPLC.h"
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
#define TAILSTAILS_SLOT    0x1D

// Spin Dash state
static uint8_t spindash_flag;
static uint16_t spindash_count;

// Sonic's pad for the last 64 frames (Sonic_Stat_Record_Buf), and how long a real pad 2 has had Tails (Tails_control_counter)
static uint8_t pad_hold[0x40], pad_press[0x40];
static uint8_t pad_head;
static uint16_t control_counter;

// The last frame loaded of each of Tails' art windows
static uint8_t last_frame_tails = 0xFF, last_frame_tail = 0xFF;

static signed int HurtTails(Object *obj, Object *src);

// General Sonic state stuff
static void Tails_Display(Object *obj) {
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
            sonspeed_max = 0x600; // BUG: Water isn't checked
            sonspeed_acc = 0xC;
            sonspeed_dec = 0x80;

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

static void Tails_LoadGfx(Object *obj) {
    Tails_LoadDPLC(obj->frame, &last_frame_tails, ArtTile_Tails);
}

// Tails_Control and TailsCPU_Control: a pad 2 that is being used keeps Tails for 300 frames; otherwise he copies Sonic's pad from 16 frames back (the CPU's other states
// only pass on to this one)
static void Tails_Control(void) {
    if (SplitScreen_Active()) // the second player has pad 2 (and a camera of his own)
        return;
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
static void Tails_AnimateReadFrame(Object *obj, const uint8_t *anim_script) {
    uint8_t cmd = anim_script[1 + obj->anim_frame];
    if (cmd < 0xF0) {
    Anim_Next:
        obj->frame = cmd;
        obj->anim_frame++;
    } else {
        if (++cmd == 0) { // restart
            obj->anim_frame = 0;
            cmd = anim_script[1];
            goto Anim_Next;
        }
        if (++cmd == 0) { // go back (next byte) frames
            obj->anim_frame -= anim_script[2 + obj->anim_frame];
            cmd = anim_script[1 + obj->anim_frame];
            goto Anim_Next;
        }
        if (++cmd == 0) // change animation
            obj->anim = anim_script[2 + obj->anim_frame];
    }
}

static void Tails_AnimateTable(Object *obj, const uint8_t *table, const Object *tails) {
    uint8_t anim = obj->anim;
    if (anim != obj->prev_anim) {
        obj->prev_anim = anim;
        obj->anim_frame = 0;
        obj->frame_time.b = 0;
    }
#define TAILS_SCRIPT(id) (table + ((table[(id) << 1] << 8) | table[((id) << 1) + 1]))
    const uint8_t *script = TAILS_SCRIPT(anim);

    int8_t wait = (int8_t)script[0];
    if (wait >= 0) { // a regular animation
        obj->render.f.x_flip = obj->status.p.f.x_flip;
        obj->render.f.y_flip = false;
        if (--obj->frame_time.b >= 0)
            return;
        obj->frame_time.b = wait;
        Tails_AnimateReadFrame(obj, script);
        return;
    }

    if (--obj->frame_time.b >= 0)
        return;
    uint16_t abs_spd = (obj->inertia < 0) ? -obj->inertia : obj->inertia;
    if (++wait == 0) { // $FF: walking or running
        uint8_t angle = obj->angle;
        uint8_t flip = obj->status.p.f.x_flip;
        if (!flip)
            angle ^= ~0;
        if ((angle += 0x10) & 0x80)
            flip ^= 3;
        obj->render.f.x_flip = (flip & 1) != 0;
        obj->render.f.y_flip = (flip & 2) != 0;
        // Tumbling (flung by a spring): the frame comes from the flip angle alone
        uint8_t fa = ((Scratch_Sonic*)&obj->scratch)->flip_angle;
        if (fa) {
            if (!obj->status.p.f.x_flip) {
                obj->render.f.x_flip = false;
                obj->render.f.y_flip = false;
                obj->frame = (uint8_t)(((uint8_t)(fa + 0xB)) / 0x16 + 0x75);
            } else {
                obj->render.f.x_flip = true;
                obj->render.f.y_flip = true;
                obj->frame = (uint8_t)(((uint8_t)((uint8_t)-fa + 0x8F)) / 0x16 + 0x75);
            }
            obj->frame_time.b = 0;
            return;
        }
        if (obj->status.p.f.pushing)
            goto Anim_Pushing;
        angle = (angle >> 4) & 6;
        const uint8_t *s = TAILS_SCRIPT(SonAnimId_Walk);
        uint8_t offset = (uint8_t)(angle << 2);
        if (abs_spd >= 0x600) {
            s = TAILS_SCRIPT(SonAnimId_Run);
            offset = (uint8_t)((angle + (angle >> 1)) << 1);
        }
        int16_t spd = 0x800 - abs_spd;
        if (spd < 0)
            spd = 0;
        obj->frame_time.b = spd >> 8;
        Tails_AnimateReadFrame(obj, s);
        obj->frame += offset;
    } else if (++wait == 0) { // $FE: rolling
        const uint8_t *s = TAILS_SCRIPT(SonAnimId_Roll2);
        if (abs_spd < 0x600)
            s = TAILS_SCRIPT(SonAnimId_Roll);
        int16_t spd = 0x400 - abs_spd;
        if (spd < 0)
            spd = 0;
        obj->frame_time.b = spd >> 8;
        obj->render.f.x_flip = obj->status.p.f.x_flip;
        obj->render.f.y_flip = false;
        Tails_AnimateReadFrame(obj, s);
    } else if (++wait == 0) { // $FD: pushing
    Anim_Pushing:;
        int16_t spd = 0x800 - abs_spd;
        if (spd < 0)
            spd = 0;
        obj->frame_time.b = spd >> 6;
        obj->render.f.x_flip = obj->status.p.f.x_flip;
        obj->render.f.y_flip = false;
        Tails_AnimateReadFrame(obj, TAILS_SCRIPT(SonAnimId_Push));
    } else { // $FC: the tails, turned the way Tails moves
        uint8_t angle = CalcAngle(tails->xsp, tails->ysp);
        uint8_t flip = 0;
        if (!obj->status.p.f.x_flip)
            angle = (uint8_t)~angle;
        else
            angle = (uint8_t)(angle + 0x80);
        angle = (uint8_t)(angle + 0x10);
        if (angle & 0x80)
            flip = 3;
        obj->render.f.x_flip = ((obj->status.p.f.x_flip ^ flip) & 1) != 0;
        obj->render.f.y_flip = ((obj->status.p.f.x_flip ^ flip) & 2) != 0;
        uint8_t offset = (uint8_t)((angle >> 3) & 0xC);
        obj->frame_time.b = 3;
        Tails_AnimateReadFrame(obj, script);
        obj->frame += offset;
    }
#undef TAILS_SCRIPT
}

static void Tails_Animate(Object *obj) {
    Tails_AnimateTable(obj, Animation_Tails, obj);
}

// Falling below the level (the only way he dies): Tails_GameOver brings him back above Sonic
void KillTails(Object *obj) {
    obj->routine = 6;
    Tails_ResetOnFloor(obj);
    obj->status.p.f.in_air = true;
    obj->ysp = -0x700;
    obj->xsp = 0;
    obj->inertia = 0;
    obj->anim = SonAnimId_Death;
    obj->tile |= TILE_PRIORITY_AND;
    PlaySound(sfx_Death);
}

// Sonic collision functions
void Tails_ResetOnFloor(Object *obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Set state
    obj->status.p.f.pushing = false;
    obj->status.p.f.in_air = false;
    obj->status.p.f.roll_jump = false;

    if (obj->status.p.f.in_ball) {
        obj->status.p.f.in_ball = false;
        obj->y_rad = TAILS_HEIGHT;
        obj->x_rad = TAILS_WIDTH;
        obj->anim = SonAnimId_Walk;
        obj->pos.l.y.f.u -= 1; // (one, not Sonic's five)
    }

    scratch->flip_angle = 0;
    scratch->jumping = false;
    item_bonus = 0;
}

static int16_t Tails_Angle(Object *obj, int16_t dist0, int16_t dist1) {
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

static void Tails_AnglePos(Object *obj) {
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
        if ((dist = Tails_Angle(obj, dist0, dist1)) != 0) {
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
        if ((dist = Tails_Angle(obj, dist0, dist1)) != 0) {
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
        if ((dist = Tails_Angle(obj, dist0, dist1)) != 0) {
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
        if ((dist = Tails_Angle(obj, dist0, dist1)) != 0) {
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

static void Tails_Floor(Object *obj) {
    // Get angle we're moving in
    // There's some weird logging stuff done here
    // Maybe testing if the CalcAngle is yielding desirable results?
    uint8_t angle = CalcAngle(obj->xsp, obj->ysp);
    angle -= 0x20;
    angle &= 0xC0;

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

        clip = -((obj->ysp >> 8) + 8);
        if (dist1 < 0 && (dist0 >= clip || dist1 >= clip)) {
            // Set state
            obj->pos.l.y.f.u += dist1;
            obj->angle = hit_angle;
            Tails_ResetOnFloor(obj);
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
                Tails_ResetOnFloor(obj);
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
                Tails_ResetOnFloor(obj);
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
                Tails_ResetOnFloor(obj);
                obj->anim = SonAnimId_Walk;

                // Get inertia
                obj->ysp = 0;
                obj->inertia = obj->xsp;
            }
        }
        break;
    }
}

static void Tails_HurtStop(Object *obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    // Die when falling below the level
    if ((limit_btm2 + SCREEN_HEIGHT) < obj->pos.l.y.f.u) {
        KillTails(obj);
        return;
    }

    // Do collision
    Tails_Floor(obj);
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
    return HurtTails(obj, hit);
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

        // Check if we're touching (same overlap test as the original's React_CheckHitboxOverlap)
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

int32_t Tails_Hurt(Object *obj, Object *src) {
    return HurtTails(obj, src);
}

// Sonic functions
static signed int HurtTails(Object *obj, Object *src)
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
        }
    }
    shield = 0;

    // Set Sonic state
    obj->routine = 4;
    spindash_flag &= ~1;
    Tails_ResetOnFloor(obj);
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

// Sonic movement functions
static bool Tails_Jump(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Don't jump if ABC isn't pressed
    if (!(jpad2_press & (JPAD_A | JPAD_C | JPAD_B)))
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

    obj->y_rad = TAILS_HEIGHT; // No idea why this is here
    obj->x_rad = TAILS_WIDTH;

    if (!obj->status.p.f.in_ball) {
        obj->y_rad = TAILS_BALL_HEIGHT;
        obj->x_rad = TAILS_BALL_WIDTH;
        obj->anim = SonAnimId_Roll;
        obj->status.p.f.in_ball = true;
        obj->pos.l.y.f.u += TAILS_BALL_SHIFT;
    } else {
        obj->status.p.f.roll_jump = true;
    }
    return true;
}

static void Tails_SlopeResist(Object *obj) {
    if (((obj->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(obj->angle) * 0x20) >> 8;
    if (obj->inertia != 0)
        obj->inertia += force;
}

static void Tails_MoveLeft(Object *obj) {
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

        // (Nick Arcade tests the masked angle against the speed where it means the speed, so its skid never happens: neither does this)
    }
}

static void Tails_MoveRight(Object *obj) {
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

        // (Nick Arcade tests the masked angle against the speed where it means the speed, so its skid never happens: neither does this)
    }
}

static void Tails_Move(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    if (f_slidemode)
        goto Tails_AngleSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad2_hold & JPAD_LEFT)
                Tails_MoveLeft(obj);
            if (jpad2_hold & JPAD_RIGHT)
                Tails_MoveRight(obj);

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
                        goto Tails_ResetScr;
                    }
                } else {
                    // Balance on level
                    if (ObjFloorDist(obj, obj->pos.l.x.f.u) >= 12) {
                        if (scratch->front_angle == 3) {
                            obj->status.p.f.x_flip = false;
                            obj->anim = SonAnimId_Balance;
                            goto Tails_ResetScr;
                        }
                        if (scratch->back_angle == 3) {
                            obj->status.p.f.x_flip = true;
                            obj->anim = SonAnimId_Balance;
                            goto Tails_ResetScr;
                        }
                    }
                }

            // Handle looking up and down
            LookUpDown:;
                if (jpad2_hold & JPAD_UP) {
                    obj->anim = SonAnimId_LookUp;
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
                    obj->anim = SonAnimId_Duck;
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
Tails_AngleSpeed:;
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

void Tails_ChkRoll(Object *obj) {
    // Enter roll state
    if (obj->status.p.f.in_ball)
        return;
    obj->status.p.f.in_ball = true;
    obj->y_rad = TAILS_BALL_HEIGHT;
    obj->x_rad = TAILS_BALL_WIDTH;
    obj->anim = SonAnimId_Roll;
    obj->pos.l.y.f.u += TAILS_BALL_SHIFT;
    PlaySound(sfx_Roll);

    // Set speed (S-tubes)
    if (!obj->inertia)
        obj->inertia = 0x200;
}

static void Tails_Roll(Object *obj) {
    // Don't allow rolling while on a water slide
    if (f_slidemode)
        return;

    // Check if we can and are trying to roll
    if (jump_only || ((obj->inertia < 0) ? -obj->inertia : obj->inertia) < 0x80)
        return;
    if ((jpad2_hold & (JPAD_LEFT | JPAD_RIGHT)) || !(jpad2_hold & JPAD_DOWN))
        return;
    Tails_ChkRoll(obj);
}

static void Tails_LevelBound(Object *obj) {
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
        // (SBZ2's drop into SBZ3 is not a kill-plane exception any more: the cutscene triggers it itself, see
        // DynamicLevelEvents' SBZ act 2 case.)
        KillTails(obj);
    }
}

static void Tails_SlopeRepel(Object *obj) {
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

static void Tails_RollRepel(Object *obj) {
    if (((obj->angle + 0x60) & 0xFF) >= 0xC0)
        return;

    int16_t force = (GetSin(obj->angle) * 0x50) >> 8;
    if (obj->inertia >= 0) { // (standing still counts as moving right: the original tests only for a negative speed)
        if (force < 0)
            force >>= 2;
        obj->inertia += force;
    } else {
        if (force >= 0)
            force >>= 2;
        obj->inertia += force;
    }
}

static void Tails_RollLeft(Object *obj) {
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

static void Tails_RollRight(Object *obj) {
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

static void Tails_RollSpeed(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    if (f_slidemode)
        goto Tails_AngledRollSpeed;

    if (!jump_only) {
        if (!scratch->control_lock) {
            // Move left and right according to held direction
            if (jpad2_hold & JPAD_LEFT)
                Tails_RollLeft(obj);
            if (jpad2_hold & JPAD_RIGHT)
                Tails_RollRight(obj);
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
        // Tails_KeepRolling: "magically gives Sonic an extra push if he's
        // going to stop rolling where it's not allowed, such as in an
        // S-curve").
        if (obj->inertia == 0) {
            if (obj->status.p.f.must_roll) {
                obj->inertia = obj->status.p.f.x_flip ? (int16_t)-0x400 : (int16_t)0x400;
            } else {
                obj->status.p.f.in_ball = false;
                obj->y_rad = TAILS_HEIGHT;
                obj->x_rad = TAILS_WIDTH;
                obj->anim = SonAnimId_Wait;
                obj->pos.l.y.f.u -= TAILS_BALL_SHIFT;
            }
        }
    }

    // Calculate global speed from inertia
Tails_AngledRollSpeed:;
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

static void Tails_JumpHeight(Object *obj) {
    Scratch_Sonic* scratch = (Scratch_Sonic*)&obj->scratch;

    if (scratch->jumping) {
        // Get minimum jump speed and apply if ABC isn't held
        int16_t spd = obj->status.p.f.underwater ? -0x200 : -0x400;
        if (obj->ysp < spd && !(jpad2_hold & (JPAD_A | JPAD_C | JPAD_B)))
            obj->ysp = spd;
    } else {
        // Cap upwards speed
        if (obj->ysp < -0xFC0)
            obj->ysp = -0xFC0;
    }
}

static void Tails_JumpDirection(Object *obj) {
    // Handle acceleration
    if (!obj->status.p.f.roll_jump) {
        int16_t xsp = obj->xsp;

        // Accelerate left
        if (jpad2_hold & JPAD_LEFT) {
            obj->status.p.f.x_flip = true;
            if ((xsp -= (sonspeed_acc << 1)) <= -sonspeed_max)
                xsp = -sonspeed_max;
        }

        // Accelerate right
        if (jpad2_hold & JPAD_RIGHT) {
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

static void Tails_JumpAngle(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    // Reset angle towards 0
    uint8_t angle = obj->angle;
    if (angle != 0) {
        if (!(angle & 0x80)) {
            if ((angle -= 2) & 0x80)
                angle = 0;
        } else {
            if (!((angle += 2) & 0x80))
                angle = 0;
        }

        obj->angle = angle;
    }

    // Tumble (a spring's flip), as Sonic's
    uint8_t flip = scratch->flip_angle;
    if (flip == 0)
        return;

    if (obj->inertia >= 0) {
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
static void Tails_Spindash_ResetScr(Object *obj) {
    // Reset camera to neutral position (same logic as Tails_Move/Tails_JumpDirection)
    if (look_shift < (96 + SCREEN_TALLADD2))
        look_shift += 2;
    else if (look_shift > (96 + SCREEN_TALLADD2))
        look_shift -= 2;

    // Since we skip the rest of the normal-movement case, run these manually
    Tails_LevelBound(obj);
    Tails_AnglePos(obj);
}

static void Tails_ChargingSpindash(Object *obj) {
    obj->anim = SonAnimId_SpinDash; // make sure Spin Dash animation stays

    // Charge decay
    spindash_count -= spindash_count >> 5;

    if (!(jpad2_press & (JPAD_A | JPAD_C | JPAD_B))) {
        Tails_Spindash_ResetScr(obj);
        return;
    }

    // Restart Spin Dash animation
    obj->anim = SonAnimId_SpinDash;
    obj->anim_frame = 0;
    obj->frame_time.b = 0;
    PlaySound(sfx_SpindashRev);

    spindash_count += 0x200;
    if (spindash_count > 0x800)
        spindash_count = 0x800;

    Tails_Spindash_ResetScr(obj);
}

static void Tails_ReleaseSpindash(Object *obj) {
    spindash_flag &= ~1;
    obj->y_rad = TAILS_BALL_HEIGHT;
    obj->x_rad = TAILS_BALL_WIDTH;
    obj->pos.l.y.f.u += TAILS_BALL_SHIFT;
    obj->anim = SonAnimId_Roll;
    obj->status.p.f.in_ball = true;
    PlaySound(sfx_Teleport);

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

    // Set new velocities immediately (same convention as Tails_Move/Tails_RollSpeed: cos->xsp, sin->ysp)
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);
    obj->xsp = (int16_t)(((int32_t)cos * obj->inertia) >> 8);
    obj->ysp = (int16_t)(((int32_t)sin * obj->inertia) >> 8);

    Tails_Spindash_ResetScr(obj);
}

static void Tails_UpdateSpindash(Object *obj) {
    // Bug fix (part 2 of the monitor fix): guarantee the Spin Dash
    // animation is set as soon as we know we're spin dashing, before
    // Monitor's own Mon_SolidSides push-vs-break check (which now also
    // exempts this animation, see Monitor.c) gets a chance to run this
    // same frame and see a stale Push animation instead.
    obj->anim = SonAnimId_SpinDash;
    if (jpad2_hold & JPAD_DOWN) {
        Tails_ChargingSpindash(obj);
        return;
    }
    Tails_ReleaseSpindash(obj);
}

// Abandons a spin dash that is still charging when Sonic leaves the ground some other
// way than releasing it (e.g. a seesaw launch). Clearing only the flag left the dust
// companion in its Dash animation, so it kept following Sonic through the air.
void Tails_CancelSpindash(void) {
    if (!(spindash_flag & 1))
        return;
    spindash_flag &= ~1;
    spindash_count = 0;
}

static bool Tails_SpinDash(Object *obj) {
    if (spindash_flag & 1) {
        Tails_UpdateSpindash(obj);
        return true;
    }

    if (obj->anim != SonAnimId_Duck)
        return false;
    if (!(jpad2_press & (JPAD_A | JPAD_C | JPAD_B)))
        return false;

    obj->anim = SonAnimId_SpinDash;
    PlaySound(sfx_SpindashRev);
    spindash_flag |= 1;
    spindash_count = 0;

    // Because we're skipping the rest of the normal-movement case
    Tails_LevelBound(obj);
    Tails_AnglePos(obj);
    return true;
}


// The tails (object 05): follows Tails, turning with his animation. His own art window, and the animation of the tails for each of Tails' animations (Obj05_Animations).
static const uint8_t TailsTails_Animations[32] = {
    0, 0, 3, 3, 0, 1, 0, 2, 1, 7, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 7, // (animation 31: the port's spin dash)
};
static uint8_t tails_tails_prev_anim = 0xFF;

void Obj_TailsTails(Object *obj) {
    const Object *tails = &objects[TAILS_SLOT];
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_Tails;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_TailsTails);
        obj->priority = 2;
        obj->width_pixels = 0x18;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        tails_tails_prev_anim = 0xFF;
        last_frame_tail = 0xFF;
        // Fallthrough
    case 2:
        obj->angle = tails->angle;
        obj->status = tails->status;
        obj->pos.l.x.f.u = tails->pos.l.x.f.u;
        obj->pos.l.y.f.u = tails->pos.l.y.f.u;
        if (tails->anim != tails_tails_prev_anim) {
            tails_tails_prev_anim = tails->anim;
            obj->anim = TailsTails_Animations[tails->anim & 0x1F];
        }
        Tails_AnimateTable(obj, Animation_TailsTails, tails);
        Tails_LoadDPLC(obj->frame, &last_frame_tail, ArtTile_TailsTails);
        DisplaySprite(obj);
        break;
    }
}

// Tails (object 02)
void Obj_Tails(Object *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Initialization
        obj->routine += 2;
        obj->y_rad = TAILS_HEIGHT;
        obj->x_rad = TAILS_WIDTH;
        obj->mappings = Mappings_Tails;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Tails);
        obj->priority = 2;
        obj->width_pixels = 0x18;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        sonspeed_max = 0x600;
        sonspeed_acc = 0xC;
        sonspeed_dec = 0x80;
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
        objects[TAILSTAILS_SLOT].type = ObjId_05; // (his tails)
        // Fallthrough
    case 2: // Regular movement
        Tails_Control();

        if (!(CharObjControl(obj) & 1)) {
            switch ((obj->status.p.f.in_ball << 2) | (obj->status.p.f.in_air << 1)) {
            case 0: // Not in ball, not in air
                if (Tails_SpinDash(obj))
                    break;
                if (Tails_Jump(obj))
                    break;
                Tails_SlopeResist(obj);
                Tails_Move(obj);
                Tails_Roll(obj);
                Tails_LevelBound(obj);
                SpeedToPos(obj);
                Tails_AnglePos(obj);
                Tails_SlopeRepel(obj);
                break;
            case 2: // Not in ball, in air
                Tails_CancelSpindash();
                Tails_JumpHeight(obj);
                Tails_JumpDirection(obj);
                Tails_LevelBound(obj);
                ObjectFall(obj);
                if (obj->status.p.f.underwater)
                    obj->ysp -= 0x28;
                Tails_JumpAngle(obj);
                Tails_Floor(obj);
                break;
            case 4: // In ball, not in air
                if (!obj->status.p.f.must_roll && Tails_Jump(obj))
                    break;
                Tails_RollRepel(obj);
                Tails_RollSpeed(obj);
                Tails_LevelBound(obj);
                SpeedToPos(obj);
                Tails_AnglePos(obj);
                Tails_SlopeRepel(obj);
                break;
            case 6: // In ball, in air
                Tails_CancelSpindash();
                Tails_JumpHeight(obj);
                Tails_JumpDirection(obj);
                Tails_LevelBound(obj);
                ObjectFall(obj);
                if (obj->status.p.f.underwater)
                    obj->ysp -= 0x28;
                Tails_JumpAngle(obj);
                Tails_Floor(obj);
                break;
            }
        }

        Tails_Display(obj);
        scratch->front_angle = angle_buffer0;
        scratch->back_angle = angle_buffer1;
        Tails_Animate(obj);
        if (!(CharObjControl(obj) & 0x80))
            ReactToItem(obj);
        Tails_LoadGfx(obj);
        break;
    case 4: // Hurt
        SpeedToPos(obj);
        obj->ysp += 0x30;
        if (obj->status.p.f.underwater)
            obj->ysp -= 0x20;
        Tails_HurtStop(obj);
        if (obj->routine != 6) {
            Tails_LevelBound(obj);
            Tails_Animate(obj);
            Tails_LoadGfx(obj);
            DisplaySprite(obj);
        }
        break;
    case 6: // Dead (fell below the level): back above Sonic once he is well out of sight
        if ((uint16_t)(limit_btm2 + 0x100) < (uint16_t)obj->pos.l.y.f.u) {
            obj->pos.l.x.f.u = player->pos.l.x.f.u - 0x40;
            obj->pos.l.y.f.u = player->pos.l.y.f.u - 0x80;
            obj->routine = 2;
            obj->tile &= ~TILE_PRIORITY_AND;
        } else {
            ObjectFall(obj);
            Tails_Animate(obj);
            Tails_LoadGfx(obj);
            DisplaySprite(obj);
        }
        break;
    }
}

// The level's own objects, beyond what Sonic 1's level start makes: Tails, a little behind Sonic (in every zone: Nick Arcade left him out of Emerald Hill, the Simon Wai prototype does not)
void Game_LevelObjects(void) {
    VDP_SetShadowHighlight((jpad1_hold1 & JPAD_C) != 0); // (the prototype enables the VDP's shadow/highlight mode, which darkens all but what is above the planes' priority, when C is held as a level loads)
    if (cli_start_level >= 0) // a level started from the command line skipped the title, which loads the main art (HUD, rings, ...)
        AddPLC(PlcId_Main);
    Object *tails = &objects[TAILS_SLOT];
    memset(tails, 0, sizeof(*tails));
    tails->type = ObjId_02;
    tails->pos.l.x.f.u = player->pos.l.x.f.u - 0x20;
    tails->pos.l.y.f.u = player->pos.l.y.f.u;
    memset(&objects[TAILSTAILS_SLOT], 0, sizeof(Object));
    SplitScreen_LoadLevel(); // (a level picked with B in the level select is a two-player one: Tails is its second player)
}
