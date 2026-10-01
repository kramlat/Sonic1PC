#include "SpecialSonic.h"
#include "Sonic.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "SpecialStage.h"
#include "Sound.h"

// Object 09 - Sonic in the special stages. Ported from s1disasm's "_incObj/09 Sonic in Special Stage.asm".
//
// Sonic is a ball in a rotating maze: the stage spins and gravity always points at the bottom of the screen, so
// in stage coordinates "down" keeps changing. Positions are in stage (unrotated) pixels; blocks are 24px.

void Ring_Collect(void); // Object/Ring.c (its header defines the ring art, so it can't be included twice)

#define SS_MAXSPEED     0x800
#define SS_ACCELERATION 0xC
#define SS_DECELERATION 0x40
#define SS_JUMPSPEED    0x680
#define SS_GRAVITY      0x2A // gravity-$E

#define SS_ROWLENGTH SS_DIM // bytes per layout row (ss_layout_rowlength)

static Scratch_SpecialSonic *SC(Object *o) { return (Scratch_SpecialSonic *)&o->scratch; }

// SS_FixCamera: keep the camera centred on Sonic.
static void SS_FixCamera(Object *obj) {
    uint16_t x = (uint16_t)obj->pos.l.x.f.u;
    if (x >= SCREEN_WIDTH / 2)
        scrpos_x.f.u = (int16_t)(x - SCREEN_WIDTH / 2);
    uint16_t y = (uint16_t)obj->pos.l.y.f.u;
    if (y >= SCREEN_HEIGHT / 2)
        scrpos_y.f.u = (int16_t)(y - SCREEN_HEIGHT / 2);
}

// ---------------------------------------------------------------------------
// Wall detection
// ---------------------------------------------------------------------------

// Is this block one Sonic can't pass through? A solid one is remembered in the touched-block fields.
static bool SS_CheckType(Object *obj, uint8_t block, uint32_t index) {
    if (block == SSB_Blank || block == SSB_1Up)
        return false;
    if (block >= SSB_Ring && block < SSB_Glass_Ani1)
        return false; // $3A-$4A are non-solid (rings, emeralds, ghost blocks...)
    SC(obj)->touched_id = block;
    SC(obj)->touched_index = index;
    return true;
}

// SonicSS_FindWall: does a position (including fraction) overlap a solid block? Checks the four blocks around it.
static bool SS_FindWall(Object *obj, int32_t y, int32_t x) {
    uint16_t row = (uint16_t)(((uint16_t)(y >> 16) + 20 + SS_BLOCKSIZE * 2) / SS_BLOCKSIZE);
    uint16_t col = (uint16_t)(((uint16_t)(x >> 16) + 20) / SS_BLOCKSIZE);
    uint32_t i = (uint32_t)row * SS_ROWLENGTH + col;

    bool solid = false;
    solid |= SS_CheckType(obj, ss_layout[i], i + 1);
    solid |= SS_CheckType(obj, ss_layout[i + 1], i + 2);
    i += SS_ROWLENGTH;
    solid |= SS_CheckType(obj, ss_layout[i], i + 1);
    solid |= SS_CheckType(obj, ss_layout[i + 1], i + 2);
    return solid;
}

// The index stored is that of the byte after the block (the real code reads with post-increment); the block is
// one before it.
static uint32_t SS_TouchedBlock(Object *obj) { return SC(obj)->touched_index - 1; }

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

static void SS_MakeGhostSolid(Object *obj) {
    if (SC(obj)->ghost_state == 2) { // a ghost block and then the invisible trigger have been passed
        for (int i = 0; i < SS_DIM * SS_DIM; i++)
            if (ss_layout[i] == SSB_Ghost)
                ss_layout[i] = SSB_RedWhite;
    }
    SC(obj)->ghost_state = 0;
}

