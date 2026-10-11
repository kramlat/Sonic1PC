// Dust Hill's boss for Sonic 2, as the alpha has it (object 57): see DHZBoss.h. It uses the same boss helpers as the Metropolis and Hill Top bosses (BossCommon): the ship's place and speed in a buffer, and the
// animation of its parts, each a pair of nibbles in a small buffer. Its parts (the front drill, the ship's body, Robotnik, the back drill) are the object's child sprites.
#include "Object/DHZBoss.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "HTZQuake.h"
#include "Level.h"
#include "MathUtil.h"
#include "Sound.h"
#include "Backend/VDP.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/DHZBoss.h"
#include "Resource/Art/DHZBossRock.h"
#include "Resource/Mappings/DHZBoss.h"

#define ArtTile_DHZBoss     0x3C0 // ($7800: the alpha's list $2C)
#define ArtTile_DHZBossRock 0x560 // (where the falling rocks' mappings point; the four tiles of their art go to tile $360)

enum { Boss_Start = 0, Boss_Run = 2, Boss_Rock = 4 };
enum { Run_Rise = 0, Run_Down = 2, Run_Charge = 4, Run_Across = 6, Run_Wreck = 8, Run_Fall = 0xA, Run_Leave = 0xC };
enum { Part_FrontDrill = 0, Part_Body = 1, Part_Robotnik = 2, Part_BackDrill = 3 };

typedef struct {
    Scratch_AlphaBoss boss;
    int16_t back_speed;  // 0x2E: how fast the back drill falls when the wreck lets go of it
    int16_t front_speed; // 0x30: and the front
    int32_t front_y;     // 0x34 (Control_Var_08): the front drill's place as 16.16
    int32_t back_y;      // 0x3A (Control_Var_0E)
} Scratch_DHZBoss;

// The palette the boss's list $19 sets over the second line (PalLoad2)
static const uint16_t boss_palette[16] = {
    0x0002, 0x0000, 0x006C, 0x008E, 0x00CE, 0x0044, 0x0EEE, 0x0AAA, 0x0888, 0x0444, 0x0666, 0x0006, 0x00EE, 0x0088, 0x002E, 0x002A,
};

