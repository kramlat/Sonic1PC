// Metropolis' objects for Sonic 2 (the Simon Wai prototype's 42, 64, 65, 66, 68, 69 and 6D; the rest of the zone's are in MTZObjects2.c, the boxes (6A), the switch (47) and the others it shares are with other zones):
// the steam vent that throws you up (42), the pistons (64), the platforms that slide out (65) with the cog that turns beside them, the springs in the walls (66), the block with an arrow of spikes that turns (68),
// the screw nut that you spin up and down (69) and the harpoon in the floor (6D). All carry Sonic and Tails.
#include "Object/MTZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/CharThrow.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "SolidSst.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/MTZBlockArrow.h"
#include "Resource/Mappings/MTZCog.h"
#include "Resource/Mappings/MTZPiston.h"
#include "Resource/Mappings/MTZPlatformA.h"
#include "Resource/Mappings/MTZScrewNut.h"
#include "Resource/Mappings/MTZSpringWall.h"
#include "Resource/Mappings/MTZSteamVent.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = Character(who); chr != NULL; chr = NULL)

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

static void ForgetIfRemembered(Sst *self) {
    if (self->respawn)
        (*SstLoad_State(self)) &= 0x7F;
}

// MarkObjGone2: when x is out of sight the object is gone and forgotten, otherwise it is drawn
static void GoneIfOffscreen(Sst *self, int16_t x) {
    if (SstOutOfRange(x)) {
        ForgetIfRemembered(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 42: the steam vent. A flat top you stand on that, every $80 frames, sinks and springs back up (two frames), throwing whoever is on it up at $600 (the subtype's bits as a spring's) and letting out steam
// to either side (child objects that hurt for a few frames). It waits $80 frames at each end
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[3];   // 0x29-0x2B
    uint8_t pad1[6];   // 0x2C-0x31
    int16_t wait;      // 0x32
    int16_t base_y;    // 0x34
    int16_t dip;       // 0x36: how far it is down (the top goes from 0x10 down to 0)
} Own_SteamVent;

enum { SteamRoutine_Init = 0, SteamRoutine_Vent = 2, SteamRoutine_Puff = 4 };
enum { SteamState_Wait = 0, SteamState_Rise = 2, SteamState_Pause = 4, SteamState_Sink = 6 };

static void SteamVent_Puff(Sst *self, Own_SteamVent *own, int16_t dx, bool flip) {
    Sst *puff = Sst_Allocate();
    if (puff == NULL)
        return;
    puff->code = self->code;
    puff->routine = SteamRoutine_Puff;
    puff->x_pos.f.u = (int16_t)(self->x_pos.f.u + dx);
    puff->y_pos.f.u = own->base_y;
    puff->anim_frame_timer = 7;
    puff->mappings = self->mappings;
    puff->art_tile = TILE_MAP(0, 1, 0, 0, 0x405);
    puff->render_flags = SST_RENDER_LEVEL;
    SST_RENDER_SET(puff, SST_RENDER_X_FLIP, flip);
    puff->width_pixels = 0x18;
    puff->priority = 4 * 0x80;
}

static void SteamVent_Throw(Sst *self, Own_SteamVent *own, Sst *chr) {
    if (self->routine_secondary != SteamState_Rise)
        return;
    chr->y_vel = -0xA00; // (Nick Arcade's was -$600: the alpha's vent throws harder)
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (self->subtype & 0x80)
        chr->x_vel = 0;
    CharThrow_Tumble(chr, self->subtype, 4, 0, 1);
    PlaySound(sfx_Spring);
}

void Obj_MTZSteamVent(Sst *self) {
    Own_SteamVent *own = (Own_SteamVent *)self->own.u8;

    if (self->routine == SteamRoutine_Puff) { // a puff of steam: hurts from its fourth frame on (the alpha's: Nick Arcade's was the third, with a smaller hitbox), and goes after its seventh
        if (--self->anim_frame_timer >= 0) {
            Sst_Draw(self);
            return;
        }
        self->anim_frame_timer = 7;
        self->collision_flags = 0;
        self->mapping_frame++;
        if (self->mapping_frame == 3)
            self->collision_flags = 0xA6;
        if (self->mapping_frame == 7) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;
    }

    if (self->routine == SteamRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_MTZSteamVent;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        self->mapping_frame = 7;
        own->base_y = self->y_pos.f.u;
        own->dip = 0x10;
        self->y_pos.f.u += 0x10;
    }

    FOR_EACH_CHARACTER(chr, who) {
        SolidSst_Character(self, chr, who, 0x1B, 0x10, 0x10, self->x_pos.f.u, NULL);
        if (self->status & (8 << who))
            SteamVent_Throw(self, own, chr);
    }

    switch (self->routine_secondary) {
    case SteamState_Wait:
        if (--own->wait < 0) {
            own->wait = 0x7F;
            self->routine_secondary += 2;
        }
        break;
    case SteamState_Rise:
        own->dip -= 8;
        if (own->dip == 0) {
            self->routine_secondary += 2;
            SteamVent_Puff(self, own, 0x28, false);
            SteamVent_Puff(self, own, -0x28, true);
        }
        self->y_pos.f.u = (int16_t)(own->dip + own->base_y);
        break;
    case SteamState_Pause:
        if (--own->wait < 0) {
            own->wait = 0x7F;
            self->routine_secondary += 2;
        }
        break;
    default: // sinking again
        own->dip += 8;
        if (own->dip == 0x10)
            self->routine_secondary = SteamState_Wait;
        self->y_pos.f.u = (int16_t)(own->dip + own->base_y);
        break;
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 64: the piston. A solid block (a big or a small one, by the subtype's high nibble) that, with subtype 1, moves in and out by its reach ($40) 8 pixels a frame and waits $3C frames at each end
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: 0 still, 1 moving (the high nibble was the size)
    uint8_t pad0[5];   // 0x29-0x2D
    uint8_t height;    // 0x2E: its half height, for the solid
    uint8_t pad1;      // 0x2F
    int16_t base_y;    // 0x30
    uint8_t pad2[2];   // 0x32-0x33
    int16_t base_x;    // 0x34
    int16_t wait;      // 0x36
    uint8_t out;       // 0x38: it is going out (1) or in (0)
    uint8_t pad3;      // 0x39
    int16_t offset;    // 0x3A: how far it has moved
    int16_t reach;     // 0x3C
} Own_Piston;

// The sizes (loc_1A8C6): half width, half height and reach, by the subtype's high nibble
static const uint8_t piston_sizes[2][3] = { { 0x40, 0x0C, 0x40 }, { 0x10, 0x20, 0x40 } };

void Obj_MTZPiston(Sst *self) {
    Own_Piston *own = (Own_Piston *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        const int size = (self->subtype >> 4) & 1; // (the prototype reads past its two entries for the rest: no layout has them)
        self->width_pixels = piston_sizes[size][0];
        own->height = piston_sizes[size][1];
        self->mapping_frame = (uint8_t)size;
        if (size == 0) {
            self->y_radius = 0x6C;
            self->height_pixels = self->y_radius;
        }
        self->mappings = Mappings_MTZPiston;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        own->reach = piston_sizes[size][2];
        self->subtype &= 0xF;
    }

    const int16_t old_x = self->x_pos.f.u;
    if (self->subtype == 1) {
        if (!own->out) {
            if (own->offset != 0) {
                own->offset -= 8;
            } else if (--own->wait < 0) {
                own->wait = 0x3C;
                own->out = 1;
            }
        }
        if (own->out) {
            if (own->offset != own->reach) {
                own->offset += 8;
            } else if (--own->wait < 0) {
                own->wait = 0x3C;
                own->out = 0;
            }
        }
        int16_t d0 = own->offset;
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(0x40 - d0);
        self->y_pos.f.u = (int16_t)(own->base_y + d0);
    }

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who)
            SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), own->height, (int16_t)(own->height + 1), old_x, NULL);
    }
    SstOnScreenTest2(self, own->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 65: the platform. Four kinds by the subtype's high nibble (0: a wide one $80 out, 1: a narrower one that comes out when someone is near, 2: only the cog of one, 3: a wide one that is out and goes in) and
// modes by its low (or, with bit 7, the kind's own): 0 still, 1 slides out when its switch is down (and then back $B4 frames later), 2 the way back, 3 slides out as someone comes near, 4 waits to be stood on
// and then 5 goes to and fro between $1800 and $1B40, 6 and 7 the same as 1 and 2 the other way round. A cog (a child object) turns beside it with its position
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the mode
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    uint8_t pad1[2];   // 0x32-0x33
    int16_t base_x;    // 0x34
    int16_t wait;      // 0x36
    uint8_t out;       // 0x38: it is going out (the switch has been seen)
    uint8_t pad2;      // 0x39
    int16_t offset;    // 0x3A: how far out it is
    int16_t reach;     // 0x3C: (the cog's: the slot of the platform, in 0x3C as a byte)
    uint8_t switch_id; // 0x3E
    uint8_t pad3;      // 0x3F
} Own_MTZPlatform;

typedef struct {
    uint8_t pad0[0x14]; // 0x28-0x3B
    uint8_t platform;   // 0x3C: the slot of the platform
} Own_MTZCog;

enum { PlatformRoutine_Init = 0, PlatformRoutine_Main = 2, PlatformRoutine_Cog = 4, PlatformRoutine_FreeCog = 6 };

static int16_t mtz_platform_cog_x; // MTZ_Platform_Cog_X: where the platform that goes to and fro is, which the loose cog follows

// The platforms' sizes (loc_1AA8A): half width, half height, reach and mode, by the subtype's high nibble
static const uint8_t platform_sizes[4][4] = { { 0x40, 0x0C, 0x80, 0x01 }, { 0x20, 0x0C, 0x40, 0x03 }, { 0x10, 0x10, 0x20, 0x00 }, { 0x40, 0x0C, 0x80, 0x07 } };
// The cog's frame by how far it is along (loc_1AE1E)
static const uint8_t cog_frames[8] = { 0, 0, 2, 2, 2, 1, 1, 1 };

static void Platform_SetX(Sst *self, Own_MTZPlatform *own, int16_t flip_total) {
    int16_t d0 = own->offset;
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(flip_total - d0);
    self->x_pos.f.u = (int16_t)(own->base_x - d0);
}

static void Platform_Remember(Sst *self, bool set) {
    if (!self->respawn)
        return;
    if (set)
        (*SstLoad_State(self)) |= 1;
    else
        (*SstLoad_State(self)) &= (uint8_t)~1;
}

void Obj_MTZPlatform(Sst *self) {
    Own_MTZPlatform *own = (Own_MTZPlatform *)self->own.u8;

    if (self->routine == PlatformRoutine_Cog) { // the cog beside a platform turns with how far out the platform is
        Sst *platform = &sst[((Own_MTZCog *)self->own.u8)->platform];
        const int16_t offset = ((Own_MTZPlatform *)platform->own.u8)->offset;
        self->mapping_frame = cog_frames[offset & 7];
        SstOnScreenTest(self);
        return;
    }
    if (self->routine == PlatformRoutine_FreeCog) { // ... and a loose one with the platform that goes to and fro
        self->mapping_frame = cog_frames[mtz_platform_cog_x & 7];
        SstOnScreenTest(self);
        return;
    }

    if (self->routine == PlatformRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_MTZPlatformA;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        const int kind = (self->subtype >> 4) & 3;
        self->width_pixels = platform_sizes[kind][0];
        self->y_radius = platform_sizes[kind][1];
        self->mapping_frame = (uint8_t)kind;
        if (kind == 1)
            self->status |= 0x80;
        if (kind == 2) { // only the cog
            self->routine += 4;
            self->mappings = Mappings_MTZCog;
            self->art_tile = TILE_MAP(0, 3, 0, 0, 0x55F);
            self->mapping_frame = cog_frames[mtz_platform_cog_x & 7];
            SstOnScreenTest(self);
            return;
        }
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        own->reach = platform_sizes[kind][2];
        if (self->subtype & 0x80) {
            own->switch_id = self->subtype & 0xF;
            self->subtype = platform_sizes[kind][3];
            if (self->subtype == 7)
                own->offset = own->reach;
            Sst *cog = Sst_AllocateAfter(self);
            if (cog != NULL) {
                cog->code = self->code;
                cog->routine = PlatformRoutine_Cog;
                cog->x_pos.f.u = (int16_t)(self->x_pos.f.u - 0x4C);
                cog->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x14);
                cog->render_flags = 0;
                if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
                    cog->x_pos.f.u = (int16_t)(cog->x_pos.f.u + 0x18);
                    SST_RENDER_SET(cog, SST_RENDER_X_FLIP, true);
                }
                cog->mappings = Mappings_MTZCog;
                cog->art_tile = TILE_MAP(0, 3, 0, 0, 0x55F);
                cog->render_flags |= SST_RENDER_LEVEL;
                cog->width_pixels = 0x10;
                cog->priority = 4 * 0x80;
                ((Own_MTZCog *)cog->own.u8)->platform = (uint8_t)(self - sst);
            }
            ForgetIfRemembered(self);
        }
        self->subtype &= 0xF;
    }

    const int16_t old_x = self->x_pos.f.u;
    Sst *sonic = player, *tails = TAILS_OBJ;
    switch (self->subtype) {
    case 0:
        break;
    case 1: // slides out when its switch is down
    case 6: // (6: after its wait)
    case 7: // (7: out, goes in when its switch is down)
    case 2: { // (2: out, goes in after its wait)
        const int mode = self->subtype;
        const bool out_direction = (mode == 1 || mode == 6);
        if (mode == 6) {
            if (!own->out) {
                if (--own->wait == 0)
                    own->out = 1;
                else {
                    Platform_SetX(self, own, 0x80);
                    break;
                }
            }
        } else if (mode == 1 || mode == 7) {
            if (!own->out) {
                if (!(f_switch[own->switch_id] & 1)) {
                    Platform_SetX(self, own, 0x80);
                    break;
                }
                own->out = 1;
            }
        } else if (mode == 2) {
            if (!own->out) {
                if (--own->wait == 0)
                    own->out = 1;
                else {
                    Platform_SetX(self, own, 0x80);
                    break;
                }
            }
        }
        if (out_direction) {
            if (own->reach == own->offset) { // all the way out: the mode changes and it waits
                self->subtype++;
                own->wait = 0xB4;
                own->out = 0;
                Platform_Remember(self, true);
            } else {
                own->offset += 2;
            }
        } else if (own->offset == 0) { // all the way in
            self->subtype--;
            own->wait = 0xB4;
            own->out = 0;
            Platform_Remember(self, false);
        } else {
            own->offset -= 2;
        }
        Platform_SetX(self, own, 0x80);
        break;
    }
    case 3: { // slides out as a character comes near (to the right of it; the left if flipped), and back
        int16_t x0 = own->base_x, x1 = own->base_x;
        if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
            x0 -= 0x20;
            x1 += 0x60;
        } else {
            x0 -= 0xA0;
            x1 -= 0x20;
        }
        const int16_t y0 = (int16_t)(self->y_pos.f.u - 0x10), y1 = (int16_t)(self->y_pos.f.u + 0x40);
        bool near = false;
        for (int i = 0; i < 2; i++) {
            Sst *chr = i == 0 ? sonic : tails;
            if ((uint16_t)(chr->x_pos.f.u - x0) < (uint16_t)(x1 - x0) && (uint16_t)(chr->y_pos.f.u - y0) < (uint16_t)(y1 - y0))
                near = true;
        }
        if (near) {
            if (own->reach == own->offset)
                break; // (it stays where it is)
            own->offset += 0x10;
        } else if (own->offset != 0) {
            own->offset -= 0x10;
        }
        Platform_SetX(self, own, 0x40);
        break;
    }
    case 4: // waits to be stood on
        if (self->status & 8)
            self->subtype++;
        break;
    case 5: // goes to and fro (the alpha's: out to $1BC0 and back to $1880; in the third act it goes right to $2940 and stays)
        if (!own->out) {
            self->x_pos.f.u += 2;
            if (LEVEL_ZONE(level_id) == ZoneId_MTZ && LEVEL_ACT(level_id) == 2) { // (the third act of Metropolis)
                if (self->x_pos.f.u == 0x2940)
                    self->subtype = 0;
            } else if (self->x_pos.f.u == 0x1BC0) {
                own->out = 1;
            }
        } else {
            self->x_pos.f.u -= 2;
            if (self->x_pos.f.u == 0x1880)
                own->out = 0;
        }
        own->base_x = self->x_pos.f.u;
        mtz_platform_cog_x = self->x_pos.f.u;
        break;
    }

    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
    if (SstOutOfRange(own->base_x)) {
        ForgetIfRemembered(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 66: the spring in the wall. Solid and not drawn (the prototype only draws it in debug mode): a character in the air that pushes against its open side (the right, or the left if flipped) is thrown away
// from it and up, at $800 each way, rolling
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
} Own_WallSpring;

static void WallSpring_Throw(Sst *self, Own_WallSpring *own, Sst *chr, int who) {
    chr->x_vel = -0x800;
    chr->y_vel = -0x800;
    chr->status |= SST_PSTATUS_X_FLIP;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
        chr->x_vel = (int16_t)-chr->x_vel;
    }
    ((Scratch_Sonic *)chr->own.u8)->control_lock = 0xF;
    chr->ground_vel = chr->x_vel;
    if (!((chr->status & SST_PSTATUS_IN_BALL) != 0))
        chr->anim = SonAnimId_Walk;
    if (self->subtype & 0x80)
        chr->y_vel = 0;
    CharThrow_Tumble(chr, self->subtype, 8, 1, 3);
    self->status &= (uint8_t)~0x60;
    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    PlaySound(sfx_Spring);
    (void)who;
}

