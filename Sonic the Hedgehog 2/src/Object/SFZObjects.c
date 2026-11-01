// Sky Fortress's objects for Sonic 2: the pre-beta's own ($B0 and up), see SFZObjects.h
#include "Object/SFZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Macros.h"
#include "Game.h"
#include "Level.h"
#include "Object/CharControl.h"
#include "Object/Tails.h"

#include "Resource/Animation/SFZTornado.h"
#include "Resource/Mappings/SFZConveyorWheels.h"
#include "Resource/Mappings/SFZClucker.h"
#include "Resource/Mappings/SFZElevator.h"
#include "Resource/Mappings/SFZSpeedBooster.h"
#include "Resource/Mappings/SFZVerticalHelix.h"
#include "Resource/Mappings/SFZLaser.h"
#include "Resource/Mappings/SFZBossSwitch.h"
#include "Resource/Animation/SFZVerticalHelix.h"
#include "Resource/Animation/SFZHorizontalHelix.h"
#include "Resource/Animation/SFZPlatformHelix.h"
#include "Resource/Mappings/SFZHorizontalHelix.h"
#include "Resource/Mappings/SFZCannon.h"
#include "Resource/Mappings/SFZClouds.h"
#include "Resource/Mappings/SFZChainHook.h"
#include "Resource/Mappings/SFZRocketBoost.h"
#include "Resource/Mappings/SFZEnginePart.h"
#include "Resource/Mappings/SFZHatchway.h"
#include "Resource/Mappings/SFZRotatingPlatforms.h"
#include "Resource/Animation/SFZEnginePart.h"
#include "Resource/Animation/SFZHatchway.h"
#include "Resource/Mappings/SFZBreakableWall.h"
#include "Resource/Animation/SFZBreakableWall.h"
#include "Resource/Animation/SFZCannonShot.h"
#include "Resource/Animation/SFZCluckerShot.h"
#include "Object/Coconuts.h"
#include "Sound.h"
#include "SoundIds.h"
#include "Resource/Mappings/SFZPlatformHelix.h"
#include "Resource/Mappings/SFZLargeLaser.h"
#include "Oscillatory Routines.h"
#include "LevelData.h"
#include "Resource/Mappings/SFZTornado.h"
#include "Object/Explosion.h"
#include "MathUtil.h"
#include "Solid.h"
#include "Object/Sonic.h"

// ---------------------------------------------------------------------------------------------------------------------------------------
// $BA, the wheels of the conveyor belts (Obj_0xBA_Conveyor_Belt_Wheels): Object_Settings' entry (its mappings, art at $3EA on palette line 3 in front, render 4, priority 4, width $10, no collision) and then
// only stays, as long as its place is near
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZConveyorWheels(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZConveyorWheels;
        self->art_tile = TILE_MAP(1, 2, 0, 0, 0x3EA);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        self->routine += 2;
        return;
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $D9, the place to hang from (Obj_0xD9_SFz_Hang_Attributes): $18 wide (as the level places it), seen by nobody. For each character, who is not holding: after the wait he is in the box (x within $18 either way,
// y from the object's down to $F) and in the air (not held by something else, not hurt, not in debug mode) he stops, goes to the bar's height, hangs (animation $14) and is held (object control 1). Holding: a jump
// lets go with a throw up of $300, and the next hold is $12 frames off, $3C when a direction was held with it.
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;      // 0x28
    uint8_t pad0[3];
    uint8_t holding[2];   // 0x30 (var_04 and var_05)
    uint8_t wait[2];      // 0x32 (the two bytes the pre-beta uses of the art word of its own scratch memory)
} Own_SFZHang;

static void Hang_Character(Sst *self, Own_SFZHang *own, int who, Sst *chr, uint8_t pressed, uint8_t held) {
    if (own->holding[who]) {
        if (!(pressed & (JPAD_A | JPAD_B | JPAD_C)))
            return;
        OBJ_CONTROL(chr) = 0;
        own->holding[who] = 0;
        own->wait[who] = 0x12;
        if (held & (JPAD_UP | JPAD_DOWN | JPAD_LEFT | JPAD_RIGHT))
            own->wait[who] = 0x3C;
        chr->y_vel = (int16_t)0xFD00;
        return;
    }
    if (own->wait[who] != 0 && --own->wait[who] != 0)
        return;
    if ((uint16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x18) >= 0x30)
        return;
    if ((uint16_t)(chr->y_pos.f.u - self->y_pos.f.u) >= 0x10)
        return;
    if ((int8_t)OBJ_CONTROL(chr) < 0 || chr->routine >= 6 || debug_use)
        return;
    chr->x_vel = 0;
    chr->y_vel = 0;
    chr->ground_vel = 0;
    chr->y_pos.f.u = self->y_pos.f.u;
    chr->anim = 0x14;
    OBJ_CONTROL(chr) = 1;
    own->holding[who] = 1;
}

void Obj_SFZHang(Sst *self) {
    Own_SFZHang *own = (Own_SFZHang *)self->own.u8;
    if (self->routine == 0) {
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x18;
        self->priority = 4 * 0x80;
        self->routine += 2;
    }
    Hang_Character(self, own, 0, player, jpad1_press2, jpad1_hold2);
    if (TAILS_OBJ->code != NULL)
        Hang_Character(self, own, 1, TAILS_OBJ, jpad2_press, jpad2_hold);
    SstDeleteIfNotInRange(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B2, the Tornado (Obj_0xB2_Tornado)
// ---------------------------------------------------------------------------------------------------------------------------------------
int16_t tornado_scroll_x, tornado_scroll_y;

typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0;        // 0x29
    int16_t timer;       // 0x2A
    uint8_t changed;     // 0x2E (var_02): the first character's standing on it has changed this frame
    uint8_t dipping;     // 0x2F (var_03): it dips under him as he lands on the plane while he is on something
    uint8_t dipping2;    // 0x30 (var_04): and when he is on nothing
    uint8_t dip_count;   // 0x31 (var_05)
    int16_t step;        // 0x32 (var_06): frames of a state
    int16_t table;       // 0x34 (var_08): where it is in the table of speeds
    uint8_t pilot_time;  // 0x37 (var_0B)
    uint8_t pilot_index; // 0x36 (var_0A)
    int16_t offset;      // 0x38 (var_0C): where it is from Sonic, in x
} Own_SFZTornado;

enum { TornadoRoutine_Init = 0, TornadoRoutine_Steered = 2, TornadoRoutine_Flying = 4, TornadoRoutine_Waiting = 6 };

static void Tornado_Scroll(Sst *self) { // (Offset_0x030C20: the level scrolls by itself, and it goes with it)
    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + tornado_scroll_x);
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u + tornado_scroll_y);
}

