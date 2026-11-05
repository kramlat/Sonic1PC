#include "SpecialSonic.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
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

static Own_SpecialSonic *SC(Sst *o) { return (Own_SpecialSonic *)o->own.u8; }

// SS_FixCamera: keep the camera centred on Sonic.
static void SS_FixCamera(Sst *self) {
    uint16_t x = (uint16_t)self->x_pos.f.u;
    if (x >= SCREEN_WIDTH / 2)
        scrpos_x.f.u = (int16_t)(x - SCREEN_WIDTH / 2);
    uint16_t y = (uint16_t)self->y_pos.f.u;
    if (y >= SCREEN_HEIGHT / 2)
        scrpos_y.f.u = (int16_t)(y - SCREEN_HEIGHT / 2);
}

// ---------------------------------------------------------------------------
// Wall detection
// ---------------------------------------------------------------------------

// Is this block one Sonic can't pass through? A solid one is remembered in the touched-block fields.
static bool SS_CheckType(Sst *self, uint8_t block, uint32_t index) {
    if (block == SSB_Blank || block == SSB_1Up)
        return false;
    if (block >= SSB_Ring && block < SSB_Glass_Ani1)
        return false; // $3A-$4A are non-solid (rings, emeralds, ghost blocks...)
    SC(self)->touched_id = block;
    SC(self)->touched_index = index;
    return true;
}

// SonicSS_FindWall: does a position (including fraction) overlap a solid block? Checks the four blocks around it.
static bool SS_FindWall(Sst *self, int32_t y, int32_t x) {
    uint16_t row = (uint16_t)(((uint16_t)(y >> 16) + 20 + SS_BLOCKSIZE * 2) / SS_BLOCKSIZE);
    uint16_t col = (uint16_t)(((uint16_t)(x >> 16) + 20) / SS_BLOCKSIZE);
    uint32_t i = (uint32_t)row * SS_ROWLENGTH + col;

    bool solid = false;
    solid |= SS_CheckType(self, ss_layout[i], i + 1);
    solid |= SS_CheckType(self, ss_layout[i + 1], i + 2);
    i += SS_ROWLENGTH;
    solid |= SS_CheckType(self, ss_layout[i], i + 1);
    solid |= SS_CheckType(self, ss_layout[i + 1], i + 2);
    return solid;
}

// The index stored is that of the byte after the block (the real code reads with post-increment); the block is
// one before it.
static uint32_t SS_TouchedBlock(Sst *self) { return SC(self)->touched_index - 1; }

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

static void SS_MakeGhostSolid(Sst *self) {
    if (SC(self)->ghost_state == 2) { // a ghost block and then the invisible trigger have been passed
        for (int i = 0; i < SS_DIM * SS_DIM; i++)
            if (ss_layout[i] == SSB_Ghost)
                ss_layout[i] = SSB_RedWhite;
    }
    SC(self)->ghost_state = 0;
}

