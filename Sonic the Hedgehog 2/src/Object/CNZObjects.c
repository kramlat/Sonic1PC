// Casino Night's objects for Sonic 2, as the alpha has them (see CNZObjects.h)
#include "Object/CNZObjects.h"
void Obj_Points(Sst *self);
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "SstLoad.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZBumper.h"
#include "Resource/Mappings/AutoSpin.h"
#include "Resource/Animation/CNZBlueBumper.h"
#include "Resource/Animation/CNZGreenBumper.h"
#include "Resource/Mappings/CNZBlueBumper.h"
#include "Resource/Mappings/CNZBumper.h"
#include "Resource/Mappings/CNZGreenBumper.h"

#define ArtTile_CNZBumper 0x39A // ($7340: the zone's list)

// Object 44: the round bumper with a star. Whoever touches it (its collision is the special kind, which only counts the touch: col_property) is thrown away from it at $700/$100 pixels a frame, whatever the
// direction (the angle from the bumper to him, with the frame counter's low two bits added to it), is in the air from then on and loses his jump; it flashes, sounds, and gives ten points (up to ten times)
static void Bumper_Throw(Sst *bumper, Sst *chr) {
    const int16_t dx = (int16_t)(bumper->x_pos.f.u - chr->x_pos.f.u);
    const int16_t dy = (int16_t)(bumper->y_pos.f.u - chr->y_pos.f.u);
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

void Obj_CNZBumper(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_CNZBumper;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZBumper);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 1 * 0x80;
        self->collision_flags = 0xD7; // the special kind, of the size $17
        // Fallthrough
    case 2:
        if (self->collision_property) {
            const uint8_t touched = self->collision_property;
            self->collision_property = 0;
            if (touched & 1)
                Bumper_Throw(self, player);
            if ((touched & 2) && TAILS_OBJ->code != NULL)
                Bumper_Throw(self, TAILS_OBJ);
            self->anim = 1;
            QueueSound2(sfx_Bumper);
            bool award = true;
            if (self->respawn) {
                if ((*SstLoad_State(self)) >= 0x8A)
                    award = false;
                else
                    (*SstLoad_State(self))++;
            }
            if (award) {
                AddPoints(1);
                Sst *points = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
                if (points != NULL) {
                    points->mapping_frame = 4;
                }
            }
        }
        Sst_Animate(self, Animation_CNZBumper);
        SstOnScreenTest(self);
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

void Obj_AutoSpin(Sst *self) {
    static const uint8_t collision[4] = { 0xC6, 0x94, 0x95, 0x00 };
    uint8_t subtype = self->subtype;

    if (self->routine == 0) {
        self->routine += 2;
        self->collision_flags = collision[subtype & 3];
        self->mappings = Mappings_AutoSpin;
        self->art_tile = TILE_MAP(1, 0, 0, 0, 0x680);
        self->render_flags = SPRITE_CAM_FIELD | 0x80;
        self->width_pixels = 0x10;
        self->priority = 7 * 0x80;
        self->mapping_frame = subtype;
    }
    if (self->collision_property) {
        const uint8_t touched = self->collision_property;
        self->collision_property = 0;
        if (touched & 1)
            AutoSpin_Roll(player);
        if ((touched & 2) && TAILS_OBJ->code != NULL)
            AutoSpin_Roll(TAILS_OBJ);
        return;
    }
    if (!camera_split && SstOutOfRange(self->x_pos.f.u)) {
        Sst_Delete(self);
        return;
    }
    if (debug_mode)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D8 (the pre-beta's): the green bumper of Casino Night. Its three kinds, by the picture (0 and 3 push up or down, 1 and 4 turn him by a mirror at 45 degrees, 2 and 5 push him sideways), throw
// whoever touches it away at $700 and flash; its colour (the palette line) steps down a line each hit, and at the third it goes. Three of one group (the subtype's low six bits) gone give 500 points.
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ArtTile_CNZGreenBumper 0x3E6 // ($7CC0: the zone's list)

static uint8_t green_bumper_groups[0x40]; // the pre-beta's $FFFFE700

void CNZ_ResetBumperGroups(void) {
    memset(green_bumper_groups, 0, sizeof(green_bumper_groups));
}

typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    uint8_t wait[2]; // 0x30, 0x31: frames before the same character can hit it again (Sonic's, Tails')
} Own_GreenBumper;

// Offset_0x02723E: the throw, and what the hit does to the bumper (the character in `chr`); the bumper's wait byte is set by the caller
static void GreenBumper_Hit(Sst *self, Sst *chr, uint8_t *wait) {
    uint8_t frame = self->mapping_frame;
    if (frame >= 3)
        frame -= 3;
    if (frame == 0) { // up or down
        self->anim = 3;
        chr->y_vel = -0x700;
        if ((int16_t)(self->y_pos.f.u - chr->y_pos.f.u) < 0)
            chr->y_vel = -chr->y_vel;
    } else if (frame == 1) { // the mirror
        self->anim = 4;
        const int16_t d3 = (self->status & SST_STATUS_X_FLIP) ? 0x20 : 0x60;
        int16_t angle = (int16_t)CalcAngle(chr->x_vel, chr->y_vel);
        angle = (int16_t)(angle - d3);
        int16_t off = angle < 0 ? (int16_t)-angle : angle;
        angle = (int16_t)(d3 - angle);
        if ((uint8_t)off >= 0x40) {
            off = (int16_t)(0x80 - off);
            if ((uint8_t)off >= 0x38)
                angle = (int16_t)(d3 + 0x80);
        } else if ((uint8_t)off >= 0x38) {
            angle = d3;
        }
        int16_t sin, cos;
        CalcSine((uint8_t)angle, &sin, &cos);
        chr->x_vel = (int16_t)(((int32_t)cos * -0x700) >> 8);
        chr->y_vel = (int16_t)(((int32_t)sin * -0x700) >> 8);
    } else { // sideways
        self->anim = 5;
        chr->x_vel = -0x700;
        if ((int16_t)(self->x_pos.f.u - chr->x_pos.f.u) < 0)
            chr->x_vel = -chr->x_vel;
    }
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t) ~(SST_PSTATUS_ROLL_JUMP | SST_PSTATUS_PUSHING);
    ((Scratch_Sonic *)chr->own.u8)->jumping = false;
    PlaySound(sfx_BonusBumper);

    uint8_t points_frame = 4;
    uint16_t points = 1;
    if (self->art_tile >= 0x2000) { // (one line less: the colour changes)
        self->art_tile -= 0x2000;
    } else { // (the last hit: it goes, and the third of its group is worth a bonus)
        self->routine = 4;
        uint8_t *count = &green_bumper_groups[self->subtype & 0x3F];
        if (++*count >= 3) {
            points_frame = 2;
            points = 0x32;
        }
    }
    AddPoints(points);
    Sst *marker = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
    if (marker != NULL)
        marker->mapping_frame = points_frame;
    *wait = 4;
}

void Obj_CNZGreenBumper(Sst *self) {
    Own_GreenBumper *own = (Own_GreenBumper *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_CNZGreenBumper;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZGreenBumper);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 1 * 0x80;
        self->collision_flags = 0xD7;
        self->anim = (uint8_t)((self->subtype >> 6) & 3);
        // Fallthrough
    case 2:
        if (self->collision_property || own->wait[0] || own->wait[1]) {
            const uint8_t touched = self->collision_property;
            if (own->wait[0]) {
                own->wait[0]--;
            } else if (touched & 1) {
                GreenBumper_Hit(self, player, &own->wait[0]);
            }
            if (own->wait[1]) {
                own->wait[1]--;
            } else if ((touched & 2) && TAILS_OBJ->code != NULL) {
                GreenBumper_Hit(self, TAILS_OBJ, &own->wait[1]);
            }
            self->collision_property = 0;
        }
        Sst_Animate(self, Animation_CNZGreenBumper);
        SstOnScreenTest(self);
        break;
    default: // 4: it is going: the flash plays out, and it is gone
        Sst_Animate(self, Animation_CNZGreenBumper);
        if (self->anim < 3)
            Sst_Delete(self);
        else
            SstOnScreenTest(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D7 (the pre-beta's): the blue bumper. Whoever touches it is thrown by the quarter of the circle he is in from it: sideways at $800, or up or down at $800 with the sideways speed he has made a
// little less ($200) or more; with a subtype (not 0) it goes to and fro 1 pixel a frame between $60 pixels either side of where it is put
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ArtTile_CNZBlueBumper 0x394 // ($7280: the zone's list)

typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    int16_t left;  // 0x30
    int16_t right; // 0x32
    uint8_t left_going; // 0x34: it is going to the left
} Own_BlueBumper;

// Offset_0x026FC8
static void BlueBumper_Throw(Sst *self, Sst *chr) {
    const int16_t d1 = (int16_t)(self->x_pos.f.u - chr->x_pos.f.u);
    const int16_t d2 = (int16_t)(self->y_pos.f.u - chr->y_pos.f.u);
    const uint8_t angle = (uint8_t)(CalcAngle(d1, d2) + 0x20);
    switch (angle & 0xC0) {
    case 0x40:
    case 0xC0:
        chr->x_vel -= 0x200;
        if (d1 < 0)
            chr->x_vel += 0x400;
        chr->y_vel = (angle & 0xC0) == 0x40 ? -0x800 : 0x800;
        self->anim = 1;
        break;
    case 0x80:
        chr->x_vel = 0x800;
        self->anim = 2;
        break;
    default:
        chr->x_vel = -0x800;
        self->anim = 2;
        break;
    }
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t) ~(SST_PSTATUS_ROLL_JUMP | SST_PSTATUS_PUSHING);
    ((Scratch_Sonic *)chr->own.u8)->jumping = false;
    PlaySound(sfx_Bumper);
}

void Obj_CNZBlueBumper(Sst *self) {
    Own_BlueBumper *own = (Own_BlueBumper *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZBlueBumper;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZBlueBumper);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 1 * 0x80;
        self->collision_flags = 0xCA;
        if (self->status & SST_STATUS_X_FLIP)
            own->left_going = 1;
        own->left = (int16_t)(self->x_pos.f.u - 0x60);
        own->right = (int16_t)(self->x_pos.f.u + 0x60);
    }

    if (self->collision_property) {
        const uint8_t touched = self->collision_property;
        self->collision_property = 0;
        if (touched & 1)
            BlueBumper_Throw(self, player);
        if ((touched & 2) && TAILS_OBJ->code != NULL)
            BlueBumper_Throw(self, TAILS_OBJ);
    }
    Sst_Animate(self, Animation_CNZBlueBumper);
    if (self->subtype == 0) {
        SstOnScreenTest(self);
        return;
    }
    if (own->left_going) {
        self->x_pos.f.u--;
        if (self->x_pos.f.u == own->left)
            own->left_going = 0;
    } else {
        self->x_pos.f.u++;
        if (self->x_pos.f.u == own->right)
            own->left_going = 1;
    }
    if (!SstOutOfRange(own->left) || !SstOutOfRange(own->right)) {
        Sst_Draw(self);
        return;
    }
    if (self->respawn)
        (*SstLoad_State(self)) &= 0x7F;
    Sst_Delete(self);
}