// Tails, the pilot, from his own art: one of his sitting frames every 8 frames, in the pre-beta's order
static void Tornado_Pilot(Own_SFZTornado *own) {
    static const uint8_t frames[0x18] = { 0x10, 0x10, 0x10, 0x10, 0x01, 0x02, 0x03, 0x02, 0x01, 0x01, 0x10, 0x10, 0x10, 0x10, 0x01, 0x02, 0x03, 0x02, 0x01, 0x01, 0x04, 0x04, 0x01, 0x01 };
    if ((int8_t)--own->pilot_time >= 0)
        return;
    own->pilot_time = 8;
    uint8_t index = (uint8_t)(own->pilot_index + 1);
    if (index >= 0x18)
        index = 0;
    own->pilot_index = index;
    Tails_LoadPilotFrame(frames[index]);
}

// Sonic's side against the plane (the characters are solid: $1B wide, 8 and 9 high)
static void Tornado_Solid(Sst *self, int16_t old_x) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x1B, 8, 9, old_x, NULL);
    }
}

static void Tornado_Move(Sst *self) { // (Offset_0x034EAA: it moves, goes with the scroll, and is solid)
    const int16_t old_x = self->x_pos.f.u;
    Sst_MoveSprite2(self);
    Tornado_Scroll(self);
    Tornado_Solid(self, old_x);
}

// Sonic is taken along: when he is more than $10 from its middle he is put $10 from it (Offset_0x03500A)
static void Tornado_Carry(Sst *self) {
    const int16_t difference = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    if ((uint16_t)(difference + 0x10) < 0x20)
        return;
    const int16_t push = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? -0x10 : 0x10;
    player->x_pos.f.u = (int16_t)(self->x_pos.f.u - push);
}

// It does not go above or below the picture (Offset_0x03502A)
static void Tornado_KeepInView(Sst *self) {
    if (self->y_vel == 0)
        return;
    if (self->y_vel > 0) {
        if ((uint16_t)self->y_pos.f.u < (uint16_t)(scrpos_y.f.u + 0xA8))
            return;
    } else if ((uint16_t)self->y_pos.f.u >= (uint16_t)(scrpos_y.f.u + 0x34)) {
        return;
    }
    self->y_vel = 0;
}

static int16_t Tornado_Slow(int16_t speed) {
    return speed > (int16_t)0xFF00 ? (int16_t)(speed - 0x20) : speed;
}

// (Offset_0x034F14: Sonic is standing on something)
static void Tornado_StandingDip(Sst *self, Own_SFZTornado *own) {
    if (!own->dipping) {
        if (!own->changed)
            return;
        own->dipping = 0xFF;
        own->dipping2 = 0;
        self->y_vel = 0x200;
        own->dip_count = 0x14;
    }
    if ((int8_t)--own->dip_count < 0) {
        own->dipping = 0;
        self->y_vel = 0;
        return;
    }
    self->y_vel = Tornado_Slow(self->y_vel);
}

// (Offset_0x034F58, after the solid test: the plane follows what Sonic does to it)
static void Tornado_Follow(Sst *self, Own_SFZTornado *own) {
    if (player->status & SST_PSTATUS_OBJECT_STAND) {
        if (!own->dipping) {
            self->y_vel = 0;
            const uint8_t held = (uint8_t)(jpad1_hold2 & (JPAD_UP | JPAD_DOWN));
            if (held != 0) {
                self->y_vel = (held & JPAD_DOWN) ? 0x80 : -0x80;
                Tornado_KeepInView(self);
                Sst_MoveSprite2(self);
            }
        }
        const int16_t difference = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
        if ((uint16_t)(difference + 0x10) < 0x20)
            return;
        int16_t inertia = player->ground_vel;
        if (inertia < 0)
            inertia = (int16_t)-inertia;
        if (inertia >= 0x900)
            return;
        const int16_t push = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? -0x10 : 0x10;
        self->x_pos.f.u = (int16_t)(player->x_pos.f.u + push);
        return;
    }
    if (!own->dipping2) {
        if (!own->changed)
            return;
        own->dipping2 = 0xFF;
        own->dipping = 0;
        self->y_vel = 0x200;
        own->dip_count = 0x2B;
    }
    if ((int8_t)--own->dip_count < 0) {
        own->dipping2 = 0;
        self->y_vel = 0;
        return;
    }
    self->y_vel = Tornado_Slow(self->y_vel);
    Tornado_KeepInView(self);
    Sst_MoveSprite2(self);
}

// (Offset_0x034EC8: where it is while Sonic steers it)
static void Tornado_Steered(Sst *self, Own_SFZTornado *own) {
    if (player->status & SST_PSTATUS_OBJECT_STAND) {
        Tornado_StandingDip(self, own);
        Tornado_KeepInView(self);
        Sst_MoveSprite2(self);
        Tornado_Scroll(self);
        return;
    }
    if (own->changed)
        own->offset = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    int16_t offset = own->offset;
    if (offset != 0)
        offset = (int16_t)(offset + (offset < 0 ? 1 : -1));
    own->offset = offset;
    self->x_pos.f.u = (int16_t)(player->x_pos.f.u + offset);
    Tornado_Scroll(self);
}

