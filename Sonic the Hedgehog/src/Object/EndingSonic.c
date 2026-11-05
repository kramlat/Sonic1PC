#include "EndingSonic.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Ending.h"
#include "SpecialStage.h"

#include "Resource/Animation/EndingSonic.h"
#include "Resource/Mappings/EndingSonic.h"

#define EMERALDS_ALL 6

// ---------------------------------------------------------------------------
// Object 87 - Sonic on the ending sequence. Works hand in hand with End_MoveSonic (GM_Ending.c); uses the secondary
// routine because it is swapped in for the real Sonic object.
// ---------------------------------------------------------------------------

enum {
    ESON_MAIN = 0,
    ESON_MAKE_EMERALDS = 2,
    ESON_ANIMATE = 4,
    ESON_LOOK_UP = 6,
    ESON_DELETE_EMERALDS = 8,
    ESON_ANIMATE2 = 0xA,
    ESON_MAKE_LOGO = 0xC,
    ESON_ANIMATE3 = 0xE,
    ESON_BAD_ENDING = 0x10,
    ESON_ANIMATE4 = 0x12,
};

#define eson_time own.u16[4] // 0x30: time to wait between events

static void ESon_Animate(Sst *self) { Sst_Animate(self, Animation_EndingSonic); }

static void ESon_MakeEmeralds(Sst *self) {
    if (--self->eson_time != 0)
        return;
    self->routine_secondary += 2;
    self->anim = 0;          // "hold" animation, restarted at once
    self->prev_anim = 0xFF;
    Sst_SpawnCode(Obj_EndChaos, 0, 0);
}

static void ESon_Main(Sst *self) {
    if (emeralds != EMERALDS_ALL) { // bad ending: skip the emerald sequence
        self->routine_secondary += 0x10;
        self->eson_time = (3 * 60) + 36;
        return;
    }
    self->routine_secondary += 2;
    self->mappings = Mappings_EndingSonic;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Sonic);
    self->render_flags = SST_RENDER_LEVEL;
    self->status = 0;
    self->priority = 2 * 0x80;
    self->mapping_frame = 0; // looking at the emeralds
    self->eson_time = (1 * 60) + 20;
    ESon_MakeEmeralds(self);
}

static void ESon_LookUp(Sst *self) {
    Sst *emerald = NULL; // the first of the emeralds Sonic holds up
    for (int i = 0; i < SST_COUNT && emerald == NULL; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_EndChaos)
            emerald = &sst[i];
    if (emerald == NULL || emerald->own.u16[10] != 0x20 * 0x100) // the emeralds spin at full radius
        return;
    restart = 1; // End_ChkEmerald: start the white flash
    self->eson_time = (1 * 60) + 30;
    self->routine_secondary += 2;
}

static void ESon_DeleteEmeralds(Sst *self) {
    if (--self->eson_time != 0)
        return;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_EndChaos)
            Sst_Delete(&sst[i]);
    restart = 1; // End_SlowFade: the emeralds are gone
    self->routine_secondary += 2;
    self->anim = 1; // confused
    self->eson_time = 1 * 60;
}

static void ESon_MakeLogo(Sst *self) {
    if (--self->eson_time != 0)
        return;
    self->routine_secondary += 2;
    self->eson_time = 3 * 60; // unused
    self->anim = 2;           // leap at the screen
    Sst_SpawnCode(Obj_EndSTH, 0, 0);
}

static void ESon_BadEnding(Sst *self) {
    if (--self->eson_time != 0)
        return;
    self->routine_secondary += 2;
    self->mappings = Mappings_EndingSonic;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Ending_Sonic);
    self->render_flags = SST_RENDER_LEVEL;
    self->status = 0;
    self->priority = 2 * 0x80;
    self->mapping_frame = 5; // first leaping frame
    self->anim = 2;
    Sst_SpawnCode(Obj_EndSTH, 0, 0);
    ESon_Animate(self);
}

void Obj_EndSonic(Sst *self) {
    switch (self->routine_secondary) {
    case ESON_MAIN: ESon_Main(self); break;
    case ESON_MAKE_EMERALDS: ESon_MakeEmeralds(self); break;
    case ESON_ANIMATE:
    case ESON_ANIMATE2:
    case ESON_ANIMATE3:
    case ESON_ANIMATE4: ESon_Animate(self); break;
    case ESON_LOOK_UP: ESon_LookUp(self); break;
    case ESON_DELETE_EMERALDS: ESon_DeleteEmeralds(self); break;
    case ESON_MAKE_LOGO: ESon_MakeLogo(self); break;
    case ESON_BAD_ENDING: ESon_BadEnding(self); break;
    }
    Sst_Draw(self);
}

