// Oil Ocean's objects for Sonic 2 (the Simon Wai prototype's 07, 33, 43, 45, 46 and 48; the collapsing platform, 1F, the switch, 47, the elevator, 19, and the swing, 15, are shared with other zones): the oil that
// is stood on and kills whoever stays in it (07), the platform that hops (33), the pair of spiked balls that roll to and fro (43), the spring and the pusher with a spring behind it (45), the ball that rolls
// off a spring (46) and the cannon that catches a character and fires it off (48). All carry Sonic and Tails.
#include "Standing.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/OOZObjects.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/CharControl.h"
#include "Object/CharThrow.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/OOZFire.h"
#include "Resource/Mappings/OOZCannon.h"
#include "Resource/Mappings/OOZFire.h"
#include "Resource/Mappings/OOZLauncher.h"
#include "Resource/Mappings/OOZPushSpring.h"
#include "Resource/Mappings/OOZSpikeball.h"
#include "Resource/Mappings/OOZSpringBall.h"

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

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 07: the oil. The level makes it (in slot $1E); it has no picture. Its top is at $758 less how deep each character has sunk (they sink one pixel a frame while they stand on it, up to $30, and
// come back as slowly when they do not): a character that has sunk all the way is killed. It follows each character's x
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F
    int16_t top;       // 0x30
    uint8_t pad1[6];   // 0x32-0x37
    uint8_t depth;     // 0x38: how far Sonic has not sunk (it starts at $30; Tails' is a byte of its own, below)
    uint8_t pad2;      // 0x39
    uint8_t depth_tails; // 0x3A (it starts at 0: the prototype only sets Sonic's)
} Own_Oil;

void Obj_OilSurface(Sst *self) {
    Own_Oil *own = (Own_Oil *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->y_pos.f.u = 0x758;
        self->width_pixels = 0x20;
        own->top = self->y_pos.f.u;
        own->depth = 0x30;
        self->status |= 0x80;
    }

    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr == NULL)
            continue;
        uint8_t *depth = (who == SolidChar_Sonic) ? &own->depth : &own->depth_tails;
        const uint8_t stand_bit = (uint8_t)(8 << who);
        if (self->status & stand_bit) {
            if (*depth == 0) { // all the way in: suffocated
                self->status &= (uint8_t)~stand_bit;
                if (who == SolidChar_Sonic)
                    KillSonicFrom(chr, false);
                else
                    KillTails(chr);
                return;
            }
            (*depth)--;
        } else if (*depth != 0x30) {
            (*depth)++;
        }
        self->x_pos.f.u = chr->x_pos.f.u;
        Solid_Platform(self, chr, who, 0x20, *depth, chr->x_pos.f.u);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 33: the launching platform (the burner lid). With subtype 0 it hops: every $78 frames it leaps and bounces on its place until it settles. With another subtype it waits for someone standing
// in the middle of it (both, if both stand on it) to hold them still, then rises $7D pixels and throws them up and away
// ---------------------------------------------------------------------------------------------------------------------------------------
// (the speed is a 16.16 number, a long at 0x32 in the prototype: it has a place of its own here, at 0x34, and the wait follows it: neither is shared with anything in this object)
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t home_y;    // 0x30
    uint8_t pad1[2];   // 0x32-0x33
    int32_t speed;     // 0x34 (the prototype's long is at 0x32; nothing else of this object is at 0x34)
    int16_t wait;      // 0x38 (the prototype's is at 0x36)
    uint8_t link;      // 0x3C: (the fire's) the slot of the platform
} Own_Launcher;

enum { Launcher_Wait = 0, Launcher_Bounce = 2, Launcher_Ready = 4, Launcher_Rise = 6, Launcher_Done = 8 };
enum { LauncherRoutine_Init = 0, LauncherRoutine_Main = 2, LauncherRoutine_Fire = 4 };