void Obj_SFZTornado(Sst *self) {
    Own_SFZTornado *own = (Own_SFZTornado *)self->own.u8;

    Tornado_Pilot(own);
    if (self->routine == TornadoRoutine_Init) {
        self->mappings = Mappings_SFZTornado;
        self->art_tile = TILE_MAP(1, 0, 0, 0, 0x500);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x40;
        self->collision_flags = 0;
        self->routine = (uint8_t)(self->subtype - 0x4E);
        Sst_Draw(self);
        return;
    }
    switch (self->routine) {
    case TornadoRoutine_Steered: {
        if (!debug_use) {
            const int16_t old_x = self->x_pos.f.u;
            self->art_tile = (uint16_t)((self->art_tile & 0x7FFF) | (player->art_tile & 0x8000));
            Tornado_Steered(self, own);
            own->changed = self->status;
            Tornado_Solid(self, old_x);
            Tornado_Follow(self, own);
            own->changed = (uint8_t)(((own->changed & 8) ^ (self->status & 8)) != 0);
            limit_left2 = scrpos_x.f.u;
            limit_right2 = (uint16_t)(scrpos_x.f.u - 0x40);
        }
        Sst_Animate(self, Animation_SFZTornado);
        Sst_Draw(self);
        return;
    }
    case TornadoRoutine_Flying:
        switch (self->routine_secondary) {
        case 0:
            self->routine_secondary += 2;
            own->step = 0xC0;
            self->x_vel = 0x100;
            break;
        case 2:
            if (--own->step < 0) {
                self->routine_secondary += 2;
                own->step = 1;
                self->x_vel = 0x100;
                self->y_vel = 0x100;
            } else {
                Tornado_Move(self);
                Tornado_Carry(self);
            }
            break;
        case 4:
            if (--own->step == 0) {
                own->step = 0x0E;
                Sst *smoke = Sst_AllocateAfter(self);
                if (smoke != NULL) {
                    smoke->code = Obj_SFZTornadoSmoke;
                    smoke->subtype = 0x88;
                    smoke->x_pos = self->x_pos;
                    smoke->y_pos = self->y_pos;
                }
            }
            Tornado_Move(self);
            break;
        }
        Sst_Animate(self, Animation_SFZTornado);
        if (SstOutOfRange(self->x_pos.f.u))
            Sst_Delete(self);
        else
            Sst_Draw(self);
        return;
    case TornadoRoutine_Waiting:
        switch (self->routine_secondary) {
        case 0:
            if ((uint16_t)player->x_pos.f.u < (uint16_t)(self->x_pos.f.u - 0x10) || (uint16_t)player->x_pos.f.u >= (uint16_t)(self->x_pos.f.u + 0x10)
                || (uint16_t)player->y_pos.f.u < (uint16_t)(self->y_pos.f.u - 0x80) || (uint16_t)player->y_pos.f.u >= (uint16_t)(self->y_pos.f.u + 0x80))
                return;
            self->routine_secondary += 2;
            self->x_vel = 0x100;
            self->y_vel = (int16_t)0xFF00;
            self->x_pos.f.u = (int16_t)(self->x_pos.f.u + 0x20);
            self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x80);
            own->timer = 0;
            return;
        case 2: {
            // The speeds it changes to as the frames go by (the pre-beta's table: its first entry is $FFFF, so that the plane keeps its first speeds for good)
            static const uint16_t when[8] = { 0xFFFF, 0x01A0, 0x0208, 0x0285, 0x0300, 0x0340, 0x0390, 0x0440 };
            static const uint8_t speeds[9][2] = { { 0xF0, 0x00 }, { 0x01, 0x01 }, { 0x00, 0x01 }, { 0x01, 0x01 }, { 0x00, 0x01 }, { 0x00, 0x01 }, { 0x01, 0x00 }, { 0x00, 0x01 }, { 0x00, 0x01 } };
            own->timer++;
            if (own->table < 16 && (uint16_t)own->timer >= when[own->table / 2]) {
                own->table = (int16_t)(own->table + 2);
                self->x_vel = (int16_t)((speeds[own->table / 2][0] << 8) | (self->x_vel & 0xFF));
                self->y_vel = (int16_t)((speeds[own->table / 2][1] << 8) | (self->y_vel & 0xFF));
            }
            Tornado_Move(self);
            Tornado_Carry(self);
            Sst_Animate(self, Animation_SFZTornado);
            SstOnScreenTest(self);
            return;
        }
        }
        return;
    }
}

// The smoke (Obj_0xC4_SFz_Tornado_Smoke): the explosion's puff (its mappings and its art), placed a little behind the plane, going up and back, five frames of 8
void Obj_SFZTornadoSmoke(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Explosion_Mappings();
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x5A0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 0x0C;
        self->routine += 2;
        self->anim_frame_timer = 7;
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u - (int16_t)(RandomNumber() & 0x1C));
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x10);
        self->y_vel = (int16_t)0xFF00;
        self->x_vel = (int16_t)0xFF00;
        return;
    }
    Sst_MoveSprite2(self);
    if ((int8_t)--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 7;
        if (++self->mapping_frame == 5) {
            Sst_Delete(self);
            return;
        }
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $8B, the switch of the palette cycle (Obj_0x8B_Palette_Cycling_Switch)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[5];
    int16_t reach;    // 0x2E (var_06)
    uint8_t right[2]; // 0x30 (var_08 and var_09): the character is to the right of the line
} Own_SFZSwitch;

static void PaletteSwitch_Character(Sst *self, Own_SFZSwitch *own, int who, const Sst *chr) {
    const int16_t line = self->x_pos.f.u;
    const bool flipped = (self->render_flags & SST_RENDER_X_FLIP) != 0;
    if (!own->right[who]) {
        if ((uint16_t)line > (uint16_t)chr->x_pos.f.u)
            return; // (still on the left)
        own->right[who] = 1;
        if ((uint16_t)chr->y_pos.f.u < (uint16_t)(self->y_pos.f.u - own->reach) || (uint16_t)chr->y_pos.f.u >= (uint16_t)(self->y_pos.f.u + own->reach))
            return;
        sfz_cycle_second = flipped ? 0 : 1;
    } else {
        if ((uint16_t)line <= (uint16_t)chr->x_pos.f.u)
            return; // (still on the right)
        own->right[who] = 0;
        if ((uint16_t)chr->y_pos.f.u < (uint16_t)(self->y_pos.f.u - own->reach) || (uint16_t)chr->y_pos.f.u >= (uint16_t)(self->y_pos.f.u + own->reach))
            return;
        sfz_cycle_second = flipped ? 1 : 0;
    }
}

