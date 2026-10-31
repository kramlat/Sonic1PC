#include "Credits.h"
#include "Resource/Mappings/Credits.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

void Obj_Credits_Construct(Sst *self) {
    // Increment routine and set position
    self->routine += 2;
    self->x_pos.f.u = 0x120 + SCREEN_WIDEADD2;
    self->y_pos.f.u = 0xF0 + SCREEN_TALLADD2;

    // Set object drawing information
    self->mappings = Mappings_Credits;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Credits_Font);
    self->mapping_frame = credits_num;
    self->render_flags = 0;
    self->priority = 0 * 0x80;
}

void Obj_Credits_SonicTeamPresents(Sst *self) {
    // Display "SONIC TEAM PRESENTS" text
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Sonic_Team_Font);
    self->mapping_frame = 10;
}

bool Obj_Credits_JapEnable(Sst *self) {
    // Hidden Japanese credits
    if (credits_cheat && jpad1_hold1 == (JPAD_A | JPAD_C | JPAD_B | JPAD_DOWN)) {
        dry_palette_dup[2][0] = 0xEEE;
        dry_palette_dup[2][1] = 0x880;
        Sst_Delete(self);
        return true;
    }
    return false;
}

// Credits object
void Obj_Credits(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        Obj_Credits_Construct(self);

        // Force "SONIC TEAM PRESENTS" text when in title screen
        if (gamemode == GameMode_Title) {
            Obj_Credits_SonicTeamPresents(self);
            if (Obj_Credits_JapEnable(self))
                break;
        }

        // Fallthrough
    case 2: // Drawing (the init above moved the routine on)
        Sst_Draw(self);
        break;
    }
}