// SonicSS_ChkItems_NonSolidActionBlock: things Sonic passes through (rings, emeralds, 1-ups, ghost blocks).
static void SS_CheckNonSolidItems(Object *obj) {
    Scratch_SpecialSonic *s = SC(obj);
    uint16_t row = (uint16_t)(((uint16_t)obj->pos.l.y.f.u + 80) / SS_BLOCKSIZE);
    uint16_t col = (uint16_t)(((uint16_t)obj->pos.l.x.f.u + 32) / SS_BLOCKSIZE);
    uint32_t cell = (uint32_t)row * SS_ROWLENGTH + col;
    uint8_t block = ss_layout[cell];

    if (block == SSB_Blank) {
        if (s->ghost_state != 0)
            SS_MakeGhostSolid(obj);
        return;
    }

    if (block == SSB_Ring) {
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_RingSparks;
            a->block = cell;
        }
        Ring_Collect();
        if (rings >= 50) { // an extra continue at 50 rings (once)
            if (!(life_num & 4)) {
                life_num |= 4;
                continues++;
                QueueSound1(sfx_Continue);
            }
        }
    } else if (block == SSB_1Up) {
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_1Up;
            a->block = cell;
        }
        lives++;
        life_count++;
        QueueSound1(bgm_ExtraLife);
    } else if (block >= SSB_Emerald1_Blue && block <= SSB_Emerald6_Grey) {
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_EmeraldSparks;
            a->block = cell;
        }
        if (emeralds != 6) {
            emerald_list[emeralds] = (uint8_t)(block - SSB_Emerald1_Blue);
            emeralds++;
        }
        QueueSound2(bgm_Emerald);
    } else if (block == SSB_Ghost) {
        s->ghost_state = 1; // passed a ghost block
    } else if (block == SSB_InvGhostTrigger) {
        if (s->ghost_state == 1)
            s->ghost_state = 2; // ...and then the invisible switch: the ghost blocks turn solid
    }
}

// SonicSS_ChkItems_SolidActionBlock: blocks Sonic bounces off that also do something (bumper, UP/DOWN, R, glass, GOAL).
static void SS_CheckSolidItems(Object *obj) {
    Scratch_SpecialSonic *s = SC(obj);
    uint8_t block = s->touched_id;

    if (block == 0) { // nothing solid was touched: let the UP/DOWN and R timeouts run down
        if (--s->timeout_updown & 0x80)
            s->timeout_updown = 0;
        if (--s->timeout_r & 0x80)
            s->timeout_r = 0;
        return;
    }

    uint32_t index = SS_TouchedBlock(obj);

    if (block == SSB_Bumper) {
        // Throw Sonic away from the bumper's centre.
        int16_t bx = (int16_t)((index & (SS_ROWLENGTH - 1)) * SS_BLOCKSIZE - 20);
        int16_t by = (int16_t)(((index >> 7) & (SS_ROWLENGTH - 1)) * SS_BLOCKSIZE - (20 + SS_BLOCKSIZE * 2));
        int16_t dx = (int16_t)(bx - obj->pos.l.x.f.u);
        int16_t dy = (int16_t)(by - obj->pos.l.y.f.u);
        uint8_t angle = (uint8_t)CalcAngle(dx, dy);
        int16_t sin, cos;
        CalcSine(angle, &sin, &cos);
        obj->xsp = (int16_t)((cos * -0x700) >> 8);
        obj->ysp = (int16_t)((sin * -0x700) >> 8);
        obj->status.p.f.in_air = true;
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_Bumper;
            a->block = index;
        }
        QueueSound2(sfx_Bumper);
    } else if (block == SSB_GOAL) {
        obj->routine += 2; // ExitStage
        QueueSound2(sfx_SSGoal);
    } else if (block == SSB_UP) {
        if (s->timeout_updown != 0)
            return;
        s->timeout_updown = SS_TIMEOUT;
        int rot = (int16_t)ss_rotate;
        if (rot < 0)
            rot = -rot;
        if (rot < SS_ROTATESPEED * 2) { // not fast yet: double the speed, and the block becomes a DOWN block
            ss_rotate = (uint16_t)((int16_t)ss_rotate * 2);
            ss_layout[index] = SSB_DOWN;
        }
        QueueSound2(sfx_SSItem);
    } else if (block == SSB_DOWN) {
        if (s->timeout_updown != 0)
            return;
        s->timeout_updown = SS_TIMEOUT;
        int rot = (int16_t)ss_rotate;
        if (rot < 0)
            rot = -rot;
        if (rot > SS_ROTATESPEED) { // not slow yet: halve the speed, and the block becomes an UP block
            ss_rotate = (uint16_t)((int16_t)ss_rotate / 2);
            ss_layout[index] = SSB_UP;
        }
        QueueSound2(sfx_SSItem);
    } else if (block == SSB_R) {
        if (s->timeout_r != 0)
            return;
        s->timeout_r = SS_TIMEOUT;
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_Reverse;
            a->block = index;
        }
        ss_rotate = (uint16_t)-(int16_t)ss_rotate; // reverse the rotation, keeping its speed
        QueueSound2(sfx_SSItem);
    } else if (block >= SSB_Glass1_Blue && block <= SSB_Glass4_Pink) {
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_GlassBlock;
            a->block = index;
            uint8_t next = (uint8_t)(ss_layout[index] + 1); // the next, weaker glass type
            a->next_id = (next > SSB_Glass4_Pink) ? 0 : next;
        }
        QueueSound2(sfx_SSGlass);
    }
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