void Obj_SFZPaletteSwitch(Sst *self) {
    static const int16_t reaches[4] = { 0x20, 0x40, 0x80, 0x100 };
    Own_SFZSwitch *own = (Own_SFZSwitch *)self->own.u8;
    if (self->routine == 0) {
        self->routine += 2;
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 5 * 0x80;
        own->reach = reaches[self->subtype & 3];
        own->right[0] = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u;
        own->right[1] = TAILS_OBJ->code != NULL && (uint16_t)self->x_pos.f.u < (uint16_t)TAILS_OBJ->x_pos.f.u;
    }
    if (!debug_use) {
        PaletteSwitch_Character(self, own, 0, player);
        if (TAILS_OBJ->code != NULL)
            PaletteSwitch_Character(self, own, 1, TAILS_OBJ);
    }
    SstDeleteIfNotInRange(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $AD, the Clucker's platform (Obj_0xAD_Clucker_Platform): Object_Settings' entry (the Clucker's mappings, art at $379, render 4, priority 4, width $18), frame $C, and solid ($1B wide, 8 and 8 high)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZCluckerPlatform(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZClucker;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x379);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x18;
        self->collision_flags = 0;
        self->mapping_frame = 0x0C;
        self->routine += 2;
        return;
    }
    const int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x1B, 8, 8, x, NULL);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $BD, the elevators of Sky Fortress (Obj_0xBD_SFz_Elevators): the placed object (routine 0 and 2) makes one every $40 frames (the first at once); each opens (frames 2, 1, 0, four frames each), is a platform ($23 wide) for
// $C8 frames moving by its subtype ($76 up, $78 down: $100 a frame), and closes (frames 0, 1, 2, two frames each) and goes
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0;
    int16_t timer;    // 0x2A
    uint8_t frame;    // 0x2C: where it is in the opening or closing
    uint8_t beat;     // 0x2D: frames to the next one of them
} Own_SFZElevator;

enum { ElevatorRoutine_Init = 0, ElevatorRoutine_Maker = 2, ElevatorRoutine_Elevator = 4 };
enum { ElevatorState_Init = 0, ElevatorState_Open = 2, ElevatorState_Carry = 4, ElevatorState_Close = 6, ElevatorState_Gone = 8 };

// The animation of its scripts: `delay`+1 frames of each of the three, and then (the pre-beta's $FA) the next state
static bool Elevator_Animate(Sst *self, Own_SFZElevator *own, bool opening) {
    if ((int8_t)--own->beat >= 0)
        return false;
    own->beat = opening ? 3 : 1;
    if (own->frame >= 3) {
        self->routine_secondary += 2;
        return true;
    }
    self->mapping_frame = opening ? (uint8_t)(2 - own->frame) : own->frame;
    own->frame++;
    return false;
}

void Obj_SFZElevator(Sst *self) {
    Own_SFZElevator *own = (Own_SFZElevator *)self->own.u8;

    switch (self->routine) {
    case ElevatorRoutine_Init:
        self->routine += 2;
        own->timer = 1;
        return;
    case ElevatorRoutine_Maker:
        if (--own->timer == 0) {
            own->timer = 0x40;
            Sst *lift = Sst_AllocateAfter(self);
            if (lift != NULL) {
                lift->code = Obj_SFZElevator;
                lift->x_pos = self->x_pos;
                lift->y_pos = self->y_pos;
                lift->routine = ElevatorRoutine_Elevator;
                lift->subtype = self->subtype;
            }
        }
        SstDeleteIfNotInRange(self);
        return;
    }
    switch (self->routine_secondary) {
    case ElevatorState_Init:
        self->mappings = Mappings_SFZElevator;
        self->art_tile = TILE_MAP(1, 3, 0, 0, 0x416);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x18;
        self->collision_flags = 0;
        self->routine_secondary += 2;
        own->timer = 0xC7;
        self->y_vel = (self->subtype == 0x76) ? (int16_t)0xFF00 : 0x0100;
        own->frame = 0;
        own->beat = 0;
        break;
    case ElevatorState_Open:
        Elevator_Animate(self, own, true);
        break;
    case ElevatorState_Carry:
        if (--own->timer < 0) {
            self->routine_secondary += 2;
            own->frame = 0;
            own->beat = 0;
        } else {
            const int16_t old_x = self->x_pos.f.u;
            Sst_MoveSprite2(self);
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
                if (chr != NULL)
                    Solid_Platform(self, chr, who, 0x23, 5, old_x);
            }
        }
        break;
    case ElevatorState_Close:
        Elevator_Animate(self, own, false);
        break;
    case ElevatorState_Gone:
        Sst_Delete(self);
        return;
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $C0, the speed booster (Obj_0xC0_Speed_Booster): Object_Settings' entry $7E (its mappings, art at $464, render 4, priority 4, width $10) and a platform ($10 wide, $11 high). The place it goes to (var_06) is its
// own, a number of 16 pixels (the subtype) to the left, or to the right if flipped
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[5];
    int16_t accel;    // 0x2C (var_04): $80 a frame, as it goes
    int16_t target;   // 0x2E (var_06)
} Own_SFZBooster;

enum { BoosterState_Wait = 0, BoosterState_Go = 2, BoosterState_Done = 4 };

// Whoever is on the platform is put on its middle with no speed of his own, facing the way it goes
static void Booster_Attach(Sst *self, Sst *chr) {
    chr->ground_vel = 0;
    chr->x_vel = 0;
    chr->x_pos.f.u = self->x_pos.f.u;
    chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
    if (!(self->status & SST_STATUS_X_FLIP))
        chr->status |= SST_PSTATUS_X_FLIP;
}

static void Booster_Throw(Sst *self, Sst *chr) {
    chr->x_vel = self->x_vel;
    chr->y_vel = (int16_t)0xFC00;
    chr->status |= SST_PSTATUS_IN_AIR;
}

static void Booster_Riders(Sst *self, void (*each)(Sst *, Sst *)) {
    if ((self->status & 0x08) != 0)
        each(self, player);
    if ((self->status & 0x10) != 0 && TAILS_OBJ->code != NULL)
        each(self, TAILS_OBJ);
}

