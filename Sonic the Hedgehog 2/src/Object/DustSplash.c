// Object 08 of the alpha (Aug 21st 1992): the dust of the spin dash, the dust of the skid and the splash of the water, one object for each player. Where the prototype only has a splash (WaterObjects.c had it), it is all this
// one now, with its own art that it brings into VRAM a frame at a time (the DPLC), 16 tiles for each player ($49C for Sonic, $48C for Tails).
#include "Object/DustSplash.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
} Own_Dust;

static Sst *Owner(const Own_Dust *dust) {
    return dust->owner ? SST_PLAYER_2 : player;
}

// The air the player has left (each has his own, in his object): the dust is only there while he is not about to drown
static uint16_t Air(const Sst *chr) {
    return ((const Scratch_Sonic *)chr->own.u8)->air;
}

// Brings the tiles of the frame into VRAM (Load_Dust_Water_Splash_Dynamic_PLC): the DPLC's runs, one after the other from where this dust's window starts
static void LoadTiles(Sst *self, Own_Dust *dust) {
    if (self->mapping_frame == dust->last_frame)
        return;
    dust->last_frame = self->mapping_frame;

    const uint8_t *dplc = Mappings_DustSplashDPLC;
    dplc += (dplc[self->mapping_frame << 1] << 8) | dplc[(self->mapping_frame << 1) + 1];
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

static void Show(Sst *self, Own_Dust *dust) {
    Sst_Animate(self, Animation_DustSplash);
    LoadTiles(self, dust);
    Sst_Draw(self);
}

// Routine 2: by what it shows
static void Main(Sst *self, Own_Dust *dust) {
    Sst *chr = Owner(dust);
    switch (self->anim) {
    case DUST_NULL:
        break;
    case DUST_SPLASH:
        self->y_pos.f.u = wtr_pos1;
        if (!self->prev_anim) { // (the first frame of it: it stays where it is afterwards)
            self->x_pos.f.u = chr->x_pos.f.u;
            self->status = 0;
            self->art_tile &= (uint16_t)~TILE_PRIORITY_AND;
        }
        break;
    case DUST_DASH:
        if (Air(chr) < 12) {
            self->anim = DUST_NULL;
            return;
        }
        if (!self->prev_anim) { // (it starts where the player is, and he stays put while he charges)
            self->x_pos.f.u = chr->x_pos.f.u;
            self->y_pos.f.u = chr->y_pos.f.u;
            self->status = chr->status;
            if (dust->tails)
                self->y_pos.f.u -= 4;
            if (chr->art_tile & TILE_PRIORITY_AND)
                self->art_tile |= TILE_PRIORITY_AND;
        }
        break;
    case DUST_SKID:
        if (Air(chr) < 12) {
            self->anim = DUST_NULL;
            return;
        }
        break;
    }
    Show(self, dust);
}

// Routine 6: while the player skids (his animation 13) it makes a puff of dust every fourth frame, each an object that plays the skid's animation and goes
static void Skid(Sst *self, Own_Dust *dust) {
    Sst *chr = Owner(dust);
    if (chr->anim != SonAnimId_Stop) {
        self->routine = 2;
        dust->skid_timer = 0;
        return;
    }
    if (--dust->skid_timer < 0) {
        dust->skid_timer = 3;
        Sst *puff = Sst_Allocate();
        if (puff != NULL) {
            memset(puff, 0, sizeof(*puff));
            puff->code = self->code;
            puff->x_pos.f.u = chr->x_pos.f.u;
            puff->y_pos.f.u = (int16_t)(chr->y_pos.f.u + 0x10);
            if (dust->tails)
                puff->y_pos.f.u -= 4;
            puff->anim = DUST_SKID;
            puff->routine = 2;
            puff->mappings = self->mappings;
            puff->render_flags = self->render_flags;
            puff->priority = 1 * 0x80;
            puff->width_pixels = 4;
            puff->art_tile = self->art_tile;
            Own_Dust *child = (Own_Dust *)puff->own.u8;
            child->tails = dust->tails;
            child->owner = dust->owner;
            child->dest = dust->dest; // (the alpha forgets this one: its puffs would write their tiles at VRAM 0)
            if (chr->art_tile != 0)
                puff->art_tile |= TILE_PRIORITY_AND;
        }
    }
    LoadTiles(self, dust);
}

void DustSplash_Show(Sst *dust, uint8_t what) {
    if (dust == NULL)
        return;
    dust->anim = what;
    dust->prev_anim = 0;
}

void Obj_DustSplash(Sst *self) {
    Own_Dust *dust = (Own_Dust *)self->own.u8;
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_DustSplash;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 0x10;
        dust->owner = dust->tails ? 1 : 0; // (the character: 0 is Sonic, 1 Tails)
        dust->dest = dust->tails ? 0x9180 : 0x9380; // tiles $48C and $49C
        self->art_tile = TILE_MAP(0, 0, 0, 0, dust->tails ? 0x48C : 0x49C);
        // Fallthrough
    case 2:
        Main(self, dust);
        break;
    case 4:
        Sst_Delete(self);
        break;
    case 6:
        Skid(self, dust);
        break;
    }
}

void DustSplash_MakeTails(void) {
    Sst *dust = &sst[DUST_TAILS_SLOT];
    memset(dust, 0, sizeof(*dust));
    dust->code = sst_object_codes[ObjId_Splash];
    ((Own_Dust *)dust->own.u8)->tails = 1;
}