// The alpha's platform has fire under it (an object of its own: the platform makes it, in the place the lid was at, $10 up from its middle): it is seen and hurts while the lid is $14 or more above it
// (Offset_0x018BCA)
static void Launcher_Fire(Sst *self, Own_Launcher *own) {
    const Sst *lid = &sst[own->link];
    if ((int16_t)(self->y_pos.f.u - lid->y_pos.f.u) < 0x14) {
        self->collision_flags = 0;
        self->anim_frame = 0;
        return;
    }
    self->collision_flags = 0x9B;
    Sst_Animate(self, Animation_OOZFire);
    SstOnScreenTest(self);
}

static void Launcher_Hold(Sst *chr) {
    OBJ_CONTROL(chr) = 1;
    chr->ground_vel = 0;
    chr->x_vel = 0;
    chr->y_vel = 0;
    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    chr->art_tile &= ~TILE_PRIORITY_AND;
}

static void Launcher_Launch(Sst *self, Sst *chr, int who) {
    if (!(self->status & (8 << who)))
        return;
    chr->x_pos.f.u = self->x_pos.f.u;
    chr->anim = SonAnimId_Roll;
    chr->ground_vel = 0x800;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->y_vel = -0x1000;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    OBJ_CONTROL(chr) = 0;
    PlaySound(sfx_Spring);
}

void Obj_OOZLauncher(Sst *self) {
    Own_Launcher *own = (Own_Launcher *)self->own.u8;

    if (self->routine == LauncherRoutine_Fire) {
        Launcher_Fire(self, own);
        return;
    }
    if (self->routine == LauncherRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_OOZLauncher;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x32C);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x18;
        own->home_y = self->y_pos.f.u;
        self->routine_secondary = Launcher_Bounce;
        own->wait = 0x78;
        if (self->subtype)
            self->routine_secondary = Launcher_Ready;
        Sst *fire = Sst_AllocateAfter(self);
        if (fire != NULL) {
            fire->code = self->code;
            fire->routine = LauncherRoutine_Fire;
            fire->x_pos.f.u = self->x_pos.f.u;
            fire->y_pos.f.u = (int16_t)(self->y_pos.f.u - 0x10);
            fire->mappings = Mappings_OOZFire;
            fire->art_tile = TILE_MAP(0, 3, 0, 0, 0x2E2); // ($5C40)
            fire->render_flags = SST_RENDER_LEVEL;
            fire->priority = 4 * 0x80;
            fire->width_pixels = 0x10;
            ((Own_Launcher *)fire->own.u8)->link = (uint8_t)(self - sst);
        }
    }

    const int16_t old_x = self->x_pos.f.u;
    switch (self->routine_secondary) {
    case Launcher_Wait:
        if (--own->wait < 0) {
            own->wait = 0x78;
            own->speed = (int32_t)0xFFF69800;
            self->routine_secondary += 2;
            PlaySound(sfx_OOZLidPop);
        }
        break;
    case Launcher_Bounce: {
        const int32_t y = self->y_pos.v + own->speed;
        self->y_pos.v = y;
        own->speed += 0x3800;
        if ((uint16_t)(y >> 16) < (uint16_t)own->home_y)
            break;
        int32_t speed = own->speed;
        if ((uint32_t)speed < 0x10000)
            self->routine_secondary -= 2;
        own->speed = -(int32_t)((uint32_t)speed >> 2);
        self->y_pos.f.u = own->home_y;
        break;
    }
    case Launcher_Ready: { // waits for a character in the middle of it
        const int16_t left = (int16_t)(self->x_pos.f.u - 0x10), right = (int16_t)(self->x_pos.f.u + 0x10);
        const uint8_t on = self->status & 0x18;
        if (on == 0)
            break;
        if (on == 0x18) { // both are on it: both must be in the middle
            const int16_t sx = player->x_pos.f.u, tx = TAILS_OBJ->x_pos.f.u;
            if (sx < left || sx >= right || tx < left || tx >= right)
                break;
            Launcher_Hold(player);
            Launcher_Hold(TAILS_OBJ);
        } else {
            const int who = (on & 8) ? SolidChar_Sonic : SolidChar_Tails;
            Sst *chr = Character(who);
            if (chr == NULL || chr->x_pos.f.u < left || chr->x_pos.f.u >= right)
                break;
            Launcher_Hold(chr);
        }
        own->speed = (int32_t)0xFFF69800;
        self->routine_secondary += 2;
        PlaySound(sfx_OOZLidPop);
        break;
    }
    case Launcher_Rise: {
        const int32_t y = self->y_pos.v + own->speed;
        self->y_pos.v = y;
        own->speed += 0x3800;
        if ((int16_t)(y >> 16) != (int16_t)(own->home_y - 0x7D))
            break;
        self->routine_secondary += 2;
        FOR_EACH_CHARACTER(chr, who)
            Launcher_Launch(self, chr, who);
        break;
    }
    default:
        break;
    }

    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 8, 9, old_x, NULL);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 43: the spiked balls. One or a pair roll to and fro between two limits (the subtype picks how far and how they start: 0 one ball, 6 and $C a pair), and a pair turn round where they meet
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t centre_x;  // 0x30
    int16_t left;      // 0x32
    int16_t right;     // 0x34
    uint8_t right_going; // 0x36
    uint8_t pad1;      // 0x37
    uint8_t pad2[4];   // 0x38-0x3B
    uint8_t partner;   // 0x3C: the slot of the other ball (its own, for a ball on its own)
} Own_Spikeball;