void Obj_SFZSpeedBooster(Sst *self) {
    Own_SFZBooster *own = (Own_SFZBooster *)self->own.u8;
    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_SFZSpeedBooster;
        self->art_tile = TILE_MAP(1, 1, 0, 0, 0x464);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        int16_t reach = (int16_t)(self->subtype << 4);
        if (!(self->status & SST_STATUS_X_FLIP))
            reach = (int16_t)-reach;
        own->target = (int16_t)(reach + self->x_pos.f.u);
    }
    switch (self->routine_secondary) {
    case BoosterState_Wait:
        if ((self->status & 0x18) != 0) {
            self->routine_secondary += 2;
            self->x_vel = 0x0C00;
            own->accel = 0x80;
            if (!(self->status & SST_STATUS_X_FLIP)) {
                self->x_vel = (int16_t)-self->x_vel;
                own->accel = (int16_t)-own->accel;
                Sst_MoveSprite2(self);
            }
            Booster_Riders(self, Booster_Attach);
        }
        break;
    case BoosterState_Go:
        self->x_vel = (int16_t)(self->x_vel + own->accel);
        Sst_MoveSprite2(self);
        if ((uint16_t)own->target > (uint16_t)self->x_pos.f.u) { // it is there
            self->routine_secondary += 2;
            self->x_pos.f.u = own->target;
            Booster_Riders(self, Booster_Throw);
        } else {
            Booster_Riders(self, Booster_Attach);
        }
        break;
    }
    const int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Platform(self, chr, who, 0x10, 0x11, x);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B4, the vertical helix (Obj_0xB4_Vertical_Helix): Object_Settings' entry (its mappings, art at $561, render 4, priority 4, width 4, collision $A8: it hurts) and its three frames turning
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZVerticalHelix(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZVerticalHelix;
        self->art_tile = TILE_MAP(1, 1, 0, 0, 0x561);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 4;
        self->collision_flags = 0xA8;
        self->routine += 2;
        return;
    }
    Sst_Animate(self, Animation_SFZVerticalHelix);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B9, the laser (Obj_0xB9_Laser): Object_Settings' entry (its mappings, art at $3C3, render 4, priority 4, width $60); when it has been on the screen once it goes left at 16 a frame, and it is gone when it is $40
// behind the screen's left edge
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZLaser(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZLaser;
        self->art_tile = TILE_MAP(1, 2, 0, 0, 0x3C3);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x60;
        self->collision_flags = 0;
        self->routine += 2;
        return;
    }
    if (self->routine == 2) {
        if (self->render_flags & SST_RENDER_ON_SCREEN) {
            self->routine += 2;
            self->x_vel = (int16_t)0xF000;
        }
    } else {
        Sst_MoveSprite2(self);
    }
    if ((int16_t)self->x_pos.f.u < (int16_t)(scrpos_x.f.u - 0x40)) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $C2, the switch that lets Sonic into the boss's plane (Obj_0xC2_SFz_Boss_Access_Switch): Object_Settings' entry (its mappings, art at $469, render 4, priority 4, width $10) and solid ($1B wide, 8 and 9 high).
// Landed on rolling it is the explosion, Sonic is in the air again, and the chunks of the level (the second row of the fortress's plane: row 8 and 9, from the $50th chunk) are put over
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZBossSwitch(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZBossSwitch;
        self->art_tile = TILE_MAP(1, 1, 0, 0, 0x469);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        self->routine += 2;
        return;
    }
    const uint8_t rolling = player->anim;
    const int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x1B, 8, 9, x, NULL);
    }
    if (!(self->status & 0x08)) {
        SstOnScreenTest(self);
        return;
    }
    self->status &= (uint8_t)~0x08;
    if (rolling != 2) {
        SstOnScreenTest(self);
        return;
    }
    self->code = Obj_Explosion;
    self->routine = 2;
    player->status |= SST_PSTATUS_IN_AIR;
    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    static const uint8_t above[6] = { 0x8A, 0x70, 0x71, 0x72, 0x73, 0x74 }, below[6] = { 0x6E, 0x78, 0x79, 0x78, 0x78, 0x7A };
    for (int i = 0; i < 6; i++) {
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x50 + i, 8, above[i]);
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x50 + i, 9, below[i]);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B5, the horizontal helix (Obj_0xB5_Horizontal_Helix): Object_Settings' entry (its mappings, art at $3CD, render 4, priority 4, width $40); its time starts at a random one under $80, and from there it
// spins at its full speed (animation 4) for $100 frames (until its time is out, and a character over it is caught) and slows (animation 5) for $100, and speeds up again (animation 0)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0;
    int16_t timer;    // 0x2A
    int16_t start;    // 0x2C (var_00)
} Own_SFZHelix;

extern Oscillatory oscillatory;

static void Helix_RandomWait(Sst *self, Own_SFZHelix *own) { // (Offset_0x0356C0)
    own->timer = (int16_t)(RandomNumber() & 0x7F);
    own->start = own->timer;
    (void)self;
}

// A character over the helix at its full speed is put on it and thrown into a spin (Offset_0x0353C2)
static void Helix_Catch(Sst *self, Sst *chr) {
    if ((uint16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x40) >= 0x80)
        return;
    const uint16_t sum = (uint16_t)((uint8_t)(oscillatory.state[5][0] >> 8) + chr->y_pos.f.u + 0x60);
    if (sum < (uint16_t)self->y_pos.f.u)
        return;
    int16_t d1 = (int16_t)(sum - (uint16_t)self->y_pos.f.u);
    if ((uint16_t)d1 >= 0x90)
        return;
    d1 = (int16_t)(d1 - 0x60);
    if (d1 >= 0) { // (no borrow: at or below $60 under it: the step is twice what is left of the $60)
        d1 = (int16_t)~d1;
        d1 = (int16_t)(d1 + d1);
    }
    d1 = (int16_t)(d1 + 0x60);
    d1 = (int16_t)(-d1 >> 4);
    chr->y_pos.f.u = (int16_t)(chr->y_pos.f.u + d1);
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->y_vel = 0;
    chr->ground_vel = 1;
    Scratch_Sonic *sc = (Scratch_Sonic *)chr->own.u8;
    if (sc->flip_angle == 0) {
        sc->flip_angle = 1;
        chr->anim = 0x0F;
        sc->flips_remaining = 0x7F;
        sc->flip_speed = 8;
    }
}

static void Helix_CatchAll(Sst *self) {
    if (self->anim != 4)
        return;
    Helix_Catch(self, player);
    if (TAILS_OBJ->code != NULL)
        Helix_Catch(self, TAILS_OBJ);
}

