#include "LavaTag.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Mappings/LavaTag.h"

// Object 54 - invisible lava tag / hurt marker (MZ) -- this is what makes
// MZ's lava actually damaging; the lava itself is just background art with
// no collision of its own, so level layouts drop one of these (sized to
// match) over every lava pool.

static const uint8_t ltag_col_types[3] = {
    0x96, // subtype 0 - small,  64x64  (col_64x64  | col_hurt)
    0x94, // subtype 1 - medium, 128x64 (col_128x64 | col_hurt)
    0x95, // subtype 2 - large,  256x64 (col_256x64 | col_hurt)
};

void Obj_LavaTag(Object *obj) {
    switch (obj->routine) {
    case 0: // Main
        obj->routine = 2;
        obj->col_type = ltag_col_types[obj->scratch.u8[0]]; // subtype selects hurt hitbox size
        obj->mappings = Mappings_LavaTag; // blank -- genuinely invisible
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        // Fallthrough
    case 2: // ChkDel
        // Real hardware sets a "rendered" flag by hand here so its own
        // ReactToItem doesn't skip this object despite its blank mappings
        // (see the real disasm's own comment on this quirk -- accidentally
        // inherited by Sonic 2's ARZ leaf generator from this same code).
        // This port's ReactToItem-equivalent gates on render.f.on_screen
        // instead, which is only ever set via the normal
        // DisplaySprite/BuildSprites path -- so, same idea, this still has
        // to go through that path every frame even though the blank
        // mapping means nothing is ever actually drawn.
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}
