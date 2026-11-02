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
#include "Solid.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZBumper.h"
#include "Resource/Mappings/AutoSpin.h"
#include "Resource/Animation/CNZBlueBumper.h"
#include "Resource/Animation/CNZGreenBumper.h"
#include "Resource/Mappings/CNZBlueBox.h"
#include "Resource/Mappings/CNZBlueBumper.h"
#include "Resource/Mappings/CNZCrawl.h"
#include "Resource/Mappings/CNZElevator.h"
#include "Resource/Mappings/CNZGreenPlatform.h"
#include "Resource/Mappings/CNZBumper.h"
#include "Resource/Mappings/CNZGreenBumper.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

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

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D5 (the pre-beta's): the elevator. It is put at one end of a run of the subtype times 8 pixels (the other end is the same distance from the middle, which is where the object was placed;
// flipped it starts at the lower end): stood on, it speeds up towards the middle by 8 a frame and slows to a stop at the other end; once the character has left it, it goes back the same way (and is
// not a platform on the way back)
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ArtTile_CNZElevator 0x384 // ($7080: the zone's list)

typedef struct {
    uint8_t subtype;
    uint8_t pad0[9];
    int16_t home;  // 0x32: the middle of the run
    int16_t state; // 0x34: 0 waits, 2 goes, 4 waits for him to leave, 6 goes back
} Own_Elevator;

static void Elevator_Stops(Sst *self, Own_Elevator *own, bool upper) {
    const int16_t run = (int16_t)(self->subtype << 2);
    self->y_pos.f.u = (int16_t)(own->home + (upper ? -run : run));
}

void Obj_CNZElevator(Sst *self) {
    Own_Elevator *own = (Own_Elevator *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZElevator;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZElevator);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        own->home = self->y_pos.f.u;
        Elevator_Stops(self, own, !(self->status & SST_STATUS_X_FLIP)); // (flipped: it starts at the lower end)
    }

    const int16_t old_x = self->x_pos.f.u;
    Sst_MoveSprite2(self);
    switch (own->state) {
    case 0:
        if (self->status & 0x18) {
            PlaySound(sfx_CNZElevator);
            own->state += 2;
        }
        break;
    case 4:
        if (!(self->status & 0x18)) {
            PlaySound(sfx_CNZElevator);
            own->state += 2;
        }
        break;
    default: { // 2 and 6: towards the middle, and a stop at the other end
        int16_t d1 = 8;
        if ((uint16_t)own->home < (uint16_t)self->y_pos.f.u)
            d1 = -8;
        self->y_vel = (int16_t)(self->y_vel + d1);
        if (self->y_vel == 0) {
            const bool flipped = (self->status & SST_STATUS_X_FLIP) != 0;
            if (own->state == 2) {
                own->state += 2;
                Elevator_Stops(self, own, flipped); // (the other end)
            } else {
                own->state = 0;
                Elevator_Stops(self, own, !flipped);
            }
        }
        break;
    }
    }
    if (own->state < 6) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Platform(self, chr, who, 0x10, 9, old_x);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D4 (the pre-beta's): the blue box, a solid $40 box that swings by being pulled to where it was put, by 4 a frame in the speed's units ($100 to a pixel): sideways (subtype 0) from $60 pixels to
// one side (the other, flipped) or up and down (2) from $60 above (flipped: below)
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ArtTile_CNZBlueBox 0x36C // ($6D80: the zone's list)

typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    int16_t home_x; // 0x30
    int16_t home_y; // 0x32
} Own_BlueBox;

void Obj_CNZBlueBox(Sst *self) {
    Own_BlueBox *own = (Own_BlueBox *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZBlueBox;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZBlueBox);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->priority = 4 * 0x80;
        own->home_x = self->x_pos.f.u;
        own->home_y = self->y_pos.f.u;
        self->x_pos.f.l = 0x8000;
        self->y_pos.f.l = 0x8000;
        if (self->subtype == 0) {
            self->x_pos.f.u -= 0x60;
            if (self->status & SST_STATUS_X_FLIP)
                self->x_pos.f.u += 0xC0;
        } else {
            self->y_pos.f.u -= 0x60;
            if (self->status & SST_STATUS_Y_FLIP)
                self->y_pos.f.u += 0xC0;
        }
    }

    const int16_t old_x = self->x_pos.f.u;
    if (self->subtype == 0) {
        self->x_vel += (uint16_t)own->home_x > (uint16_t)self->x_pos.f.u ? 4 : -4;
    } else {
        self->y_vel += (uint16_t)own->home_y > (uint16_t)self->y_pos.f.u ? 4 : -4;
    }
    Sst_MoveSprite2(self);
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(self, chr, who, 0x2B, 0x20, 0x21, old_x, NULL);
    SstOnScreenTest2(self, own->home_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D2 (the pre-beta's): the green platform, a solid line of squares that turns about its place: each of its 16 frames (every 16 ticks) has its own place, size and mapping. When it has been
// through them it waits the subtype times 16 ticks, let go of whoever was on it, and begins again
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ArtTile_CNZGreenPlatform 0x37C // ($6F80: the zone's list)

static const uint8_t green_platform_frames[16][4] = {
    { 0xD8, 0x18, 0x08, 0x08 },
    { 0xD8, 0x10, 0x08, 0x10 },
    { 0xD8, 0x08, 0x08, 0x18 },
    { 0xD8, 0x00, 0x08, 0x20 },
    { 0xE0, 0x00, 0x10, 0x20 },
    { 0xE8, 0xF8, 0x18, 0x18 },
    { 0xF0, 0xF0, 0x20, 0x10 },
    { 0xF8, 0xE8, 0x28, 0x08 },
    { 0x08, 0xE8, 0x28, 0x08 },
    { 0x10, 0xF0, 0x20, 0x10 },
    { 0x18, 0xF8, 0x18, 0x18 },
    { 0x20, 0x00, 0x10, 0x20 },
    { 0x28, 0x00, 0x08, 0x20 },
    { 0x28, 0x08, 0x08, 0x18 },
    { 0x28, 0x10, 0x08, 0x10 },
    { 0x28, 0x18, 0x08, 0x08 },
};

typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    int16_t home_x; // 0x30
    int16_t home_y; // 0x32
    int16_t width;  // 0x34
    int16_t height; // 0x36
    int16_t wait;   // 0x38
    int16_t timer;  // 0x3A
} Own_GreenPlatform;

// Offset_0x0260F8: where the frame puts it
static void GreenPlatform_Place(Sst *self, Own_GreenPlatform *own) {
    const uint8_t *entry = green_platform_frames[self->mapping_frame & 0xF];
    int16_t dx = (int8_t)entry[0];
    if (self->status & SST_STATUS_X_FLIP)
        dx = (int16_t)-dx;
    self->x_pos.f.u = (int16_t)(own->home_x + dx);
    self->y_pos.f.u = (int16_t)(own->home_y + (int8_t)entry[1]);
    own->width = entry[2];
    self->width_pixels = entry[2];
    own->height = entry[3];
}

static void GreenPlatform_LetGo(Sst *self, Sst *chr, int who) {
    const uint8_t stand_bit = (uint8_t)(1 << (3 + who));
    if (!(self->status & stand_bit))
        return;
    self->status &= (uint8_t)~stand_bit;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->status |= SST_PSTATUS_IN_AIR;
}

void Obj_CNZGreenPlatform(Sst *self) {
    Own_GreenPlatform *own = (Own_GreenPlatform *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZGreenPlatform;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_CNZGreenPlatform);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 8;
        self->priority = 4 * 0x80;
        own->home_x = self->x_pos.f.u;
        own->home_y = self->y_pos.f.u;
        own->wait = (int16_t)(self->subtype << 4);
        GreenPlatform_Place(self, own);
    }

    if (own->wait != 0) { // (waiting for its turn: it is only a picture)
        own->wait--;
        SstOnScreenTest(self);
        return;
    }
    if (--own->timer < 0) {
        own->timer = 0xF;
        self->mapping_frame = (uint8_t)((self->mapping_frame + 1) & 0xF);
        if (self->mapping_frame == 0) { // (a round done: it is let go of whoever is on it, and waits)
            own->wait = (int16_t)(self->subtype << 4);
            if (self->status & 0x18) {
                GreenPlatform_LetGo(self, player, SolidChar_Sonic);
                if (TAILS_OBJ->code != NULL)
                    GreenPlatform_LetGo(self, TAILS_OBJ, SolidChar_Tails);
            }
            self->x_pos.f.u = own->home_x;
            GreenPlatform_Place(self, own);
            return;
        }
        GreenPlatform_Place(self, own);
    }
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(self, chr, who, (int16_t)(own->width + 0xB), own->height, (int16_t)(own->height + 1), self->x_pos.f.u, NULL);
    SstOnScreenTest2(self, own->home_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object C8 (the pre-beta's): the Crawl, a guard (it is not hurt: its collision is the bumper's kind, $D7). A character within $60 either way, in the box, turns it to him and it raises its shield (frame 1);
// whoever touches it is thrown away from it at $700, as by a bumper; it lowers it again when he has gone
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ArtTile_CNZCrawl 0x340 // ($6800: the zone's list)

// Offset_0x030B3A: a character in the box of `range` about it: returns it, and (side) whether it is on the right of the Crawl
static Sst *Crawl_Near(const Sst *self, int16_t range, bool *right) {
    FOR_EACH_CHARACTER(chr, who) {
        const uint16_t dx = (uint16_t)(self->x_pos.f.u - chr->x_pos.f.u + range);
        const uint16_t dy = (uint16_t)(self->y_pos.f.u - chr->y_pos.f.u + range);
        if (dx < (uint16_t)(range * 2) && dy < (uint16_t)(range * 2)) {
            *right = (uint16_t)self->x_pos.f.u < (uint16_t)chr->x_pos.f.u;
            return chr;
        }
    }
    return NULL;
}

void Obj_CNZCrawl(Sst *self) {
    bool right = false;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_CNZCrawl;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_CNZCrawl);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0xD7;
        return;

    case 2:
        if (Crawl_Near(self, 0x60, &right) != NULL) {
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, right);
            self->routine += 2;
            self->mapping_frame = 1;
        }
        SstOnScreenTest(self);
        return;

    default: // 4
        if (Crawl_Near(self, 0x60, &right) == NULL) {
            self->routine -= 2;
            self->mapping_frame = 0;
        } else if (self->collision_property) {
            const uint8_t touched = self->collision_property;
            self->collision_property = 0;
            if (touched & 1)
                Bumper_Throw(self, player);
            if ((touched & 2) && TAILS_OBJ->code != NULL)
                Bumper_Throw(self, TAILS_OBJ);
            self->anim = 1;
            QueueSound2(sfx_Bumper);
            if (self->respawn && (*SstLoad_State(self)) < 0x8A)
                (*SstLoad_State(self))++;
        }
        SstOnScreenTest(self);
        return;
    }
}