void Obj_SFZHorizontalHelix(Sst *self) {
    Own_SFZHelix *own = (Own_SFZHelix *)self->own.u8;
    if (self->routine == 0) {
        self->mappings = Mappings_SFZHorizontalHelix;
        self->art_tile = TILE_MAP(1, 1, 0, 0, 0x3CD);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x40;
        self->collision_flags = 0;
        Helix_RandomWait(self, own);
        self->anim = 4;
        self->routine = (uint8_t)(self->subtype - 0x5C);
        return;
    }
    if (self->routine == 2) {
        if (self->routine_secondary == 0) {
            Helix_CatchAll(self);
            if (--own->timer < 0) {
                self->routine_secondary += 2;
                self->anim = 5;
                own->timer = 0x100;
            }
        } else {
            Helix_CatchAll(self);
            if (--own->timer < 0) {
                self->routine_secondary -= 2;
                self->anim = 0;
                own->timer = 0x100;
            }
        }
    }
    Sst_Animate(self, Animation_SFZHorizontalHelix);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B7, the large vertical laser (Obj_0xB7_Vertical_Large_Laser): Object_Settings' entry (its mappings, art at $39F, render 4, priority 4, width $18, collision $A9: it hurts); it is there $20 frames and shows every
// other one
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZLargeLaser(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZLargeLaser;
        self->art_tile = TILE_MAP(1, 2, 0, 0, 0x39F);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x18;
        self->collision_flags = 0xA9;
        self->routine += 2;
        self->own.u8[0x2A - 0x28] = 0x20;
        return;
    }
    if (--self->own.u8[0x2A - 0x28] == 0) {
        Sst_Delete(self);
        return;
    }
    self->own.u8[0x2B - 0x28] ^= 1;
    if (self->own.u8[0x2B - 0x28] & 1)
        SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B6, the helix with a platform (Obj_0xB6_Platform_Helix): Object_Settings' entry (its mappings, art at $393, render 4, priority 4, width $10), and four kinds by the subtype less $60: routine 2 a platform ($23
// wide) that is there for a random time under $80 and then folds away (animations 0 to 2) and comes back after $C0 frames, 4 one that is a platform for $20 frames and then fires a laser ($B7), 6 one that folds away
// when it is stood on, and 8 a small one ($F wide and $18 high) that is there for a random time. What is on it when it folds is let go
// ---------------------------------------------------------------------------------------------------------------------------------------
static void PlatformHelix_Solid(Sst *self, int16_t x_rad, int16_t y_air, int16_t y_walk) {
    const int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, x_rad, y_air, y_walk, x, NULL);
    }
}

static void PlatformHelix_Release(Sst *self) { // (Offset_0x035704)
    if ((self->status & 0x18) == 0)
        return;
    if (self->status & 0x08) {
        self->status &= (uint8_t)~0x08;
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        player->status |= SST_PSTATUS_IN_AIR;
    }
    if (self->status & 0x10) {
        self->status &= (uint8_t)~0x10;
        if (TAILS_OBJ->code != NULL) {
            TAILS_OBJ->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            TAILS_OBJ->status |= SST_PSTATUS_IN_AIR;
        }
    }
}

static void PlatformHelix_Laser(Sst *self) { // (Offset_0x035740)
    Sst *laser = Sst_AllocateAfter(self);
    if (laser == NULL)
        return;
    laser->code = Obj_SFZLargeLaser;
    laser->subtype = 0x6A;
    laser->x_pos = self->x_pos;
    laser->y_pos = self->y_pos;
}

