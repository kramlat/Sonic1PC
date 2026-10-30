#include "BigSpikeBall.h"
#include "Object.h"
#include "Resource/Mappings/BigSpikedBall.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Macros.h"

// Internal movement types
static void Obj_BigSpikeBall_Type01(Sst *self) {
    Own_BigSpikeBall *own = (Own_BigSpikeBall *)self->own.u8;

   // (v_oscillate+$E).w maps to state[3][0]; only the high byte (the
   // integer part of the fixed-point value) is meaningful here.
    int16_t dist = (uint8_t)(oscillatory.state[3][0] >> 8);

    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        dist = -dist + 0x60;
    }

    self->x_pos.f.u = own->orig_x - dist;
}

static void Obj_BigSpikeBall_Type02(Sst *self) {
    Own_BigSpikeBall *own = (Own_BigSpikeBall *)self->own.u8;

    // (v_oscillate+$E).w maps to state[3][0]; only the high byte (the
    // integer part of the fixed-point value) is meaningful here.
    int16_t dist = (uint8_t)(oscillatory.state[3][0] >> 8);

    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        dist = -dist + 0x80;
    }

    self->y_pos.f.u = own->orig_y - dist;
}

static void Obj_BigSpikeBall_Type03(Sst *self) {
    Own_BigSpikeBall *own = (Own_BigSpikeBall *)self->own.u8;

    // Real hardware does `add.w speed,obAngle(a0)` -- a 16-bit add into a
    // word straddling obAngle and the following (otherwise-unused) byte,
    // with only the high byte (self->angle) ever read back for CalcSine.
    // That makes the effective turn rate speed/256 per frame rather than
    // the raw speed value.
    uint16_t accum = (uint16_t)((self->angle << 8) | own->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)own->speed);
    self->angle = (uint8_t)(accum >> 8);
    own->angle_frac = (uint8_t)accum;

    // Calculate circular movement
    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);

    // Apply radius (asr.l #8 is equivalent to / 256)
    self->y_pos.f.u = own->orig_y + ((own->radius * sin) >> 8);
    self->x_pos.f.u = own->orig_x + ((own->radius * cos) >> 8);
}

void Obj_BigSpikeBall(Sst *self) {
    Own_BigSpikeBall *own = (Own_BigSpikeBall *)self->own.u8;

    switch (self->routine) {
        case 0: // BBall_Main
            self->routine += 2;
            self->mappings = Mappings_BigSpikedBall;
            // SYZ Spikeball VRAM tile index
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SYZ_Big_Spikeball);
            self->render_flags = SST_RENDER_LEVEL;
            self->priority = 4 * 0x80;
            self->width_pixels = 24;
            self->collision_flags = 0x86;

            own->orig_x = self->x_pos.f.u;
            own->orig_y = self->y_pos.f.u;

            // Set speed from the upper nibble of the level-placed subtype
            // byte (e.g., $20 -> speed $100). This must be treated as a
            // SIGNED byte before widening/scaling: subtypes with the upper
            // nibble >= 8 (e.g. $E0) are meant to produce a negative speed
            // (reverse/counterclockwise rotation), matching the original's
            // ext.w sign-extension -- a plain unsigned shift would instead
            // produce a large wrong positive value for those subtypes.
            own->speed = (int16_t)(int8_t)(self->subtype & 0xF0) * 8;

            // Set starting angle based on flip bits (status bits 0 & 1)
            uint8_t start_angle = self->status;
            self->angle = (start_angle << 6) | (start_angle >> 2);
            self->angle &= 0xC0;

            own->radius = 0x50;
            // Fallthrough

        case 2: // BBall_Move
        {
            uint8_t move_type = self->subtype & 0x07;

            if (move_type == 1)      Obj_BigSpikeBall_Type01(self);
            else if (move_type == 2) Obj_BigSpikeBall_Type02(self);
            else if (move_type == 3) Obj_BigSpikeBall_Type03(self);

            // Visibility check using original X center
            SstOnScreenTest2(self, own->orig_x);
            break;
        }
    }
}
