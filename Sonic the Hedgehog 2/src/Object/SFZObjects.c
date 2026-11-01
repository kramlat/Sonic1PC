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

#include "Resource/Mappings/SFZConveyorWheels.h"

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
