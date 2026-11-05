#include "Bumper.h"

#include "Level.h"
#include "Object/Points.h"
#include "SstLoad.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Bumper.h"
#include "Resource/Mappings/Bumper.h"
#include "Sound.h"

// Object 47 - pinball bumper (SYZ). Collision is handled through
// ReactToItem (see Sonic.c's col_16x16_alt case), which just increments
// col_property each time Sonic touches it; this object's own routine
// reads/clears that flag each frame to trigger the actual bounce.

static void Bump_Hit(Sst *self) {
    if (self->collision_property) {
        self->collision_property = 0;

        int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
        int16_t dy = (int16_t)(self->y_pos.f.u - player->y_pos.f.u);
        uint16_t angle = CalcAngle(dx, dy);
        int16_t sin, cos;
        CalcSine((uint8_t)angle, &sin, &cos);

        player->x_vel = (int16_t)(((int32_t)cos * -0x700) >> 8);
        player->y_vel = (int16_t)(((int32_t)sin * -0x700) >> 8);

        player->status |= SST_PSTATUS_IN_AIR;
        player->status &= (uint8_t)~SST_PSTATUS_ROLL_JUMP;
        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        Scratch_Sonic *pscratch = (Scratch_Sonic *)player->own.u8;
        pscratch->jumping = false;

        self->anim = 1; // bumper "hit" animation
        QueueSound2(sfx_Bumper);

        bool award_points = true;
        if (self->respawn) { // (the count of hits is in the object's byte of the respawn table, under its loaded bit)
            uint8_t *hits = &sst_respawn_table[self->respawn - 1];
            if (*hits >= (10 + 0x80))
                award_points = false;
            else
                (*hits)++;
        }
        if (award_points) {
            AddPoints(1); // 10 points (AddPoints counts in tens)

            Sst *points = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
            if (points != NULL) {
                points->mapping_frame = 4; // "10" frame
            }
        }
    }

    Sst_Animate(self, Animation_Bumper);
    SstOnScreenTest(self);
}

void Obj_Bumper(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_Bumper;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SYZ_Bumper);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 1 * 0x80;
        self->collision_flags = 0x17 | 0xC0; // col_16x16_alt | col_special
        __attribute__((fallthrough));
    case 2:
        Bump_Hit(self);
        break;
    }
}