void Obj_SFZPlatformHelix(Sst *self) {
    Own_SFZHelix *own = (Own_SFZHelix *)self->own.u8;
    if (self->routine == 0) {
        self->mappings = Mappings_SFZPlatformHelix;
        self->art_tile = TILE_MAP(1, 1, 0, 0, 0x393);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        self->routine = (uint8_t)(self->subtype - 0x60);
        return;
    }
    switch (self->routine) {
    case 2:
        switch (self->routine_secondary) {
        case 0:
            self->routine_secondary += 2;
            Helix_RandomWait(self, own);
            break;
        case 2:
            PlatformHelix_Solid(self, 0x23, 4, 4);
            if (--own->timer < 0) {
                self->routine_secondary += 2;
                self->anim = 0;
                PlatformHelix_Release(self);
            }
            break;
        case 4:
            Sst_Animate(self, Animation_SFZPlatformHelix);
            break;
        case 6:
            self->routine_secondary = 2;
            own->timer = 0xC0;
            break;
        }
        break;
    case 4:
        switch (self->routine_secondary) {
        case 0:
            self->routine_secondary += 2;
            Helix_RandomWait(self, own);
            break;
        case 2:
            PlatformHelix_Solid(self, 0x23, 4, 4);
            if (--own->timer < 0) {
                self->routine_secondary += 2;
                own->timer = 0x20;
                self->anim = 3;
                self->anim_frame = 0;
                self->anim_frame_timer = 0;
                PlatformHelix_Release(self);
                PlatformHelix_Laser(self);
            }
            break;
        case 4:
            if ((int8_t)--own->timer < 0) {
                self->routine_secondary = 2;
                self->mapping_frame = 0;
                own->timer = 0xC0;
            } else {
                Sst_Animate(self, Animation_SFZPlatformHelix);
            }
            break;
        }
        break;
    case 6:
        switch (self->routine_secondary) {
        case 0:
            PlatformHelix_Solid(self, 0x23, 4, 4);
            if (self->status & 0x18) {
                self->routine_secondary += 2;
                own->timer = 0x10;
            }
            break;
        case 2:
            PlatformHelix_Solid(self, 0x23, 4, 4);
            if (--own->timer < 0) {
                self->routine_secondary += 2;
                self->anim = 0;
                self->status &= (uint8_t)~SST_STATUS_X_FLIP;
                if ((uint16_t)self->x_pos.f.u >= (uint16_t)player->x_pos.f.u)
                    self->status |= SST_STATUS_X_FLIP;
                PlatformHelix_Release(self);
            }
            break;
        case 4:
            Sst_Animate(self, Animation_SFZPlatformHelix);
            break;
        case 6:
            self->routine_secondary = 0;
            break;
        }
        break;
    case 8:
        switch (self->routine_secondary) {
        case 0:
            self->routine_secondary += 2;
            self->mapping_frame = 2;
            Helix_RandomWait(self, own);
            break;
        case 2:
            PlatformHelix_Solid(self, 0x0F, 0x18, 0x18);
            if (--own->timer < 0) {
                self->routine_secondary += 2;
                self->anim = 4;
                PlatformHelix_Release(self);
            }
            break;
        case 4:
            Sst_Animate(self, Animation_SFZPlatformHelix);
            break;
        case 6:
            self->routine_secondary = 2;
            own->timer = 0xC0;
            break;
        }
        break;
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $B8, the cannon (Obj_0xB8_Cannon): Object_Settings' entry (the cannon's mappings, art at $3AB, render 4, priority 4, width $10, collision $86). When it has been on the screen and a character is below it and not more
// than $5F to either side it goes on (it gets its time, 2); it turns to the character (frame 0 within $20 of straight under it, 1 if he is to its left, 2 to its right) and, every $40 frames, fires: the shot
// (object $98, the cannon's own: frame 3 and 4 turning) leaves from the cannon's mouth, a pixel a frame the way the cannon points and one down
// ---------------------------------------------------------------------------------------------------------------------------------------
const uint8_t *SFZ_CannonMappings(void) {
    return Mappings_SFZCannon;
}

const uint8_t *SFZ_CluckerMappings(void) {
    return Mappings_SFZClucker;
}

const uint8_t *SFZ_CannonShotAnimation(void) {
    return Animation_SFZCannonShot;
}

const uint8_t *SFZ_CluckerShotAnimation(void) {
    return Animation_SFZCluckerShot;
}

enum { CannonRoutine_Init = 0, CannonRoutine_Wait = 2, CannonRoutine_Aim = 4 };

static void Cannon_Fire(Sst *self, Own_SFZHelix *own) {
    static const int8_t shots[3][4] = { { 0x00, 0x18, 0x00, 0x01 }, { (int8_t)0xEF, 0x10, (int8_t)0xFF, 0x01 }, { 0x11, 0x10, 0x01, 0x01 } };
    (void)own;
    Sst *shot = Sst_AllocateAfter(self);
    if (shot == NULL)
        return;
    shot->code = Obj_EnemyWeapon;
    ((Own_Weapon *)shot->own.u8)->weapon = Weapon_CannonShot;
    shot->mapping_frame = 3;
    const int8_t *s = shots[self->mapping_frame % 3];
    shot->x_pos.f.u = (int16_t)(self->x_pos.f.u + s[0]);
    shot->y_pos.f.u = (int16_t)(self->y_pos.f.u + s[1]);
    shot->x_vel = (int16_t)(s[2] * 256);
    shot->y_vel = (int16_t)(s[3] * 256);
}

void Obj_SFZCannon(Sst *self) {
    Own_SFZHelix *own = (Own_SFZHelix *)self->own.u8;
    switch (self->routine) {
    case CannonRoutine_Init:
        self->mappings = Mappings_SFZCannon;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x3AB);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x86;
        self->routine += 2;
        return;
    case CannonRoutine_Wait: {
        const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
        if ((self->render_flags & SST_RENDER_ON_SCREEN) && (uint16_t)self->y_pos.f.u < (uint16_t)player->y_pos.f.u && (uint16_t)(dx + 0x60) < 0xC0) {
            self->routine += 2;
            own->timer = 2;
        }
        SstOnScreenTest(self);
        return;
    }
    case CannonRoutine_Aim: {
        const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
        uint8_t frame = 0;
        if ((uint16_t)(dx + 0x20) >= 0x40)
            frame = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? 2 : 1;
        self->mapping_frame = frame;
        if (--own->timer == 0) {
            own->timer = 0x40;
            Cannon_Fire(self, own);
        }
        SstOnScreenTest(self);
        return;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// $C1, the breakable wall (Obj_0xC1_SFz_Breakable_Wall): Object_Settings' entry $80 (its mappings, art at $48C, render 4, priority 4, width $40, collision $E1: the special kind that counts the touches). Its time (var_04)
// is the subtype times $3C frames. Touched on its left, by a character who is to its right of $14 before it, he is put there ($14 off, facing left, still, in the hang pose and held) and the wall is held (var_06). While it is
// held Up and Down move him by one a frame between $18 above its middle and $18 below; when its time is out, or he jumps, he is let go and it breaks into four pieces (sound $CB), each of which waits a time of its own and goes
// left with $400, falling a little more each frame
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[3];
    int16_t time;     // 0x2C (var_04)
    uint8_t held;     // 0x2E (var_06)
    uint8_t wait;     // 0x3F (var_13): a piece's wait
} Own_SFZWall;

enum { WallRoutine_Init = 0, WallRoutine_Hold = 2, WallRoutine_Piece = 4 };

static void Wall_Break(Sst *self) {
    static const int16_t offsets[4][2] = { { -0x10, -0x10 }, { -0x10, 0x10 }, { -0x30, -0x10 }, { -0x30, 0x10 } };
    static const uint8_t waits[4] = { 0x00, 0x04, 0x18, 0x20 };
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    for (int i = 0; i < 4; i++) {
        Sst *piece = i == 0 ? self : Sst_AllocateAfter(self);
        if (piece == NULL)
            break;
        piece->code = Obj_SFZBreakableWall;
        piece->routine = WallRoutine_Piece;
        piece->mappings = Mappings_SFZBreakableWall;
        piece->art_tile = self->art_tile;
        piece->render_flags = (uint8_t)(SST_RENDER_LEVEL | SST_RENDER_ON_SCREEN); // ($84: it counts as seen until it is drawn)
        piece->x_pos.f.u = (int16_t)(x + offsets[i][0]);
        piece->y_pos.f.u = (int16_t)(y + offsets[i][1]);
        piece->priority = (uint16_t)(3 * 0x80);
        piece->width_pixels = 0x10;
        piece->mapping_frame = 1;
        piece->x_vel = (int16_t)0xFC00;
        piece->y_vel = 0;
        piece->collision_flags = 0;
        ((Own_SFZWall *)piece->own.u8)->wait = waits[i];
    }
    PlaySound(sfx_WallSmash);
}

void Obj_SFZBreakableWall(Sst *self) {
    Own_SFZWall *own = (Own_SFZWall *)self->own.u8;
    switch (self->routine) {
    case WallRoutine_Init:
        self->mappings = Mappings_SFZBreakableWall;
        self->art_tile = TILE_MAP(1, 3, 0, 0, 0x48C);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x40;
        self->collision_flags = 0xE1;
        own->time = (int16_t)(self->subtype * 0x3C);
        self->routine += 2;
        // Fallthrough
    case WallRoutine_Hold:
        if (!own->held) {
            if (self->collision_property != 0) {
                const int16_t edge = (int16_t)(self->x_pos.f.u - 0x14);
                if ((uint16_t)edge < (uint16_t)player->x_pos.f.u) {
                    self->collision_property = 0;
                    if (player->routine < 4) {
                        player->x_vel = 0;
                        player->y_vel = 0;
                        player->x_pos.f.u = edge;
                        player->status |= SST_PSTATUS_X_FLIP;
                        player->anim = 0x11;
                        OBJ_CONTROL(player) = 1;
                        own->held = 1;
                    }
                }
            }
            SstOnScreenTest(self);
            return;
        }
        {
            bool let_go = false;
            if (own->time != 0 && --own->time == 0) {
                let_go = true;
            } else {
                int16_t top = (int16_t)(self->y_pos.f.u - 0x18);
                if (jpad1_hold2 & JPAD_UP) {
                    player->y_pos.f.u--;
                    if ((uint16_t)top >= (uint16_t)player->y_pos.f.u)
                        player->y_pos.f.u = top;
                }
                const int16_t bottom = (int16_t)(top + 0x30);
                if (jpad1_hold2 & JPAD_DOWN) {
                    player->y_pos.f.u++;
                    if ((uint16_t)bottom < (uint16_t)player->y_pos.f.u)
                        player->y_pos.f.u = bottom;
                }
                if (jpad1_press2 & (JPAD_A | JPAD_B | JPAD_C))
                    let_go = true;
            }
            if (!let_go) {
                SstOnScreenTest(self);
                return;
            }
        }
        self->collision_flags = 0;
        OBJ_CONTROL(player) = 0;
        own->held = 0;
        Wall_Break(self);
        // Fallthrough
    case WallRoutine_Piece:
        if (self->routine != WallRoutine_Piece)
            self->routine = WallRoutine_Piece;
        if (own->wait != 0) {
            own->wait--;
        } else {
            Sst_MoveSprite2(self);
            self->y_vel = (int16_t)(self->y_vel + 8);
            Sst_Animate(self, Animation_SFZBreakableWall);
        }
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) { // (gone when it has left the picture)
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The pre-beta's other objects of Sky Fortress (placed by no layout of it)
// ---------------------------------------------------------------------------------------------------------------------------------------

// $B3, the clouds (Obj_0xB3_Clouds): Object_Settings' entry (their mappings, art at $54F, palette line 3, render 4, priority 6, width $30); the subtype less $56 is the cloud (frame 0, 1 or 2, going left at $80, $40 or $20
// of 256ths of a pixel a frame), and each also goes with the level's own scroll; gone when it is behind the picture
void Obj_SFZClouds(Sst *self) {
    if (self->routine == 0) {
        static const int16_t speeds[3] = { (int16_t)0xFF80, (int16_t)0xFFC0, (int16_t)0xFFE0 };
        const int index = ((self->subtype - 0x56) >> 1) % 3;
        self->mappings = Mappings_SFZClouds;
        self->art_tile = TILE_MAP(0, 2, 0, 0, 0x54F);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 6 * 0x80;
        self->width_pixels = 0x30;
        self->collision_flags = 0;
        self->x_vel = speeds[index];
        self->mapping_frame = (uint8_t)index;
        self->routine += 2;
        return;
    }
    Sst_MoveSprite2(self);
    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + tornado_scroll_x);
    SstDeleteIfNotInRange(self);
    if (self->code != NULL)
        Sst_Draw(self);
}

// $BB, the chain hook (Obj_0xBB): Object_Settings' entry (its mappings, art at $3FA on palette line 2, render 4, priority 4, width $C, collision $09) and it stays
void Obj_SFZChainHook(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZChainHook;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x3FA);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x0C;
        self->collision_flags = 0x09;
        self->routine += 2;
        return;
    }
    SstOnScreenTest(self);
}

// $BC, the rocket boost (Obj_0xBC_Rocket_Boost): Object_Settings' entry (its mappings, art at $40E in front on palette line 3, render 4, priority 4, width $10) and it shows every other frame
void Obj_SFZRocketBoost(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZRocketBoost;
        self->art_tile = TILE_MAP(1, 2, 0, 0, 0x40E);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        self->routine += 2;
        return;
    }
    self->own.u8[0x2A - 0x28] ^= 1;
    if (self->own.u8[0x2A - 0x28] & 1)
        SstOnScreenTest(self);
}

