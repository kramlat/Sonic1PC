// Casino Night's boss for Sonic 2, as the alpha has it (object 51): see CNZBoss.h. Like Dust Hill's it uses the shared boss helpers (BossCommon): the ship's place and speed in a buffer, and the animation of its
// parts, each a pair of nibbles in a small buffer. Its parts are the object's child sprites: the right catcher, the ship, Robotnik and the left catcher (its own sprite is the flame under the ship).
#include "Object/CNZBoss.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "Level.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZBoss.h"
#include "Resource/Mappings/CNZBoss.h"

#define ArtTile_CNZBoss 0x3A7 // ($74E0: the alpha's list $2D)

enum { Boss_Start = 0, Boss_Run = 2, Boss_Ball = 4 };
// What the boss is doing (Obj_Ani_Boss_Routine; the animations of its parts move it on too)
enum { Run_Move = 0, Run_Choose = 2, Run_Drop = 4, Run_Catch = 6, Run_Wreck = 8, Run_Fall = 0xA, Run_Leave = 0xC };
// Where it goes (Obj_Control_Var_0C): a round of the arena's corners
enum { Path_Right = 0, Path_Up = 2, Path_Left = 4, Path_Down = 6 };
enum { Part_RightCatcher = 0, Part_Ship = 1, Part_Robotnik = 2, Part_LeftCatcher = 3 };
enum { Ball_Make = 0, Ball_Follow = 2, Ball_Fall = 4, Ball_Wait = 6, Ball_Bounce = 8 };

typedef struct {
    Scratch_AlphaBoss boss; // flag: the path (0x38), side: it has been beaten (0x2C), count: which side it is catching on, and whether the catchers have opened (0x3E)
    int16_t left_speed;     // 0x2E: how fast the left catcher falls when the wreck lets go of it
    int16_t right_speed;    // 0x30
    int32_t right_y;        // 0x34 (Control_Var_08)
    int32_t left_y;         // 0x3A (Control_Var_0E)
} Scratch_CNZBoss;

// The ball's own: it shares the object's scratch memory with the ship's (the alpha's own fields)
typedef struct {
    Scratch_AlphaBoss boss; // (the first byte: which routine)
    uint16_t drop;          // 0x28: how far it has fallen from the catcher, a pixel a frame up to $2E
    int16_t timer;          // 0x30: how long it bounces about
    int16_t x_speed;        // 0x10
    int16_t y_speed;        // 0x12
    uint8_t parent;         // 0x34: the slot of the boss it fell from
} Scratch_CNZBall;

// The lights of the boss's palette (Offset_0x0272C0): every fourth frame the colours 2 to 4 of the second line turn round, and colours 14 and 15 go to and fro along a table (the nibble-pairs' own state
// is the byte that says which, with its top bit for the way)
static uint8_t light_a, light_b;

static void Boss_Lights(void) {
    if (frame_count & 3)
        return;
    uint16_t *line = dry_palette[1];
    const uint16_t saved = line[2];
    line[2] = line[3];
    line[3] = line[4];
    line[4] = saved;

    static const uint16_t colours_a[6] = { 0x0A0E, 0x0C0C, 0x0EA0, 0x00AE, 0x00EA, 0x0AE0 };
    static const uint16_t colours_b[5] = { 0x000E, 0x000C, 0x000A, 0x0008, 0x0006 };

    line[14] = colours_a[(light_a & 0x7F) >> 1];
    if (!(light_a & 0x80)) {
        light_a += 2;
        if ((int8_t)light_a >= 0xC)
            light_a = 0x88;
    } else {
        int8_t next = (int8_t)((light_a & 0x7F) - 2);
        light_a = next >= 0 ? (uint8_t)(next | 0x80) : 2;
    }

    line[15] = colours_b[(light_b & 0x7F) >> 1];
    if (!(light_b & 0x80)) {
        light_b += 2;
        if ((int8_t)light_b >= 0xA)
            light_b = 0x86;
    } else {
        int8_t next = (int8_t)((light_b & 0x7F) - 2);
        light_b = next >= 0 ? (uint8_t)(next | 0x80) : 2;
    }
}

