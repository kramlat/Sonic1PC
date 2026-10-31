#include "ChainStomp.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/ChainedStompers.h"
#include "Sound.h"

// Object 31 - stomping metal blocks on chains (MZ). Each placed stomper
// spawns up to 3 more objects alongside the main block: a spike tip, a
// chain (whose display length/frame tracks the block's own current
// extension), and a base piece fixed to the ceiling. The spikeless
// "small" size skips the spike tip. 7 movement types: one rises only
// while a matching switch is held (used once, in MZ1, and coordinates
// with the not-yet-ported PushBlock object via the shared obj31_ypos
// global), four fall and re-rise on their own on a timer, and two wait
// until Sonic wanders within range before switching into an auto-stomp
// type.

// Chain lengths (8.8 fixed point, i.e. already <<8), indexed by subtype's
// own low nibble.
static const uint16_t cstom_lengths[7] = {
    0x7000, 0xA000, 0x5000, 0x7800, 0x3800, 0x5800, 0xB800,
};

typedef struct {
    uint8_t routine;
    int8_t yoff;
    uint8_t frame;
} CStomVar;

// {routine, Y-offset from the block's own spawn position, frame}, in
// spawn order (block itself always spawns first, reusing this very
// object -- no allocation needed for it).
static const CStomVar cstom_vars[4] = {
    { 2, 0, 0 },            // block
    { 4, 0x1C, 1 },         // spikes
    { 8, (int8_t)0xCC, 3 }, // chain
    { 6, (int8_t)0xF0, 2 }, // base at ceiling
};

static void CStom_UpdateBlockY(Sst *self, Own_ChainStomp *own) {
    self->y_pos.f.u = (int16_t)(own->orig_y + (uint8_t)(own->current >> 8));
}

// Types 1/2/4/6 - rises and stomps down on its own on a timer.
static void CStom_AutoStomp(Sst *self, Own_ChainStomp *own) {
    if (own->rising) {
        if (own->delay != 0) {
            own->delay--;
        } else {
            if (((uint8_t)frame_count & 0xF) == 0 && (self->render_flags & SST_RENDER_ON_SCREEN))
                QueueSound2(sfx_ChainRise);

            bool underflow = own->current < 0x80;
            own->current = (uint16_t)(own->current - 0x80);
            if (underflow) {
                own->current = 0;
                self->y_vel = 0;
                own->rising = false;
            }
        }
    } else if (own->current != own->length) {
        int16_t speed = self->y_vel;
        self->y_vel += 0x70;
        own->current = (uint16_t)(own->current + (uint16_t)speed);

        if (own->current >= own->length) {
            own->current = own->length;
            self->y_vel = 0;
            own->rising = true;
            own->delay = 60;

            if ((self->render_flags & SST_RENDER_ON_SCREEN))
                QueueSound2(sfx_ChainStomp);
        }
    }

    CStom_UpdateBlockY(self, own);
}

// Type 0 - rises only while the matching switch is held down, and stops
// short of the ceiling if a pushable block is currently sitting on top of
// it (so the block doesn't get stuck poking into the ceiling).
static void CStom_SwitchActivated(Sst *self, Own_ChainStomp *own) {
    if (!f_switch[own->switch_id]) {
        if (own->current != own->length) {
            int16_t speed = self->y_vel;
            self->y_vel += 0x70;
            own->current = (uint16_t)(own->current + (uint16_t)speed);

            if (own->current >= own->length) {
                own->current = own->length;
                self->y_vel = 0;
                if ((self->render_flags & SST_RENDER_ON_SCREEN))
                    QueueSound2(sfx_ChainStomp);
            }
        }
        CStom_UpdateBlockY(self, own);
        return;
    }

    bool blocked_by_pushblock = obj31_ypos < 0 && (uint8_t)(own->current >> 8) == 0x10;
    if (!blocked_by_pushblock && own->current != 0) {
        if (((uint8_t)frame_count & 0xF) == 0 && (self->render_flags & SST_RENDER_ON_SCREEN))
            QueueSound2(sfx_ChainRise);

        bool underflow = own->current < 0x80;
        own->current = (uint16_t)(own->current - 0x80);
        if (underflow)
            own->current = 0;
    }

    self->y_vel = 0;
    CStom_UpdateBlockY(self, own);
}

// Types 3/5 - waits until Sonic is horizontally in range, then switches
// its own subtype to the next one along (always an auto-stomp type).
static void CStom_WaitForSonic(Sst *self, Own_ChainStomp *own) {
    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    if (d0 < 144)
        self->subtype++;
    CStom_UpdateBlockY(self, own);
}

static void CStom_Types(Sst *self, Own_ChainStomp *own) {
    switch (self->subtype & 0xF) {
    case 0:
        CStom_SwitchActivated(self, own);
        break;
    case 1:
    case 2:
    case 4:
    case 6:
        CStom_AutoStomp(self, own);
        break;
    case 3:
    case 5:
        CStom_WaitForSonic(self, own);
        break;
    default: // unused/undefined on real hardware too
        CStom_UpdateBlockY(self, own);
        break;
    }
}

