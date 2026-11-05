#include "Electrocuter.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "Resource/Animation/Electrocuter.h"
#include "Resource/Mappings/Electrocuter.h"
#include "Sound.h"

// Object 6E - electrocution orbs (SBZ)
//
// Zap intervals are defined through the subtype, which must always be a
// power of 2. The subtype gets turned into an ANDable value against the
// frame counter, and zaps whenever that AND comes out to 0.

static void Elec_Main(Sst *self, Own_Electrocuter *own) {
    self->routine = 2; // advance to Elec_Shock
    self->mappings = Mappings_Electrocuter;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Electric_Orb);
    self->render_flags |= SST_RENDER_LEVEL;
    self->width_pixels = 80 / 2;
    // AnimateSprite only resets anim_frame/frame_time when self->anim !=
    // self->prev_anim. Setting just self->anim = 0 here isn't enough: if a
    // pooled slot's stale prev_anim also happens to already be 0 (a very
    // common leftover value across object types), that reset guard never
    // fires, and AnimateSprite reads anim_script[1 + self->anim_frame] with
    // self->anim_frame still holding whatever garbage the slot's previous
    // occupant left behind -- an unbounded out-of-bounds read that
    // corrupts frame/render/routine, both for this object and (via shared
    // sprite-buffer/queue corruption) neighboring ones. All four fields
    // AnimateSprite's guard depends on must be reset directly.
    self->anim = 0;
    self->prev_anim = 0;
    self->anim_frame = 0;
    self->anim_frame_timer = 0;

    uint16_t freq = (uint16_t)(self->subtype << 4);
    own->freq = (uint16_t)(freq - 1);
}

static void Elec_Shock(Sst *self, Own_Electrocuter *own) {
    if (((uint16_t)frame_count & own->freq) == 0) {
        self->anim = 1; // "zap" animation
        if ((self->render_flags & SST_RENDER_ON_SCREEN))
            QueueSound2(sfx_Electric);
    }

    Sst_Animate(self, Animation_Electrocuter);

    self->collision_flags = 0; // col_none -- harmless by default
    if (self->mapping_frame == 4)
        self->collision_flags = 0x24 | 0x80; // col_144x16 | col_hurt

    SstOnScreenTest(self);
}

void Obj_Electrocuter(Sst *self) {
    Own_Electrocuter *own = (Own_Electrocuter *)self->own.u8;

    if (self->routine == 0)
        Elec_Main(self, own);

    Elec_Shock(self, own);
}
