#include "Constants.h"
#include "Game.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Object/TitleMenu.h"
#include "Resource/Mappings/TitleMenu.h"

// The menu of the title screen, 1 PLAYER and 2 PLAYER VS (the pre-beta's object $0F): the lit one is the frame (0, 1 PLAYER; 1, 2 PLAYER VS), which Up and Down turn over; Start takes the frame
// as the number of players (TitleMenu_TwoPlayers). Its art is loaded at tile $2F0 by the title (Art_TitleMenu).
#define TITLE_MENU_ART 0x2F0

void Obj_TitleMenu(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        self->routine += 2;
        self->x_pos.f.u = 0x128 + (PLANE_WIDEADD * 4);
        self->y_pos.f.u = 0x14C + SCREEN_TALLADD2;
        self->mappings = Mappings_TitleMenu;
        self->art_tile = TILE_MAP(0, 0, 0, 0, TITLE_MENU_ART);
        self->render_flags = 0;
        self->priority = 0 * 0x80;
        break;
    case 2: // Up or Down changes the choice
        if (jpad1_press1 & (JPAD_UP | JPAD_DOWN))
            self->mapping_frame ^= 1;
        break;
    }
    Sst_Draw(self);
}

bool TitleMenu_TwoPlayers(void) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_TitleMenu)
            return sst[i].mapping_frame == 1;
    return false;
}
