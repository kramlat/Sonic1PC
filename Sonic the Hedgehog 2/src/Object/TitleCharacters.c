#include "Object.h"
#include "Constants.h"

// Sonic and Tails from the title screen (the prototypes' object $0E, which took Sonic 1's title Sonic's place): both are drawn where they stand, as in the prototypes (they do not rise). Sonic is frame 0 of the
// mappings and Tails frame 1 (the title code gives Tails frame 1 when it makes him).
#include "Resource/Mappings/TitleCharacters.h"

#define TITLE_CHARACTERS_ART 0x200 // their art is loaded at VRAM 0x4000

void Obj_TitleCharacters(Object *obj) {
    switch (obj->routine) {
    case 0: // Initialization
        obj->routine += 2;
        obj->mappings = Mappings_TitleCharacters;
        obj->priority = 1;
        if (obj->frame == 0) { // Sonic
            obj->pos.s.x = 0x148 + (PLANE_WIDEADD * 4);
            obj->pos.s.y = 0xC4 + SCREEN_TALLADD2;
            obj->tile = TILE_MAP(0, 2, 0, 0, TITLE_CHARACTERS_ART);
        } else { // Tails
            obj->pos.s.x = 0xFC + (PLANE_WIDEADD * 4);
            obj->pos.s.y = 0xCC + SCREEN_TALLADD2;
            obj->tile = TILE_MAP(0, 1, 0, 0, TITLE_CHARACTERS_ART);
        }
        DisplaySprite(obj);
        break;
    case 2: // In place (the prototypes' object branches to DisplaySprite from its init: the code that made them rise like Sonic 1's is unreachable there)
        DisplaySprite(obj);
        break;
    }
}
