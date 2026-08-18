#include "Electrocuter.h"

#include "Level.h"
#include "Resource/Animation/Electrocuter.h"
#include "Resource/Mappings/Electrocuter.h"
#include "Sound.h"

// Object 6E - electrocution orbs (SBZ)
//
// Zap intervals are defined through the subtype, which must always be a
// power of 2. The subtype gets turned into an ANDable value against the
// frame counter, and zaps whenever that AND comes out to 0.

static void Elec_Main(Object *obj, Scratch_Electrocuter *scratch) {
    obj->routine = 2; // advance to Elec_Shock
    obj->mappings = Mappings_Electrocuter;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Electric_Orb);
    obj->render.f.align_fg = true;
    obj->width_pixels = 80 / 2;
    // AnimateSprite only resets anim_frame/frame_time when obj->anim !=
    // obj->prev_anim. Setting just obj->anim = 0 here isn't enough: if a
    // pooled slot's stale prev_anim also happens to already be 0 (a very
    // common leftover value across object types), that reset guard never
    // fires, and AnimateSprite reads anim_script[1 + obj->anim_frame] with
    // obj->anim_frame still holding whatever garbage the slot's previous
    // occupant left behind -- an unbounded out-of-bounds read that
    // corrupts frame/render/routine, both for this object and (via shared
    // sprite-buffer/queue corruption) neighboring ones. All four fields
    // AnimateSprite's guard depends on must be reset directly.
    obj->anim = 0;
    obj->prev_anim = 0;
    obj->anim_frame = 0;
    obj->frame_time.b = 0;

    uint16_t freq = (uint16_t)(obj->scratch.u8[0] << 4);
    scratch->freq = (uint16_t)(freq - 1);
}

static void Elec_Shock(Object *obj, Scratch_Electrocuter *scratch) {
    if (((uint16_t)frame_count & scratch->freq) == 0) {
        obj->anim = 1; // "zap" animation
        if (obj->render.f.on_screen)
            QueueSound2(sfx_Electric);
    }

    AnimateSprite(obj, Animation_Electrocuter);

    obj->col_type = 0; // col_none -- harmless by default
    if (obj->frame == 4)
        obj->col_type = 0x24 | 0x80; // col_144x16 | col_hurt

    RememberState(obj);
}

void Obj_Electrocuter(Object *obj) {
    Scratch_Electrocuter *scratch = (Scratch_Electrocuter *)&obj->scratch;

    if (obj->routine == 0)
        Elec_Main(obj, scratch);

    Elec_Shock(obj, scratch);
}