enum { SpikeballRoutine_Init = 0, SpikeballRoutine_First = 2, SpikeballRoutine_Second = 4 };

// The sizes by subtype (loc_17F20): balls - 1, how far each way from the middle, and how far the balls start from it
static const struct { uint8_t more; uint8_t reach; int16_t first_x, second_x; } spikeball_sizes[3] = {
    { 0, 0x68, 0, 0 }, { 1, 0xE8, -0x18, 0x18 }, { 1, 0xA8, -0x58, -0x28 },
};

static void Spikeball_Setup(Sst *ball) {
    Own_Spikeball *own = (Own_Spikeball *)ball->own.u8;
    ball->mappings = Mappings_OOZSpikeball;
    ball->art_tile = TILE_MAP(1, 2, 0, 0, 0x30C);
    ball->render_flags = SST_RENDER_LEVEL;
    ball->priority = 4 * 0x80;
    ball->width_pixels = 0x18;
    ball->collision_flags = 0xA5;
    own->centre_x = ball->x_pos.f.u;
}

static void Spikeball_Move(Sst *ball) {
    Own_Spikeball *own = (Own_Spikeball *)ball->own.u8;
    if (!own->right_going) {
        ball->x_pos.f.u--;
        if (ball->x_pos.f.u == own->left) {
            own->right_going = 1;
            PlaySound(sfx_SlidingSpike);
        }
    } else {
        ball->x_pos.f.u++;
        if (ball->x_pos.f.u == own->right) {
            own->right_going = 0;
            PlaySound(sfx_SlidingSpike);
        }
    }
}

