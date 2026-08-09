#include "Splash.h"

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

static void Splash_LoadGfx(Object *obj) {
    uint8_t frame = obj->frame;
    if (frame == splashdust_frame_num)
        return;
    splashdust_frame_num = frame;

    const uint8_t *dplc_script = Mappings_SplashDustDPLC;
    uint16_t off = frame;
    off <<= 1;
    dplc_script += (dplc_script[off] << 8) | (dplc_script[off + 1] << 0);

    int8_t entries = (*dplc_script++) - 1;
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

static void Splash_Construct(Object *obj) {
    obj->mappings = Mappings_SplashDust;
    obj->render.f.align_fg = true;
    obj->priority = 1;
    obj->width_pixels = 0x10;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SplashDust);
}

// Spawns a short-lived skid dust puff at the given position. Self-deletes
// via Obj08Ani_Skid's trailing "increment routine" (0xFC) animation command
// once its 4-frame animation finishes -- matches Sonic 2's own Obj08_SkidDust
// (a fresh temporary object per puff, not a mode on the fixed companion).
void Splash_SpawnSkidDust(Object *parent) {
    Object *puff = FindFreeObj();
    if (puff == NULL)
        return;

    puff->type = ObjId_Splash;
    puff->routine = 2; // Main -- skip Init, set up directly below
    Splash_Construct(puff);
    puff->pos.l.x.f.u = parent->pos.l.x.f.u;
    puff->pos.l.y.f.u = (int16_t)(parent->pos.l.y.f.u + 0x10);
    puff->status.o.f.x_flip = parent->status.p.f.x_flip; // see SplashAnim_Dash's comment in Obj_Splash
    puff->anim = SplashAnim_Skid;
    puff->prev_anim = 0xFF; // force AnimateSprite to restart from frame 0
}

void Obj_Splash(Object *obj) {
    switch (obj->routine) {
    case 0: // Init
        obj->routine = 2;
        Splash_Construct(obj);
        // Fallthrough
    case 2: // Main
        switch (obj->anim) {
        case SplashAnim_Null:
            return; // nothing to display

        case SplashAnim_Splash:
            // Follow the water surface every frame, but only snap to
            // Sonic's X position on the first frame of a new splash --
            // afterwards it just animates in place.
            obj->pos.l.y.f.u = wtr_pos1;
            if (obj->prev_anim != SplashAnim_Splash)
                obj->pos.l.x.f.u = player->pos.l.x.f.u;
            break;

        case SplashAnim_Dash:
            // Follow Sonic every frame while he's spinning in place charging.
            // Set status.o.f.x_flip (not render.f.x_flip directly) --
            // AnimateSprite recomputes render.f.x_flip from this field on
            // every frame-advancing tick, so writing render.f.x_flip here
            // would only "stick" on the ticks where the animation doesn't
            // advance, producing a flip that alternates frame to frame.
            obj->pos.l.x.f.u = player->pos.l.x.f.u;
            obj->pos.l.y.f.u = player->pos.l.y.f.u;
            obj->status.o.f.x_flip = player->status.p.f.x_flip;
            break;

        case SplashAnim_Skid:
            break; // positioned once at spawn, doesn't move afterwards
        }

        AnimateSprite(obj, Animation_SplashDust);
        Splash_LoadGfx(obj);
        DisplaySprite(obj);
        break;

    case 4: // Delete (self-destructing skid dust puffs only)
        ObjectDelete(obj);
        break;
    }
}
