// Casino Night's objects for Sonic 2, as the alpha has them (see CNZObjects.h)
#include "Object/CNZObjects.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "SstLoad.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/CNZBumper.h"
#include "Resource/Mappings/AutoSpin.h"
#include "Resource/Mappings/CNZBumper.h"

#define ArtTile_CNZBumper 0x39A // ($7340: the zone's list)

// Object 44: the round bumper with a star. Whoever touches it (its collision is the special kind, which only counts the touch: col_property) is thrown away from it at $700/$100 pixels a frame, whatever the
// direction (the angle from the bumper to him, with the frame counter's low two bits added to it), is in the air from then on and loses his jump; it flashes, sounds, and gives ten points (up to ten times)
static void Bumper_Throw(Object *bumper, Sst *chr) {
    const int16_t dx = (int16_t)(bumper->pos.l.x.f.u - chr->x_pos.f.u);
    const int16_t dy = (int16_t)(bumper->pos.l.y.f.u - chr->y_pos.f.u);
    const uint8_t angle = (uint8_t)(CalcAngle(dx, dy) + (frame_count & 3));
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    chr->x_vel = (int16_t)(((int32_t)cos * -0x700) >> 8);
    chr->y_vel = (int16_t)(((int32_t)sin * -0x700) >> 8);
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_ROLL_JUMP;
    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    ((Scratch_Sonic *)chr->own.u8)->jumping = false;
}

void Obj_CNZBumper(Object *obj) {
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_CNZBumper;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZBumper);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x10;
        obj->priority = 1;
        obj->col_type = 0xD7; // the special kind, of the size $17
        // Fallthrough
    case 2:
        if (obj->col_property) {
            const uint8_t touched = obj->col_property;
            obj->col_property = 0;
            if (touched & 1)
                Bumper_Throw(obj, player);
            if ((touched & 2) && TAILS_OBJ->code != NULL)
                Bumper_Throw(obj, TAILS_OBJ);
            obj->anim = 1;
            QueueSound2(sfx_Bumper);
            bool award = true;
            if (obj->respawn_index) {
                if (objstate[obj->respawn_index] >= 0x8A)
                    award = false;
                else
                    objstate[obj->respawn_index]++;
            }
            if (award) {
                AddPoints(1);
                Sst *points = Sst_Spawn(ObjId_Points, obj->pos.l.x.f.u, obj->pos.l.y.f.u);
                if (points != NULL) {
                    points->mapping_frame = 4;
                }
            }
        }
        AnimateSprite(obj, Animation_CNZBumper);
        RememberState(obj);
        break;
    }
}

// Object 84: the auto spin, an invisible box (only shown with debug mode on) that rolls whoever touches it into a ball (Hill Top's, Hidden Palace's and Casino Night's use it): the subtype is its size (the
// frame: 0 is 32 pixels across, 1 is 96 and 2 is 224), and the collision it has ($C6, the special kind that counts the touch; the subtypes 1 and 2's $94 and $95 are the hurting kinds, which nothing uses)
static void AutoSpin_Roll(Sst *chr) {
    if (((chr->status & SST_PSTATUS_IN_BALL) != 0))
        return;
    chr->status |= SST_PSTATUS_IN_BALL;
    chr->y_radius = 0x0E;
    chr->x_radius = 7;
    chr->anim = SonAnimId_Roll;
    chr->y_pos.f.u += 5;
    QueueSound2(sfx_Roll);
    if (chr->ground_vel == 0)
        chr->ground_vel = 0x200;
}

void Obj_AutoSpin(Object *obj) {
    static const uint8_t collision[4] = { 0xC6, 0x94, 0x95, 0x00 };
    uint8_t subtype = obj->scratch.u8[0];

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->col_type = collision[subtype & 3];
        obj->mappings = Mappings_AutoSpin;
        obj->tile = TILE_MAP(1, 0, 0, 0, 0x680);
        obj->render.b = SPRITE_CAM_FIELD | 0x80;
        obj->width_pixels = 0x10;
        obj->priority = 7;
        obj->frame = subtype;
    }
    if (obj->col_property) {
        const uint8_t touched = obj->col_property;
        obj->col_property = 0;
        if (touched & 1)
            AutoSpin_Roll(player);
        if ((touched & 2) && TAILS_OBJ->code != NULL)
            AutoSpin_Roll(TAILS_OBJ);
        return;
    }
    if (!camera_split && IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    if (debug_mode)
        DisplaySprite(obj);
}