void Obj_OOZSpikeball(Sst *self) {
    Own_Spikeball *own = (Own_Spikeball *)self->own.u8;

    switch (self->routine) {
    case SpikeballRoutine_Init: {
        self->routine += 2;
        const int kind = (self->subtype == 6) ? 1 : (self->subtype == 0xC) ? 2 : 0;
        const int more = spikeball_sizes[kind].more;
        const int16_t reach = spikeball_sizes[kind].reach;
        Sst *last = self;
        Spikeball_Setup(self);
        for (int i = 0; i < more; i++) {
            Sst *other = Sst_AllocateAfter(self);
            if (other == NULL)
                break;
            other->code = self->code;
            other->routine = SpikeballRoutine_Second;
            other->x_pos.f.u = self->x_pos.f.u;
            other->y_pos.f.u = self->y_pos.f.u;
            ((Own_Spikeball *)other->own.u8)->right_going = 1;
            Spikeball_Setup(other);
            last = other;
        }
        ((Own_Spikeball *)last->own.u8)->partner = (uint8_t)(self - sst);
        own->partner = (uint8_t)(last - sst);
        const int16_t left = (int16_t)(own->centre_x - reach);
        const int16_t right = (int16_t)(left + reach + reach);
        own->left = ((Own_Spikeball *)last->own.u8)->left = left;
        own->right = ((Own_Spikeball *)last->own.u8)->right = right;
        self->x_pos.f.u += spikeball_sizes[kind].first_x;
        last->x_pos.f.u += spikeball_sizes[kind].second_x;
    }
        // Fallthrough
    case SpikeballRoutine_First:
        Spikeball_Move(self);
        if (!SstOutOfRange(own->left) || !SstOutOfRange(own->right)) {
            Sst_Draw(self);
            return;
        }
        if (own->partner != 0 && &sst[own->partner] != self)
            Sst_Delete(&sst[own->partner]);
        ForgetIfRemembered(self);
        Sst_Delete(self);
        break;
    case SpikeballRoutine_Second: {
        Spikeball_Move(self);
        // the balls turn round when they touch (this one's left side to the other's right)
        Sst *other = &sst[own->partner];
        if (own->partner != 0 && other != self && other->code == self->code) {
            if ((int16_t)(other->x_pos.f.u + 0x18) == (int16_t)(self->x_pos.f.u - 0x18)) {
                own->right_going ^= 1;
                ((Own_Spikeball *)other->own.u8)->right_going ^= 1;
                PlaySound(sfx_SlidingSpike);
            }
        }
        Sst_Draw(self);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 45: the push spring. Subtype bit 4 clear: a spring that squashes while stood on and throws you up (speed by bit 1: $1000 or $A00) or, if flagged, sideways and tumbling. Bit 4 set: a pusher: push it
// along (as far as $12 from where it is) and it flings you back, harder the further you pushed it
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t power;     // 0x30: the throw (a speed up)
    uint8_t press;     // 0x32: how far it is pressed
    uint8_t pad1;      // 0x33
    int16_t home_x;    // 0x34: (the pusher's) where it rests
    uint8_t moved;     // 0x36: (the pusher) it was pushed this frame
} Own_PushSpring;

enum { PushRoutine_Init = 0, PushRoutine_Spring = 2, PushRoutine_Pusher = 4 };

static void PushSpring_Launch(Sst *self, Own_PushSpring *own, Sst *chr, int who) {
    const uint8_t bit = (uint8_t)(8 << who);
    if (!(self->status & bit))
        return;
    self->status &= (uint8_t)~bit;
    chr->y_vel = own->power;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (self->subtype & 0x80)
        chr->x_vel = 0;
    CharThrow_Tumble(chr, self->subtype, 4, 0, 1);
    PlaySound(sfx_Spring);
}

// loc_18478: whoever was pushing it (status bits 5 and 6) is thrown away from it, by how far it has been pushed
static void PushSpring_Fling(Sst *self, Own_PushSpring *own) {
    if (!(self->status & 0x60))
        return;
    FOR_EACH_CHARACTER(chr, who) {
        const uint8_t bit = (uint8_t)(0x20 << who);
        if (!(self->status & bit))
            continue;
        self->status &= (uint8_t)~bit;
        int16_t d0 = (int16_t)(own->home_x - self->x_pos.f.u);
        if (d0 < 0)
            d0 = -d0;
        d0 += 0xA;
        d0 = (int16_t)(d0 << 7);
        chr->x_vel = (int16_t)-d0;
        chr->x_pos.f.u -= 4;
        chr->status |= SST_PSTATUS_X_FLIP;
        if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
            chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
            chr->x_pos.f.u += 8;
            chr->x_vel = (int16_t)-chr->x_vel;
        }
        ((Scratch_Sonic *)chr->own.u8)->control_lock = 0xF;
        chr->ground_vel = chr->x_vel;
        if (!((chr->status & SST_PSTATUS_IN_BALL) != 0))
            chr->anim = SonAnimId_Walk;
        if (self->subtype & 0x80)
            chr->y_vel = 0;
        CharThrow_Tumble(chr, self->subtype, 8, 1, 3);
        chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        chr->prev_anim = 1;
        PlaySound(sfx_Spring);
    }
}

// loc_183E4: the pusher is pushed by `chr` (who has `penetration` pixels in it)
static void Pusher_Push(Sst *self, Own_PushSpring *own, Sst *chr, int16_t penetration) {
    int16_t d0, d1;
    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        if (((chr->status & SST_PSTATUS_X_FLIP) != 0))
            return;
        if (penetration == 0) {
            if (chr->ground_vel > 0)
                own->moved = 1;
            return;
        }
        if ((int16_t)(own->home_x + 0x12) == self->x_pos.f.u) {
            own->moved = 1;
            return;
        }
        self->x_pos.f.u++;
        d0 = 1;
        d1 = 0x40;
    } else {
        if (!((chr->status & SST_PSTATUS_X_FLIP) != 0))
            return;
        if (penetration == 0) {
            if (chr->ground_vel < 0)
                own->moved = 1;
            return;
        }
        if ((int16_t)(own->home_x - 0x12) == self->x_pos.f.u) {
            own->moved = 1;
            return;
        }
        self->x_pos.f.u--;
        d0 = -1;
        d1 = -0x40;
    }
    chr->x_pos.f.u += d0;
    chr->ground_vel = d1;
    chr->x_vel = 0;
    d0 = (int16_t)(own->home_x - self->x_pos.f.u);
    if (d0 < 0)
        d0 = -d0;
    self->mapping_frame = (uint8_t)(d0 + 0xA);
    own->moved = 1;
}

