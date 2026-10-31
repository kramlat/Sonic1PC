// The basic platforms for Sonic 2 (object 18, "shima"): Sonic 1's platform object with Sonic 2's changes: it carries Sonic and Tails, has Emerald Hill's and Hill Top's own mappings (their
// tiles are the level's) and sizes from a table, and lets go of both when it starts to fall (Nick Arcade's let go of Sonic only; the alpha's does both, and drops further before it goes). Subtype: bits 4-7 the size, bits 0-3 the movement (0 and 9 still, 1 right and left, 2 down and up,
// 3 falls half a second after being stood on, 4 falling, 5 left and right, 6 up and down, 7 rises when a button is pressed, 8 rising, $A and $B a gentle up and down, $C and $D a slow one).
#include "Object/Platform.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/PlatformEHZ.h"
#include "Resource/Mappings/PlatformNGHZ.h"

typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[3];  // 0x29-0x2B
    dword_s base_y;   // 0x2C: where the platform is, 16.16 (without the sink under a character)
    int16_t base_x;   // 0x32
    int16_t centre_y; // 0x34: where it moves about
    uint8_t sink;     // 0x38: how far down the weight on it has pushed it (a sine angle)
    uint8_t pad1;     // 0x39
    int16_t timer;    // 0x3A
} Own_Platform;

// Width and mapping frame by size (Obj18_Conf)
static const uint8_t sizes[5][2] = { { 0x20, 0 }, { 0x20, 1 }, { 0x20, 2 }, { 0x40, 3 }, { 0x30, 4 } };

enum { PlatRoutine_Init = 0, PlatRoutine_Main = 2, PlatRoutine_Delete = 4, PlatRoutine_Falling = 6, PlatRoutine_Solid = 8 };

static void Platform_ChangeMotion(Sst *self) {
    self->angle = (uint8_t)(oscillatory.state[6][0] >> 8);
}

// A player the falling platform lets go of (Offset_0x009492)
static void Platform_Release(const Sst *self, Sst *chr) {
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
    chr->y_vel = self->y_vel;
}

// The movement of the subtype (sub_8926)
static void Platform_Move(Sst *self, Own_Platform *own) {
    switch (self->subtype & 0xF) {
    case 1:
        self->x_pos.f.u = own->base_x + (int8_t)(self->angle - 0x40);
        Platform_ChangeMotion(self);
        break;
    case 2:
        own->base_y.f.u = own->centre_y + (int8_t)(self->angle - 0x40);
        Platform_ChangeMotion(self);
        break;
    case 3: // waits for someone to stand on it, then half a second, then falls
        if (own->timer == 0) {
            if (self->status & 0x18)
                own->timer = 0x1E;
        } else if (--own->timer == 0) {
            own->timer = 0x20;
            self->subtype++;
        }
        break;
    case 4: // falling: whoever stands on it is let go after a moment (the alpha's: both players)
        if (own->timer != 0 && --own->timer == 0) {
            if (self->status & 8) {
                self->status &= (uint8_t)~8;
                Platform_Release(self, player);
            }
            if (self->status & 0x10) {
                self->status &= (uint8_t)~0x10;
                Platform_Release(self, TAILS_OBJ);
            }
            self->routine = PlatRoutine_Falling;
        }
        own->base_y.v += (int32_t)self->y_vel << 8;
        self->y_vel += 0x38;
        if ((uint16_t)(limit_btm2 + 0x120) < (uint16_t)own->base_y.f.u)
            self->routine = PlatRoutine_Delete;
        break;
    case 5:
        self->x_pos.f.u = own->base_x + (int8_t)(0x40 - self->angle);
        Platform_ChangeMotion(self);
        break;
    case 6:
        own->base_y.f.u = own->centre_y + (int8_t)(0x40 - self->angle);
        Platform_ChangeMotion(self);
        break;
    case 7: // waits for a button, then a second, then rises
        if (own->timer == 0) {
            if (f_switch[self->subtype >> 4])
                own->timer = 0x3C;
        } else if (--own->timer == 0) {
            self->subtype++;
        }
        break;
    case 8:
        own->base_y.f.u -= 2;
        if ((int16_t)(own->centre_y - 0x200) == own->base_y.f.u)
            self->subtype = 0;
        break;
    case 0xA:
        own->base_y.f.u = own->centre_y + ((int8_t)(self->angle - 0x40) >> 1);
        Platform_ChangeMotion(self);
        break;
    case 0xB:
        own->base_y.f.u = own->centre_y + ((int8_t)(0x40 - self->angle) >> 1);
        Platform_ChangeMotion(self);
        break;
    case 0xC:
        own->base_y.f.u = own->centre_y + (int8_t)((uint8_t)(oscillatory.state[3][0] >> 8) - 0x30);
        Platform_ChangeMotion(self);
        break;
    case 0xD:
        own->base_y.f.u = own->centre_y + (int8_t)(0x30 - (uint8_t)(oscillatory.state[3][0] >> 8));
        Platform_ChangeMotion(self);
        break;
    }
}

