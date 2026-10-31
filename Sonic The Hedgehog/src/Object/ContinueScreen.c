#include "ContinueScreen.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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

static void CSI_Main(Sst *self) {
    self->routine += 2;
    self->mappings = Mappings_ContinueScreen;
    self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Continue_Sonic);
    self->render_flags = 0; // positioned on the screen
    self->width_pixels = 120 / 2;
    self->x_pos.f.u = 0x80 + (SCREEN_WIDTH / 2);
    self->y_pos.f.u = 0x80 + 64 + SCREEN_TALLADD2;
    rings = 0;
    Sst_Draw(self);
}

static void CSI_MakeMiniSonic(Sst *self) {
    // One of the continues is the one being used, and stays hidden: only a second one shows anything at all
    int count = continues - 1;
    if (count < 1) {
        Sst_Delete(self);
        return;
    }

    bool flash_the_used_one = true;
    if (count > CSI_MAX_MINI_SONICS) {
        count = CSI_MAX_MINI_SONICS;
        flash_the_used_one = false; // too many to tell: nothing flashes
    }
    bool odd_shift = ((count - 1) & 1) != 0; // an even number of them is shifted left to stay centred

    // The first mini Sonic is this object itself, the others are made after it
    Sst *last = self;
    for (int i = 0; i < count; i++) {
        Sst *mini = (i == 0) ? self : Sst_AllocateAfter(self);
        if (mini == NULL)
            break;
        last = mini;
        mini->code = Obj_ContScrItem;
        mini->x_pos.f.u = (int16_t)csi_mini_sonic_x[i] - (odd_shift ? 10 : 0) + SCREEN_WIDEADD2;
        mini->y_pos.f.u = 0x80 + 0x50 + SCREEN_TALLADD2;
        mini->mapping_frame = CSI_FRAME_MINI_FOOT_DOWN;
        mini->routine = CSI_SHOW_MINI_SONIC;
        mini->mappings = Mappings_ContinueScreen;
        mini->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Mini_Sonic);
        mini->render_flags = 0;
    }
    // The rightmost one is the one that flashes once Sonic uses the continue
    last->status = flash_the_used_one ? 1 : 0;
}

static void CSI_ShowMiniSonic(Sst *self) {
    // The mini Sonic that stands for the continue being used flashes while Sonic runs off, and is gone once he moves
    if (self->status != 0 && SONIC_RUNNING_AWAY && !(vbla_count & 1)) {
        if (player->x_vel != 0)
            Sst_Delete(self);
        return; // hidden this frame
    }
    if (!(vbla_count & 0xF))
        self->mapping_frame ^= 1; // foot up / foot down
    Sst_Draw(self);
}

void Obj_ContScrItem(Sst *self) {
    switch (self->routine) {
    case CSI_MAIN:
        CSI_Main(self);
        break;
    case CSI_DISPLAY:
        Sst_Draw(self);
        break;
    case CSI_MAKE_MINI_SONIC:
        CSI_MakeMiniSonic(self);
        if (self->code == NULL)
            break;
        // The first mini Sonic is this object, which carries on with its own routine in the same frame
        CSI_ShowMiniSonic(self);
        break;
    case CSI_SHOW_MINI_SONIC:
        CSI_ShowMiniSonic(self);
        break;
    }
}