void Obj_OOZPushSpring(Sst *self) {
    Own_PushSpring *own = (Own_PushSpring *)self->own.u8;

    if (self->routine == PushRoutine_Init) {
        self->routine += 2;
        self->mappings = Mappings_OOZPushSpring;
        self->art_tile = TILE_MAP(0, 2, 0, 0, 0x3C5);
        self->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        if (self->subtype & 0x10) { // the pusher
            self->routine = PushRoutine_Pusher;
            self->anim = 1;
            self->mapping_frame = 0xA;
            self->width_pixels = 0x14;
            own->home_x = self->x_pos.f.u;
        }
        own->power = (self->subtype & 2) ? (int16_t)0xF600 : (int16_t)0xF000;
        SstOnScreenTest(self);
        return;
    }

    if (self->routine == PushRoutine_Spring) {
        if (self->status & 0x18) {
            if (own->press == 9) { // pressed all the way: it throws whoever stands on it
                FOR_EACH_CHARACTER(chr, who)
                    PushSpring_Launch(self, own, chr, who);
                SstOnScreenTest(self);
                return;
            }
            own->press++;
        } else if (own->press) {
            own->press--;
        }
        self->mapping_frame = own->press;
        const int16_t offset = (int16_t)(own->press * 2);
        self->y_pos.f.u += offset; // (the top is lower, pressed)
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, 0x1B, 0x14, 0x14, self->x_pos.f.u, NULL);
        self->y_pos.f.u -= offset;
        SstOnScreenTest(self);
        return;
    }

    // the pusher (loc_18322)
    own->moved = 0;
    FOR_EACH_CHARACTER(chr, who) {
        const int16_t before = chr->x_pos.f.u;
        if (Solid_Character(self, chr, who, 0x1F, 0xC, 0xD, self->x_pos.f.u, NULL) != 1)
            continue;
        const int16_t penetration = (int16_t)(before - chr->x_pos.f.u);
        uint8_t d1 = self->status;
        if (self->x_pos.f.u >= chr->x_pos.f.u)
            d1 ^= 1;
        if (!(d1 & 1))
            Pusher_Push(self, own, chr, penetration);
    }
    if (!own->moved && own->home_x != self->x_pos.f.u) {
        if ((uint16_t)own->home_x >= (uint16_t)self->x_pos.f.u) { // it is to the left of home: back to the right
            self->mapping_frame -= 4;
            self->x_pos.f.u += 4;
            if ((uint16_t)own->home_x < (uint16_t)self->x_pos.f.u) {
                self->mapping_frame = 0xA;
                self->x_pos.f.u = own->home_x;
            }
        } else {
            self->mapping_frame -= 4;
            self->x_pos.f.u -= 4;
            if ((uint16_t)own->home_x >= (uint16_t)self->x_pos.f.u) {
                self->mapping_frame = 0xA;
                self->x_pos.f.u = own->home_x;
            }
        }
        PushSpring_Fling(self, own);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 46: the ball on a spring. Its spring is an object of its own. When player 2's A button is pressed, or its switch (the subtype's high nibble) is down, the ball springs off, falls, and rolls along the
