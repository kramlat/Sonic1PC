#ifndef _BOSSFINAL_H
#define _BOSSFINAL_H

#include "Object.h"

// Object 85 - Eggman (Final Zone boss) and its 5 helper objects (panel, legs, cockpit, empty ship, flame);
// Object 84 - the 4 cylinders he hides in; Object 86 - the plasma launcher and its energy balls.
// Ported from s1disasm's "_incObj/85,84,86 Boss - FZ Main, Cylinders, and Plasma Balls.asm".
//
// The original passes pointers between these objects through a few shared offsets; here every pointer is an
// objects[] index (parent_index), and offsets that the original reuses for different jobs are named for the job.
typedef struct {
    uint8_t pad0;            // 0x28
    uint8_t child_cmd;       // 0x29 -- command sent to a cylinder / the launcher, one-shot (BossFinal_ChildCmd)
    uint8_t pad1[6];         // 0x2A-0x2F
    int16_t attack_state;    // 0x30 -- boss: -1 = pick the next attack; also the escape timer (BossFinal_AttackState)
    int16_t child_counter;   // 0x32 -- boss and launcher: helper objects still running (BossFinal_ChildCounter)
    uint8_t link;            // 0x34 -- helpers: the boss's index. Boss: its own phase (0 wait ... $E escape)
    uint8_t hit_flash;       // 0x35 -- boss: frames of hit flashing left
    uint8_t plasma_index;    // 0x36 -- boss: the plasma launcher
    uint8_t pad2;            // 0x37
    uint8_t cylinders[4];    // 0x38-0x3B -- boss: the 4 cylinders (top-left, top-right, bottom-left, bottom-right)
    uint8_t pad3[4];         // 0x3C-0x3F
} Scratch_BossFinal;

typedef struct {
    uint8_t number;          // 0x28 -- cylinder number x2 (obSubtype): 0/2 floor cylinders, 4/6 ceiling ones
    uint8_t child_cmd;       // 0x29 -- the boss's command: nonzero = extend now
    uint8_t pad0[6];         // 0x2A-0x2F
    int16_t has_eggman;      // 0x30 -- -1 = Eggman is in this one, 0 = decoy
    uint8_t pad1[2];         // 0x32-0x33
    uint8_t parent_index;    // 0x34
    uint8_t pad2[3];         // 0x35-0x37
    dword_s base_y;          // 0x38-0x3B -- resting Y as a fixed-point value
    dword_s displacement;    // 0x3C-0x3F -- how far it has extended (pixels.fraction); negative = up
} Scratch_EggmanCylinder;

typedef struct {
    uint8_t pad0;            // 0x28
    uint8_t child_cmd;       // 0x29 -- launcher: fire plasma (one-shot)
    uint8_t pad1[6];         // 0x2A-0x2F
    int16_t target_x;        // 0x30 -- launcher: spread offset counter; ball: X it spreads out to
    int16_t child_counter;   // 0x32 -- launcher: balls that haven't reached their spread X yet
    uint8_t parent_index;    // 0x34 -- launcher: the boss; ball: its launcher
    uint8_t pad2[3];         // 0x35-0x37
    int16_t balls_alive;     // 0x38 -- launcher: balls still in the air
    int16_t timer;           // 0x3A -- ball: frames left in the current phase
    uint8_t pad3[4];         // 0x3C-0x3F
} Scratch_BossPlasma;

void Obj_BossFinal(Object *obj);
void Obj_EggmanCylinder(Object *obj);
void Obj_BossPlasma(Object *obj);

#endif //_BOSSFINAL_H
