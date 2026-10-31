// The collapsing platform of Sonic 2 (the prototype's object 1F, "Collapsing_Platforms"): the platform in Dust Hill and Oil Ocean that, a moment after somebody has stood on it, breaks into pieces that fall one after another
// (a piece for each of the frame's sprite pieces, each with its own delay). Like the collapsing ledge (1A, Hidden Palace's), whose pieces it shares, but its top is flat. It carries Sonic and Tails.
// Dust Hill's is 64 pixels wide (a half width of $20) of 6 pieces, Oil Ocean's 128 of 7. (Other zones: the prototype has a plain default with no art here, which no layout places.)
#include "Constants.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "Mappings.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/CollapsePlatformDHZ.h"
#include "Resource/Mappings/CollapsePlatformNGHZ.h"
#include "Resource/Mappings/CollapsePlatformOOZ.h"
#include "Resource/Mappings/SSZCollapsingPlatform.h"

typedef struct {
    uint8_t subtype;   // 0x28 (not used)
    uint8_t pad0[0xF]; // 0x29-0x37
    uint8_t timer;     // 0x38: from being touched to collapsing (and, in a piece, to falling)
    uint8_t pad1;      // 0x39
    uint8_t touched;   // 0x3A
} Own_CollapsingPlatform;

enum { CollapseRoutine_Init = 0, CollapseRoutine_Main = 2, CollapseRoutine_Piece = 6 };

// When each piece begins to fall (loc_946B for Oil Ocean's 7 pieces, loc_9472 for Dust Hill's 6)
static const uint8_t delays_ooz[7] = { 0x1A, 0x12, 0x0A, 0x02, 0x16, 0x0E, 0x06 };
static const uint8_t delays_dhz[6] = { 0x1A, 0x16, 0x12, 0x0E, 0x0A, 0x02 };
static const uint8_t delays_nghz[8] = { 0x16, 0x1A, 0x18, 0x12, 0x06, 0x0E, 0x0A, 0x02 }; // (the alpha's data for Neo Green Hill's 8 pieces)

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

static bool IsOilOcean(void) {
    return LEVEL_ZONE(level_id) == ZoneId_OOZ;
}

static bool IsSandShower(void) {
    return LEVEL_ZONE(level_id) == ZoneId_SSZ;
}

static bool IsNeoGreenHill(void) {
    return LEVEL_ZONE(level_id) == ZoneId_ARZ;
}

static void Platform(Sst *self) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr != NULL)
            Solid_Platform(self, chr, who, self->width_pixels, 0x10, self->x_pos.f.u);
    }
}

void Obj_CollapsingPlatform(Sst *self) {
    Own_CollapsingPlatform *own = (Own_CollapsingPlatform *)self->own.u8;

    switch (self->routine) {
    case CollapseRoutine_Init:
        self->routine += 2;
        self->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
        self->priority = 4 * 0x80;
        own->timer = 7;
        if (IsOilOcean()) {
            self->mappings = Mappings_CollapsePlatformOOZ;
            self->art_tile = TILE_MAP(0, 3, 0, 0, 0x39D);
            self->width_pixels = 0x40;
        } else if (IsSandShower()) { // (the staged objpatch-1F.asm: its mappings, the art at $500 on palette 0 (the mapping says line 2), 128 pixels wide, and Oil Ocean's delays)
            self->mappings = Mappings_SSZCollapsingPlatform;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0x500);
            self->width_pixels = 0x40;
        } else if (IsNeoGreenHill()) {
            self->mappings = Mappings_CollapsePlatformNGHZ;
            self->art_tile = TILE_MAP(0, 2, 0, 0, 0);
            self->width_pixels = 0x20;
        } else {
            self->mappings = Mappings_CollapsePlatformDHZ;
            self->art_tile = TILE_MAP(0, 3, 0, 0, 0x3F4);
            self->width_pixels = 0x20;
        }
        // Fallthrough
    case CollapseRoutine_Main:
        if (own->touched) {
            if (own->timer == 0) { // it collapses
                self->mapping_frame += 1;
                SstFragmentatePlatform(self, IsOilOcean() ? 7 : IsSandShower() ? 4 : IsNeoGreenHill() ? 8 : 6, IsOilOcean() || IsSandShower() ? delays_ooz : IsNeoGreenHill() ? delays_nghz : delays_dhz);
                return;
            }
            own->timer--;
        }
        if (self->status & 0x18)
            own->touched = 1;
        Platform(self);
        SstOnScreenTest(self);
        break;
    case CollapseRoutine_Piece:
        if (own->timer == 0) { // falling
            Sst_MoveSprite(self);
            if (!(self->render_flags & SST_RENDER_ON_SCREEN))
                Sst_Delete(self);
            else
                Sst_Draw(self);
            break;
        }
        if (!own->touched) { // a piece that waits to fall
            own->timer--;
            Sst_Draw(self);
            break;
        }
        // the first piece (the platform itself) is still a platform until it lets go
        Platform(self);
        SstOnScreenTest(self);
        if (--own->timer == 0) {
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Sst *chr = Character(who);
                if (chr != NULL && ((chr->status & SST_PSTATUS_OBJECT_STAND) != 0)) {
                    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    chr->prev_anim = 1;
                }
            }
        }
        break;
    }
}
