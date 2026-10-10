// Object 08 of the alpha (Aug 21st 1992): the dust of the spin dash, the dust of the skid and the splash of the water, one object for each player. Where the prototype only has a splash (WaterObjects.c had it), it is all this
// one now, with its own art that it brings into VRAM a frame at a time (the DPLC), 16 tiles for each player ($49C for Sonic, $48C for Tails).
#include "Object/DustSplash.h"
#include "Constants.h"

#include "Level.h"
#include "Backend/VDP.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"

#include <string.h>

#include "Resource/Animation/DustSplash.h"
#include "Resource/Mappings/DustSplash.h"
#include "Resource/Mappings/DustSplashDPLC.h"
#include "Resource/Art/DustSplash.h"

typedef struct {
    uint8_t tails;       // 0x3F in the alpha: the dust is Tails'
    uint8_t last_frame;  // 0x30: the frame whose tiles are in VRAM
    int8_t skid_timer;   // 0x32
    uint8_t owner;       // 0x3E: the slot of the player it belongs to
    uint16_t dest;       // 0x3C: where in VRAM its tiles go
} Scratch_Dust;

static Object *Owner(const Scratch_Dust *dust) {
    return &objects[dust->owner];
}

// The air the player has left (each has his own, in his object): the dust is only there while he is not about to drown
static uint16_t Air(const Object *owner) {
    return ((const Scratch_Sonic *)&owner->scratch)->air;
}

// Brings the tiles of the frame into VRAM (Load_Dust_Water_Splash_Dynamic_PLC): the DPLC's runs, one after the other from where this dust's window starts
static void LoadTiles(Object *obj, Scratch_Dust *dust) {
    if (obj->frame == dust->last_frame)
        return;
    dust->last_frame = obj->frame;

    const uint8_t *dplc = Mappings_DustSplashDPLC;
    dplc += (dplc[obj->frame << 1] << 8) | dplc[(obj->frame << 1) + 1];
    int entries = (int16_t)((dplc[0] << 8) | dplc[1]);
    dplc += 2;

    size_t vram = dust->dest;
    while (entries-- > 0) {
        const uint16_t run = (uint16_t)((dplc[0] << 8) | dplc[1]);
        dplc += 2;
        const size_t tiles = (size_t)((run >> 12) & 0xF) + 1;
        VDP_SeekVRAM(vram);
        VDP_WriteVRAM(Art_DustSplash + (size_t)(run & 0xFFF) * 0x20, tiles * 0x20);
        vram += tiles * 0x20;
    }
}

static void Show(Object *obj, Scratch_Dust *dust) {
    AnimateSprite(obj, Animation_DustSplash);
    LoadTiles(obj, dust);
    DisplaySprite(obj);
}

// Routine 2: by what it shows
static void Main(Object *obj, Scratch_Dust *dust) {
    Object *owner = Owner(dust);
    switch (obj->anim) {
    case DUST_NULL:
        break;
    case DUST_SPLASH:
        obj->pos.l.y.f.u = wtr_pos1;
        if (!obj->prev_anim) { // (the first frame of it: it stays where it is afterwards)
            obj->pos.l.x.f.u = owner->pos.l.x.f.u;
            obj->status.b = 0;
            obj->tile &= (uint16_t)~TILE_PRIORITY_AND;
        }
        break;
    case DUST_DASH:
        if (Air(owner) < 12) {
            obj->anim = DUST_NULL;
            return;
        }
        if (!obj->prev_anim) { // (it starts where the player is, and he stays put while he charges)
            obj->pos.l.x.f.u = owner->pos.l.x.f.u;
            obj->pos.l.y.f.u = owner->pos.l.y.f.u;
            obj->status.b = owner->status.b;
            if (dust->tails)
                obj->pos.l.y.f.u -= 4;
            if (owner->tile & TILE_PRIORITY_AND)
                obj->tile |= TILE_PRIORITY_AND;
        }
        break;
    case DUST_SKID:
        if (Air(owner) < 12) {
            obj->anim = DUST_NULL;
            return;
        }
        break;
    }
    Show(obj, dust);
}

// Routine 6: while the player skids (his animation 13) it makes a puff of dust every fourth frame, each an object that plays the skid's animation and goes
static void Skid(Object *obj, Scratch_Dust *dust) {
    Object *owner = Owner(dust);
    if (owner->anim != SonAnimId_Stop) {
        obj->routine = 2;
        dust->skid_timer = 0;
        return;
    }
    if (--dust->skid_timer < 0) {
        dust->skid_timer = 3;
        Object *puff = FindFreeObj();
        if (puff != NULL) {
            memset(puff, 0, sizeof(*puff));
            puff->type = obj->type;
            puff->pos.l.x.f.u = owner->pos.l.x.f.u;
            puff->pos.l.y.f.u = (int16_t)(owner->pos.l.y.f.u + 0x10);
            if (dust->tails)
                puff->pos.l.y.f.u -= 4;
            puff->anim = DUST_SKID;
            puff->routine = 2;
            puff->mappings = obj->mappings;
            puff->render.b = obj->render.b;
            puff->priority = 1;
            puff->width_pixels = 4;
            puff->tile = obj->tile;
            Scratch_Dust *child = (Scratch_Dust *)&puff->scratch;
            child->tails = dust->tails;
            child->owner = dust->owner;
            child->dest = dust->dest; // (the alpha forgets this one: its puffs would write their tiles at VRAM 0)
            if (owner->tile != 0)
                puff->tile |= TILE_PRIORITY_AND;
        }
    }
    LoadTiles(obj, dust);
}

void DustSplash_Show(Object *dust, uint8_t what) {
    dust->anim = what;
    dust->prev_anim = 0;
}

void Obj_DustSplash(Object *obj) {
    Scratch_Dust *dust = (Scratch_Dust *)&obj->scratch;
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_DustSplash;
        obj->render.f.level_fg = true;
        obj->priority = 1;
        obj->width_pixels = 0x10;
        dust->owner = dust->tails ? TAILS_SLOT : 0;
        dust->dest = dust->tails ? 0x9180 : 0x9380; // tiles $48C and $49C
        obj->tile = TILE_MAP(0, 0, 0, 0, dust->tails ? 0x48C : 0x49C);
        // Fallthrough
    case 2:
        Main(obj, dust);
        break;
    case 4:
        ObjectDelete(obj);
        break;
    case 6:
        Skid(obj, dust);
        break;
    }
}

void DustSplash_MakeTails(void) {
    Object *dust = &objects[DUST_TAILS_SLOT];
    memset(dust, 0, sizeof(*dust));
    dust->type = ObjId_Splash;
    ((Scratch_Dust *)&dust->scratch)->tails = 1;
}
