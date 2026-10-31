#include "GiantRing.h"
#include "Resource/Mappings/RingFlash.h"
#include "Resource/Mappings/GiantRing.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "SpecialStage.h"
#include "Sound.h"

// Object 4B - Giant Ring for entry to Special Stage
void Obj_GiantRing(Sst *self) {
    switch (self->routine) {
    case 0: // Main
        self->mappings = Mappings_GiantRing;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Giant_Ring); // | Tile_Pal2 (real hardware's palette lines are 1-indexed -- Tile_Pal2 = 0-indexed line 1)
        // Only OR the camera-relative flag in, like the real object: clearing the
        // whole byte here every frame also wiped the "on screen" bit that the
        // renderer sets, so the on-screen check below could never succeed.
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 128 / 2;

        // The real object only decides whether to show the ring once it is on
        // screen. Until then (it is always off screen on the frame it is
        // loaded) it just runs the Animate step below WITHOUT advancing the
        // routine, so the check is repeated every frame. The port used to
        // advance the routine in the off-screen case, which made the ring
        // appear and become collectable no matter how many rings you had.
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (emeralds == 6) {
                Sst_Delete(self);
                return;
            }
            if (rings < 50)
                return; // not enough rings -- stay invisible, try again next frame

            self->routine += 2; // GRing_Okay
            self->priority = 2 * 0x80;
            self->collision_flags = 0x52; // col_16x32 | col_item
            gfx_big_ring = 0xC40; // triggers AniArt_GiantRing to stream in the ring's own art
        }
        // Fallthrough
    case 2: // Animate
        self->mapping_frame = sprite_anim[1].frame;
        SstOnScreenTest2(self, self->x_pos.f.u);
        break;
    case 4: { // Collect
        self->routine -= 2;
        self->collision_flags = 0x00;

        Sst *flash = Sst_Allocate();
        if (flash != NULL) {
            flash->code = Obj_RingFlash;
            flash->x_pos.f.u = self->x_pos.f.u;
            flash->y_pos.f.u = self->y_pos.f.u;
            ((Own_RingFlash *)flash->own.u8)->parent = self;
            if (player->x_pos.f.u >= self->x_pos.f.u)
                SST_RENDER_SET(flash, SST_RENDER_X_FLIP, true);
        }
        PlaySound(sfx_GiantRing);
        break; // keep animating the ring until the flash deletes it
    }
    case 6: // Delete
        Sst_Delete(self);
        break;
    }
}

// Object 7C - Flash effect when you collect the Giant Ring
void Obj_RingFlash(Sst *self) {
    Own_RingFlash *own = (Own_RingFlash *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_RingFlash;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Giant_Ring_Flash); // | Tile_Pal2
        self->render_flags |= SST_RENDER_LEVEL; // x_flip (if set by GiantRing's Collect case, above) is untouched by this
        self->priority = 0 * 0x80;
        self->width_pixels = 64 / 2;
        self->anim_frame_timer = -1;
        // Fallthrough
    case 2: // ChkDel: advance the flash animation, delete Sonic, and set the "collected" flag
        if (--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 1;
            self->mapping_frame++;
            if (self->mapping_frame >= 8) {
                self->routine += 2;
                // Delete Sonic -- matches the real driver's "move.w #0,
                // (v_player).w", which zeroes the object's first word
                // (type + render) rather than the fixed v_player address
                // itself (player is a fixed slot here too, not reassignable).
                player->code = sst_object_codes[ObjId_Null];
                player->render_flags = 0;
                break;
            }
            if (self->mapping_frame == 3) {
                own->parent->routine = 6; // delete the parent Giant Ring
                big_ring = 1;
                invincibility = 0;
                shield = 0;
            }
        }
        SstOnScreenTest2(self, self->x_pos.f.u);
        break;
    case 4: // Delete
        Sst_Delete(self);
        break;
    }
}