// floor at $100 (one way, or the other if flipped), solid, until it leaves the level's bottom
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    uint8_t released;  // 0x30: (the spring) the ball has gone: let it out
    uint8_t pad1[3];   // 0x31-0x33
    int16_t home_x;    // 0x34
    int16_t home_y;    // 0x36
    uint8_t pad2[4];   // 0x38-0x3B
    uint8_t other;     // 0x3C: the slot of the spring (of the ball)
} Own_SpringBall;

enum { BallRoutine_Init = 0, BallRoutine_Wait = 2, BallRoutine_Roll = 4, BallRoutine_Spring = 6 };

// loc_18C98: the ball's turning frames, by its speed ($14's high byte: the one the animation reads)
static void SpringBall_Animate(Sst *self) {
    if (self->mapping_frame != 0) {
        self->mapping_frame = 0;
        return;
    }
    int8_t speed = (int8_t)((uint16_t)self->ground_vel >> 8);
    if (speed > 0) {
        if (--self->anim_frame_timer < 0) {
            int d0 = -speed + 8;
            self->anim_frame_timer = (int8_t)(d0 >= 0 ? d0 : 0);
            uint8_t turn = self->anim_frame + 1;
            if (turn == 4)
                turn = 1;
            self->anim_frame = turn;
        }
    } else if (speed < 0) {
        if (--self->anim_frame_timer < 0) {
            int d0 = speed + 8;
            self->anim_frame_timer = (int8_t)(d0 >= 0 ? d0 : 0);
            uint8_t turn = self->anim_frame - 1;
            if (turn == 0)
                turn = 3;
            self->anim_frame = turn;
        }
    }
    self->mapping_frame = self->anim_frame;
}

