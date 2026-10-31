// Dust Hill's boss for Sonic 2, as the alpha has it (object 57): see DHZBoss.h. It uses the same boss helpers as the Metropolis and Hill Top bosses (BossCommon): the ship's place and speed in a buffer, and the
// animation of its parts, each a pair of nibbles in a small buffer. Its parts (the front drill, the ship's body, Robotnik, the back drill) are the object's child sprites.
#include "Object/DHZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
    Own_AlphaBoss boss;
    int16_t back_speed;  // 0x2E: how fast the back drill falls when the wreck lets go of it
    int16_t front_speed; // 0x30: and the front
    int32_t front_y;     // 0x34 (Control_Var_08): the front drill's place as 16.16
    int32_t back_y;      // 0x3A (Control_Var_0E)
} Own_DHZBoss;

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
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_DHZBoss];
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
static void Boss_Parts(Sst *self, Own_DHZBoss *own) {
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    self->children[Part_Body].x = x;
    self->children[Part_Body].y = y;
    self->children[Part_Robotnik].x = x;
    self->children[Part_Robotnik].y = y;
    if (!own->boss.side) {
        self->children[Part_BackDrill].x = x;
        self->children[Part_BackDrill].y = y;
        self->children[Part_FrontDrill].x = x;
        self->children[Part_FrontDrill].y = y;
        own->back_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->back_y & 0xFFFF);
        own->front_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->front_y & 0xFFFF);
        self->children[Part_BackDrill].x = (int16_t)(x + (((self->render_flags & SST_RENDER_X_FLIP) != 0) ? 0x28 : -0x28));
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    self->children[Part_BackDrill].x--;
    own->back_y += (int32_t)own->back_speed << 8;
    own->back_speed += 0x38;
    self->children[Part_BackDrill].y = (int16_t)(own->back_y >> 16);
    if (self->children[Part_BackDrill].y >= 0x6F0)
        own->back_speed = 0;
    self->children[Part_FrontDrill].x++;
    own->front_y += (int32_t)own->front_speed << 8;
    own->front_speed += 0x38;
    self->children[Part_FrontDrill].y = (int16_t)(own->front_y >> 16);
    if (self->children[Part_FrontDrill].y >= 0x6F0)
        own->front_speed = 0;
}

// Offset_0x026DB0: every eighth frame, while the boss digs, a rock comes loose at a random place along the arena
static void Boss_DropRock(void) {
    if (frame_count & 7)
        return;
    int16_t x;
    do {
        x = (int16_t)((RandomNumber() & 0x1FF) + 0x20F0);
    } while (x > 0x2228);
    Sst *rock = Sst_Allocate();
    if (rock == NULL)
        return;
    rock->code = sst_object_codes[ObjId_DHZBoss];
    ((Own_DHZBoss *)rock->own.u8)->boss.boss_routine = Boss_Rock;
    rock->x_pos.f.u = x;
    rock->y_pos.f.u = 0x5F0;
    rock->mappings = Mappings_DHZBoss;
    rock->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_DHZBossRock);
    rock->render_flags |= SST_RENDER_LEVEL;
    rock->priority = 3 * 0x80;
    rock->mapping_frame = 0xD;
}

// Offset_0x026E0A: the hit, and the flash of Robotnik's face when it has just been hit
static void Boss_Struck(Sst *self, Own_DHZBoss *own) {
    Boss_Hit(self, &own->boss);
    if (own->boss.flash == 0x1F)
        SetPartAnim(Part_Robotnik, 0xE);
}

// What every frame of the digging and charging ends with
static void Boss_Show(Sst *self, Own_DHZBoss *own) {
    self->y_pos.f.u = boss_move.y.f.u;
    self->x_pos.f.u = boss_move.x.f.u;
    Boss_Struck(self, own);
    Boss_AnimateSprite(self, &own->boss, Animation_DHZBoss);
    Boss_Parts(self, own);
    Sst_Draw(self);
}

// What the wreck's frames end with (no hit, no animation of its own)
static void Boss_ShowWreck(Sst *self, Own_DHZBoss *own) {
    self->y_pos.f.u = boss_move.y.f.u;
    self->x_pos.f.u = boss_move.x.f.u;
    Boss_Parts(self, own);
    Sst_Draw(self);
}

