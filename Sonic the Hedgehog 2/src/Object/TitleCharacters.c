#include "Constants.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

// Sonic and Tails from the title screen (the prototypes' object $0E, which took Sonic 1's title Sonic's place): both are drawn where they stand, as in the prototypes (they do not rise). Sonic is frame 0 of the
// mappings and Tails frame 1 (the title code gives Tails frame 1 when it makes him).
#include "Resource/Mappings/TitleCharacters.h"

#define TITLE_CHARACTERS_ART 0x200 // their art is loaded at VRAM 0x4000

void Obj_TitleCharacters(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        self->routine += 2;
        self->mappings = Mappings_TitleCharacters;
        self->priority = 1 * 0x80;
        if (self->mapping_frame == 0) { // Sonic
            self->x_pos.f.u = 0x148 + (PLANE_WIDEADD * 4);
            self->y_pos.f.u = 0xC4 + SCREEN_TALLADD2;
            self->art_tile = TILE_MAP(0, 2, 0, 0, TITLE_CHARACTERS_ART);
        } else { // Tails
            self->x_pos.f.u = 0xFC + (PLANE_WIDEADD * 4);
            self->y_pos.f.u = 0xCC + SCREEN_TALLADD2;
            self->art_tile = TILE_MAP(0, 1, 0, 0, TITLE_CHARACTERS_ART);
        }
        Sst_Draw(self);
        break;
    case 2: // In place (the prototypes' object branches to DisplaySprite from its init: the code that made them rise like Sonic 1's is unreachable there)
        Sst_Draw(self);
        break;
    }
}
