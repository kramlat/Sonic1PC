#include "GeyserMaker.h"

#include "LavaGeyser.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Mappings_LavaGeyser and Animation_LavaGeyser are owned by LavaGeyser.c
// (its own includes directly bring in the actual array definitions) -- a
// bare extern here avoids a second definition and a link failure.
extern const uint8_t Mappings_LavaGeyser[];
extern const uint8_t Animation_LavaGeyser[];

// Object 4C - lava geyser / lavafall producer (MZ). Invisible until Sonic
// walks underneath (subtype 0, geyser) or on a fixed timer regardless
// (subtype 1, lavafall), at which point it spawns a LavaGeyser (object
// 4D) and bubbles until that finishes. Subtype-0 makers are only ever
// spawned programmatically by PushBlock (to launch the block into the
// air) -- never placed directly in a level layout.

static void GMake_Display(Object *obj) {
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    AnimateSprite(obj, Animation_LavaGeyser);
    DisplaySprite(obj);
}

void Obj_GeyserMaker(Object *obj) {
    Scratch_GeyserMaker *scratch = (Scratch_GeyserMaker *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_LavaGeyser;
        obj->tile = TILE_MAP(1, 3, 0, 0, 0x3A8); // ArtTile_MZ_Lava | Tile_Pal4 | Tile_Prio
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 1;
        obj->width_pixels = 112 / 2;
        scratch->time = 120;
        __attribute__((fallthrough));
    case 2: // Wait
        if (--scratch->timer < 0) {
            scratch->timer = scratch->time;

            int16_t sonic_y = player->pos.l.y.f.u;
            int16_t maker_y = obj->pos.l.y.f.u;
            if (sonic_y < maker_y && sonic_y >= (int16_t)(maker_y - 0x170))
                obj->routine += 2;
        }
        if (IS_OFFSCREEN(obj->pos.l.x.f.u))
            ObjectDelete(obj);
        break;
    case 4: // ChkType
        if (obj->scratch.u8[0] == 0) { // geyser -- fall straight into Display this same frame
            GMake_Display(obj);
            break;
        }
        obj->routine += 2;
        if (IS_OFFSCREEN(obj->pos.l.x.f.u))
            ObjectDelete(obj);
        break;
    case 6: { // MakeLava
        obj->routine += 2;

        Object *geyser = FindNextFreeObj(obj);
        if (geyser != NULL) {
            geyser->type = ObjId_LavaGeyser;
            geyser->pos.l.x.f.u = obj->pos.l.x.f.u;
            geyser->pos.l.y.f.u = obj->pos.l.y.f.u;
            geyser->scratch.u8[0] = obj->scratch.u8[0];
            ((Scratch_LavaGeyser *)&geyser->scratch)->parent_index = (uint8_t)(obj - objects);
        }

        obj->anim = 1; // ".bubble2"
        if (obj->scratch.u8[0] != 0) {
            obj->anim = 4; // ".blank" for lavafall
        } else {
            // Shoot the parent pushable block up into the air.
            Object *parent = &objects[scratch->parent_index];
            parent->status.b |= 0x02; // bit1 -- PushBlock's own "shot up by geyser" flag, see PushBlock.c
            parent->ysp = (int16_t)-0x580;
        }
        GMake_Display(obj);
        break;
    }
    case 8: // Display
        GMake_Display(obj);
        break;
    case 0xA: // Delete
        obj->anim = 0; // ".bubble1"
        obj->routine = 2;
        if (obj->scratch.u8[0] == 0) {
            ObjectDelete(obj); // geyser makers are one-shot
            return;
        }
        if (IS_OFFSCREEN(obj->pos.l.x.f.u))
            ObjectDelete(obj); // lavafall makers stay alive indefinitely otherwise
        break;
    }
}