void DHZBoss_Events(void) {
    switch (dle_routine) {
    case 0: // the camera is near the bottom of the shaft at the end of the level, past its middle: the boss's arena
        if (scrpos_y.f.u < (0x5C0 - SCREEN_TALLADD2))
            break;
        if (scrpos_x.f.u < (0x1C00 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = 0x600 - SCREEN_TALLADD;
        dle_routine += 2;
        {
            Object *boss = FindFreeObj();
            if (boss != NULL)
                boss->type = ObjId_DHZBoss;
        }
        VDP_SeekVRAM(0x6C00); // (the rocks' four tiles)
        VDP_WriteVRAM(Art_DHZBossRock, sizeof(Art_DHZBossRock));
        boss_flag = 5;
        AddPLC(PlcId_DHZBoss);
        for (int i = 0; i < 16; i++)
            dry_palette[1][i] = boss_palette[i];
        break;
    case 2: // the camera has come to the arena: it stays there, and the floor creeps up
        if (scrpos_x.f.u >= (0x20F0 - SCREEN_WIDEADD2)) {
            limit_right1 = limit_right2 = 0x20F0 + SCREEN_WIDEADD2;
            limit_left2 = 0x20F0 - SCREEN_WIDEADD2;
            dle_routine += 2;
        }
        limit_btm1--;
        if (limit_btm1 < (0x5E0 - SCREEN_TALLADD))
            limit_btm1 = 0x5E0 - SCREEN_TALLADD;
        break;
    case 4: // when it is beaten, the results card
        if (boss_status)
            GotThroughAct();
        break;
    }
}

// The animation buffer's nibble of a part, set to another animation (the buffer holds a pair of bytes for the object's own sprite and then each child's: the low nibble of the first is the animation)
static void SetPartAnim(int part, uint8_t anim) {
    boss_anim[(part + 1) * 2] = (uint8_t)((boss_anim[(part + 1) * 2] & 0xF0) | anim);
}

// Offset_0x026CF4: where the parts are, from the ship's place; once it has been beaten the drills fall (the back first)
static void Boss_Parts(Object *obj, Scratch_DHZBoss *scratch) {
    const int16_t x = obj->pos.l.x.f.u, y = obj->pos.l.y.f.u;
    obj->children[Part_Body].x = x;
    obj->children[Part_Body].y = y;
    obj->children[Part_Robotnik].x = x;
    obj->children[Part_Robotnik].y = y;
    if (!scratch->boss.side) {
        obj->children[Part_BackDrill].x = x;
        obj->children[Part_BackDrill].y = y;
        obj->children[Part_FrontDrill].x = x;
        obj->children[Part_FrontDrill].y = y;
        scratch->back_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (scratch->back_y & 0xFFFF);
        scratch->front_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (scratch->front_y & 0xFFFF);
        obj->children[Part_BackDrill].x = (int16_t)(x + (obj->render.f.x_flip ? 0x28 : -0x28));
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    obj->children[Part_BackDrill].x--;
    scratch->back_y += (int32_t)scratch->back_speed << 8;
    scratch->back_speed += 0x38;
    obj->children[Part_BackDrill].y = (int16_t)(scratch->back_y >> 16);
    if (obj->children[Part_BackDrill].y >= 0x6F0)
        scratch->back_speed = 0;
    obj->children[Part_FrontDrill].x++;
    scratch->front_y += (int32_t)scratch->front_speed << 8;
    scratch->front_speed += 0x38;
    obj->children[Part_FrontDrill].y = (int16_t)(scratch->front_y >> 16);
    if (obj->children[Part_FrontDrill].y >= 0x6F0)
        scratch->front_speed = 0;
}

// Offset_0x026DB0: every eighth frame, while the boss digs, a rock comes loose at a random place along the arena
static void Boss_DropRock(void) {
    if (frame_count & 7)
        return;
    int16_t x;
    do {
        x = (int16_t)((RandomNumber() & 0x1FF) + 0x20F0);
    } while (x > 0x2228);
    Object *rock = FindFreeObj();
    if (rock == NULL)
        return;
    rock->type = ObjId_DHZBoss;
    ((Scratch_DHZBoss *)&rock->scratch)->boss.boss_routine = Boss_Rock;
    rock->pos.l.x.f.u = x;
    rock->pos.l.y.f.u = 0x5F0;
    rock->mappings = Mappings_DHZBoss;
    rock->tile = TILE_MAP(0, 0, 0, 0, ArtTile_DHZBossRock);
    rock->render.b |= SPRITE_CAM_FIELD;
    rock->priority = 3;
    rock->frame = 0xD;
}

// Offset_0x026E0A: the hit, and the flash of Robotnik's face when it has just been hit
static void Boss_Struck(Object *obj, Scratch_DHZBoss *scratch) {
    Boss_Hit(obj, &scratch->boss);
    if (scratch->boss.flash == 0x1F)
        SetPartAnim(Part_Robotnik, 0xE);
}

// What every frame of the digging and charging ends with
static void Boss_Show(Object *obj, Scratch_DHZBoss *scratch) {
    obj->pos.l.y.f.u = boss_move.y.f.u;
    obj->pos.l.x.f.u = boss_move.x.f.u;
    Boss_Struck(obj, scratch);
    Boss_AnimateSprite(obj, &scratch->boss, Animation_DHZBoss);
    Boss_Parts(obj, scratch);
    DisplaySprite(obj);
}

// What the wreck's frames end with (no hit, no animation of its own)
static void Boss_ShowWreck(Object *obj, Scratch_DHZBoss *scratch) {
    obj->pos.l.y.f.u = boss_move.y.f.u;
    obj->pos.l.x.f.u = boss_move.x.f.u;
    Boss_Parts(obj, scratch);
    DisplaySprite(obj);
}

static void Boss_Dig(Object *obj, Scratch_DHZBoss *scratch) {
    Scratch_AlphaBoss *boss = &scratch->boss;

    switch (boss->anim_routine) {
    case Run_Rise: // up out of the floor to the middle of the arena, and then to where Sonic is
        if (--boss_move.timer < 0) {
            Boss_Move();
            if (boss_move.y.f.u <= 0x560) {
                boss_move.ysp = 0x100;
                int16_t x = (int16_t)(player->pos.l.x.f.u + (int8_t)(RandomNumber() & 0xFF));
                if (x <= 0x2100)
                    x = 0x2100;
                else if (x >= 0x2218)
                    x = 0x2218;
                boss_move.x.f.u = x;
                boss->anim_routine += 2;
            }
        }
        if (boss_move.y.f.u < 0x620) {
            dhz_shake = 1;
            Boss_DropRock();
        }
        Boss_Show(obj, scratch);
        break;

    case Run_Down: // down again, drilling: the drills spin faster, Robotnik laughs
        Boss_Move();
        Boss_DropRock();
        if (boss_move.y.f.u >= 0x620) {
            SetPartAnim(Part_FrontDrill, 6);
            SetPartAnim(Part_BackDrill, 6);
            SetPartAnim(Part_Robotnik, 0xD);
            boss_anim[7] = 0x30;
            boss->anim_routine += 2;
            boss_move.timer = 0xC8;
            dhz_shake = 0;
        }
        Boss_Show(obj, scratch);
        break;

    case Run_Charge: // a moment at the bottom (turning to Sonic), and then it goes on down to its floor
        if (--boss_move.timer <= 0) {
            if (boss_move.timer == 0) {
                boss_anim[1] = 0x30;
                boss_move.ysp = 0x400;
                obj->render.f.x_flip = false;
                if ((int16_t)(player->pos.l.x.f.u - boss_move.x.f.u) >= 0)
                    obj->render.f.x_flip = true;
            }
            Boss_Move();
            if (boss_move.y.f.u >= 0x650) {
                boss_move.y.f.u = 0x650;
                boss->anim_routine += 2;
                boss_move.xsp = (int16_t)0xFE00;
                boss_move.ysp = 0;
                if (obj->render.f.x_flip)
                    boss_move.xsp = (int16_t)-boss_move.xsp;
            }
        }
        Boss_Show(obj, scratch);
        break;

    case Run_Across: // along the floor to the wall, and up again
        Boss_Move();
        if (boss_move.x.f.u <= 0x2100 || boss_move.x.f.u >= 0x2218) {
            boss_move.x.f.u = boss_move.x.f.u <= 0x2100 ? 0x2100 : 0x2218;
            boss_move.xsp = 0;
            boss->anim_routine = Run_Rise;
            SetPartAnim(Part_FrontDrill, 0xB);
            SetPartAnim(Part_BackDrill, 0xB);
            boss_anim[1] = 0;
            SetPartAnim(Part_Robotnik, 0xD);
            boss_anim[7] = 0x30;
            boss_move.timer = 0xC8;
            boss_move.ysp = (int16_t)0xFF40;
        }
        Boss_Show(obj, scratch);
        break;

    case Run_Wreck: // blowing up where it is
        boss->side = 0xFF;
        dhz_shake = 0;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(obj);
        } else {
            obj->children[Part_Robotnik].frame = 0x13;
            obj->frame = 7; // (and the main sprite's frame the wreck has)
            obj->render.f.x_flip = true;
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = -0x12;
        }
        Boss_ShowWreck(obj, scratch);
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
        Boss_ShowWreck(obj, scratch);
        break;

    case Run_Leave: // up and away to the right, the arena opening as it goes; when it is off the screen the act is over
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x2180 + SCREEN_WIDEADD2)
            limit_right1 = limit_right2 += 2;
        else if (!obj->render.f.on_screen) {
            if (!boss_status)
                boss_status = 1;
            ObjectDelete(obj);
            return;
        }
        Boss_Move();
        Boss_ShowWreck(obj, scratch);
        break;
    }
}

void Obj_DHZBoss(Object *obj) {
    Scratch_DHZBoss *scratch = (Scratch_DHZBoss *)&obj->scratch;

    switch (scratch->boss.boss_routine) {
    case Boss_Start:
        obj->mappings = Mappings_DHZBoss;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_DHZBoss);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 3;
        obj->width_pixels = 0x60;
        obj->pos.l.x.f.u = 0x21A0;
        obj->pos.l.y.f.u = 0x678;
        scratch->boss.boss_routine += 2;
        obj->render.f.multi_sprite = true;
        obj->child_count = 4;
        obj->col_type = 0x0F;
        scratch->boss.hits = 8;
        boss_move.x.f.u = obj->pos.l.x.f.u;
        boss_move.y.f.u = obj->pos.l.y.f.u;
        boss_move.ysp = (int16_t)0xFF40;
        boss_move.xsp = 0;
        boss_move.timer = 0x28;
        obj->frame = 5; // (the alpha's Obj_Boss_Ani_Map, the main sprite's frame: the ship's flames, animated by the first pair of the animation buffer)
        for (int i = 0; i < 4; i++) {
            obj->children[i].x = obj->pos.l.x.f.u;
            obj->children[i].y = obj->pos.l.y.f.u;
        }
        obj->children[Part_FrontDrill].frame = 2;
        obj->children[Part_Body].frame = 1;
        obj->children[Part_Robotnik].frame = 0xE;
        obj->children[Part_BackDrill].frame = 2;
        obj->children[Part_BackDrill].x -= 0x28;
        scratch->back_speed = (int16_t)0xFC80;
        scratch->front_speed = (int16_t)0xFC80;
        scratch->front_y = (int32_t)((uint32_t)(uint16_t)obj->pos.l.y.f.u << 16);
        scratch->back_y = scratch->front_y;
        {
            static const uint8_t buffer[10] = { 0x02, 0x00, 0x03, 0x00, 0x10, 0x00, 0x0D, 0x00, 0x03, 0x00 };
            memcpy(boss_anim, buffer, sizeof(buffer));
        }
        break;

    case Boss_Run:
        Boss_Dig(obj, scratch);
        break;

    case Boss_Rock: // a rock that has come loose from the ceiling: it falls (a little less quickly than things usually do)
        ObjectFall(obj);
        obj->ysp -= 0x14;
        if (obj->pos.l.y.f.u > 0x6F0)
            ObjectDelete(obj);
        else
            DisplaySprite(obj);
        break;
    }
}
