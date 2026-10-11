// Neo Green Hill's boss for Sonic 2, as the alpha has it (object 89): see NGHZBoss.h. It uses the same boss helpers as the others (BossCommon). The object comes in three kinds (the alpha's Obj_Boss_Routine): 0 waits
// for the arena and then becomes the ship (2), which makes the launcher (4) with the walls' pillars as its children; the launcher makes the arrow-pillars (4 as well, told apart by their own state).
#include "Object/NGHZBoss.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "HTZQuake.h"
#include "Level.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Game.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/NGHZBoss.h"
#include "Resource/Animation/NGHZBossPillars.h"
#include "Resource/Mappings/NGHZBoss.h"
#include "Resource/Mappings/NGHZBossLauncher.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)


#define ArtTile_NGHZBoss 0x3E0 // ($7C00: the alpha's list $2B)

enum { Boss_Wait = 0, Boss_Ship = 2, Boss_Parts = 4 };
// the ship's states (Obj_Ani_Boss_Routine); the launcher's are 0 up to the walls, 2 wait for the ship to turn, 4 a delay, 8 down again; and 6 is an arrow-pillar
enum { Ship_Descend = 0, Ship_Cross = 2, Ship_Hammer = 4, Ship_Turn = 6, Ship_Wreck = 8, Ship_Fall = 0xA, Ship_Leave = 0xC };
enum { Launcher_Rise = 0, Launcher_Wait = 2, Launcher_Delay = 4, Pillar = 6, Launcher_Sink = 8 };
enum { Part_Robotnik = 0, Part_Hammer = 1, Part_Boost = 2 };
enum { Pillar_Make = 0, Pillar_Fly = 2, Pillar_Shake = 4, Pillar_Fall = 6, Pillar_Gone = 8 };

typedef struct {
    Scratch_AlphaBoss boss; // flag: which way the ship goes (0x38: $FF left), side: it has been beaten (0x2C), count: bit 0 the ship has turned (0x3E)
    int16_t timer;          // 0x30: (the launcher's) how long it waits
    int16_t hammer_speed;   // 0x2E: how fast the hammer falls when the wreck lets go of it
    int32_t hammer_y;       // 0x3A
    uint8_t ship;           // 0x2A: (the launcher's) the slot of the ship, (a pillar's: the next field is its state)
    uint8_t state;          // 0x2B... a pillar's state (Obj_Timer's byte)
    uint8_t parent;         // 0x34: (a pillar's) the slot of the launcher
    uint8_t side;           // 0x28: (a pillar's) the side it comes from
} Scratch_NGHZBoss;

// The arena's walls and arrows (the alpha's numbers)
static const int16_t arrow_targets[4] = { 0x458, 0x478, 0x498, 0x4B8 };

