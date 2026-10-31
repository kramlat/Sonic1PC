#include "Object/HUD.h"
#include "Resource/Mappings/HUD.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "EditorLive.h"
#include "Level.h"

// Where the lives counter goes in a widescreen picture: an object of its own in the top right corner. (The original has it in
// the bottom left, as part of the HUD's own sprite; the widescreen frames of that sprite leave it out.) The HUD keeps the slot of it
// (plus one) in the first byte of its own memory.
#define HUD_WIDE_FRAMES 4 // the HUD frames without the lives counter come after the four with it

// A split screen (Sonic 2's) puts the lives counter in the lower left of each view, where the original's picture has it, not in the corner of a wide picture
bool hud_lives_lower_left;

static bool HUD_IsWide(void) { return SCREEN_WIDTH > 320; }

static void HUD_UpdateLivesObject(Sst *self) {
    uint8_t *slot = &self->own.u8[0];
    Sst *lives_obj = NULL;
    if (*slot != 0 && sst[*slot - 1].code == sst_object_codes[ObjId_HUD] && sst[*slot - 1].routine == 4)
        lives_obj = &sst[*slot - 1];
    if (!HUD_IsWide()) {
        if (lives_obj != NULL)
            Sst_Delete(lives_obj);
        *slot = 0;
        return;
    }
    if (lives_obj == NULL) {
        lives_obj = Sst_AllocateAfter(self);
        if (lives_obj == NULL)
            return;
        lives_obj->code = sst_object_codes[ObjId_HUD];
        lives_obj->routine = 4;
        *slot = (uint8_t)((lives_obj - sst) + 1);
    }
    lives_obj->mappings = Mappings_HUD;
    lives_obj->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
    lives_obj->render_flags = 0;
    lives_obj->priority = 0 * 0x80;
    lives_obj->mapping_frame = 2 * HUD_WIDE_FRAMES; // just the lives
    if (hud_lives_lower_left) {
        lives_obj->x_pos.f.u = 0x90;
        lives_obj->y_pos.f.u = 0x148 + SCREEN_TALLADD2; // (the lives-only frame has its pieces at 0, the full frames at $40)
    } else {
        lives_obj->x_pos.f.u = (int16_t)(0x80 + SCREEN_WIDTH - 0x40);
        lives_obj->y_pos.f.u = 0x88;
    }
    Sst_Draw(lives_obj);
}

// HUD object
void Obj_HUD(Sst *self) {
    if (editor_present && !editor_playtest)
        return; // (the level editor draws its own)
    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set screen position
        self->x_pos.f.u = 0x90;
        self->y_pos.f.u = 0x108 + SCREEN_TALLADD2;

        // Initialize object drawing information
        self->mappings = Mappings_HUD;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
        self->render_flags = 0;
        self->priority = 0 * 0x80;
        // Fallthrough
    case 2: {
        self->y_pos.f.u = 0x108 + SCREEN_TALLADD2; // (the picture may have changed size)
        uint8_t frame = 0;
        // Determine frame. TIME flashes at 9:00 whether or not you have rings (the original only did it with no rings, which
        // the later games fixed), so this is applied in every build.
        if (!(frame_count & 8)) {
            if (!rings)
                frame += 1; // Flash RINGS
            if (level_time.min == 9)
                frame += 2; // Flash TIME
        }
        self->mapping_frame = frame + (HUD_IsWide() ? HUD_WIDE_FRAMES : 0);
        Sst_Draw(self);
        HUD_UpdateLivesObject(self);
        break;
    }
    case 4: // the lives counter of a widescreen picture: HUD_UpdateLivesObject draws it
        break;
    }
}