void Obj_MTZSpringWall(Sst *self) {
    Own_WallSpring *own = (Own_WallSpring *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_MTZSpringWall;
        self->art_tile = TILE_MAP(1, 0, 0, 0, 0x680);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 8;
        self->priority = 4 * 0x80;
        self->y_radius = 0x40;
        self->mapping_frame = (self->subtype >> 4) & 7;
        if (self->mapping_frame != 0)
            self->y_radius = (int8_t)0x80;
    }

    FOR_EACH_CHARACTER(chr, who) {
        const uint8_t height = (uint8_t)self->y_radius;
        if (SolidSst_Character(self, chr, who, 0x13, height, (int16_t)(height + 1), self->x_pos.f.u, NULL) != 1)
            continue;
        if (!((chr->status & SST_PSTATUS_IN_AIR) != 0))
            continue;
        uint8_t d1 = self->status;
        if (self->x_pos.f.u >= chr->x_pos.f.u)
            d1 ^= 1;
        if (!(d1 & 1))
            WallSpring_Throw(self, own, chr, who);
    }
    if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    if (debug_use)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 68: the block with an arrow. A solid block (frame 4) that makes a spiked arrow (a child object that is the same object) which slides out to $20 pixels and back, in the direction (up, right, down, left) of
// its frame, and then turns a quarter to the next, a long breath between. Object 6D (the harpoon in the floor) is the same spike
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_x;    // 0x30
    int16_t base_y;    // 0x32
    uint16_t slide;    // 0x34: how far it has slid out (16.8: the high byte is pixels)
    int16_t going_in;  // 0x36
    int16_t waiting;   // 0x38
    int16_t pause;     // 0x3A: (the harpoon's)
} Own_BlockArrow;

