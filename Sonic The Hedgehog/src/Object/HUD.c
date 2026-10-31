#include "Object/HUD.h"
#include "Resource/Mappings/HUD.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "EditorLive.h"
#include "Level.h"

// The HUD is not an object: it needs no slot and no place in the id table, the level's frame just draws it (HUD_Draw, called by GM_Level and GM_Ending). Its two sprites are
// plain Sst structures the draw queue points at.
// Where the lives counter goes in a widescreen picture: a sprite of its own in the top right corner. (The original has it in
// the bottom left, as part of the HUD's own sprite; the widescreen frames of that sprite leave it out.)
#define HUD_WIDE_FRAMES 4 // the HUD frames without the lives counter come after the four with it

// A split screen (Sonic 2's) puts the lives counter in the lower left of each view, where the original's picture has it, not in the corner of a wide picture
bool hud_lives_lower_left;

// Whether the screen has one (a level, the ending): GM_Level and GM_Ending say so
bool hud_visible;

static Sst hud_sprite, hud_lives_sprite;

static bool HUD_IsWide(void) { return SCREEN_WIDTH > 320; }

void HUD_Draw(void) {
    if (!hud_visible || (editor_present && !editor_playtest))
        return; // (the level editor draws its own)

    Sst *self = &hud_sprite;
    self->x_pos.f.u = 0x90;
    self->y_pos.f.u = 0x108 + SCREEN_TALLADD2; // (the picture may have changed size)
    self->mappings = Mappings_HUD;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
    self->render_flags = 0;
    self->priority = 0 * 0x80;

    // Determine frame. TIME flashes at 9:00 whether or not you have rings (the original only did it with no rings, which
    // the later games fixed), so this is applied in every build.
    uint8_t frame = 0;
    if (!(frame_count & 8)) {
        if (!rings)
            frame += 1; // Flash RINGS
        if (level_time.min == 9)
            frame += 2; // Flash TIME
    }
    self->mapping_frame = frame + (HUD_IsWide() ? HUD_WIDE_FRAMES : 0);
    Sst_Draw(self);

    if (!HUD_IsWide())
        return;
    Sst *lives = &hud_lives_sprite;
    lives->mappings = Mappings_HUD;
    lives->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HUD);
    lives->render_flags = 0;
    lives->priority = 0 * 0x80;
    lives->mapping_frame = 2 * HUD_WIDE_FRAMES; // just the lives
    if (hud_lives_lower_left) {
        lives->x_pos.f.u = 0x90;
        lives->y_pos.f.u = 0x148 + SCREEN_TALLADD2; // (the lives-only frame has its pieces at 0, the full frames at $40)
    } else {
        lives->x_pos.f.u = (int16_t)(0x80 + SCREEN_WIDTH - 0x40);
        lives->y_pos.f.u = 0x88;
    }
    Sst_Draw(lives);
}