void CNZBoss_Events(void) {
    switch (dle_routine) {
    case 0: // the camera is at the arena
        if (scrpos_x.f.u < (0x2400 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = limit_btm2 = 0x5C0 - SCREEN_TALLADD;
        dle_routine += 2;
        {
            Object *boss = FindFreeObj();
            if (boss != NULL)
                boss->type = ObjId_CNZBoss;
        }
        boss_flag = 6;
        light_a = light_b = 0;
        AddPLC(PlcId_CNZBoss);
        break;
    case 2: // and the right edge of it
        if (scrpos_x.f.u >= (0x2700 - SCREEN_WIDEADD2)) {
            limit_right1 = limit_right2 = 0x2748 + SCREEN_WIDEADD2;
            limit_left2 = 0x2700 - SCREEN_WIDEADD2;
            dle_routine += 2;
        }
        break;
    case 4: // when it is beaten, the results card
        if (boss_status)
            GotThroughAct();
        break;
    }
}

static void SetPartAnim(int part, uint8_t anim) {
    boss_anim[(part + 1) * 2] = (uint8_t)((boss_anim[(part + 1) * 2] & 0xF0) | anim);
}

// Offset_0x024580: the bob (the sine of its angle, a byte that goes on by 2 a frame, about the ship's place) and then the hit
static void Boss_Bob(Object *obj, Scratch_CNZBoss *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->boss.angle, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    obj->pos.l.x.f.u = boss_move.x.f.u;
    scratch->boss.angle = (uint8_t)(scratch->boss.angle + 2);
    Boss_Hit(obj, &scratch->boss);
}

// Offset_0x02768E: where the parts are, from the ship's place; once it has been beaten the catchers fall (the left first)
static void Boss_Parts(Object *obj, Scratch_CNZBoss *scratch) {
    const int16_t x = obj->pos.l.x.f.u, y = obj->pos.l.y.f.u;
    obj->children[Part_Ship].x = x;
    obj->children[Part_Ship].y = y;
    obj->children[Part_Robotnik].x = x;
    obj->children[Part_Robotnik].y = y;
    if (!scratch->boss.side) {
        obj->children[Part_LeftCatcher].x = x;
        obj->children[Part_LeftCatcher].y = y;
        obj->children[Part_RightCatcher].x = x;
        obj->children[Part_RightCatcher].y = y;
        scratch->left_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (scratch->left_y & 0xFFFF);
        scratch->right_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (scratch->right_y & 0xFFFF);
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    obj->children[Part_LeftCatcher].x--;
    scratch->left_y += (int32_t)scratch->left_speed << 8;
    scratch->left_speed += 0x38;
    obj->children[Part_LeftCatcher].y = (int16_t)(scratch->left_y >> 16);
    if (obj->children[Part_LeftCatcher].y >= 0x6F0)
        scratch->left_speed = 0;
    if (boss_move.timer > 0x3C)
        return;
    obj->children[Part_RightCatcher].x++;
    scratch->right_y += (int32_t)scratch->right_speed << 8;
    scratch->right_speed += 0x38;
    obj->children[Part_RightCatcher].y = (int16_t)(scratch->right_y >> 16);
    if (obj->children[Part_RightCatcher].y >= 0x6F0)
        scratch->right_speed = 0;
}

// Offset_0x0275B6: the path round the arena (it changes direction where the ship is at each corner)
static void Boss_Path(Object *obj, Scratch_CNZBoss *scratch) {
    switch (scratch->boss.flag) {
    case Path_Right:
        if (boss_move.x.f.u < 0x2850)
            return;
        boss_move.x.f.u = 0x2850;
        boss_move.xsp = 0;
        boss_move.ysp = (int16_t)0xFE80;
        scratch->boss.flag = Path_Up;
        break;
    case Path_Up:
        if (boss_move.y.f.u > 0x5D0)
            return;
        obj->render.f.x_flip = false;
        if (boss_move.y.f.u > 0x5A0)
            return;
        boss_move.y.f.u = 0x5A0;
        boss_move.ysp = 0;
        boss_move.xsp = (int16_t)0xFE80;
        scratch->boss.flag = Path_Left;
        scratch->boss.count &= 0x7F;
        break;
    case Path_Left:
        if (boss_move.x.f.u > 0x27C0)
            return;
        if (!(scratch->boss.count & 0x80)) {
            scratch->boss.count |= 0x80;
            scratch->boss.anim_routine += 2;
        }
        if (boss_move.x.f.u > 0x2730)
            return;
        boss_move.x.f.u = 0x2730;
        boss_move.xsp = 0;
        boss_move.ysp = 0x180;
        scratch->boss.flag = Path_Down;
        break;
    case Path_Down:
        if (boss_move.y.f.u < 0x5D0)
            return;
        obj->render.f.x_flip = true;
        if (boss_move.y.f.u < 0x600)
            return;
        boss_move.y.f.u = 0x600;
        boss_move.ysp = 0;
        boss_move.xsp = 0x180;
        scratch->boss.flag = Path_Right;
        break;
    }
}

// Offset_0x0273A0: what a frame of the boss's moving is (the several waits and catches below go on with it)
static void Boss_Moving(Object *obj, Scratch_CNZBoss *scratch) {
    Boss_Move();
    Boss_Bob(obj, scratch);
    Boss_Path(obj, scratch);
    Boss_Parts(obj, scratch);
    Boss_AnimateSprite(obj, &scratch->boss, Animation_CNZBoss);
    DisplaySprite(obj);
}

static void Boss_OpenCatchers(void) { // Offset_0x02740E
    boss_anim[3] = 0x20;
    boss_anim[9] = 0x20;
    boss_anim[1] = 0x40;
    SetPartAnim(Part_Robotnik, 5);
}

static void Boss_ShutCatchers(void) { // Offset_0x027434
    boss_anim[3] = 0;
    boss_anim[9] = 0;
    boss_anim[1] = 0;
}

static void Boss_DropBall(Object *obj) { // Offset_0x0274AC
    Object *ball = FindFreeObj();
    if (ball == NULL)
        return;
    ball->type = ObjId_CNZBoss;
    ((Scratch_CNZBall *)&ball->scratch)->boss.boss_routine = Boss_Ball;
    ((Scratch_CNZBall *)&ball->scratch)->parent = (uint8_t)(obj - objects);
}

static void Boss_Dig(Object *obj, Scratch_CNZBoss *scratch) {
    Scratch_AlphaBoss *boss = &scratch->boss;

    switch (boss->anim_routine) {
    case Run_Move:
        Boss_Moving(obj, scratch);
        break;

    case Run_Choose: // on one side it lets the ball fall, on the other it only opens its catchers
        if (!(boss->count & 1)) {
            boss->count++;
            obj->frame = 0x0C; // (the main sprite shows while the catchers wait)
            boss->anim_routine += 2;
            boss_move.timer = 0xC8;
        } else {
            boss->count--;
            obj->frame = 0;
            boss->anim_routine += 4;
            Boss_DropBall(obj);
            boss_move.timer = 0xDC;
        }
        Boss_Moving(obj, scratch);
        break;

    case Run_Drop:
        boss_move.timer--;
        if (boss_move.timer < 0) {
            if (boss_move.timer == (int16_t)0xFFB5)
                Boss_OpenCatchers();
            else if (boss_move.timer == (int16_t)0xFF6A) {
                boss->anim_routine = Run_Move;
                Boss_ShutCatchers();
            }
        } else if (boss_move.timer == 0) {
            Boss_ShutCatchers();
        } else if (boss_move.timer == 0x4B) {
            Boss_OpenCatchers();
        }
        Boss_Moving(obj, scratch);
        break;

    case Run_Catch:
        boss_move.timer--;
        if (boss_move.timer == 0) {
            boss_anim[3] = 0x20;
            boss_anim[9] = 0x20;
            SetPartAnim(Part_Robotnik, 5);
        } else if (boss_move.timer <= -0x14) {
            boss_anim[3] = 0;
            boss_anim[9] = 0;
            boss->anim_routine = Run_Move;
        }
        Boss_Moving(obj, scratch);
        break;

    case Run_Wreck: // blowing up where it is
        boss->side = 0xFF;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(obj);
        } else {
            obj->children[Part_Robotnik].frame = 0x0B;
            obj->frame = 0;
            obj->render.f.x_flip = true;
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = -0x12;
        }
        obj->pos.l.y.f.u = boss_move.y.f.u;
        obj->pos.l.x.f.u = boss_move.x.f.u;
        Boss_Parts(obj, scratch);
        DisplaySprite(obj);
        break;

    case Run_Fall: // the ship drops a little and hovers, and then goes
        boss_move.timer++;
        if (boss_move.timer == 0) {
            boss_move.ysp = 0;
        } else if (boss_move.timer > 0) {
            if (boss_move.timer < 0x18)
                boss_move.ysp -= 8;
            else if (boss_move.timer == 0x18)
                boss_move.ysp = 0;
            else if (boss_move.timer >= 0x20)
                boss->anim_routine += 2;
        } else {
            boss_move.ysp += 0x18;
        }
        Boss_Move();
        Boss_Bob(obj, scratch);
        obj->pos.l.y.f.u = boss_move.y.f.u;
        obj->pos.l.x.f.u = boss_move.x.f.u;
        Boss_Parts(obj, scratch);
        DisplaySprite(obj);
        break;

    case Run_Leave: // up and away to the right, the arena opening as it goes; when it is off the screen the act is over
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x2980 + SCREEN_WIDEADD2)
            limit_right1 = limit_right2 += 2;
        else if (!obj->render.f.on_screen) {
            if (!boss_status)
                boss_status = 1;
            ObjectDelete(obj);
            return;
        }
        Boss_Move();
        Boss_Bob(obj, scratch);
        obj->pos.l.y.f.u = boss_move.y.f.u;
        obj->pos.l.x.f.u = boss_move.x.f.u;
        Boss_Parts(obj, scratch);
        DisplaySprite(obj);
        break;
    }
}

// What the ball (routine 4) does: it falls from the catcher, to the floor, and bounces about the arena for a while
static void Ball_Move(Object *obj, Scratch_CNZBall *ball) { // Offset_0x0278B8
    obj->pos.l.x.v += (int32_t)ball->x_speed << 8;
    obj->pos.l.y.v += (int32_t)ball->y_speed << 8;
    ball->y_speed += 0x38;
}

static void Ball_Run(Object *obj, Scratch_CNZBall *ball) {
    switch (obj->routine_sec) {
    case Ball_Make: {
        Object *boss = &objects[ball->parent];
        obj->mappings = Mappings_CNZBoss;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 5;
        obj->routine_sec += 2;
        obj->pos.l.x.f.u = boss->pos.l.x.f.u;
        obj->pos.l.y.f.u = (int16_t)(boss->pos.l.y.f.u + 0x30);
        obj->width_pixels = 8;
        obj->frame = 0x12;
        break;
    }
    case Ball_Follow: { // it is held under the catcher, and goes down a little further each frame
        Object *boss = &objects[ball->parent];
        obj->pos.l.x.f.u = boss->pos.l.x.f.u;
        obj->pos.l.x.f.l = 0;
        obj->pos.l.y.f.u = (int16_t)(boss->pos.l.y.f.u + ball->drop);
        if (ball->drop + 1 >= 0x2E)
            ball->drop = 0x2E;
        else
            ball->drop++;
        if (boss_move.timer == 0) {
            obj->routine_sec += 2;
            ball->x_speed = boss_move.xsp;
            ball->timer = 0x258;
        }
        DisplaySprite(obj);
        break;
    }
    case Ball_Fall:
        Ball_Move(obj, ball);
        ball->x_speed -= 8;
        {
            const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (d < 0) {
                obj->pos.l.y.f.u += d;
                ball->y_speed = 0;
                obj->routine_sec += 2;
            }
        }
        DisplaySprite(obj);
        break;
    case Ball_Wait:
        if (--ball->timer < 0) {
            ObjectDelete(obj);
            return;
        }
        if ((frame_count & 0xF) == 0) { // every sixteen frames it picks a way to go
            static const int16_t choices[8] = { 0x0100, (int16_t)0xFCE0, (int16_t)0xFF40, 0x0240, (int16_t)0xFF80, 0x0320, (int16_t)0xFEE0, 0x0210 };
            const uint32_t rand = RandomNumber();
            ball->x_speed = choices[rand & 7];
            int16_t y = choices[(rand >> 16) & 7];
            if (y >= 0)
                y = (int16_t)-y;
            ball->y_speed = y;
            obj->routine_sec += 2;
        }
        DisplaySprite(obj);
        break;
    case Ball_Bounce:
        if (--ball->timer < 0) {
            ObjectDelete(obj);
            return;
        }
        Ball_Move(obj, ball);
        if (obj->pos.l.x.f.u <= 0x2700)
            ball->x_speed = (int16_t)-ball->x_speed;
        else if (obj->pos.l.x.f.u >= 0x2880)
            ball->x_speed = (int16_t)-ball->x_speed;
        {
            const int16_t d = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (d < 0) {
                obj->pos.l.y.f.u += d;
                ball->y_speed = 0;
                obj->routine_sec -= 2;
            }
        }
        DisplaySprite(obj);
        break;
    }
}

void Obj_CNZBoss(Object *obj) {
    Scratch_CNZBoss *scratch = (Scratch_CNZBoss *)&obj->scratch;

    Boss_Lights();
    switch (scratch->boss.boss_routine) {
    case Boss_Start:
        obj->mappings = Mappings_CNZBoss;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 3;
        obj->width_pixels = 0x60;
        obj->pos.l.x.f.u = 0x2850;
        obj->pos.l.y.f.u = 0x600;
        scratch->boss.boss_routine += 2;
        obj->render.f.multi_sprite = true;
        obj->child_count = 4;
        obj->col_type = 0x0F;
        scratch->boss.hits = 8;
        boss_move.x.f.u = obj->pos.l.x.f.u;
        boss_move.y.f.u = obj->pos.l.y.f.u;
        boss_move.xsp = 0;
        boss_move.timer = 0;
        for (int i = 0; i < 4; i++) {
            obj->children[i].x = obj->pos.l.x.f.u;
            obj->children[i].y = obj->pos.l.y.f.u;
        }
        obj->children[Part_RightCatcher].frame = 5;
        obj->children[Part_Ship].frame = 1;
        obj->children[Part_Robotnik].frame = 6;
        obj->children[Part_LeftCatcher].frame = 2;
        scratch->boss.flag = Path_Up;
        boss_move.ysp = (int16_t)0xE800;
        scratch->left_y = scratch->right_y = (int32_t)((uint32_t)(uint16_t)obj->pos.l.y.f.u << 16);
        {
            static const uint8_t buffer[10] = { 0x04, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x02, 0x00 };
            memcpy(boss_anim, buffer, sizeof(buffer));
        }
        break;

    case Boss_Run:
        Boss_Dig(obj, scratch);
        break;

    case Boss_Ball:
        Ball_Run(obj, (Scratch_CNZBall *)&obj->scratch);
        break;
    }
}
