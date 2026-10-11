// Casino Night's objects for Sonic 2, as the alpha has them (see CNZObjects.h)
#include "Object/CNZObjects.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
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
static void Bumper_Throw(Object *bumper, Object *chr) {
    const int16_t dx = (int16_t)(bumper->pos.l.x.f.u - chr->pos.l.x.f.u);
    const int16_t dy = (int16_t)(bumper->pos.l.y.f.u - chr->pos.l.y.f.u);
    const uint8_t angle = (uint8_t)(CalcAngle(dx, dy) + (frame_count & 3));
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    chr->xsp = (int16_t)(((int32_t)cos * -0x700) >> 8);
    chr->ysp = (int16_t)(((int32_t)sin * -0x700) >> 8);
    chr->status.p.f.in_air = true;
    chr->status.p.f.roll_jump = false;
    chr->status.p.f.pushing = false;
    ((Scratch_Sonic *)&chr->scratch)->jumping = false;
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
            if ((touched & 2) && TAILS_OBJ->type != 0)
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
                Object *points = FindFreeObj();
                if (points != NULL) {
                    points->type = ObjId_Points;
                    points->pos.l.x.f.u = obj->pos.l.x.f.u;
                    points->pos.l.y.f.u = obj->pos.l.y.f.u;
                    points->frame = 4;
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
static void AutoSpin_Roll(Object *chr) {
    if (chr->status.p.f.in_ball)
        return;
    chr->status.p.f.in_ball = true;
    chr->y_rad = 0x0E;
    chr->x_rad = 7;
    chr->anim = SonAnimId_Roll;
    chr->pos.l.y.f.u += 5;
    QueueSound2(sfx_Roll);
    if (chr->inertia == 0)
        chr->inertia = 0x200;
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
        if ((touched & 2) && TAILS_OBJ->type != 0)
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
