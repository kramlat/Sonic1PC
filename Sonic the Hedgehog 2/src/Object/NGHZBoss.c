// Neo Green Hill's boss for Sonic 2, as the alpha has it (object 89): see NGHZBoss.h. It uses the same boss helpers as the others (BossCommon). The object comes in three kinds (the alpha's Obj_Boss_Routine): 0 waits
// for the arena and then becomes the ship (2), which makes the launcher (4) with the walls' pillars as its children; the launcher makes the arrow-pillars (4 as well, told apart by their own state).
#include "Object/NGHZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
#include "SolidSst.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/NGHZBoss.h"
#include "Resource/Animation/NGHZBossPillars.h"
#include "Resource/Mappings/NGHZBoss.h"
#include "Resource/Mappings/NGHZBossLauncher.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)


#define ArtTile_NGHZBoss 0x3E0 // ($7C00: the alpha's list $2B)

enum { Boss_Wait = 0, Boss_Ship = 2, Boss_Parts = 4 };
// the ship's states (Obj_Ani_Boss_Routine); the launcher's are 0 up to the walls, 2 wait for the ship to turn, 4 a delay, 8 down again; and 6 is an arrow-pillar
enum { Ship_Descend = 0, Ship_Cross = 2, Ship_Hammer = 4, Ship_Turn = 6, Ship_Wreck = 8, Ship_Fall = 0xA, Ship_Leave = 0xC };
enum { Launcher_Rise = 0, Launcher_Wait = 2, Launcher_Delay = 4, Pillar = 6, Launcher_Sink = 8 };
enum { Part_Robotnik = 0, Part_Hammer = 1, Part_Boost = 2 };
enum { Pillar_Make = 0, Pillar_Fly = 2, Pillar_Shake = 4, Pillar_Fall = 6, Pillar_Gone = 8 };

typedef struct {
    Own_AlphaBoss boss; // flag: which way the ship goes (0x38: $FF left), side: it has been beaten (0x2C), count: bit 0 the ship has turned (0x3E)
    int16_t timer;          // 0x30: (the launcher's) how long it waits
    int16_t hammer_speed;   // 0x2E: how fast the hammer falls when the wreck lets go of it
    int32_t hammer_y;       // 0x3A
    uint8_t ship;           // 0x2A: (the launcher's) the slot of the ship, (a pillar's: the next field is its state)
    uint8_t state;          // 0x2B... a pillar's state (Obj_Timer's byte)
    uint8_t parent;         // 0x34: (a pillar's) the slot of the launcher
    uint8_t side;           // 0x28: (a pillar's) the side it comes from
} Own_NGHZBoss;

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
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_NGHZBoss];
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
static void Ship_Parts(Sst *self, Own_NGHZBoss *own) {
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    self->children[Part_Robotnik].x = x;
    self->children[Part_Robotnik].y = y;
    self->children[Part_Boost].x = x;
    self->children[Part_Boost].y = y;
    if (!own->boss.side) {
        self->children[Part_Hammer].x = x;
        self->children[Part_Hammer].y = y;
        own->hammer_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->hammer_y & 0xFFFF);
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    self->children[Part_Hammer].x--;
    own->hammer_y += (int32_t)own->hammer_speed << 8;
    own->hammer_speed += 0x38;
    self->children[Part_Hammer].y = (int16_t)(own->hammer_y >> 16);
    if (self->children[Part_Hammer].y >= 0x540)
        own->hammer_speed = 0;
}

