#define SwingingPlatform_Build
#include "SwingingPlatform.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Macros.h"
#include "Game.h"
#include "Oscillatory Routines.h"

#include "Resource/Mappings/GHZSwing.h"
#include "Resource/Mappings/SLZSwing.h"
#include "Resource/Mappings/GHZBall.h"
extern const uint8_t Mappings_BigSpikedBall[];

// Forward declarations
void Obj_SwingingPlatform_Move(Object *obj);
void Obj_SwingingPlatform_Move2(Object *obj, int16_t sin, int16_t cos);

static void Obj_SwingingPlatform_ChkDel(Object *obj) {
    Scratch_Swing *scratch = (Scratch_Swing*)&obj->scratch;

    if (IS_OFFSCREEN(scratch->orig_x)) {
        uint8_t count = scratch->child_count;
        for (int i = 0; i <= count; i++) {
            Object *child = objects + scratch->child_idx[i];
            if (child != obj) {
                ObjectDelete(child);
            }
        }
        ObjectDelete(obj);
    }
}

void Obj_SwingingPlatform(Object *obj) {
    Scratch_Swing *scratch = (Scratch_Swing*)&obj->scratch;

    switch (obj->routine) {
        case 0: { // Swing_Main
            obj->routine += 2;
            obj->mappings = Mappings_GHZSwing;
            obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_MZ_Swing);
            obj->render.f.align_fg = true;
            obj->priority = 3;
            obj->width_pixels = 0x18;
            obj->y_rad = 8;
            scratch->orig_y = obj->pos.l.y.f.u;
            scratch->orig_x = obj->pos.l.x.f.u;

            if (LEVEL_ZONE(level_id) == ZoneId_SLZ) {
                obj->mappings = Mappings_SLZSwing;
                obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Swing);
                obj->width_pixels = 0x20;
                obj->y_rad = 0x10;
                obj->col_type = 0x99;
            }

            if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
                obj->mappings = Mappings_BigSpikedBall;
                obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Swing); // (Art_SYZSpike1, reused for SBZ)
                obj->width_pixels = 0x18;
                obj->y_rad = 0x18;
                obj->col_type = 0x86;
                obj->routine = 12; // Swing_Action
            }

            // Chain Creation
            // Read the level-placed subtype byte. It lives at scratch offset
            // 0 (obSubtype), aliased here as child_count -- read it BEFORE
            // clearing it to 0 just below, matching the original's own
            // read-then-reuse-the-same-byte pattern.
            uint8_t subtype = scratch->child_count;
            uint8_t count = subtype & 0xF;
            uint8_t d3 = (count << 4) + 8;
            scratch->chain_length = d3;
            d3 -= 8;

            if (obj->frame != 0) {
                d3 += 8;
                count--;
            }

            scratch->child_count = 0;
            for (int i = count; i >= 0; i--) {
                Object *link = FindFreeObj();
                if (!link) break;

                // Store link index in parent's scratch
                scratch->child_idx[scratch->child_count++] = (uint8_t)(link - objects);

                link->routine = 10; // Swing_Display
                link->type = obj->type;
                link->mappings = obj->mappings;
                link->tile = obj->tile & ~(1 << 14); // bclr #6 on the high byte -> bit 14 of the full word (palette line 3 -> 1)
                link->render.f.align_fg = true;
                link->priority = 4;
                link->width_pixels = 8;
                link->frame = 1;

                Scratch_Swing *link_scratch = (Scratch_Swing*)&link->scratch;
                link_scratch->chain_length = d3;

                d3 -= 0x10;
                if ((int8_t)d3 < 0) {
                    link->frame = 2;
                    link->priority = 3;
                    link->tile |= (1 << 14); // bset #6 on the high byte -> bit 14 (palette line 1 -> 3)
                }
            }

            // Set parent as the last entry in the list
            scratch->child_idx[scratch->child_count] = (uint8_t)(obj - objects);
            // Note: the original also sets obAngle and GBall_Swing_Speed
            // here, but both are explicitly commented as "GHZ wrecking ball
            // boss only" -- that's separate, later work (the boss isn't
            // ported yet), and obAngle can't be safely translated here
            // anyway (the original writes it as a word, spilling into an
            // adjacent unnamed byte -- a boss-only trick, not something to
            // replicate for the general swinging-platform case).

            if (subtype & 0x10) { // is object type $1X? (unused in real levels)
                obj->mappings = Mappings_GHZBall;
                obj->tile = TILE_MAP(0, 2, 0, 0, (ArtTile_GHZ_Giant_Ball));
                obj->frame = 1;
                obj->priority = 2;
                obj->col_type = 0x81;
            }

            if (LEVEL_ZONE(level_id) == ZoneId_SBZ) break;
            __attribute__((fallthrough));
        }

        case 2: { // Swing_SetSolid / Swing_Platform
            PlatformObject_CustomHeight(obj, obj->width_pixels, obj->y_rad);
            Obj_SwingingPlatform_Move(obj);
            DisplaySprite(obj);
            Obj_SwingingPlatform_ChkDel(obj);
            break;
        }

        case 4: { // Swing_Action2 (Sonic on top)
            int16_t x_off;
            if (!ExitPlatform(obj, obj->width_pixels, obj->width_pixels, &x_off)) {
                int16_t prev_x = obj->pos.l.x.f.u;
                Obj_SwingingPlatform_Move(obj);
                // Move Sonic with platform -- y is the pre-computed top
                // surface (post-move platform Y minus custom height + 1px)
                MvSonicOnPtfm(obj, obj->pos.l.y.f.u - (obj->y_rad + 1), prev_x);
            }
            DisplaySprite(obj);
            Obj_SwingingPlatform_ChkDel(obj);
            break;
        }

        case 6: // Swing_Delete
        case 8:
            ObjectDelete(obj);
            break;

        case 10: // Swing_Display (Chain links)
            DisplaySprite(obj);
            break;

        case 12: // Swing_Action
            Obj_SwingingPlatform_Move(obj);
            DisplaySprite(obj);
            Obj_SwingingPlatform_ChkDel(obj);
            break;
    }
}

void Obj_SwingingPlatform_Move(Object *obj) {
    // GHZ/MZ/SLZ logic using oscillation table (entry 6: frequency 8, middle
    // value $40). The value field is a 16-bit fixed-point accumulator; only
    // its high byte (the integer part) is meaningful here.
    int16_t angle = (uint8_t)(oscillatory.state[6][0] >> 8);
    if (obj->status.o.f.x_flip) {
        angle = -angle + 0x80;
    }

    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    Obj_SwingingPlatform_Move2(obj, sin, cos);
}

void Obj_SwingingPlatform_Move2(Object *obj, int16_t sin, int16_t cos) {
    Scratch_Swing *scratch = (Scratch_Swing*)&obj->scratch;

    // Update all children (links and the platform itself)
    for (int i = 0; i <= scratch->child_count; i++) {
        Object *child = objects + scratch->child_idx[i];
        Scratch_Swing *child_scratch = (Scratch_Swing*)&child->scratch;

        int32_t length = child_scratch->chain_length;

        // ASM: muls.w d0,d4 -> asr.l #8,d4
        child->pos.l.y.f.u = scratch->orig_y + ((length * sin) >> 8);
        child->pos.l.x.f.u = scratch->orig_x + ((length * cos) >> 8);
    }
}
