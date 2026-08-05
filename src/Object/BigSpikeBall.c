#include "BigSpikeBall.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Macros.h"

// Internal movement types
static void Obj_BigSpikeBall_Type01(Object *obj) {
    Scratch_BigSpikeBall *scratch = (Scratch_BigSpikeBall*)&obj->scratch;

   // (v_oscillate+$E).w maps to state[3][0]; only the high byte (the
   // integer part of the fixed-point value) is meaningful here.
    int16_t dist = (uint8_t)(oscillatory.state[3][0] >> 8);

    if (obj->status.o.f.x_flip) {
        dist = -dist + 0x60;
    }

    obj->pos.l.x.f.u = scratch->orig_x - dist;
}

static void Obj_BigSpikeBall_Type02(Object *obj) {
    Scratch_BigSpikeBall *scratch = (Scratch_BigSpikeBall*)&obj->scratch;

    // (v_oscillate+$E).w maps to state[3][0]; only the high byte (the
    // integer part of the fixed-point value) is meaningful here.
    int16_t dist = (uint8_t)(oscillatory.state[3][0] >> 8);

    if (obj->status.o.f.x_flip) {
        dist = -dist + 0x80;
    }

    obj->pos.l.y.f.u = scratch->orig_y - dist;
}

static void Obj_BigSpikeBall_Type03(Object *obj) {
    Scratch_BigSpikeBall *scratch = (Scratch_BigSpikeBall*)&obj->scratch;

    // Update angle based on speed
    obj->angle += scratch->speed;

    // Calculate circular movement
    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);

    // Apply radius (asr.l #8 is equivalent to / 256)
    obj->pos.l.y.f.u = scratch->orig_y + ((scratch->radius * sin) >> 8);
    obj->pos.l.x.f.u = scratch->orig_x + ((scratch->radius * cos) >> 8);
}

void Obj_BigSpikeBall(Object *obj) {
    Scratch_BigSpikeBall *scratch = (Scratch_BigSpikeBall*)&obj->scratch;

    switch (obj->routine) {
        case 0: // BBall_Main
            obj->routine += 2;
            obj->mappings = Mappings_BigSpikedBall;
            // SYZ Spikeball VRAM tile index
            obj->tile = TILE_MAP(0, 0, 0, 0, 0x396);
            obj->render.f.align_fg = true;
            obj->priority = 4;
            obj->width_pixels = 24;
            obj->col_type = 0x86;

            scratch->orig_x = obj->pos.l.x.f.u;
            scratch->orig_y = obj->pos.l.y.f.u;

            // Set speed from the upper nibble of the level-placed subtype
            // byte (e.g., $20 -> speed $100). This must be treated as a
            // SIGNED byte before widening/scaling: subtypes with the upper
            // nibble >= 8 (e.g. $E0) are meant to produce a negative speed
            // (reverse/counterclockwise rotation), matching the original's
            // ext.w sign-extension -- a plain unsigned shift would instead
            // produce a large wrong positive value for those subtypes.
            scratch->speed = (int16_t)(int8_t)(scratch->subtype & 0xF0) * 8;

            // Set starting angle based on flip bits (status bits 0 & 1)
            uint8_t start_angle = obj->status.b;
            obj->angle = (start_angle << 6) | (start_angle >> 2);
            obj->angle &= 0xC0;

            scratch->radius = 0x50;
            // Fallthrough

        case 2: // BBall_Move
        {
            uint8_t move_type = scratch->subtype & 0x07;

            if (move_type == 1)      Obj_BigSpikeBall_Type01(obj);
            else if (move_type == 2) Obj_BigSpikeBall_Type02(obj);
            else if (move_type == 3) Obj_BigSpikeBall_Type03(obj);

            // Visibility check using original X center
            if (IS_OFFSCREEN(scratch->orig_x)) {
                ObjectDelete(obj);
            } else {
                DisplaySprite(obj);
            }
            break;
        }
    }
}
