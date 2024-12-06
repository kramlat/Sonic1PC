#include "Credits.h"

void Obj_Credits_Construct(Object* obj) {
    // Increment routine and set position
    obj->routine += 2;
    obj->pos.s.x = 0x120 + SCREEN_WIDEADD2;
    obj->pos.s.y = 0xF0 + SCREEN_TALLADD2;

    // Set object drawing information
    obj->mappings = Mappings_Credits;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x5A0);
    obj->frame = credits_num;
    obj->render.b = 0;
    obj->priority = 0;
}

void Obj_Credits_SonicTeamPresents(Object* obj) {
    // Display "SONIC TEAM PRESENTS" text
    obj->tile = TILE_MAP(0, 0, 0, 0, 0xA6);
    obj->frame = 10;
}

bool Obj_Credits_JapEnable(Object* obj) {
    // Hidden Japanese credits
    if (credits_cheat && jpad1_hold1 == (JPAD_A | JPAD_C | JPAD_B | JPAD_DOWN)) {
        dry_palette_dup[2][0] = 0xEEE;
        dry_palette_dup[2][1] = 0x880;
        ObjectDelete(obj);
        return true;
    }
    return false;
}

// Credits object
void Obj_Credits(Object* obj) {
    switch (obj->routine) {
    case 0: // Initialization
        Obj_Credits_Construct(obj);

        // Force "SONIC TEAM PRESENTS" text when in title screen
        if (gamemode == GameMode_Title) {
            Obj_Credits_SonicTeamPresents(obj);
            if (Obj_Credits_JapEnable(obj))
                break;
        }

        // Fallthrough
    case 1: // Drawing
        DisplaySprite(obj);
        break;
    }
}