static void SS_MoveLeft(Object *obj) {
    obj->status.p.f.x_flip = true;
    int16_t speed = obj->inertia;
    if (speed <= 0) {
        speed = (int16_t)(speed - SS_ACCELERATION);
        if (speed <= -SS_MAXSPEED)
            speed = -SS_MAXSPEED;
    } else {
        speed = (int16_t)(speed - SS_DECELERATION); // turning round
    }
    obj->inertia = speed;
}

static void SS_MoveRight(Object *obj) {
    obj->status.p.f.x_flip = false;
    int16_t speed = obj->inertia;
    if (speed >= 0) {
        speed = (int16_t)(speed + SS_ACCELERATION);
        if (speed >= SS_MAXSPEED)
            speed = SS_MAXSPEED;
    } else {
        speed = (int16_t)(speed + SS_DECELERATION); // turning round
    }
    obj->inertia = speed;
}

static void SS_Move(Object *obj) {
    if (jpad1_hold2 & JPAD_LEFT)
        SS_MoveLeft(obj);
    if (jpad1_hold2 & JPAD_RIGHT)
        SS_MoveRight(obj);

    // Slow down when nothing is held.
    if (!(jpad1_hold2 & (JPAD_LEFT | JPAD_RIGHT)) && obj->inertia != 0) {
        if (obj->inertia > 0) {
            int32_t v = obj->inertia - SS_ACCELERATION;
            obj->inertia = (int16_t)(v < 0 ? 0 : v);
        } else {
            int32_t v = obj->inertia + SS_ACCELERATION;
            obj->inertia = (int16_t)(v > 0 ? 0 : v);
        }
    }

    // Move along the stage's current "floor" (the rotation snapped to the nearest 90 degrees).
    int16_t sin, cos;
    CalcSine((uint8_t)(-(((ss_angle.f.u & 0xFF) + 0x20) & 0xC0)), &sin, &cos);
    int32_t dx = cos * obj->inertia;
    int32_t dy = sin * obj->inertia;
    obj->pos.l.x.v += dx;
    obj->pos.l.y.v += dy;
    if (SS_FindWall(obj, obj->pos.l.y.v, obj->pos.l.x.v)) {
        obj->pos.l.x.v -= dx;
        obj->pos.l.y.v -= dy;
        obj->inertia = 0;
    }
}

static void SS_Jump(Object *obj) {
    if (!(jpad1_press2 & (JPAD_A | JPAD_B | JPAD_C)))
        return;
    int16_t sin, cos;
    CalcSine((uint8_t)(-(ss_angle.f.u & 0xFC) - 0x40), &sin, &cos);
    obj->xsp = (int16_t)((cos * SS_JUMPSPEED) >> 8);
    obj->ysp = (int16_t)((sin * SS_JUMPSPEED) >> 8);
    obj->status.p.f.in_air = true;
    QueueSound2(sfx_Jump);
}

