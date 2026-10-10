#include "Object/HUD.h"
#include "Constants.h"
#include "Level.h"

// Where the lives counter goes in a widescreen picture: an object of its own in the top right corner. (The original has it in
// the bottom left, as part of the HUD's own sprite; the widescreen frames of that sprite leave it out.)
uint8_t hud_lives_slot = 29; // a free one: slots 30 and 31 are LZ's water surfaces (WaterSurface.h); Sonic 2 has Tails' tails in this one and uses another
#define HUD_LIVES_SLOT hud_lives_slot
#define HUD_WIDE_FRAMES 4 // the HUD frames without the lives counter come after the four with it

// A split screen (Sonic 2's) puts the lives counter in the lower left of each view, where the original's picture has it, not in the corner of a wide picture
bool hud_lives_lower_left;

static bool HUD_IsWide(void) { return SCREEN_WIDTH > 320; }

static void HUD_UpdateLivesObject(void) {
    Object *lives_obj = &objects[HUD_LIVES_SLOT];
    if (!HUD_IsWide()) {
        lives_obj->type = 0;
        return;
    }
    lives_obj->type = ObjId_HUD;
    lives_obj->routine = 4;
    lives_obj->mappings = Mappings_HUD;
    lives_obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
    lives_obj->render.b = 0;
    lives_obj->priority = 0;
    lives_obj->frame = 2 * HUD_WIDE_FRAMES; // just the lives
    if (hud_lives_lower_left) {
        lives_obj->pos.s.x = 0x90;
        lives_obj->pos.s.y = 0x148 + SCREEN_TALLADD2; // (the lives-only frame has its pieces at 0, the full frames at $40)
    } else {
        lives_obj->pos.s.x = (int16_t)(0x80 + SCREEN_WIDTH - 0x40);
        lives_obj->pos.s.y = 0x88;
    }
    DisplaySprite(lives_obj);
}

// HUD object
void Obj_HUD(Object* obj) {
    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine
        obj->routine += 2;

        // Set screen position
        obj->pos.s.x = 0x90;
        obj->pos.s.y = 0x108 + SCREEN_TALLADD2;

        // Initialize object drawing information
        obj->mappings = Mappings_HUD;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
        obj->render.b = 0;
        obj->priority = 0;
        // Fallthrough
    case 2: {
        obj->pos.s.y = 0x108 + SCREEN_TALLADD2; // (the picture may have changed size)
        uint8_t frame = 0;
        // Determine frame. TIME flashes at 9:00 whether or not you have rings (the original only did it with no rings, which
        // the later games fixed), so this is applied in every build.
        if (!(frame_count & 8)) {
            if (!rings)
                frame += 1; // Flash RINGS
            if (level_time.min == 9)
                frame += 2; // Flash TIME
        }
        obj->frame = frame + (HUD_IsWide() ? HUD_WIDE_FRAMES : 0);
        DisplaySprite(obj);
        HUD_UpdateLivesObject();
        break;
    }
    case 4: // the lives counter of a widescreen picture: HUD_UpdateLivesObject draws it
        break;
    }
}
