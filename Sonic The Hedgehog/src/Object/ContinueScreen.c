#include "ContinueScreen.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Sonic.h"
#include "Sound.h"

#include "Resource/Mappings/ContinueScreen.h"
extern const uint8_t Mappings_Sonic[]; // (defined with the Sonic object)

// ---------------------------------------------------------------------------
// Object 80 - the text and light spot of the continue screen, and the mini Sonics that show how many continues are left
// ---------------------------------------------------------------------------

enum {
    CSI_MAIN = 0,
    CSI_DISPLAY = 2,
    CSI_MAKE_MINI_SONIC = 4,
    CSI_SHOW_MINI_SONIC = 6,
};

enum { CSI_FRAME_MINI_FOOT_DOWN = 6, CSI_MAX_MINI_SONICS = 15 };

#define SONIC_RUNNING_AWAY (player->routine >= 6) // CSon_RunRight: a continue has been used

// X positions of the mini Sonics, pseudo-interlaced instead of left to right so that consecutively collected continues
// stay roughly centred.
static const uint16_t csi_mini_sonic_x[CSI_MAX_MINI_SONICS] = {
    0x116, 0x12A, 0x102, 0x13E, 0xEE, 0x152, 0xDA, 0x166, 0xC6, 0x17A, 0xB2, 0x18E, 0x9E, 0x1A2, 0x8A,
};

static void CSI_Main(Object *obj) {
    obj->routine += 2;
    obj->mappings = Mappings_ContinueScreen;
    obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Continue_Sonic);
    obj->render.b = 0; // positioned on the screen
    obj->width_pixels = 120 / 2;
    obj->pos.s.x = 0x80 + (SCREEN_WIDTH / 2);
    obj->pos.s.y = 0x80 + 64 + SCREEN_TALLADD2;
    rings = 0;
    DisplaySprite(obj);
}

static void CSI_MakeMiniSonic(Object *obj) {
    // One of the continues is the one being used, and stays hidden: only a second one shows anything at all
    int count = continues - 1;
    if (count < 1) {
        ObjectDelete(obj);
        return;
    }

    bool flash_the_used_one = true;
    if (count > CSI_MAX_MINI_SONICS) {
        count = CSI_MAX_MINI_SONICS;
        flash_the_used_one = false; // too many to tell: nothing flashes
    }
    bool odd_shift = ((count - 1) & 1) != 0; // an even number of them is shifted left to stay centred

    // The first mini Sonic is this object itself, the others follow it in the next slots
    for (int i = 0; i < count; i++) {
        Object *mini = obj + i;
        mini->type = ObjId_80;
        mini->pos.s.x = (int16_t)csi_mini_sonic_x[i] - (odd_shift ? 10 : 0) + SCREEN_WIDEADD2;
        mini->pos.s.y = 0x80 + 0x50 + SCREEN_TALLADD2;
        mini->frame = CSI_FRAME_MINI_FOOT_DOWN;
        mini->routine = CSI_SHOW_MINI_SONIC;
        mini->mappings = Mappings_ContinueScreen;
        mini->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Mini_Sonic);
        mini->render.b = 0;
    }
    // The rightmost one is the one that flashes once Sonic uses the continue
    (obj + count - 1)->status.b = flash_the_used_one ? 1 : 0;
}

static void CSI_ShowMiniSonic(Object *obj) {
    // The mini Sonic that stands for the continue being used flashes while Sonic runs off, and is gone once he moves
    if (obj->status.b != 0 && SONIC_RUNNING_AWAY && !(vbla_count & 1)) {
        if (player->x_vel != 0)
            ObjectDelete(obj);
        return; // hidden this frame
    }
    if (!(vbla_count & 0xF))
        obj->frame ^= 1; // foot up / foot down
    DisplaySprite(obj);
}

void Obj_ContScrItem(Object *obj) {
    switch (obj->routine) {
    case CSI_MAIN:
        CSI_Main(obj);
        break;
    case CSI_DISPLAY:
        DisplaySprite(obj);
        break;
    case CSI_MAKE_MINI_SONIC:
        CSI_MakeMiniSonic(obj);
        if (obj->type == ObjId_Null)
            break;
        // The first mini Sonic is this object, which carries on with its own routine in the same frame
        CSI_ShowMiniSonic(obj);
        break;
    case CSI_SHOW_MINI_SONIC:
        CSI_ShowMiniSonic(obj);
        break;
    }
}
