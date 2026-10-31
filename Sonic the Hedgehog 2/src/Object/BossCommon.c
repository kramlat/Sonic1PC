// What the alpha's bosses of Metropolis and Hill Top share: see BossCommon.h
#include "Object/BossCommon.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "MathUtil.h"
#include "Sound.h"

#include "Macros.h"

// The boss objects' shared buffers (Boss_Move_Buffer at $FFFFF750 and Boss_Animate_Buffer at $FFFFF740)
BossMoveBuffer boss_move;
uint8_t boss_anim[16];
uint8_t boss_flag;

// Boss_Move
void Boss_Move(void) {
    boss_move.x.v += (int32_t)boss_move.xsp << 8;
    boss_move.y.v += (int32_t)boss_move.ysp << 8;
}

// One part's step of Boss_AnimateSprite (Offset_0x026338): `pair` is the part's two bytes of the buffer, `frame` where its frame goes. Returns false where the original leaves without storing (the routine moves on)
static bool BossAnimatePart(Own_AlphaBoss *own, const uint8_t *table, uint8_t *pair, uint8_t *frame) {
    uint8_t d0 = pair[0];
    uint8_t d1 = d0 >> 4;
    d0 &= 0xF;
    uint8_t d2 = d0;
    const bool changed = d0 != d1;
    uint8_t d5 = (uint8_t)((d0 << 4) | d0);
    d0 = pair[1];
    d1 = d0 >> 4;
    if (changed) {
        d0 = 0;
        d1 = 0;
    }
    d0 &= 0xF;
    d0 = (uint8_t)(d0 - 1);
    if ((int8_t)d0 < 0) {
        const uint8_t *script = table + (uint16_t)((table[d2 * 2] << 8) | table[d2 * 2 + 1]);
        d0 = script[0];
        d2 = script[1 + d1];
        while ((int8_t)d2 < 0) {
            if (d2 == 0xFF) { // loops
                d1 = 0;
                d2 = script[1];
            } else if (d2 == 0xFE) { // moves the routine on
                own->anim_routine += 2;
                return false;
            } else if (d2 == 0xFD) { // changes the animation
                d5 = (uint8_t)((d5 & 0xF0) | script[2 + d1]);
                goto store;
            } else if (d2 == 0xFC) { // jumps to a frame
                d1 = script[2 + d1];
                d2 = script[1 + d1];
            } else {
                return false;
            }
        }
        *frame = d2 & 0x7F;
        d1++;
    }
store:
    pair[1] = (uint8_t)((d0 & 0xF) | (d1 << 4));
    pair[0] = d5;
    return true;
}

void Boss_AnimateSprite(Sst *self, Own_AlphaBoss *own, const uint8_t *table) {
    uint8_t *pair = boss_anim;
    if (self->mapping_frame != 0) {
        if (!BossAnimatePart(own, table, pair, &self->mapping_frame))
            return;
        pair += 2;
    } else {
        pair += 2;
    }
    for (int i = 0; i < self->child_count; i++, pair += 2) {
        if (!BossAnimatePart(own, table, pair, &self->children[i].frame))
            return;
    }
}

// Offset_0x024580 and Boss_Hit: the ship bobs on its sine, and a hit flashes it (and lets the balls go)
void Boss_Hit(Sst *self, Own_AlphaBoss *own) {
    if (own->anim_routine >= 8)
        return;
    if (self->collision_property == 0) { // (the touch code has counted them out)
        AddPoints(100);
        boss_move.timer = 0xB3;
        own->anim_routine = 8;
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (own->flash == 0) {
        own->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash == 0)
        self->collision_flags = 0xF;
}


// The wreck blows up (Boss_Defeated): every eight frames an explosion, a random distance (up to $20 each way) from the boss
void Boss_Defeated(Sst *self) {
    if (frame_count & 7)
        return;
    Sst *exp = Sst_Allocate();
    if (exp == NULL)
        return;
    exp->code = sst_object_codes[ObjId_BossExplosionPiece];
    exp->x_pos.f.u = self->x_pos.f.u;
    exp->y_pos.f.u = self->y_pos.f.u;
    const uint32_t rand = RandomNumber();
    exp->x_pos.f.u += (int16_t)(((rand & 0xFF) >> 2) - 0x20);
    exp->y_pos.f.u += (int16_t)((((rand >> 8) & 0xFF) >> 2) - 0x20);
}