void Obj_OOZSpringBall(Sst *self) {
    Own_SpringBall *own = (Own_SpringBall *)self->own.u8;

    switch (self->routine) {
    case BallRoutine_Init: {
        if (self->respawn) {
            (*SstLoad_State(self)) &= 0x7F;
            const bool used = ((*SstLoad_State(self)) & 1) != 0;
            (*SstLoad_State(self)) |= 1;
            if (used) {
                Sst_Delete(self);
                return;
            }
        }
        self->routine += 2;
        self->y_radius = 0x0F;
        self->x_radius = 0x0F;
        self->mappings = Mappings_OOZSpringBall;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x354);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        own->home_x = self->x_pos.f.u;
        own->home_y = self->y_pos.f.u;
        self->width_pixels = 0x10;
        self->mapping_frame = 0;
        self->ground_vel = 0;
        self->anim_frame = 1; // (the frame the turning goes from: $1F)
        Sst *spring = Sst_Allocate();
        own->other = 0;
        if (spring != NULL) {
            spring->code = sst_object_codes[0x46];
            spring->routine = BallRoutine_Spring;
            spring->x_pos.f.u = self->x_pos.f.u;
            spring->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x12);
            spring->mappings = Mappings_OOZPushSpring;
            spring->art_tile = TILE_MAP(0, 2, 0, 0, 0x3C5);
            spring->render_flags = 0;
            spring->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
            spring->width_pixels = 0x10;
            spring->priority = 4 * 0x80;
            spring->mapping_frame = 9;
            ((Own_SpringBall *)spring->own.u8)->other = (uint8_t)(self - sst);
            own->other = (uint8_t)(spring - sst);
        }
    }
        // Fallthrough
    case BallRoutine_Wait: {
        const bool pad = (jpad2_press & JPAD_A) != 0;
        if (pad || f_switch[(self->subtype >> 4) & 0xF]) {
            self->routine += 2;
            self->status |= 2;
            self->y_vel = -0x300;
            self->ground_vel = 0x100;
            if (own->other != 0) {
                Sst *spring = &sst[own->other];
                ((Own_SpringBall *)spring->own.u8)->released = 1;
            }
            if (((self->status & SST_STATUS_X_FLIP) != 0))
                self->ground_vel = -self->ground_vel;
        }
        SpringBall_Animate(self);
        SstOnScreenTest(self);
        break;
    }
    case BallRoutine_Roll: {
        const int16_t old_x = self->x_pos.f.u;
        Sst_MoveSprite2(self);
        bool gone = false;
        if (self->status & 2) { // in the air
            self->y_vel += 0x18;
            if (self->y_vel >= 0) {
                if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)self->y_pos.f.u) {
                    gone = true;
                } else {
                    const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
                    if (d1 < 0) {
                        self->y_pos.f.u += d1;
                        self->y_vel = 0;
                        self->status &= (uint8_t)~2;
                        self->x_vel = 0x100;
                        if (((self->status & SST_STATUS_X_FLIP) != 0))
                            self->x_vel = -self->x_vel;
                    }
                }
            }
        } else { // on the ground
            const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
            if (d1 >= 8)
                self->status |= 2;
            else
                self->y_pos.f.u += d1;
        }
        if (gone) {
            ForgetIfRemembered(self);
            Sst_Delete(self);
            return;
        }
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 0x10, 0x11, old_x, NULL);
        SpringBall_Animate(self);
        SstOnScreenTest(self);
        break;
    }
    case BallRoutine_Spring:
        if (own->released) { // the spring lets go of the ball: its frame runs back to 0
            if (--self->mapping_frame == 0)
                own->released = 0;
        }
        SstOnScreenTest(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 48: the cannon. A character that comes within $10 of its middle is caught (rolled up, held, its own steering off) and the barrel rises or falls to a stop; then it is shot off in the direction the
// subtype says (up, right, down, left: the low two bits, turned by the object's flip) at $1000, flying by itself until another cannon takes it. Tails has a state of his own.
// With subtype bit 7 the character is let go at once, to fly free
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[3];     // 0x29-0x2B
    uint8_t state_sonic; // 0x2C
    uint8_t pad1[9];     // 0x2D-0x35
    uint8_t state_tails; // 0x36
    uint8_t pad2[5];     // 0x37-0x3B
    int16_t wait;        // 0x3C
    uint8_t down;        // 0x3E: the barrel frames count down
    uint8_t base_frame;  // 0x3F
} Own_Cannon;

enum { CannonState_Idle = 0, CannonState_Aim = 2, CannonState_Fly = 4, CannonState_Wait = 6 };

// The render flags and the frame the barrel starts at, by the subtype's low nibble (+4 when flipped): loc_19272
static const uint8_t cannon_render[8] = { 0x04, 0x06, 0x07, 0x05, 0x05, 0x04, 0x06, 0x07 };
static const uint8_t cannon_frame[8] = { 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07 };
// Where it fires, by direction: x and y speed (loc_1944E): up, right, down, left
static const int16_t cannon_fire[4][2] = { { 0x0000, -0x1000 }, { 0x1000, 0x0000 }, { 0x0000, 0x1000 }, { -0x1000, 0x0000 } };

