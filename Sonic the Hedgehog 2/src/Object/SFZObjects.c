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