// Offset_0x024580 and the flash of Robotnik's face (Offset_0x02628C): the bob and the hit
static void Ship_Bob(Sst *self, Own_NGHZBoss *own) {
    int16_t sin, cos;
    CalcSine(own->boss.angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    self->x_pos.f.u = boss_move.x.f.u;
    own->boss.angle = (uint8_t)(own->boss.angle + 2);
    Boss_Hit(self, &own->boss);
}

static void Ship_Hit(Sst *self, Own_NGHZBoss *own) {
    Ship_Bob(self, own);
    if (own->boss.flash == 0x1F)
        SetPartAnim(Part_Robotnik, 6);
}

static void Ship_Show(Sst *self, Own_NGHZBoss *own) {
    Boss_AnimateSprite(self, &own->boss, Animation_NGHZBoss);
    Sst_Draw(self);
}

static void Ship_Run(Sst *self, Own_NGHZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Ship_Descend:
        Boss_Move();
        Ship_Hit(self, own);
        Ship_Parts(self, own);
        if (boss_move.y.f.u >= 0x430) {
            boss_move.y.f.u = 0x430;
            boss->anim_routine += 2;
            boss_move.ysp = 0;
            boss_move.xsp = (int16_t)0xFF9C;
            boss->flag = 0xFF;
        }
        Ship_Show(self, own);
        break;

    case Ship_Cross: // along the arena, and at the other side Robotnik looks at the hammer
        Boss_Move();
        Ship_Hit(self, own);
        Ship_Parts(self, own);
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
        Ship_Show(self, own);
        break;

    case Ship_Hammer: // the hammer swings (its animation moves the state on) when the bob is at its lowest
        Boss_Move();
        Ship_Hit(self, own);
        Ship_Parts(self, own);
        if (boss->angle == 0xC0)
            SetPartAnim(Part_Hammer, 3);
        Ship_Show(self, own);
        break;

    case Ship_Turn: // it has hit: it turns and the wall throws an arrow
        SetPartAnim(Part_Hammer, 2);
        boss->count |= 1;
        boss->anim_routine = Ship_Cross;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, !((self->render_flags & SST_RENDER_X_FLIP) != 0));
        boss->flag ^= 0xFF;
        boss_move.xsp = boss->flag != 0 ? (int16_t)0xFF9C : 0x64;
        Boss_Move();
        Ship_Hit(self, own);
        Ship_Parts(self, own);
        Ship_Show(self, own);
        break;

    case Ship_Wreck:
        boss->side = 0xFF;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(self);
        } else {
            self->child_count = 2;
            self->children[Part_Robotnik].frame = 5;
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = -0x12;
        }
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Ship_Parts(self, own);
        Sst_Draw(self);
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
        Ship_Bob(self, own);
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Ship_Parts(self, own);
        Sst_Draw(self);
        break;

    case Ship_Leave: // up and away to the right; the alpha's check of the arena's edge is the one of Dust Hill's ($2180), which this arena never has, so the edge opens for ever and the act does not end
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x2180) {
            limit_right1 = limit_right2 += 2;
        } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (!boss_status)
                boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Boss_Move();
        Ship_Bob(self, own);
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Ship_Parts(self, own);
        Sst_Draw(self);
        break;
    }
}

// Offset_0x0265DC: the solid that goes with the launcher, at a place of its own ($2A68, $498)
static void Launcher_Solid(Sst *self) {
    const int16_t saved_x = self->x_pos.f.u, saved_y = self->y_pos.f.u;
    self->x_pos.f.u = 0x2A68;
    self->y_pos.f.u = 0x498;
    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Character(self, chr, who, 0x1B, 8, 0x11, 0x2A68, NULL);
    self->x_pos.f.u = saved_x;
    self->y_pos.f.u = saved_y;
}

static void Launcher_Throw(Sst *self, Own_NGHZBoss *own, bool right) { // Offset_0x02654C
    Own_AlphaBoss *boss = &own->boss;
    boss->anim_routine += 2;
    if (!right) {
        self->mapping_frame = 2;
        self->x_pos.f.u = 0x2A62;
    } else {
        self->mapping_frame = 3;
        self->x_pos.f.u = 0x2B4E;
    }
    Sst *pillar = Sst_Allocate();
    if (pillar != NULL) {
        pillar->code = sst_object_codes[ObjId_NGHZBoss];
        Own_NGHZBoss *ps = (Own_NGHZBoss *)pillar->own.u8;
        ps->boss.boss_routine = Boss_Parts;
        ps->boss.anim_routine = Pillar;
        ps->parent = (uint8_t)(self - sst);
        ps->side = right ? 0xFF : 0;
    }
    own->timer = 0x28;
    self->y_pos.f.u = arrow_targets[RandomNumber() & 3];
}

