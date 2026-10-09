// The collapsing platform of Sonic 2 (the prototype's object 1F, "Collapsing_Platforms"): the platform in Dust Hill and Oil Ocean that, a moment after somebody has stood on it, breaks into pieces that fall one after another
// (a piece for each of the frame's sprite pieces, each with its own delay). Like the collapsing ledge (1A, Hidden Palace's), whose pieces it shares, but its top is flat. It carries Sonic and Tails.
// Dust Hill's is 64 pixels wide (a half width of $20) of 6 pieces, Oil Ocean's 128 of 7. (Other zones: the prototype has a plain default with no art here, which no layout places.)
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "Mappings.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/CollapsePlatformDHZ.h"
#include "Resource/Mappings/CollapsePlatformOOZ.h"

typedef struct {
    uint8_t subtype;   // 0x28 (not used)
    uint8_t pad0[0xF]; // 0x29-0x37
    uint8_t timer;     // 0x38: from being touched to collapsing (and, in a piece, to falling)
    uint8_t pad1;      // 0x39
    uint8_t touched;   // 0x3A
} Scratch_CollapsingPlatform;

enum { CollapseRoutine_Init = 0, CollapseRoutine_Main = 2, CollapseRoutine_Piece = 6 };

// When each piece begins to fall (loc_946B for Oil Ocean's 7 pieces, loc_9472 for Dust Hill's 6)
static const uint8_t delays_ooz[7] = { 0x1A, 0x12, 0x0A, 0x02, 0x16, 0x0E, 0x06 };
static const uint8_t delays_dhz[6] = { 0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x02 };

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

static bool IsOilOcean(void) {
    return LEVEL_ZONE(level_id) == ZoneId_OOZ;
}

static void Platform(Object *obj) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr != NULL)
            Solid_Platform(obj, chr, who, obj->width_pixels, 0x10, obj->pos.l.x.f.u);
    }
}

void Obj_CollapsingPlatform(Object *obj) {
    Scratch_CollapsingPlatform *scratch = (Scratch_CollapsingPlatform *)&obj->scratch;

    switch (obj->routine) {
    case CollapseRoutine_Init:
        obj->routine += 2;
        obj->render.b |= 4; // ori.b #4,1(a0): the layout's facing stays
        obj->priority = 4;
        scratch->timer = 7;
        if (IsOilOcean()) {
            obj->mappings = Mappings_CollapsePlatformOOZ;
            obj->tile = TILE_MAP(0, 3, 0, 0, 0x39D);
            obj->width_pixels = 0x40;
        } else {
            obj->mappings = Mappings_CollapsePlatformDHZ;
            obj->tile = TILE_MAP(0, 3, 0, 0, 0x3F4);
            obj->width_pixels = 0x20;
        }
        // Fallthrough
    case CollapseRoutine_Main:
        if (scratch->touched) {
            if (scratch->timer == 0) { // it collapses
                obj->frame += 1;
                FragmentatePlatform(obj, IsOilOcean() ? 7 : 6, IsOilOcean() ? delays_ooz : delays_dhz);
                return;
            }
            scratch->timer--;
        }
        if (obj->status.b & 0x18)
            scratch->touched = 1;
        Platform(obj);
        RememberState(obj);
        break;
    case CollapseRoutine_Piece:
        if (scratch->timer == 0) { // falling
            ObjectFall(obj);
            if (!obj->render.f.on_screen)
                ObjectDelete(obj);
            else
                DisplaySprite(obj);
            break;
        }
        if (!scratch->touched) { // a piece that waits to fall
            scratch->timer--;
            DisplaySprite(obj);
            break;
        }
        // the first piece (the platform itself) is still a platform until it lets go
        Platform(obj);
        RememberState(obj);
        if (--scratch->timer == 0) {
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Object *chr = Character(who);
                if (chr != NULL && chr->status.p.f.object_stand) {
                    chr->status.p.f.object_stand = false;
                    chr->status.p.f.pushing = false;
                    chr->prev_anim = 1;
                }
            }
        }
        break;
    }
}