static void Cannon_Char(Sst *self, Own_Cannon *own, Sst *chr, int who) {
    uint8_t *state = (who == SolidChar_Sonic) ? &own->state_sonic : &own->state_tails;
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;

    switch (*state) {
    case CannonState_Idle: {
        if (debug_use)
            return;
        if ((uint16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x10) >= 0x20)
            return;
        if ((uint16_t)(chr->y_pos.f.u - self->y_pos.f.u + 0x10) >= 0x20)
            return;
        if (((chr->status & SST_PSTATUS_OBJECT_STAND) != 0) && sscratch->standing_obj >= STANDING_SST) { // (it was on another cannon: that one lets go of it)
            Sst *prev = &sst[(sscratch->standing_obj - STANDING_SST) % SST_COUNT];
            if (prev != self && prev->code == self->code) {
                Own_Cannon *other = (Own_Cannon *)prev->own.u8;
                if (who == SolidChar_Sonic)
                    other->state_sonic = 0;
                else
                    other->state_tails = 0;
            }
        }
        sscratch->standing_obj = Standing_IdOfSst(self);
        *state += 2;
        chr->x_pos.f.u = self->x_pos.f.u;
        chr->y_pos.f.u = self->y_pos.f.u;
        OBJ_CONTROL(chr) = 0x81;
        chr->anim = SonAnimId_Roll;
        chr->ground_vel = 0x1000;
        chr->x_vel = 0;
        chr->y_vel = 0;
        self->status &= (uint8_t)~0x20; // (bit 5 for either: the prototype clears the first character's)
        chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        chr->status |= SST_PSTATUS_IN_AIR;
        chr->status |= SST_PSTATUS_OBJECT_STAND;
        self->mapping_frame = own->base_frame;
        PlaySound(sfx_Roll);
        break;
    }
    case CannonState_Aim: {
        bool ready;
        if (!own->down) { // the barrel rises to frame 7
            ready = self->mapping_frame == 7;
            if (!ready) {
                if (--self->anim_frame_timer >= 0)
                    return;
                self->anim_frame_timer = 7;
                self->mapping_frame++;
                ready = self->mapping_frame == 7;
            }
        } else { // ... or falls to frame 0
            ready = self->mapping_frame == 0;
            if (!ready) {
                if (--self->anim_frame_timer >= 0)
                    return;
                self->anim_frame_timer = 7;
                self->mapping_frame--;
                ready = self->mapping_frame == 0;
            }
        }
        if (!ready)
            return;
        *state += 2;
        int dir = self->subtype + 1;
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            dir -= 2;
        dir &= 3;
        chr->x_vel = cannon_fire[dir][0];
        chr->y_vel = cannon_fire[dir][1];
        self->anim_frame_timer = 3;
        if (self->subtype & 0x80) { // let go to fly free
            OBJ_CONTROL(chr) = 0;
            chr->status |= SST_PSTATUS_IN_AIR;
            chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            sscratch->jumping = 0;
            chr->routine = 2;
            *state = CannonState_Wait;
            own->wait = 7;
        }
        break;
    }
    case CannonState_Fly:
        if (own->state_sonic != CannonState_Aim && own->state_tails != CannonState_Aim) {
            if (--self->anim_frame_timer < 0) {
                self->anim_frame_timer = 1;
                if (own->down) {
                    if (self->mapping_frame != 7)
                        self->mapping_frame++;
                } else if (self->mapping_frame != 0) {
                    self->mapping_frame--;
                }
            }
        }
        chr->x_pos.v += (int32_t)chr->x_vel << 8;
        chr->y_pos.v += (int32_t)chr->y_vel << 8;
        break;
    case CannonState_Wait:
        if (--own->wait < 0)
            *state = CannonState_Idle;
        break;
    }
}

void Obj_OOZCannon(Sst *self) {
    Own_Cannon *own = (Own_Cannon *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_OOZCannon;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x368);
        int index = self->subtype & 0xF;
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            index += 4;
        index &= 7;
        self->render_flags = cannon_render[index];
        own->base_frame = cannon_frame[index];
        if (own->base_frame)
            own->down = 1;
        self->mapping_frame = own->base_frame;
        self->width_pixels = 0x28;
        self->priority = 1 * 0x80;
    }

    FOR_EACH_CHARACTER(chr, who)
        Cannon_Char(self, own, chr, who);

    if (own->state_sonic + own->state_tails == 0) {
        SstOnScreenTest(self);
        return;
    }
    Sst_Draw(self);
}