// SonicSS_ChkItems_NonSolidActionBlock: things Sonic passes through (rings, emeralds, 1-ups, ghost blocks).
static void SS_CheckNonSolidItems(Sst *self) {
    Own_SpecialSonic *s = SC(self);
    uint16_t row = (uint16_t)(((uint16_t)self->y_pos.f.u + 80) / SS_BLOCKSIZE);
    uint16_t col = (uint16_t)(((uint16_t)self->x_pos.f.u + 32) / SS_BLOCKSIZE);
    uint32_t cell = (uint32_t)row * SS_ROWLENGTH + col;
    uint8_t block = ss_layout[cell];

    if (block == SSB_Blank) {
        if (s->ghost_state != 0)
            SS_MakeGhostSolid(self);
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
static void SS_CheckSolidItems(Sst *self) {
    Own_SpecialSonic *s = SC(self);
    uint8_t block = s->touched_id;

    if (block == 0) { // nothing solid was touched: let the UP/DOWN and R timeouts run down
        if (--s->timeout_updown & 0x80)
            s->timeout_updown = 0;
        if (--s->timeout_r & 0x80)
            s->timeout_r = 0;
        return;
    }

    uint32_t index = SS_TouchedBlock(self);

    if (block == SSB_Bumper) {
        // Throw Sonic away from the bumper's centre.
        int16_t bx = (int16_t)((index & (SS_ROWLENGTH - 1)) * SS_BLOCKSIZE - 20);
        int16_t by = (int16_t)(((index >> 7) & (SS_ROWLENGTH - 1)) * SS_BLOCKSIZE - (20 + SS_BLOCKSIZE * 2));
        int16_t dx = (int16_t)(bx - self->x_pos.f.u);
        int16_t dy = (int16_t)(by - self->y_pos.f.u);
        uint8_t angle = (uint8_t)CalcAngle(dx, dy);
        int16_t sin, cos;
        CalcSine(angle, &sin, &cos);
        self->x_vel = (int16_t)((cos * -0x700) >> 8);
        self->y_vel = (int16_t)((sin * -0x700) >> 8);
        self->status |= SST_PSTATUS_IN_AIR;
        SS_Animation *a = SS_FindFreeAnimationSlot();
        if (a != NULL) {
            a->id = SSAni_Bumper;
            a->block = index;
        }
        QueueSound2(sfx_Bumper);
    } else if (block == SSB_GOAL) {
        self->routine += 2; // ExitStage
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

static void SS_MoveLeft(Sst *self) {
    self->status |= SST_PSTATUS_X_FLIP;
    int16_t speed = self->ground_vel;
    if (speed <= 0) {
        speed = (int16_t)(speed - SS_ACCELERATION);
        if (speed <= -SS_MAXSPEED)
            speed = -SS_MAXSPEED;
    } else {
        speed = (int16_t)(speed - SS_DECELERATION); // turning round
    }
    self->ground_vel = speed;
}

static void SS_MoveRight(Sst *self) {
    self->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
    int16_t speed = self->ground_vel;
    if (speed >= 0) {
        speed = (int16_t)(speed + SS_ACCELERATION);
        if (speed >= SS_MAXSPEED)
            speed = SS_MAXSPEED;
    } else {
        speed = (int16_t)(speed + SS_DECELERATION); // turning round
    }
    self->ground_vel = speed;
}

static void SS_Move(Sst *self) {
    if (jpad1_hold2 & JPAD_LEFT)
        SS_MoveLeft(self);
    if (jpad1_hold2 & JPAD_RIGHT)
        SS_MoveRight(self);

    // Slow down when nothing is held.
    if (!(jpad1_hold2 & (JPAD_LEFT | JPAD_RIGHT)) && self->ground_vel != 0) {
        if (self->ground_vel > 0) {
            int32_t v = self->ground_vel - SS_ACCELERATION;
            self->ground_vel = (int16_t)(v < 0 ? 0 : v);
        } else {
            int32_t v = self->ground_vel + SS_ACCELERATION;
            self->ground_vel = (int16_t)(v > 0 ? 0 : v);
        }
    }

    // Move along the stage's current "floor" (the rotation snapped to the nearest 90 degrees).
    int16_t sin, cos;
    CalcSine((uint8_t)(-(((ss_angle.f.u & 0xFF) + 0x20) & 0xC0)), &sin, &cos);
    int32_t dx = cos * self->ground_vel;
    int32_t dy = sin * self->ground_vel;
    self->x_pos.v += dx;
    self->y_pos.v += dy;
    if (SS_FindWall(self, self->y_pos.v, self->x_pos.v)) {
        self->x_pos.v -= dx;
        self->y_pos.v -= dy;
        self->ground_vel = 0;
    }
}

static void SS_Jump(Sst *self) {
    if (!(jpad1_press2 & (JPAD_A | JPAD_B | JPAD_C)))
        return;
    int16_t sin, cos;
    CalcSine((uint8_t)(-(ss_angle.f.u & 0xFC) - 0x40), &sin, &cos);
    self->x_vel = (int16_t)((cos * SS_JUMPSPEED) >> 8);
    self->y_vel = (int16_t)((sin * SS_JUMPSPEED) >> 8);
    self->status |= SST_PSTATUS_IN_AIR;
    QueueSound2(sfx_Jump);
}

// Gravity points at the bottom of the screen, i.e. along the stage's current angle.
static void SS_Fall(Sst *self) {
    int32_t y = self->y_pos.v;
    int32_t x = self->x_pos.v;
    int16_t sin, cos;
    CalcSine((uint8_t)(ss_angle.f.u & 0xFC), &sin, &cos);

    int32_t dx = sin * SS_GRAVITY + ((int32_t)self->x_vel << 8);
    int32_t dy = cos * SS_GRAVITY + ((int32_t)self->y_vel << 8);

    x += dx;
    if (SS_FindWall(self, y, x)) { // hit a wall sideways
        x -= dx;
        dx = 0;
        self->x_vel = 0;
        self->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
        y += dy;
        if (SS_FindWall(self, y, x)) { // ...and the floor
            dy = 0;
            self->y_vel = 0;
            return;
        }
        self->x_vel = (int16_t)(dx >> 8);
        self->y_vel = (int16_t)(dy >> 8);
        return;
    }

    y += dy;
    if (SS_FindWall(self, y, x)) { // landed on the floor
        dy = 0;
        self->y_vel = 0;
        self->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
        self->x_vel = (int16_t)(dx >> 8);
        return;
    }
    self->x_vel = (int16_t)(dx >> 8);
    self->y_vel = (int16_t)(dy >> 8);
    self->status |= SST_PSTATUS_IN_AIR;
}

static void SS_Display(Sst *self) {
    SS_CheckNonSolidItems(self);
    SS_CheckSolidItems(self);

    Sst_MoveSprite2(self);
    SS_FixCamera(self);

    ss_angle.v = (uint16_t)(ss_angle.v + ss_rotate);
    Sonic_Animate(self);
}

// ---------------------------------------------------------------------------
// Object routines
// ---------------------------------------------------------------------------

static void SS_Main(Sst *self) {
    self->routine += 2;
    self->y_radius = 14;
    self->x_radius = 7;
    self->mappings = Mappings_Sonic;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 0 * 0x80;
    self->anim = SonAnimId_Roll;
    self->status |= SST_PSTATUS_IN_BALL;
    self->status |= SST_PSTATUS_IN_AIR;
}

static void SS_Control(Sst *self) {
    Own_SpecialSonic *s = SC(self);
    s->touched_id = 0;

    // B enters debug mode when the cheat is active
    if (debug_mode && (jpad1_press1 & JPAD_B)) {
        debug_use = 1;
        return;
    }

    if (!((self->status & SST_PSTATUS_IN_AIR) != 0)) { // while touching a block
        SS_Jump(self);
        SS_Move(self);
        SS_Fall(self);
    } else { // while airborne
        SS_Move(self);
        SS_Fall(self);
    }
    SS_Display(self);

    Sonic_LoadGfx(self);
    Sst_Draw(self);
}

// Exiting: the stage spins faster and faster until the level takes over.
static void SS_ExitStage(Sst *self) {
    ss_rotate = (uint16_t)(ss_rotate + SS_ROTATESPEED);
    if (ss_rotate == 0x60 * SS_ROTATESPEED)
        gamemode = GameMode_Level; // the stage loop notices and fades out
    if ((int16_t)ss_rotate >= 2 * (0x60 * SS_ROTATESPEED)) { // never reached in practice
        ss_rotate = 0;
        ss_angle.v = 0x4000;
        self->routine += 2;
        SC(self)->exit_timer = 60;
    }

    ss_angle.v = (uint16_t)(ss_angle.v + ss_rotate);
    Sonic_Animate(self);
    Sonic_LoadGfx(self);
    SS_FixCamera(self);
    Sst_Draw(self);
}

static void SS_ExitStageUnused(Sst *self) {
    if (--SC(self)->exit_timer == 0)
        gamemode = GameMode_Level;
    Sonic_Animate(self);
    Sonic_LoadGfx(self);
    SS_FixCamera(self);
    Sst_Draw(self);
}

void Obj_SpecialSonic(Sst *self) {
    if (debug_use) {
        Sonic_DebugMode(self);
        SS_FixCamera(self);
        return;
    }
    switch (self->routine) {
    case 0:
        SS_Main(self);
        // fall through into Control, like the real routine 0
        __attribute__((fallthrough));
    case 2: SS_Control(self); break;
    case 4: SS_ExitStage(self); break;
    case 6: SS_ExitStageUnused(self); break;
    }
}
