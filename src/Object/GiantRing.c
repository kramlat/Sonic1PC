#include "GiantRing.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "SpecialStage.h"
#include "Sound.h"

// Object 4B - Giant Ring for entry to Special Stage
void Obj_GiantRing(Object *obj) {
    switch (obj->routine) {
    case 0: // Main
        obj->mappings = Mappings_GiantRing;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x400); // ArtTile_Giant_Ring | Tile_Pal2 (real hardware's palette lines are 1-indexed -- Tile_Pal2 = 0-indexed line 1)
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 128 / 2;

        if (obj->render.f.on_screen) {
            // Only offer the ring once you actually have all the
            // ingredients for a Special Stage attempt.
            if (emeralds == 6) {
                ObjectDelete(obj);
                return;
            }
            if (rings < 50)
                return; // not enough rings yet -- stay invisible, try again next frame
        }

        obj->routine += 2;
        obj->priority = 2;
        obj->col_type = 0x52; // col_16x32 | col_item
        gfx_big_ring = 0xC40; // triggers AniArt_GiantRing to stream in the ring's own art
        // Fallthrough
    case 2: // Animate
        obj->frame = sprite_anim[1].frame;
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    case 4: { // Collect
        obj->routine -= 2;
        obj->col_type = 0x00;

        Object *flash = FindFreeObj();
        if (flash != NULL) {
            flash->type = ObjId_RingFlash;
            flash->pos.l.x.f.u = obj->pos.l.x.f.u;
            flash->pos.l.y.f.u = obj->pos.l.y.f.u;
            ((Scratch_RingFlash *)&flash->scratch)->parent = obj;
            if (player->pos.l.x.f.u >= obj->pos.l.x.f.u)
                flash->render.f.x_flip = true;
        }
        PlaySound(sfx_GiantRing);
        break; // keep animating the ring until the flash deletes it
    }
    case 6: // Delete
        ObjectDelete(obj);
        break;
    }
}

// Object 7C - Flash effect when you collect the Giant Ring
void Obj_RingFlash(Object *obj) {
    Scratch_RingFlash *scratch = (Scratch_RingFlash *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_RingFlash;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x462); // ArtTile_Giant_Ring_Flash | Tile_Pal2
        obj->render.f.align_fg = true; // x_flip (if set by GiantRing's Collect case, above) is untouched by this
        obj->priority = 0;
        obj->width_pixels = 64 / 2;
        obj->frame_time.b = -1;
        // Fallthrough
    case 2: // ChkDel: advance the flash animation, delete Sonic, and set the "collected" flag
        if (--obj->frame_time.b < 0) {
            obj->frame_time.b = 1;
            obj->frame++;
            if (obj->frame >= 8) {
                obj->routine += 2;
                // Delete Sonic -- matches the real driver's "move.w #0,
                // (v_player).w", which zeroes the object's first word
                // (type + render) rather than the fixed v_player address
                // itself (player is a fixed slot here too, not reassignable).
                player->type = ObjId_Null;
                player->render.b = 0;
                break;
            }
            if (obj->frame == 3) {
                scratch->parent->routine = 6; // delete the parent Giant Ring
                big_ring_collected = 1;
                invincibility = 0;
                shield = 0;
            }
        }
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    case 4: // Delete
        ObjectDelete(obj);
        break;
    }
}
