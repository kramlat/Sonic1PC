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