static void CStom_MainBlock(Sst *self, Own_ChainStomp *own) {
    CStom_Types(self, own);
    obj31_ypos = self->y_pos.f.u;

    int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
    SstSolidObject(self, (uint16_t)x_rad, 24 / 2, 26 / 2, self->x_pos.f.u, NULL, NULL);

    if (((self->status & SST_STATUS_PLAYER_STAND) != 0) && (uint8_t)(own->current >> 8) < 0x10)
        KillSonicFrom(player, false);
}

static void CStom_Spikes(Sst *self, Own_ChainStomp *own) {
    Sst *parent = &sst[own->parent_index];
    Own_ChainStomp *pscratch = (Own_ChainStomp *)parent->own.u8;
    self->y_pos.f.u = (int16_t)(own->orig_y + (uint8_t)(pscratch->current >> 8));
}

static void CStom_Chain(Sst *self, Own_ChainStomp *own) {
    // Real ASM sets these once every tick this routine runs (not just at
    // spawn): obHeight=256/2 + the custom-height render bit, since the
    // chain can be up to 256px tall and the default ~32px assumed cull
    // height would treat most of a long chain as off-screen.
    // 128 doesn't fit in y_rad's signed int8_t range -- stored via its
    // bit-identical uint8_t cast (-128), which BuildSprites' own
    // explicit_height cull path already reinterprets back to unsigned 128 (see
    // its own comment on exactly this pattern, added for this same reason).
    self->y_radius = (int8_t)(uint8_t)(256 / 2);
    self->height_pixels = self->y_radius;
    Sst *parent = &sst[own->parent_index];
    Own_ChainStomp *pscratch = (Own_ChainStomp *)parent->own.u8;
    self->mapping_frame = (uint8_t)((((uint8_t)(pscratch->current >> 8)) >> 5) + 3);
    CStom_Spikes(self, own); // shared Y-alignment
}

static void CStom_Main(Sst *self, Own_ChainStomp *own) {
    uint8_t subtype = self->subtype;

    if (subtype & 0x80) {
        // Switch-activated -- both real table entries just map index N to
        // switch N (the subtype override value is always 0), so no
        // lookup table is needed here.
        own->switch_id = subtype & 0x7F;
        subtype = 0;
        self->subtype = subtype;
    }

    uint16_t length = cstom_lengths[subtype & 0xF];
    own->length = length;
    if ((subtype & 0xF) == 0)
        own->current = length; // spawn already fully extended

    bool spikeless = (subtype & 0xF0) == 0x20;
    Sst *last = self;

    for (int i = 0; i < 4; i++) {
        if (i == 1 && spikeless)
            continue;

        Sst *seg = self;
        if (i > 0) {
            seg = Sst_AllocateAfter(self);
            if (seg == NULL)
                break;
        }

        seg->routine = cstom_vars[i].routine;
        seg->code = Obj_ChainStomp;
        seg->x_pos.f.u = self->x_pos.f.u;
        seg->y_pos.f.u = (int16_t)(self->y_pos.f.u + cstom_vars[i].yoff);
        seg->mappings = Mappings_ChainedStompers;
        seg->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Spike_Stomper);
        seg->render_flags = SST_RENDER_LEVEL;

        Own_ChainStomp *sscratch = (Own_ChainStomp *)seg->own.u8;
        sscratch->orig_y = seg->y_pos.f.u;
        seg->subtype = subtype;
        seg->width_pixels = 32 / 2;
        sscratch->length = length;
        seg->priority = 4 * 0x80;
        seg->mapping_frame = cstom_vars[i].frame;

        if (i == 1) { // spikes
            seg->width_pixels = 112 / 2;
            seg->collision_flags = 0x90; // col_80x32 | col_hurt
        }

        sscratch->parent_index = (uint8_t)(self - sst);
        last = seg;
    }

    last->priority = 3 * 0x80; // whichever piece was spawned last (normally the ceiling base) gets a higher priority

    static const struct {
        uint8_t width, frame;
    } block_size[3] = {
        { 0x70 / 2, 0 },  // large
        { 0x60 / 2, 9 },  // medium
        { 0x20 / 2, 0xA }, // small
    };
    uint8_t size = (subtype >> 4) & 0xF;
    if (size > 2)
        size = 2;
    self->width_pixels = block_size[size].width;
    self->mapping_frame = block_size[size].frame;
}

void Obj_ChainStomp(Sst *self) {
    Own_ChainStomp *own = (Own_ChainStomp *)self->own.u8;

    switch (self->routine) {
    case 0:
        CStom_Main(self, own);
        __attribute__((fallthrough));
    case 2:
        CStom_MainBlock(self, own);
        break;
    case 4:
        CStom_Spikes(self, own);
        break;
    case 6: // base at ceiling -- doesn't move
        break;
    case 8:
        CStom_Chain(self, own);
        break;
    }

    SstOnScreenTest2(self, self->x_pos.f.u);
}
