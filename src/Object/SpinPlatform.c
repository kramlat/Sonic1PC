#include "SpinPlatform.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Resource/Animation/SpinPlatform.h"
#include "Resource/Mappings/SpinningPlatforms.h"
#include "Resource/Mappings/Trapdoor.h"
#include "Sound.h"

// Object 69 - stationary spinning platforms and trapdoors (SBZ)

static void Spin_SolidAndDisplay(Object *obj, int16_t width_half, int16_t height_half) {
    if (obj->frame == 0) { // fully closed / not spinning
        int16_t x_rad = (int16_t)(width_half + 11 /* sonic_solid_width */);
        SolidObject(obj, (uint16_t)x_rad, height_half, (uint16_t)(height_half + 1), obj->pos.l.x.f.u, NULL, NULL);
        RememberState(obj);
        return;
    }

    if (obj->status.o.f.player_stand) {
        player->status.p.f.object_stand = false;
        obj->status.o.f.player_stand = false;
    }
    RememberState(obj);
}

static void Spin_Trapdoor(Object *obj, Scratch_SpinPlatform *scratch) {
    if (--scratch->timer < 0) {
        scratch->timer = scratch->timelen;
        obj->anim ^= 1; // toggle between open/close trapdoor animations
        if (obj->render.f.on_screen)
            QueueSound2(sfx_Door);
    }

    AnimateSprite(obj, Animation_SpinPlatform); // stays on last trapdoor frame indefinitely
    Spin_SolidAndDisplay(obj, 128 / 2, 24 / 2);
}

static void Spin_Spinner(Object *obj, Scratch_SpinPlatform *scratch) {
    if (((uint16_t)frame_count & scratch->syncmask) == 0)
        scratch->spinning = 1;

    if (scratch->spinning) {
        if (--scratch->timer < 0) {
            scratch->timer = scratch->timelen;
            scratch->spinning = 0;
            obj->anim ^= 1; // toggle between the (otherwise identical) spin animations
        }
    }

    AnimateSprite(obj, Animation_SpinPlatform); // stays on last frame (ID 0) indefinitely
    Spin_SolidAndDisplay(obj, 32 / 2, 14 / 2);
}

void Obj_SpinPlatform(Object *obj) {
    Scratch_SpinPlatform *scratch = (Scratch_SpinPlatform *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine = 2; // advance to Spin_Trapdoor
        obj->mappings = Mappings_Trapdoor;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x492); // ArtTile_SBZ_Trap_Door | Tile_Pal3
        obj->render.f.align_fg = true;
        obj->width_pixels = 128 / 2; // FixBugs: real hardware used 256/2 here, causing screen-wrap issues

        uint8_t subtype = obj->scratch.u8[0];
        scratch->timelen = (int16_t)((subtype & 0x0F) * 60);

        if (subtype & 0x80) { // spinning platform, not a trapdoor
            obj->routine = 4; // advance to Spin_Spinner
            obj->mappings = Mappings_SpinningPlatforms;
            obj->tile = TILE_MAP(0, 0, 0, 0, 0x4DF); // ArtTile_SBZ_Spinning_Platform
            obj->width_pixels = 32 / 2;
            obj->anim = 2;

            int16_t t = (int16_t)((subtype & 0x0F) * 6);
            scratch->timer = t;
            scratch->timelen = t;
            scratch->syncmask = (uint16_t)((((subtype & 0x70) + 0x10) << 2) - 1);

            Spin_Spinner(obj, scratch);
            return;
        }
    }

    if (obj->routine == 4)
        Spin_Spinner(obj, scratch);
    else
        Spin_Trapdoor(obj, scratch);
}
