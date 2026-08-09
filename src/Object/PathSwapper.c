#include "PathSwapper.h"

#include "Level.h"
#include "LevelCollision.h"

// Object 03 - Path Swapper / Collision Switcher. See PathSwapper.h and
// s1disasm ProjectSonic1TwoEight's "_incObj/03 Collision Switcher.asm" (the
// real source this was ported from) for the full picture. Two orientations
// (vertical trigger line checked via X-crossing = MainX, horizontal trigger
// line checked via Y-crossing = MainY), each with a forward and a backward
// crossing, each independently choosing a target collision path and Sonic's
// sprite priority. Purely invisible/logic-only in normal play (no debug
// on-screen marker rendering here, unlike the real disassembly's optional
// DebugPathSwappers build).

#define PSWAP_BIT_SIZE_MASK      0x03
#define PSWAP_BIT_HORIZONTAL     0x04
#define PSWAP_BIT_PATH2_FORWARD  0x08 // rightdown
#define PSWAP_BIT_PATH2_BACKWARD 0x10 // leftup
#define PSWAP_BIT_PRIORITY_FORWARD  0x20 // rightdown
#define PSWAP_BIT_PRIORITY_BACKWARD 0x40 // leftup
#define PSWAP_BIT_GROUNDONLY     0x80

static const int16_t PSwapper_Sizes[4] = { 0x20, 0x40, 0x80, 0x100 };

// Applies the path/priority switch for one crossing direction. "priority
// only" (render.f.x_flip, repurposed the same way the real object reuses
// its obRender X-flip bit) skips the collision-path change but still
// updates Sonic's draw priority.
static void PSwapper_Trigger(Object *obj, Scratch_PathSwapper *scratch, bool forward) {
    if (scratch->subtype & PSWAP_BIT_GROUNDONLY) {
        if (player->status.p.f.in_air)
            return;
    }

    if (!obj->render.f.x_flip) {
        bool target_path2 = forward ? (scratch->subtype & PSWAP_BIT_PATH2_FORWARD) != 0
                                     : (scratch->subtype & PSWAP_BIT_PATH2_BACKWARD) != 0;
        collision_path = target_path2 ? 1 : 0;
    }

    bool priority = forward ? (scratch->subtype & PSWAP_BIT_PRIORITY_FORWARD) != 0
                             : (scratch->subtype & PSWAP_BIT_PRIORITY_BACKWARD) != 0;
    player->tile &= ~0x8000;
    if (priority)
        player->tile |= 0x8000;
}

static void PSwapper_MainX(Object *obj) {
    Scratch_PathSwapper *scratch = (Scratch_PathSwapper*)&obj->scratch;
    if (debug_use)
        return;

    int16_t obj_x = obj->pos.l.x.f.u;
    int16_t player_x = player->pos.l.x.f.u;
    bool forward = !scratch->passed;

    if (forward) {
        if (obj_x > player_x)
            return;
        scratch->passed = true;
    } else {
        if (obj_x <= player_x)
            return;
        scratch->passed = false;
    }

    int16_t top = obj->pos.l.y.f.u - scratch->size;
    int16_t bottom = obj->pos.l.y.f.u + scratch->size;
    int16_t player_y = player->pos.l.y.f.u;
    if (player_y < top || player_y >= bottom)
        return;

    PSwapper_Trigger(obj, scratch, forward);
}

static void PSwapper_MainY(Object *obj) {
    Scratch_PathSwapper *scratch = (Scratch_PathSwapper*)&obj->scratch;
    if (debug_use)
        return;

    int16_t obj_y = obj->pos.l.y.f.u;
    int16_t player_y = player->pos.l.y.f.u;
    bool forward = !scratch->passed;

    if (forward) {
        if (obj_y > player_y)
            return;
        scratch->passed = true;
    } else {
        if (obj_y <= player_y)
            return;
        scratch->passed = false;
    }

    int16_t left = obj->pos.l.x.f.u - scratch->size;
    int16_t right = obj->pos.l.x.f.u + scratch->size;
    int16_t player_x = player->pos.l.x.f.u;
    if (player_x < left || player_x >= right)
        return;

    PSwapper_Trigger(obj, scratch, forward);
}

void Obj_PathSwapper(Object *obj) {
    Scratch_PathSwapper *scratch = (Scratch_PathSwapper*)&obj->scratch;

    switch (obj->routine) {
    case 0: { // PSwapper_Init
        scratch->size = PSwapper_Sizes[scratch->subtype & PSWAP_BIT_SIZE_MASK];

        if (scratch->subtype & PSWAP_BIT_HORIZONTAL) {
            obj->routine = 4; // PSwapper_MainY
            // If Sonic spawned already below the trigger line, treat it as
            // already-crossed so the first real trigger is the reverse one.
            if (obj->pos.l.y.f.u < player->pos.l.y.f.u)
                scratch->passed = true;
        } else {
            obj->routine = 2; // PSwapper_MainX
            if (obj->pos.l.x.f.u < player->pos.l.x.f.u)
                scratch->passed = true;
        }
        break;
    }
    case 2:
        PSwapper_MainX(obj);
        break;
    case 4:
        PSwapper_MainY(obj);
        break;
    }
}