// Its height: the base, and a sine of the weight on it (sub_890C)
static void Platform_Sink(Sst *self, Own_Platform *own) {
    int16_t sin, cos;
    CalcSine(own->sink, &sin, &cos);
    self->y_pos.f.u = (int16_t)(own->base_y.f.u + ((sin * 0x400) >> 16));
}

static void Platform_Display(Sst *self, Own_Platform *own) {
    SstOnScreenTest2(self, own->base_x);
}

void Obj_BasicPlatform(Sst *self) {
    Own_Platform *own = (Own_Platform *)self->own.u8;

    switch (self->routine) {
    case PlatRoutine_Init: {
        self->routine += 2;
        const uint8_t *size = sizes[((self->subtype >> 4) & 7) % 5]; // (the Simon Wai prototype's: bit 7 is the solid kind below, so the size has three bits)
        self->width_pixels = size[0];
        self->mapping_frame = size[1];
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level);
        self->mappings = LEVEL_ZONE(level_id) == ZoneId_ARZ ? Mappings_PlatformNGHZ : Mappings_PlatformEHZ; // (the prototype has one mapping for every zone but Neo Green Hill: the tiles of the level art)
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->base_y.f.u = self->y_pos.f.u;
        own->base_y.f.l = 0;
        own->centre_y = self->y_pos.f.u;
        own->base_x = self->x_pos.f.u;
        self->angle = 0x80;
        const bool solid = self->subtype & 0x80; // (a block that is solid on every side, with a height of its own)
        self->subtype &= 0xF;
        if (solid) {
            self->routine = PlatRoutine_Solid;
            self->y_radius = LEVEL_ZONE(level_id) == ZoneId_ARZ ? 0x28 : 0x30;
            self->height_pixels = self->y_radius;
            goto solid_block;
        }
    }
        // Fallthrough
    case PlatRoutine_Main: {
        // The weight on it pushes it down, and it springs back
        if (!(self->status & 0x18)) {
            if (own->sink != 0)
                own->sink -= 4;
        } else if (own->sink != 0x40) {
            own->sink += 4;
        }

        int16_t x = self->x_pos.f.u;
        Platform_Move(self, own);
        Platform_Sink(self, own);
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) { // (even on the frame it starts to fall)
            Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
            if (chr != NULL)
                Solid_Platform(self, chr, who, self->width_pixels, 8, x);
        }
        Platform_Display(self, own);
        break;
    }
    case PlatRoutine_Solid:
    solid_block: {
        if (!(self->status & 0x18)) {
            if (own->sink != 0)
                own->sink -= 4;
        } else if (own->sink != 0x40) {
            own->sink += 4;
        }
        int16_t x = self->x_pos.f.u;
        Platform_Move(self, own);
        Platform_Sink(self, own);
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
                Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
                if (chr != NULL)
                    Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), x, NULL);
            }
        }
        Platform_Display(self, own);
        break;
    }
    case PlatRoutine_Falling:
        Platform_Move(self, own);
        Platform_Sink(self, own);
        Platform_Display(self, own);
        break;
    case PlatRoutine_Delete:
        Sst_Delete(self);
        break;
    }
}
