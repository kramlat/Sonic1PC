#define SwingingPlatform_Build
#include "SwingingPlatform.h"
#include "Level.h"
#include "MathUtil.h"
#include "Macros.h"
#include "GameState.h"

#include "Resource/Mappings/SwingGHZ.h"
#include "Resource/Mappings/SwingSLZ.h"
#include "Resource/Mappings/BallSBZ.h"
#include "Resource/Mappings/BallGHZ.h"

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
            obj->mappings = Mappings_SwingGHZ;
            obj->tile = TILE_MAP(0, 2, 0, 0, 0x380); // ArtTile_GHZ_MZ_Swing
            obj->render.f.align_fg = true;
            obj->priority = 3;
            obj->width_pixels = 0x18;
            obj->height_pixels = 8;
            scratch->orig_y = obj->pos.l.y.f.u;
            scratch->orig_x = obj->pos.l.x.f.u;

            if (v_zone == ZoneID_SLZ) {
                obj->mappings = Mappings_SwingSLZ;
                obj->tile = TILE_MAP(0, 2, 0, 0, 0x300); // ArtTile_SLZ_Swing
                obj->width_pixels = 0x20;
                obj->height_pixels = 0x10;
                obj->col_type = 0x99;
            }

            if (v_zone == ZoneID_SBZ) {
                obj->mappings = Mappings_BallSBZ;
                obj->tile = TILE_MAP(0, 0, 0, 0, 0x300); // ArtTile_SBZ_Swing
                obj->width_pixels = 0x18;
                obj->height_pixels = 0x18;
                obj->col_type = 0x86;
                obj->routine = 12; // Swing_Action
            }

            // Chain Creation
            uint8_t subtype = obj->type; // Original subtype
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
                link->tile = obj->tile & ~(1 << 11); // bclr #6 in Gfx (Priority bit)
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
                    link->tile |= (1 << 11); // bset #6
                }
            }

            // Set parent as the last entry in the list
            scratch->child_idx[scratch->child_count] = (uint8_t)(obj - objects);
            obj->angle = 0x4080;
            scratch->speed = -0x200;

            if (subtype & 0x10) { // is object type $1X?
                obj->mappings = Mappings_BallGHZ;
                obj->tile = TILE_MAP(0, 2, 0, 0, 0x396); // ArtTile_GHZ_Giant_Ball
                obj->frame = 1;
                obj->priority = 2;
                obj->col_type = 0x81;
            }

            if (v_zone == ZoneID_SBZ) break;
            // Fallthrough to Swing_SetSolid
        }

        case 2: { // Swing_SetSolid
            Platform3(obj, obj->width_pixels);
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
                // Move Sonic with platform
                MvSonicOnPtfm(obj, prev_x, obj->height_pixels + 1);
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
    // GHZ/MZ/SLZ logic using oscillation table
    int16_t angle = (int16_t)v_oscillate[0x1A];
    if (obj->status.f.x_flip) {
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
