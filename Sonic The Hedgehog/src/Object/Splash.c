#include "Splash.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "Object/Sonic.h"

#include <string.h>

#include "Resource/Art/SplashDust.h"
#include "Resource/Mappings/SplashDust.h"
#include "Resource/Mappings/SplashDustDPLC.h"
#include "Resource/Animation/SplashDust.h"

// Dynamic VRAM window shared by every display mode (splash/dash dust/skid
// dust) -- same pattern as Sonic's own sgfx_buffer/sonframe_chg, refreshed
// only when the object's mapping frame actually changes.
uint8_t splashdust_gfx_buffer[SPLASHDUST_GFX_SIZE];
uint8_t splashdust_frame_chg;
static uint8_t splashdust_frame_num = 0xFF;

static void Splash_LoadGfx(Sst *self) {
    uint8_t frame = self->mapping_frame;
    if (frame == splashdust_frame_num)
        return;
    splashdust_frame_num = frame;

    const uint8_t *dplc_script = Mappings_SplashDustDPLC;
    uint16_t off = frame;
    off <<= 1;
    dplc_script += (dplc_script[off] << 8) | (dplc_script[off + 1] << 0);

    int16_t entries = (int16_t)((dplc_script[0] << 8) | dplc_script[1]) - 1; // the count is a word (Sonic 2 format)
    dplc_script += 2;
    if (entries < 0)
        return;

    uint16_t dest = 0;
    splashdust_frame_chg = true;

    do {
        uint16_t tile = *dplc_script++;
        uint8_t tiles = tile >> 4;
        tile = ((tile << 8) | (*dplc_script++)) << 5;

        const uint8_t *fromp = Art_SplashDust + tile;
        uint8_t *top = splashdust_gfx_buffer + dest;
        uint8_t n = tiles;
        do {
            memcpy(top, fromp, 0x20);
            fromp += 0x20;
            top += 0x20;
        } while (n-- > 0);
        dest = (uint16_t)(dest + (tiles + 1) * 0x20);
    } while (entries-- > 0);
}

static void Splash_Construct(Sst *self) {
    self->mappings = Mappings_SplashDust;
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 1 * 0x80;
    self->width_pixels = 0x10;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SplashDust);
}

// Spawns a short-lived skid dust puff at the given position. Self-deletes
// via Obj08Ani_Skid's trailing "increment routine" (0xFC) animation command
// once its 4-frame animation finishes -- matches Sonic 2's own Obj08_SkidDust
// (a fresh temporary object per puff, not a mode on the fixed companion).
void Splash_SpawnSkidDust(Sst *parent) {
    Sst *puff = Sst_Allocate();
    if (puff == NULL)
        return;

    puff->code = sst_object_codes[ObjId_Splash]; // (the game's own, by the table: Sonic 2's is its own object, with its own art window)
    puff->routine = 2; // Main -- skip Init, set up directly below
    Splash_Construct(puff);
    puff->x_pos.f.u = parent->x_pos.f.u;
    puff->y_pos.f.u = (int16_t)(parent->y_pos.f.u + 0x10);
    SST_STATUS_SET(puff, SST_STATUS_X_FLIP, (parent->status & SST_PSTATUS_X_FLIP) != 0); // see SplashAnim_Dash's comment in Obj_Splash
    puff->anim = SplashAnim_Skid;
    puff->prev_anim = 0xFF; // force AnimateSprite to restart from frame 0
}

// The companion that Sonic shows the splash and the dust with (made when the level starts)
static Sst *splash_companion;

void Splash_Make(void) {
    splash_companion = Sst_Spawn(ObjId_Splash, 0, 0); // (by the table: Sonic 2's dust and splash is its own object, not Sonic 1's, whose art window is Tails')
    if (splash_companion != NULL)
        splash_companion->subtype = 1;
}

Sst *Splash_Companion(void) {
    return splash_companion != NULL && splash_companion->code == sst_object_codes[ObjId_Splash] && splash_companion->subtype == 1 ? splash_companion : NULL;
}

void Splash_SetAnim(uint8_t anim) {
    Sst *c = Splash_Companion();
    if (c != NULL)
        c->anim = anim;
}

uint8_t Splash_GetAnim(void) {
    const Sst *c = Splash_Companion();
    return c != NULL ? c->anim : SplashAnim_Null;
}

void Obj_Splash(Sst *self) {
    switch (self->routine) {
    case 0: // Init
        self->routine = 2;
        Splash_Construct(self);
        // Fallthrough
    case 2: // Main
        switch (self->anim) {
        case SplashAnim_Null:
            return; // nothing to display

        case SplashAnim_Splash:
            // Follow the water surface every frame, but only snap to
            // Sonic's X position on the first frame of a new splash --
            // afterwards it just animates in place.
            self->y_pos.f.u = wtr_pos1;
            if (self->prev_anim != SplashAnim_Splash)
                self->x_pos.f.u = player->x_pos.f.u;
            break;

        case SplashAnim_Dash:
            // Follow Sonic every frame while he's spinning in place charging.
            // Set status.o.f.x_flip (not render.f.x_flip directly) --
            // AnimateSprite recomputes render.f.x_flip from this field on
            // every frame-advancing tick, so writing render.f.x_flip here
            // would only "stick" on the ticks where the animation doesn't
            // advance, producing a flip that alternates frame to frame.
            self->x_pos.f.u = player->x_pos.f.u;
            self->y_pos.f.u = player->y_pos.f.u;
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, ((player->status & SST_PSTATUS_X_FLIP) != 0));
            break;

        case SplashAnim_Skid:
            break; // positioned once at spawn, doesn't move afterwards
        }

        Sst_Animate(self, Animation_SplashDust);
        Splash_LoadGfx(self);
        Sst_Draw(self);
        break;

    case 4: // Delete (self-destructing skid dust puffs only)
        Sst_Delete(self);
        break;
    }
}
