#ifndef _BOSSCOMMON_H
#define _BOSSCOMMON_H

#include "Object.h"
#include "MathUtil.h"

// What the alpha's bosses of Metropolis and Hill Top share (its Boss_Move, Boss_AnimateSprite and Boss_Hit, and the explosions of a wreck, Boss_Defeated): a buffer for the boss's place as 16.16 numbers and its speed,
// a buffer for the animation of the main sprite and its child, and the boss's own state in its scratch memory.

typedef struct {
    uint8_t boss_routine; // the boss's own routine (the alpha's Obj_Boss_Routine): 0 starts it, 2 runs it
    uint8_t anim_routine; // 0x26: what it is doing (Boss_AnimateSprite also moves it on, at the end of an animation)
    uint8_t hits;         // 0x32: the hit counter that Boss_Hit looks at (Sonic's touch counts it down: a boss is a multi-sprite object)
    uint8_t flash;        // 0x14: how long its hit flash lasts
    uint8_t angle;        // 0x1A: the angle of its bob
    uint8_t flag;         // 0x38
    uint8_t side;         // 0x2C: (Hill Top's) which side of the arena it is on
    uint8_t count;        // 0x3E
} Scratch_AlphaBoss;

typedef struct {
    dword_s x, y;
    int16_t xsp, ysp;
    int16_t timer;
} BossMoveBuffer;

extern BossMoveBuffer boss_move;
extern uint8_t boss_anim[8];
extern uint8_t boss_flag; // the alpha's Boss_Flag: which boss is there (3: Hill Top's)

void Boss_Move(void);
void Boss_AnimateSprite(Object *obj, Scratch_AlphaBoss *boss, const uint8_t *table);
void Boss_Hit(Object *obj, Scratch_AlphaBoss *boss);
void Boss_Defeated(Object *obj);

#define ObjId_BossExplosionPiece 0x58

#endif //_BOSSCOMMON_H