enum { ArrowRoutine_Init = 0, ArrowRoutine_Block = 2, ArrowRoutine_Arrow = 4 };

static const uint8_t arrow_cols[4] = { 0x84, 0xA6, 0x84, 0xA6 };

void Obj_MTZBlockArrow(Sst *self) {
    Own_BlockArrow *own = (Own_BlockArrow *)self->own.u8;

    if (self->routine == ArrowRoutine_Arrow) {
        // loc_1B656: the slide
        bool slide = true;
        if (own->waiting != 0) {
            if ((frame_count & 0x3F) != 0)
                slide = false;
            else {
                own->waiting = 0;
                if ((self->render_flags & SST_RENDER_ON_SCREEN))
                    PlaySound(sfx_SpikesMove);
            }
        }
        if (slide) {
            if (own->going_in) {
                own->slide = (uint16_t)(own->slide - 0x800);
                if (own->slide > 0x2000) { // (it went below 0: a borrow)
                    own->slide = 0;
                    own->going_in = 0;
                    own->waiting = 1;
                    self->routine_secondary = (uint8_t)((self->routine_secondary + 1) & 3);
                    self->mapping_frame = self->routine_secondary;
                    self->collision_flags = arrow_cols[self->routine_secondary];
                }
            } else {
                own->slide = (uint16_t)(own->slide + 0x800);
                if (own->slide >= 0x2000) {
                    own->slide = 0x2000;
                    own->going_in = 1;
                    own->waiting = 1;
                }
            }
        }
        const int16_t hi = (int16_t)(own->slide >> 8);
        switch (self->routine_secondary) {
        case 0: self->y_pos.f.u = (int16_t)(own->base_y - hi); break;
        case 1: self->x_pos.f.u = (int16_t)(own->base_x + hi); break;
        case 2: self->y_pos.f.u = (int16_t)(own->base_y + hi); break;
        default: self->x_pos.f.u = (int16_t)(own->base_x - hi); break;
        }
        GoneIfOffscreen(self, own->base_x);
        return;
    }

    if (self->routine == ArrowRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_MTZBlockArrow;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x414);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        Sst *arrow = Sst_AllocateAfter(self);
        if (arrow != NULL) {
            arrow->code = self->code;
            arrow->routine = ArrowRoutine_Arrow;
            arrow->x_pos.f.u = self->x_pos.f.u;
            arrow->y_pos.f.u = self->y_pos.f.u;
            Own_BlockArrow *other = (Own_BlockArrow *)arrow->own.u8;
            other->base_x = arrow->x_pos.f.u;
            other->base_y = arrow->y_pos.f.u;
            arrow->mappings = self->mappings;
            arrow->art_tile = TILE_MAP(0, 1, 0, 0, 0x41C);
            arrow->render_flags |= SST_RENDER_LEVEL;
            arrow->width_pixels = 0x10;
            arrow->priority = 4 * 0x80;
            const uint16_t t = (uint16_t)(frame_count >> 6);
            other->going_in = (int16_t)(t & 1);
            const int direction = (((t >> 1) + self->subtype) & 3);
            arrow->routine_secondary = (uint8_t)direction;
            arrow->mapping_frame = (uint8_t)direction;
            arrow->collision_flags = arrow_cols[direction];
        }
        self->mapping_frame = 4;
    }

    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Character(self, chr, who, 0x1B, 0x10, 0x11, self->x_pos.f.u, NULL);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6D: the harpoon in the floor. A spike (the block arrow's frame 0, hurting) that comes up $20 pixels, and goes down, on a beat the subtype is the offset of: it waits the other half of its beat
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_MTZHarpoon(Sst *self) {
    Own_BlockArrow *own = (Own_BlockArrow *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_MTZBlockArrow;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x41C);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 4;
        self->priority = 4 * 0x80;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        self->collision_flags = 0x84;
    }

    // loc_1B788
    if (own->pause != 0) {
        own->pause--;
    } else {
        bool move = true;
        if (own->waiting != 0) {
            if (((frame_count - self->subtype) & 0x7F) != 0)
                move = false;
            else
                own->waiting = 0;
        }
        if (move) {
            if (own->going_in) {
                own->slide = (uint16_t)(own->slide - 0x400);
                if (own->slide > 0x2000) {
                    own->slide = 0;
                    own->going_in = 0;
                    own->waiting = 1;
                }
            } else {
                own->slide = (uint16_t)(own->slide + 0x400);
                if (own->slide >= 0x2000) {
                    own->slide = 0x2000;
                    own->going_in = 1;
                    own->pause = 3;
                }
            }
        }
    }
    self->y_pos.f.u = (int16_t)(own->base_y - (int16_t)(own->slide >> 8));
    GoneIfOffscreen(self, own->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 69: the screw nut. Stand on it and roll to its middle, and walking across it turns the screw: the nut goes down (or up) as the frames turn, by the subtype's range x 8; at the bottom of a subtype of bit 7 it
// falls to the floor. Each character has a state of his own (the prototype keeps them at 0x38 and 0x3C)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[3];   // 0x29-0x2B
    uint8_t pad1[4];   // 0x2C-0x2F
    uint8_t pad2[2];   // 0x30-0x31
    int16_t base_y;    // 0x32
    int16_t turn;      // 0x34: how far the screw has been turned
    int16_t range;     // 0x36
    uint8_t state[2];  // 0x38 and 0x3A? (the prototype's are 0x38 and 0x3C: here each is a state byte and a side byte)
    uint8_t side[2];
} Own_ScrewNut;

enum { NutRoutine_Init = 0, NutRoutine_Screw = 2, NutRoutine_Fall = 4, NutRoutine_Solid = 6 };

static void ScrewNut_Frame(Sst *self, Own_ScrewNut *own) {
    const int16_t d1 = (int16_t)(own->turn >> 3);
    self->mapping_frame = (uint8_t)(((d1 >> 1) & 3));
    self->y_pos.f.u = (int16_t)(own->base_y - d1);
}

static void ScrewNut_Char(Sst *self, Own_ScrewNut *own, Sst *chr, int who) {
    uint8_t *state = &own->state[who];
    uint8_t *side = &own->side[who];
    if (!(self->status & (8 << who)))
        *state = 0;
    switch (*state) {
    case 0:
        if (!(self->status & (8 << who)))
            return;
        *state += 2;
        *side = (uint8_t)(self->x_pos.f.u < chr->x_pos.f.u);
        // Fallthrough
    case 2: {
        int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u);
        if (*side)
            d0 += 0xF;
        if ((uint16_t)d0 < 0x10) {
            chr->x_pos.f.u = self->x_pos.f.u;
            *state += 2;
        }
        return;
    }
    default: {
        const int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u);
        if (d0 < 0) { // walking left of the middle turns it down
            own->turn += d0;
            chr->x_pos.f.u = self->x_pos.f.u;
            ScrewNut_Frame(self, own);
            const int16_t depth = (int16_t)(-(own->turn >> 3));
            if (depth < own->range)
                return;
            self->y_pos.f.u = (int16_t)(own->base_y + own->range);
            own->turn = (int16_t)(-(own->range << 3));
            self->mapping_frame = 0;
            if (!(self->subtype & 0x80))
                *state = 0;
            else
                self->routine = NutRoutine_Fall;
        } else { // ... and right turns it up
            own->turn += d0;
            chr->x_pos.f.u = self->x_pos.f.u;
            ScrewNut_Frame(self, own);
        }
        return;
    }
    }
}

void Obj_MTZScrewNut(Sst *self) {
    Own_ScrewNut *own = (Own_ScrewNut *)self->own.u8;

    switch (self->routine) {
    case NutRoutine_Init:
        self->routine += 2;
        self->mappings = Mappings_MTZScrewNut;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x500);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->y_radius = 0x0B;
        self->priority = 4 * 0x80;
        own->base_y = self->y_pos.f.u;
        own->range = (int16_t)((self->subtype & 0x7F) << 3);
        // Fallthrough
    case NutRoutine_Screw:
        FOR_EACH_CHARACTER(chr, who)
            ScrewNut_Char(self, own, chr, who);
        break;
    case NutRoutine_Fall: {
        Sst_MoveSprite2(self);
        self->y_vel += 0x38;
        const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < 0) {
            self->y_pos.f.u += d1;
            self->y_vel = 0;
            self->routine += 2;
        }
        break;
    }
    default:
        break;
    }
    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Character(self, chr, who, 0x2B, 0x0C, 0x0D, self->x_pos.f.u, NULL);
    SstOnScreenTest(self);
}
