#define SwingingPlatform_Build
#include "SwingingPlatform.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
void Obj_SwingingPlatform_Move(Sst *self);
void Obj_SwingingPlatform_Move2(Sst *self, int16_t sin, int16_t cos);

static void Obj_SwingingPlatform_ChkDel(Sst *self) {
    Own_Swing *own = (Own_Swing *)self->own.u8;

    if (SstOutOfRange(own->orig_x)) {
        uint8_t count = own->child_count;
        for (int i = 0; i <= count; i++) {
            Sst *child = sst + own->child_idx[i];
            if (child != self) {
                Sst_Delete(child);
            }
        }
        SstLoad_Forget(self);
        Sst_Delete(self);
    }
}

void Obj_SwingingPlatform(Sst *self) {
    Own_Swing *own = (Own_Swing *)self->own.u8;

    switch (self->routine) {
        case 0: { // Swing_Main
            self->routine += 2;
            self->mappings = Mappings_GHZSwing;
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_MZ_Swing);
            self->render_flags = SST_RENDER_LEVEL;
            self->priority = 3 * 0x80;
            self->width_pixels = 0x18;
            self->y_radius = 8;
            own->orig_y = self->y_pos.f.u;
            own->orig_x = self->x_pos.f.u;

            if (LEVEL_ZONE(level_id) == ZoneId_SLZ) {
                self->mappings = Mappings_SLZSwing;
                self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Swing);
                self->width_pixels = 0x20;
                self->y_radius = 0x10;
                self->collision_flags = 0x99;
            }

            if (LEVEL_ZONE(level_id) == ZoneId_SBZ) {
                self->mappings = Mappings_BigSpikedBall;
                self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Swing); // (Art_SYZSpike1, reused for SBZ)
                self->width_pixels = 0x18;
                self->y_radius = 0x18;
                self->collision_flags = 0x86;
                self->routine = 12; // Swing_Action
            }

            // Chain Creation
            // The level-placed subtype byte (obSubtype: its own field in the
            // status table; the original reuses that byte as the link count).
            uint8_t subtype = self->subtype;
            uint8_t count = subtype & 0xF;
            uint8_t d3 = (count << 4) + 8;
            own->chain_length = d3;
            d3 -= 8;

            if (self->mapping_frame != 0) {
                d3 += 8;
                count--;
            }

            own->child_count = 0;
            for (int i = count; i >= 0; i--) {
                Sst *link = Sst_Allocate();
                if (!link) break;

                // Store link index in parent's scratch
                own->child_idx[own->child_count++] = (uint8_t)(link - sst);

                link->routine = 10; // Swing_Display
                link->code = self->code;
                link->mappings = self->mappings;
                link->art_tile = self->art_tile & ~(1 << 14); // bclr #6 on the high byte -> bit 14 of the full word (palette line 3 -> 1)
                link->render_flags = SST_RENDER_LEVEL;
                link->priority = 4 * 0x80;
                link->width_pixels = 8;
                link->mapping_frame = 1;

                Own_Swing *link_scratch = (Own_Swing *)link->own.u8;
                link_scratch->chain_length = d3;

                d3 -= 0x10;
                if ((int8_t)d3 < 0) {
                    link->mapping_frame = 2;
                    link->priority = 3 * 0x80;
                    link->art_tile |= (1 << 14); // bset #6 on the high byte -> bit 14 (palette line 1 -> 3)
                }
            }

            // Set parent as the last entry in the list
            own->child_idx[own->child_count] = (uint8_t)(self - sst);
            // Note: the original also sets obAngle and GBall_Swing_Speed
            // here, but both are explicitly commented as "GHZ wrecking ball
            // boss only" -- that's separate, later work (the boss isn't
            // ported yet), and obAngle can't be safely translated here
            // anyway (the original writes it as a word, spilling into an
            // adjacent unnamed byte -- a boss-only trick, not something to
            // replicate for the general swinging-platform case).

            if (subtype & 0x10) { // is object type $1X? (unused in real levels)
                self->mappings = Mappings_GHZBall;
                self->art_tile = TILE_MAP(0, 2, 0, 0, (ArtTile_GHZ_Giant_Ball));
                self->mapping_frame = 1;
                self->priority = 2 * 0x80;
                self->collision_flags = 0x81;
            }

            if (LEVEL_ZONE(level_id) == ZoneId_SBZ) break;
            __attribute__((fallthrough));
        }

        case 2: { // Swing_SetSolid / Swing_Platform
            SstPlatformObject_CustomHeight(self, self->width_pixels, self->y_radius);
            Obj_SwingingPlatform_Move(self);
            Sst_Draw(self);
            Obj_SwingingPlatform_ChkDel(self);
            break;
        }

        case 4: { // Swing_Action2 (Sonic on top)
            int16_t x_off;
            if (!SstExitPlatform(self, self->width_pixels, self->width_pixels, &x_off)) {
                int16_t prev_x = self->x_pos.f.u;
                Obj_SwingingPlatform_Move(self);
                // Move Sonic with platform -- y is the pre-computed top
                // surface (post-move platform Y minus custom height + 1px)
                SstMvSonicOnPtfm(self, self->y_pos.f.u - (self->y_radius + 1), prev_x);
            }
            Sst_Draw(self);
            Obj_SwingingPlatform_ChkDel(self);
            break;
        }

        case 6: // Swing_Delete
        case 8:
            Sst_Delete(self);
            break;

        case 10: // Swing_Display (Chain links)
            Sst_Draw(self);
            break;

        case 12: // Swing_Action
            Obj_SwingingPlatform_Move(self);
            Sst_Draw(self);
            Obj_SwingingPlatform_ChkDel(self);
            break;
    }
}

void Obj_SwingingPlatform_Move(Sst *self) {
    // GHZ/MZ/SLZ logic using oscillation table (entry 6: frequency 8, middle
    // value $40). The value field is a 16-bit fixed-point accumulator; only
    // its high byte (the integer part) is meaningful here.
    int16_t angle = (uint8_t)(oscillatory.state[6][0] >> 8);
    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        angle = -angle + 0x80;
    }

    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    Obj_SwingingPlatform_Move2(self, sin, cos);
}

void Obj_SwingingPlatform_Move2(Sst *self, int16_t sin, int16_t cos) {
    Own_Swing *own = (Own_Swing *)self->own.u8;

    // Update all children (links and the platform itself)
    for (int i = 0; i <= own->child_count; i++) {
        Sst *child = sst + own->child_idx[i];
        Own_Swing *child_scratch = (Own_Swing *)child->own.u8;

        int32_t length = child_scratch->chain_length;

        // ASM: muls.w d0,d4 -> asr.l #8,d4
        child->y_pos.f.u = own->orig_y + ((length * sin) >> 8);
        child->x_pos.f.u = own->orig_x + ((length * cos) >> 8);
    }
}