void NGHZBoss_Events(void) {
    switch (dle_routine) {
    case 0: // the camera is at the arena
        if (scrpos_x.f.u < (0x28A0 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = limit_btm2 = 0x400 - SCREEN_TALLADD;
        dle_routine += 2;
        {
            Object *boss = FindFreeObj();
            if (boss != NULL)
                boss->type = ObjId_NGHZBoss;
        }
        boss_flag = 4;
        AddPLC(PlcId_NGHZBoss);
        break;
    case 2: // and the walls
        if (scrpos_x.f.u >= (0x2A40 - SCREEN_WIDEADD2)) {
            limit_right1 = limit_right2 = 0x2A40 + SCREEN_WIDEADD2;
            limit_left2 = 0x2A40 - SCREEN_WIDEADD2;
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

static void FillAnimationBuffer(void) { // Offset_0x02612C
    static const uint8_t buffer[8] = { 0x04, 0x00, 0x10, 0x00, 0x02, 0x00, 0x01, 0x00 };
    memcpy(boss_anim, buffer, sizeof(buffer));
}

// Offset_0x0262AA: where the parts are, from the ship's place; once it has been beaten the hammer falls
static void Ship_Parts(Object *obj, Scratch_NGHZBoss *scratch) {
    const int16_t x = obj->pos.l.x.f.u, y = obj->pos.l.y.f.u;
    obj->children[Part_Robotnik].x = x;
    obj->children[Part_Robotnik].y = y;
    obj->children[Part_Boost].x = x;
    obj->children[Part_Boost].y = y;
    if (!scratch->boss.side) {
        obj->children[Part_Hammer].x = x;
        obj->children[Part_Hammer].y = y;
        scratch->hammer_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (scratch->hammer_y & 0xFFFF);
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    obj->children[Part_Hammer].x--;
    scratch->hammer_y += (int32_t)scratch->hammer_speed << 8;
    scratch->hammer_speed += 0x38;
    obj->children[Part_Hammer].y = (int16_t)(scratch->hammer_y >> 16);
    if (obj->children[Part_Hammer].y >= 0x540)
        scratch->hammer_speed = 0;
}

// Offset_0x024580 and the flash of Robotnik's face (Offset_0x02628C): the bob and the hit
static void Ship_Bob(Object *obj, Scratch_NGHZBoss *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->boss.angle, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    obj->pos.l.x.f.u = boss_move.x.f.u;
    scratch->boss.angle = (uint8_t)(scratch->boss.angle + 2);
    Boss_Hit(obj, &scratch->boss);
}

static void Ship_Hit(Object *obj, Scratch_NGHZBoss *scratch) {
    Ship_Bob(obj, scratch);
    if (scratch->boss.flash == 0x1F)
        SetPartAnim(Part_Robotnik, 6);
}

static void Ship_Show(Object *obj, Scratch_NGHZBoss *scratch) {
    Boss_AnimateSprite(obj, &scratch->boss, Animation_NGHZBoss);
    DisplaySprite(obj);
}

static void Ship_Run(Object *obj, Scratch_NGHZBoss *scratch) {
    Scratch_AlphaBoss *boss = &scratch->boss;

    switch (boss->anim_routine) {
    case Ship_Descend:
        Boss_Move();
        Ship_Hit(obj, scratch);
        Ship_Parts(obj, scratch);
        if (boss_move.y.f.u >= 0x430) {
            boss_move.y.f.u = 0x430;
            boss->anim_routine += 2;
            boss_move.ysp = 0;
            boss_move.xsp = (int16_t)0xFF9C;
            boss->flag = 0xFF;
        }
        Ship_Show(obj, scratch);
        break;

    case Ship_Cross: // along the arena, and at the other side Robotnik looks at the hammer
        Boss_Move();
        Ship_Hit(obj, scratch);
        Ship_Parts(obj, scratch);
        if (boss->flag == 0) {
            if (boss_move.x.f.u >= 0x2B08) {
                boss->anim_routine += 2;
                boss_move.xsp = 0;
                SetPartAnim(Part_Robotnik, 5);
            }
        } else if (boss_move.x.f.u <= 0x2AA8) {
            boss->anim_routine += 2;
            boss_move.xsp = 0;
            SetPartAnim(Part_Robotnik, 5);
        }
        Ship_Show(obj, scratch);
        break;

    case Ship_Hammer: // the hammer swings (its animation moves the state on) when the bob is at its lowest
        Boss_Move();
        Ship_Hit(obj, scratch);
        Ship_Parts(obj, scratch);
        if (boss->angle == 0xC0)
            SetPartAnim(Part_Hammer, 3);
        Ship_Show(obj, scratch);
        break;

    case Ship_Turn: // it has hit: it turns and the wall throws an arrow
        SetPartAnim(Part_Hammer, 2);
        boss->count |= 1;
        boss->anim_routine = Ship_Cross;
        obj->render.f.x_flip = !obj->render.f.x_flip;
        boss->flag ^= 0xFF;
        boss_move.xsp = boss->flag != 0 ? (int16_t)0xFF9C : 0x64;
        Boss_Move();
        Ship_Hit(obj, scratch);
        Ship_Parts(obj, scratch);
        Ship_Show(obj, scratch);
        break;

    case Ship_Wreck:
        boss->side = 0xFF;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(obj);
        } else {
            obj->child_count = 2;
            obj->children[Part_Robotnik].frame = 5;
            obj->render.f.x_flip = true;
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = -0x12;
        }
        obj->pos.l.y.f.u = boss_move.y.f.u;
        obj->pos.l.x.f.u = boss_move.x.f.u;
        Ship_Parts(obj, scratch);
        DisplaySprite(obj);
        break;

    case Ship_Fall:
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
        Ship_Bob(obj, scratch);
        obj->pos.l.y.f.u = boss_move.y.f.u;
        obj->pos.l.x.f.u = boss_move.x.f.u;
        Ship_Parts(obj, scratch);
        DisplaySprite(obj);
        break;

    case Ship_Leave: // up and away to the right; the alpha's check of the arena's edge is the one of Dust Hill's ($2180), which this arena never has, so the edge opens for ever and the act does not end
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x2180) {
            limit_right1 = limit_right2 += 2;
        } else if (!obj->render.f.on_screen) {
            if (!boss_status)
                boss_status = 1;
            ObjectDelete(obj);
            return;
        }
        Boss_Move();
        Ship_Bob(obj, scratch);
        obj->pos.l.y.f.u = boss_move.y.f.u;
        obj->pos.l.x.f.u = boss_move.x.f.u;
        Ship_Parts(obj, scratch);
        DisplaySprite(obj);
        break;
    }
}

// Offset_0x0265DC: the solid that goes with the launcher, at a place of its own ($2A68, $498)
static void Launcher_Solid(Object *obj) {
    const int16_t saved_x = obj->pos.l.x.f.u, saved_y = obj->pos.l.y.f.u;
    obj->pos.l.x.f.u = 0x2A68;
    obj->pos.l.y.f.u = 0x498;
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(obj, chr, who, 0x1B, 8, 0x11, 0x2A68, NULL);
    obj->pos.l.x.f.u = saved_x;
    obj->pos.l.y.f.u = saved_y;
}

static void Launcher_Throw(Object *obj, Scratch_NGHZBoss *scratch, bool right) { // Offset_0x02654C
    Scratch_AlphaBoss *boss = &scratch->boss;
    boss->anim_routine += 2;
    if (!right) {
        obj->frame = 2;
        obj->pos.l.x.f.u = 0x2A62;
    } else {
        obj->frame = 3;
        obj->pos.l.x.f.u = 0x2B4E;
    }
    Object *pillar = FindFreeObj();
    if (pillar != NULL) {
        pillar->type = ObjId_NGHZBoss;
        Scratch_NGHZBoss *ps = (Scratch_NGHZBoss *)&pillar->scratch;
        ps->boss.boss_routine = Boss_Parts;
        ps->boss.anim_routine = Pillar;
        ps->parent = (uint8_t)(obj - objects);
        ps->side = right ? 0xFF : 0;
    }
    scratch->timer = 0x28;
    obj->pos.l.y.f.u = arrow_targets[RandomNumber() & 3];
}

static void Launcher_Run(Object *obj, Scratch_NGHZBoss *scratch) {
    Scratch_AlphaBoss *boss = &scratch->boss;
    Object *ship = &objects[scratch->ship];
    Scratch_NGHZBoss *ship_scratch = (Scratch_NGHZBoss *)&ship->scratch;
    if (ship_scratch->boss.anim_routine >= Ship_Wreck && boss->anim_routine < Launcher_Sink)
        boss->anim_routine = Launcher_Sink;

    switch (boss->anim_routine) {
    case Launcher_Rise:
        Launcher_Solid(obj);
        obj->children[0].y -= 2;
        obj->children[1].y -= 2;
        if (obj->children[0].y <= 0x488) {
            boss->anim_routine += 2;
            dhz_shake = 0;
        }
        DisplaySprite(obj);
        break;

    case Launcher_Wait:
        Launcher_Solid(obj);
        if (ship_scratch->boss.count & 1) {
            ship_scratch->boss.count &= (uint8_t)~1;
            Launcher_Throw(obj, scratch, ship_scratch->boss.flag != 0);
        }
        DisplaySprite(obj);
        break;

    case Launcher_Delay:
        Launcher_Solid(obj);
        if (--scratch->timer < 0) {
            obj->frame = 0;
            boss->anim_routine = Launcher_Wait;
        }
        DisplaySprite(obj);
        break;

    case Launcher_Sink:
        dhz_shake = 1;
        obj->children[0].y += 2;
        obj->children[1].y += 2;
        if (obj->children[0].y >= 0x510) {
            dhz_shake = 0;
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}

// The arrow-pillar the launcher throws: it flies across to the other wall, shakes, and falls
static void Pillar_Run(Object *obj, Scratch_NGHZBoss *scratch) {
    Object *launcher = &objects[scratch->parent];
    Scratch_NGHZBoss *launcher_scratch = (Scratch_NGHZBoss *)&launcher->scratch;
    if (((Scratch_NGHZBoss *)&objects[launcher_scratch->ship].scratch)->boss.anim_routine >= Ship_Wreck) {
        ObjectDelete(obj);
        return;
    }

    switch (scratch->state) {
    case Pillar_Make:
        obj->mappings = Mappings_NGHZBossLauncher;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_NGHZBoss);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->y_rad = (int8_t)0x90;
        obj->priority = 4;
        scratch->state += 2;
        obj->pos.l.x.f.u = launcher->pos.l.x.f.u;
        obj->pos.l.y.f.u = launcher->pos.l.y.f.u;
        obj->ysp = 4; // (how fast it falls later)
        obj->frame = 4;
        obj->pos.l.y.f.u += 5;
        if (scratch->side) {
            obj->status.o.f.x_flip = true;
            obj->render.f.x_flip = true;
            obj->xsp = -4;
        } else {
            obj->xsp = 4;
        }
        break;

    case Pillar_Fly: {
        if (obj->status.b & 0x80)
            scratch->state = Pillar_Gone;
        int16_t x = (int16_t)(obj->pos.l.x.f.u + obj->xsp);
        bool arrived = false;
        if (obj->xsp < 0) {
            if (x <= 0x2A76) {
                x = 0x2A76;
                arrived = true;
            }
        } else if (x >= 0x2B3A) {
            x = 0x2B3A;
            arrived = true;
        }
        if (arrived)
            scratch->state += 2;
        obj->pos.l.x.f.u = x;
        DisplaySprite(obj);
        break;
    }

    case Pillar_Shake: // its animation (shaking, then waiting) moves the state on at its end ($F9, which adds 2 to the state: the engine's animation has no such command)
        if (obj->status.b & 0x80)
            scratch->state += 2;
        if (obj->anim == 1 && obj->anim_frame >= 31 && obj->frame_time.b <= 0)
            scratch->state += 2;
        else
            AnimateSprite(obj, Animation_NGHZBossPillars);
        DisplaySprite(obj);
        break;

    case Pillar_Fall: {
        const int16_t y = (int16_t)(obj->pos.l.y.f.u + obj->ysp);
        if (y > 0x4F0) {
            ObjectDelete(obj);
            return;
        }
        obj->pos.l.y.f.u = y;
        DisplaySprite(obj);
        break;
    }

    default:
        ObjectDelete(obj);
        break;
    }
}

void Obj_NGHZBoss(Object *obj) {
    Scratch_NGHZBoss *scratch = (Scratch_NGHZBoss *)&obj->scratch;

    switch (scratch->boss.boss_routine) {
    case Boss_Wait:
        if (limit_left2 == 0x2A40 - SCREEN_WIDEADD2 && player->pos.l.x.f.u >= 0x2A68 && player->pos.l.x.f.u <= 0x2B48) {
            dhz_shake = 1;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
            obj->mappings = Mappings_NGHZBoss;
            obj->render.b |= SPRITE_CAM_FIELD;
            obj->y_rad = 0x20;
            obj->priority = 2;
            obj->width_pixels = 0x90;
            scratch->boss.boss_routine = Boss_Ship;
            obj->pos.l.x.f.u = 0x2AD8;
            obj->pos.l.y.f.u = 0x388;
            boss_move.x.f.u = 0x2AD8;
            boss_move.y.f.u = 0x388;
            boss_move.xsp = 0;
            boss_move.timer = 0;
            obj->render.f.multi_sprite = true;
            obj->child_count = 3;
            obj->col_type = 0x0F;
            scratch->boss.hits = 8;
            obj->frame = 8; // (the main sprite)
            scratch->hammer_speed = (int16_t)0xFC80;
            scratch->hammer_y = (int32_t)(0x488u << 16);
            for (int i = 0; i < 3; i++) {
                obj->children[i].x = 0x2AD8;
                obj->children[i].y = 0x488;
            }
            obj->children[Part_Robotnik].frame = 0;
            obj->children[Part_Hammer].frame = 9;
            obj->children[Part_Boost].frame = 6;
            boss_move.ysp = 0x100;
            Object *launcher = FindFreeObj();
            if (launcher != NULL) {
                launcher->type = ObjId_NGHZBoss;
                launcher->mappings = Mappings_NGHZBossLauncher;
                launcher->tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
                launcher->y_rad = (int8_t)0x90;
                launcher->priority = 4;
                launcher->width_pixels = 0xFF;
                launcher->pos.l.x.f.u = 0x2AD8;
                launcher->pos.l.y.f.u = 0x488;
                launcher->frame = 0;
                launcher->render.b |= SPRITE_CAM_FIELD;
                launcher->render.f.multi_sprite = true;
                launcher->render.f.explicit_height = true;
                launcher->child_count = 2;
                Scratch_NGHZBoss *ls = (Scratch_NGHZBoss *)&launcher->scratch;
                ls->boss.boss_routine = Boss_Parts;
                ls->ship = (uint8_t)(obj - objects);
                launcher->children[0].x = 0x2A58;
                launcher->children[0].y = 0x510;
                launcher->children[0].frame = 0;
                launcher->children[1].x = 0x2B58;
                launcher->children[1].y = 0x510;
                launcher->children[1].frame = 1;
            }
        }
        FillAnimationBuffer();
        break;

    case Boss_Ship:
        Ship_Run(obj, scratch);
        break;

    case Boss_Parts:
        if (obj->render.f.multi_sprite)
            Launcher_Run(obj, scratch);
        else
            Pillar_Run(obj, scratch);
        break;
    }
}