static void Launcher_Run(Sst *self, Own_NGHZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;
    Sst *ship = &sst[own->ship];
    Own_NGHZBoss *ship_scratch = (Own_NGHZBoss *)ship->own.u8;
    if (ship_scratch->boss.anim_routine >= Ship_Wreck && boss->anim_routine < Launcher_Sink)
        boss->anim_routine = Launcher_Sink;

    switch (boss->anim_routine) {
    case Launcher_Rise:
        Launcher_Solid(self);
        self->children[0].y -= 2;
        self->children[1].y -= 2;
        if (self->children[0].y <= 0x488) {
            boss->anim_routine += 2;
            dhz_shake = 0;
        }
        Sst_Draw(self);
        break;

    case Launcher_Wait:
        Launcher_Solid(self);
        if (ship_scratch->boss.count & 1) {
            ship_scratch->boss.count &= (uint8_t)~1;
            Launcher_Throw(self, own, ship_scratch->boss.flag != 0);
        }
        Sst_Draw(self);
        break;

    case Launcher_Delay:
        Launcher_Solid(self);
        if (--own->timer < 0) {
            self->mapping_frame = 0;
            boss->anim_routine = Launcher_Wait;
        }
        Sst_Draw(self);
        break;

    case Launcher_Sink:
        dhz_shake = 1;
        self->children[0].y += 2;
        self->children[1].y += 2;
        if (self->children[0].y >= 0x510) {
            dhz_shake = 0;
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

// The arrow-pillar the launcher throws: it flies across to the other wall, shakes, and falls
static void Pillar_Run(Sst *self, Own_NGHZBoss *own) {
    Sst *launcher = &sst[own->parent];
    Own_NGHZBoss *launcher_scratch = (Own_NGHZBoss *)launcher->own.u8;
    if (((Own_NGHZBoss *)sst[launcher_scratch->ship].own.u8)->boss.anim_routine >= Ship_Wreck) {
        Sst_Delete(self);
        return;
    }

    switch (own->state) {
    case Pillar_Make:
        self->mappings = Mappings_NGHZBossLauncher;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_NGHZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->y_radius = (int8_t)0x90;
        self->priority = 4 * 0x80;
        own->state += 2;
        self->x_pos.f.u = launcher->x_pos.f.u;
        self->y_pos.f.u = launcher->y_pos.f.u;
        self->y_vel = 4; // (how fast it falls later)
        self->mapping_frame = 4;
        self->y_pos.f.u += 5;
        if (own->side) {
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            self->x_vel = -4;
        } else {
            self->x_vel = 4;
        }
        break;

    case Pillar_Fly: {
        if (self->status & 0x80)
            own->state = Pillar_Gone;
        int16_t x = (int16_t)(self->x_pos.f.u + self->x_vel);
        bool arrived = false;
        if (self->x_vel < 0) {
            if (x <= 0x2A76) {
                x = 0x2A76;
                arrived = true;
            }
        } else if (x >= 0x2B3A) {
            x = 0x2B3A;
            arrived = true;
        }
        if (arrived)
            own->state += 2;
        self->x_pos.f.u = x;
        Sst_Draw(self);
        break;
    }

    case Pillar_Shake: // its animation (shaking, then waiting) moves the state on at its end ($F9, which adds 2 to the state: the engine's animation has no such command)
        if (self->status & 0x80)
            own->state += 2;
        if (self->anim == 1 && self->anim_frame >= 31 && self->anim_frame_timer <= 0)
            own->state += 2;
        else
            Sst_Animate(self, Animation_NGHZBossPillars);
        Sst_Draw(self);
        break;

    case Pillar_Fall: {
        const int16_t y = (int16_t)(self->y_pos.f.u + self->y_vel);
        if (y > 0x4F0) {
            Sst_Delete(self);
            return;
        }
        self->y_pos.f.u = y;
        Sst_Draw(self);
        break;
    }

    default:
        Sst_Delete(self);
        break;
    }
}

void Obj_NGHZBoss(Sst *self) {
    Own_NGHZBoss *own = (Own_NGHZBoss *)self->own.u8;

    switch (own->boss.boss_routine) {
    case Boss_Wait:
        if (limit_left2 == 0x2A40 - SCREEN_WIDEADD2 && player->x_pos.f.u >= 0x2A68 && player->x_pos.f.u <= 0x2B48) {
            dhz_shake = 1;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
            self->mappings = Mappings_NGHZBoss;
            self->render_flags |= SST_RENDER_LEVEL;
            self->y_radius = 0x20;
            self->priority = 2 * 0x80;
            self->width_pixels = 0x90;
            own->boss.boss_routine = Boss_Ship;
            self->x_pos.f.u = 0x2AD8;
            self->y_pos.f.u = 0x388;
            boss_move.x.f.u = 0x2AD8;
            boss_move.y.f.u = 0x388;
            boss_move.xsp = 0;
            boss_move.timer = 0;
            self->render_flags |= SST_RENDER_MULTI_DRAW;
            self->child_count = 3;
            self->collision_flags = 0x0F;
            self->collision_property = 8;
            self->mapping_frame = 8; // (the main sprite)
            own->hammer_speed = (int16_t)0xFC80;
            own->hammer_y = (int32_t)(0x488u << 16);
            for (int i = 0; i < 3; i++) {
                self->children[i].x = 0x2AD8;
                self->children[i].y = 0x488;
            }
            self->children[Part_Robotnik].frame = 0;
            self->children[Part_Hammer].frame = 9;
            self->children[Part_Boost].frame = 6;
            boss_move.ysp = 0x100;
            Sst *launcher = Sst_Allocate();
            if (launcher != NULL) {
                launcher->code = sst_object_codes[ObjId_NGHZBoss];
                launcher->mappings = Mappings_NGHZBossLauncher;
                launcher->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
                launcher->y_radius = (int8_t)0x90;
                launcher->priority = 4 * 0x80;
                launcher->width_pixels = 0xFF;
                launcher->x_pos.f.u = 0x2AD8;
                launcher->y_pos.f.u = 0x488;
                launcher->mapping_frame = 0;
                launcher->render_flags |= SST_RENDER_LEVEL;
                launcher->render_flags |= SST_RENDER_MULTI_DRAW;
                launcher->height_pixels = launcher->y_radius;
                launcher->child_count = 2;
                Own_NGHZBoss *ls = (Own_NGHZBoss *)launcher->own.u8;
                ls->boss.boss_routine = Boss_Parts;
                ls->ship = (uint8_t)(self - sst);
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
        Ship_Run(self, own);
        break;

    case Boss_Parts:
        if ((self->render_flags & SST_RENDER_MULTI_DRAW))
            Launcher_Run(self, own);
        else
            Pillar_Run(self, own);
        break;
    }
}