// $BF, the engine part (Obj_0xBF_Engine_Part): Object_Settings' entry (its mappings, art at $458 on palette line 4 in front, render 4, priority 4, width 4) and its three frames, two ticks each
void Obj_SFZEnginePart(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZEnginePart;
        self->art_tile = TILE_MAP(1, 3, 0, 0, 0x458);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 4;
        self->collision_flags = 0;
        self->routine += 2;
        return;
    }
    Sst_Animate(self, Animation_SFZEnginePart);
    SstOnScreenTest(self);
}

// $BE, the hatchways (Obj_0xBE_Hatchways): Object_Settings' entry (its mappings, art at $422 on palette line 4, render 4, priority 4, width $18) and a random wait under $80 frames; then it opens and shuts (the
// script says when: the routine counter goes on by 2 at its end), and while its frame is 3 or 4 (open) it is a platform ($23 wide, $19 high); shut, what stands on it is let go; and then $C0 frames to the next time
void Obj_SFZHatchway(Sst *self) {
    Own_SFZHelix *own = (Own_SFZHelix *)self->own.u8;
    switch (self->routine) {
    case 0:
        self->mappings = Mappings_SFZHatchway;
        self->art_tile = TILE_MAP(1, 3, 0, 0, 0x422);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x18;
        self->collision_flags = 0;
        Helix_RandomWait(self, own);
        self->routine += 2;
        return;
    case 2:
        if (--own->timer < 0) {
            self->routine += 2;
            self->anim = 0;
        }
        break;
    case 4:
        if (self->mapping_frame == 3 || self->mapping_frame == 4) {
            const int16_t x = self->x_pos.f.u;
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
                if (chr != NULL)
                    Solid_Platform(self, chr, who, 0x23, 0x19, x);
            }
        } else {
            PlatformHelix_Release(self);
        }
        Sst_Animate(self, Animation_SFZHatchway);
        break;
    case 6:
        self->routine = 2;
        own->timer = 0xC0;
        break;
    }
    SstOnScreenTest(self);
}

// $C3, the rotating platforms (Obj_0xC3_SFz_Rotating_Platforms): Object_Settings' entry (its mappings, art at $46D on palette line 2, render 4, priority 4, width $20, collision $2A) and it stays
void Obj_SFZRotatingPlatforms(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_SFZRotatingPlatforms;
        self->art_tile = TILE_MAP(1, 1, 0, 0, 0x46D);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x20;
        self->collision_flags = 0x2A;
        self->routine += 2;
        return;
    }
    SstOnScreenTest(self);
}