static void Boss_Dig(Sst *self, Own_DHZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Run_Rise: // up out of the floor to the middle of the arena, and then to where Sonic is
        if (--boss_move.timer < 0) {
            Boss_Move();
            if (boss_move.y.f.u <= 0x560) {
                boss_move.ysp = 0x100;
                int16_t x = (int16_t)(player->x_pos.f.u + (int8_t)(RandomNumber() & 0xFF));
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
        Boss_Show(self, own);
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
        Boss_Show(self, own);
        break;

    case Run_Charge: // a moment at the bottom (turning to Sonic), and then it goes on down to its floor
        if (--boss_move.timer <= 0) {
            if (boss_move.timer == 0) {
                boss_anim[1] = 0x30;
                boss_move.ysp = 0x400;
                SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
                if ((int16_t)(player->x_pos.f.u - boss_move.x.f.u) >= 0)
                    SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            }
            Boss_Move();
            if (boss_move.y.f.u >= 0x650) {
                boss_move.y.f.u = 0x650;
                boss->anim_routine += 2;
                boss_move.xsp = (int16_t)0xFE00;
                boss_move.ysp = 0;
                if (((self->render_flags & SST_RENDER_X_FLIP) != 0))
                    boss_move.xsp = (int16_t)-boss_move.xsp;
            }
        }
        Boss_Show(self, own);
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
        Boss_Show(self, own);
        break;

    case Run_Wreck: // blowing up where it is
        boss->side = 0xFF;
        dhz_shake = 0;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(self);
        } else {
            self->children[Part_Robotnik].frame = 0x13;
            self->mapping_frame = 7; // (and the main sprite's frame the wreck has)
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = -0x12;
        }
        Boss_ShowWreck(self, own);
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
        Boss_ShowWreck(self, own);
        break;

    case Run_Leave: // up and away to the right, the arena opening as it goes; when it is off the screen the act is over
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x2180 + SCREEN_WIDEADD2)
            limit_right1 = limit_right2 += 2;
        else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (!boss_status)
                boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Boss_Move();
        Boss_ShowWreck(self, own);
        break;
    }
}

void Obj_DHZBoss(Sst *self) {
    Own_DHZBoss *own = (Own_DHZBoss *)self->own.u8;

    switch (own->boss.boss_routine) {
    case Boss_Start:
        self->mappings = Mappings_DHZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_DHZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x60;
        self->x_pos.f.u = 0x21A0;
        self->y_pos.f.u = 0x678;
        own->boss.boss_routine += 2;
        self->render_flags |= SST_RENDER_MULTI_DRAW;
        self->child_count = 4;
        self->collision_flags = 0x0F;
        self->collision_property = 8;
        boss_move.x.f.u = self->x_pos.f.u;
        boss_move.y.f.u = self->y_pos.f.u;
        boss_move.ysp = (int16_t)0xFF40;
        boss_move.xsp = 0;
        boss_move.timer = 0x28;
        self->mapping_frame = 5; // (the alpha's Obj_Boss_Ani_Map, the main sprite's frame: the ship's flames, animated by the first pair of the animation buffer)
        for (int i = 0; i < 4; i++) {
            self->children[i].x = self->x_pos.f.u;
            self->children[i].y = self->y_pos.f.u;
        }
        self->children[Part_FrontDrill].frame = 2;
        self->children[Part_Body].frame = 1;
        self->children[Part_Robotnik].frame = 0xE;
        self->children[Part_BackDrill].frame = 2;
        self->children[Part_BackDrill].x -= 0x28;
        own->back_speed = (int16_t)0xFC80;
        own->front_speed = (int16_t)0xFC80;
        own->front_y = (int32_t)((uint32_t)(uint16_t)self->y_pos.f.u << 16);
        own->back_y = own->front_y;
        {
            static const uint8_t buffer[10] = { 0x02, 0x00, 0x03, 0x00, 0x10, 0x00, 0x0D, 0x00, 0x03, 0x00 };
            memcpy(boss_anim, buffer, sizeof(buffer));
        }
        break;

    case Boss_Run:
        Boss_Dig(self, own);
        break;

    case Boss_Rock: // a rock that has come loose from the ceiling: it falls (a little less quickly than things usually do)
        Sst_MoveSprite(self);
        self->y_vel -= 0x14;
        if (self->y_pos.f.u > 0x6F0)
            Sst_Delete(self);
        else
            Sst_Draw(self);
        break;
    }
}