// Gravity points at the bottom of the screen, i.e. along the stage's current angle.
static void SS_Fall(Object *obj) {
    int32_t y = obj->pos.l.y.v;
    int32_t x = obj->pos.l.x.v;
    int16_t sin, cos;
    CalcSine((uint8_t)(ss_angle.f.u & 0xFC), &sin, &cos);

    int32_t dx = sin * SS_GRAVITY + ((int32_t)obj->xsp << 8);
    int32_t dy = cos * SS_GRAVITY + ((int32_t)obj->ysp << 8);

    x += dx;
    if (SS_FindWall(obj, y, x)) { // hit a wall sideways
        x -= dx;
        dx = 0;
        obj->xsp = 0;
        obj->status.p.f.in_air = false;
        y += dy;
        if (SS_FindWall(obj, y, x)) { // ...and the floor
            dy = 0;
            obj->ysp = 0;
            return;
        }
        obj->xsp = (int16_t)(dx >> 8);
        obj->ysp = (int16_t)(dy >> 8);
        return;
    }

    y += dy;
    if (SS_FindWall(obj, y, x)) { // landed on the floor
        dy = 0;
        obj->ysp = 0;
        obj->status.p.f.in_air = false;
        obj->xsp = (int16_t)(dx >> 8);
        return;
    }
    obj->xsp = (int16_t)(dx >> 8);
    obj->ysp = (int16_t)(dy >> 8);
    obj->status.p.f.in_air = true;
}

static void SS_Display(Object *obj) {
    SS_CheckNonSolidItems(obj);
    SS_CheckSolidItems(obj);

    SpeedToPos(obj);
    SS_FixCamera(obj);

    ss_angle.v = (uint16_t)(ss_angle.v + ss_rotate);
    Sonic_Animate(obj);
}

// ---------------------------------------------------------------------------
// Object routines
// ---------------------------------------------------------------------------

static void SS_Main(Object *obj) {
    obj->routine += 2;
    obj->y_rad = 14;
    obj->x_rad = 7;
    obj->mappings = Mappings_Sonic;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 0;
    obj->anim = SonAnimId_Roll;
    obj->status.p.f.in_ball = true;
    obj->status.p.f.in_air = true;
}

static void SS_Control(Object *obj) {
    Scratch_SpecialSonic *s = SC(obj);
    s->touched_id = 0;

    // B enters debug mode when the cheat is active
    if (debug_mode && (jpad1_press1 & JPAD_B)) {
        debug_use = 1;
        return;
    }

    if (!obj->status.p.f.in_air) { // while touching a block
        SS_Jump(obj);
        SS_Move(obj);
        SS_Fall(obj);
    } else { // while airborne
        SS_Move(obj);
        SS_Fall(obj);
    }
    SS_Display(obj);

    Sonic_LoadGfx(obj);
    DisplaySprite(obj);
}

// Exiting: the stage spins faster and faster until the level takes over.
static void SS_ExitStage(Object *obj) {
    ss_rotate = (uint16_t)(ss_rotate + SS_ROTATESPEED);
    if (ss_rotate == 0x60 * SS_ROTATESPEED)
        gamemode = GameMode_Level; // the stage loop notices and fades out
    if ((int16_t)ss_rotate >= 2 * (0x60 * SS_ROTATESPEED)) { // never reached in practice
        ss_rotate = 0;
        ss_angle.v = 0x4000;
        obj->routine += 2;
        SC(obj)->exit_timer = 60;
    }

    ss_angle.v = (uint16_t)(ss_angle.v + ss_rotate);
    Sonic_Animate(obj);
    Sonic_LoadGfx(obj);
    SS_FixCamera(obj);
    DisplaySprite(obj);
}

static void SS_ExitStageUnused(Object *obj) {
    if (--SC(obj)->exit_timer == 0)
        gamemode = GameMode_Level;
    Sonic_Animate(obj);
    Sonic_LoadGfx(obj);
    SS_FixCamera(obj);
    DisplaySprite(obj);
}

void Obj_SpecialSonic(Object *obj) {
    if (debug_use) {
        Sonic_DebugMode(obj);
        SS_FixCamera(obj);
        return;
    }
    switch (obj->routine) {
    case 0:
        SS_Main(obj);
        // fall through into Control, like the real routine 0
        __attribute__((fallthrough));
    case 2: SS_Control(obj); break;
    case 4: SS_ExitStage(obj); break;
    case 6: SS_ExitStageUnused(obj); break;
    }
}
