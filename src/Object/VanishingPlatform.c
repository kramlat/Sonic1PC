#include "VanishingPlatform.h"

#include "Level.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 6C - vanishing platforms (SBZ)

static void VanP_Animate(Object *obj) {
    AnimateSprite(obj, Animation_VanishingPlatforms);
    RememberState(obj);
}

// Shared body of VanP_Detect (routine 2) and VanP_StoodOn (routine 4)
static void VanP_DetectOrStoodOn(Object *obj) {
    Scratch_VanishPlatform *scratch = (Scratch_VanishPlatform *)&obj->scratch;

    if (--scratch->timer < 0) {
        scratch->timer = 0x80 - 1; // reset timer (while vanishing, anim == 0)

        if (obj->anim != 0) { // currently appearing -- use the visible-duration timer instead
            scratch->timer = scratch->timelen;
        }

        obj->anim ^= 1;
    }

    AnimateSprite(obj, Animation_VanishingPlatforms);

    if (obj->frame & 2) {
        // Not visible -- can't be solid
        if (obj->status.o.f.player_stand) {
            player->status.p.f.object_stand = false;
            obj->status.o.f.player_stand = false;
            obj->routine = 2; // VanP_Detect
        }
        RememberState(obj);
        return;
    }

    if (obj->routine != 2) {
        // Sonic is already standing on the platform
        int16_t prev_x = obj->pos.l.x.f.u;
        ExitPlatform(obj, obj->width_pixels, obj->width_pixels, NULL);
        MvSonicOnPtfm(obj, obj->pos.l.y.f.u - 9, prev_x);
        RememberState(obj);
        return;
    }

    PlatformObject(obj, obj->width_pixels);
    RememberState(obj);
}

void Obj_VanishPlatform(Object *obj) {
    Scratch_VanishPlatform *scratch = (Scratch_VanishPlatform *)&obj->scratch;

    switch (obj->routine) {
        case 0: { // VanP_Main
            obj->routine += 6; // VanP_Sync
            obj->mappings = Mappings_VanishingPlatforms;
            obj->tile = TILE_MAP(0, 2, 0, 0, 0x4C3); // ArtTile_SBZ_Vanishing_Block | Tile_Pal3
            obj->render.f.align_fg = true;
            obj->width_pixels = 32 / 2;
            obj->priority = 4;

            uint8_t subtype = scratch->subtype;
            int16_t d0 = ((subtype & 0xF) + 1) << 7;
            int16_t d1 = d0;
            d0 -= 1;
            scratch->timer = d0;
            scratch->timelen = d0;

            uint16_t d0b = (subtype & 0xF0);
            d1 += 0x80;
            d0b = ((uint32_t)d0b * (uint32_t)d1) >> 8;
            scratch->syncoffset = d0b;
            d1 -= 1;
            scratch->syncmask = d1;
            break;
        }

        case 2: // VanP_Detect
        case 4: // VanP_StoodOn
            VanP_DetectOrStoodOn(obj);
            break;

        case 6: { // VanP_Sync
            uint16_t d0 = frame_count - scratch->syncoffset;
            d0 &= scratch->syncmask;
            if (d0 == 0) {
                obj->routine = 2; // VanP_Detect
                VanP_DetectOrStoodOn(obj);
                break;
            }
            VanP_Animate(obj);
            break;
        }
    }
}
