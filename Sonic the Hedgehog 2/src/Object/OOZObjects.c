// Oil Ocean's objects for Sonic 2 (the Simon Wai prototype's 07, 33, 43, 45, 46 and 48; the collapsing platform, 1F, the switch, 47, the elevator, 19, and the swing, 15, are shared with other zones): the oil that
// is stood on and kills whoever stays in it (07), the platform that hops (33), the pair of spiked balls that roll to and fro (43), the spring and the pusher with a spring behind it (45), the ball that rolls
// off a spring (46) and the cannon that catches a character and fires it off (48). All carry Sonic and Tails.
#include "Object/OOZObjects.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/OOZCannon.h"
#include "Resource/Mappings/OOZLauncher.h"
#include "Resource/Mappings/OOZPushSpring.h"
#include "Resource/Mappings/OOZSpikeball.h"
#include "Resource/Mappings/OOZSpringBall.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Object *chr = Character(who); chr != NULL; chr = NULL)

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

static void ForgetIfRemembered(Object *obj) {
    if (obj->respawn_index)
        objstate[obj->respawn_index] &= 0x7F;
}

// The character's tumble when a spring or pusher throws it (the subtype's bit 0, and bit 1 for a short one; bits 2 and 3 pick the collision path): flips_remaining, flip_speed and the path are the same ones
// the Chemical Plant spring tube sets (Obj_CPZTubeSpring)
static void ThrowTumble(Object *chr, uint8_t subtype, uint8_t flip_speed, uint8_t flips_short, uint8_t flips_long) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;
    if (subtype & 1) {
        chr->inertia = 1;
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = flips_short;
        sscratch->flip_speed = flip_speed;
        if (!(subtype & 2))
            sscratch->flips_remaining = flips_long;
        if (chr->status.p.f.x_flip) {
            sscratch->flip_angle = (uint8_t)-sscratch->flip_angle;
            chr->inertia = -chr->inertia;
        }
    }
    switch (subtype & 0xC) {
    case 4:
        sscratch->top_solid_bit = 0xC;
        sscratch->lrb_solid_bit = 0xD;
        break;
    case 8:
        sscratch->top_solid_bit = 0xE;
        sscratch->lrb_solid_bit = 0xF;
        break;
    }
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
} Scratch_Oil;

void Obj_OilSurface(Object *obj) {
    Scratch_Oil *scratch = (Scratch_Oil *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->pos.l.y.f.u = 0x758;
        obj->width_pixels = 0x20;
        scratch->top = obj->pos.l.y.f.u;
        scratch->depth = 0x30;
        obj->status.b |= 0x80;
    }

    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr == NULL)
            continue;
        uint8_t *depth = (who == SolidChar_Sonic) ? &scratch->depth : &scratch->depth_tails;
        const uint8_t stand_bit = (uint8_t)(8 << who);
        if (obj->status.b & stand_bit) {
            if (*depth == 0) { // all the way in: suffocated
                obj->status.b &= (uint8_t)~stand_bit;
                if (who == SolidChar_Sonic)
                    KillSonic(chr, obj);
                else
                    KillTails(chr);
                return;
            }
            (*depth)--;
        } else if (*depth != 0x30) {
            (*depth)++;
        }
        obj->pos.l.x.f.u = chr->pos.l.x.f.u;
        Solid_Platform(obj, chr, who, 0x20, *depth, chr->pos.l.x.f.u);
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
} Scratch_Launcher;

enum { Launcher_Wait = 0, Launcher_Bounce = 2, Launcher_Ready = 4, Launcher_Rise = 6, Launcher_Done = 8 };

static void Launcher_Hold(Object *chr) {
    OBJ_CONTROL(chr) = 1;
    chr->inertia = 0;
    chr->xsp = 0;
    chr->ysp = 0;
    chr->status.p.f.pushing = false;
    chr->tile &= ~TILE_PRIORITY_AND;
}

static void Launcher_Launch(Object *obj, Object *chr, int who) {
    if (!(obj->status.b & (8 << who)))
        return;
    chr->pos.l.x.f.u = obj->pos.l.x.f.u;
    chr->anim = SonAnimId_Roll;
    chr->inertia = 0x800;
    chr->status.p.f.in_air = true;
    chr->ysp = -0x1000;
    chr->status.p.f.object_stand = false;
    OBJ_CONTROL(chr) = 0;
    PlaySound(sfx_Spring);
}

void Obj_OOZLauncher(Object *obj) {
    Scratch_Launcher *scratch = (Scratch_Launcher *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_OOZLauncher;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x32C);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 3;
        obj->width_pixels = 0x18;
        scratch->home_y = obj->pos.l.y.f.u;
        obj->routine_sec = Launcher_Bounce;
        scratch->wait = 0x78;
        if (scratch->subtype)
            obj->routine_sec = Launcher_Ready;
    }

    const int16_t old_x = obj->pos.l.x.f.u;
    switch (obj->routine_sec) {
    case Launcher_Wait:
        if (--scratch->wait < 0) {
            scratch->wait = 0x78;
            scratch->speed = (int32_t)0xFFF69800;
            obj->routine_sec += 2;
        }
        break;
    case Launcher_Bounce: {
        const int32_t y = obj->pos.l.y.v + scratch->speed;
        obj->pos.l.y.v = y;
        scratch->speed += 0x3800;
        if ((uint16_t)(y >> 16) < (uint16_t)scratch->home_y)
            break;
        int32_t speed = scratch->speed;
        if ((uint32_t)speed < 0x10000)
            obj->routine_sec -= 2;
        scratch->speed = -(int32_t)((uint32_t)speed >> 2);
        obj->pos.l.y.f.u = scratch->home_y;
        break;
    }
    case Launcher_Ready: { // waits for a character in the middle of it
        const int16_t left = (int16_t)(obj->pos.l.x.f.u - 0x10), right = (int16_t)(obj->pos.l.x.f.u + 0x10);
        const uint8_t on = obj->status.b & 0x18;
        if (on == 0)
            break;
        if (on == 0x18) { // both are on it: both must be in the middle
            const int16_t sx = player->pos.l.x.f.u, tx = TAILS_OBJ->pos.l.x.f.u;
            if (sx < left || sx >= right || tx < left || tx >= right)
                break;
            Launcher_Hold(player);
            Launcher_Hold(TAILS_OBJ);
        } else {
            const int who = (on & 8) ? SolidChar_Sonic : SolidChar_Tails;
            Object *chr = Character(who);
            if (chr == NULL || chr->pos.l.x.f.u < left || chr->pos.l.x.f.u >= right)
                break;
            Launcher_Hold(chr);
        }
        scratch->speed = (int32_t)0xFFF69800;
        obj->routine_sec += 2;
        break;
    }
    case Launcher_Rise: {
        const int32_t y = obj->pos.l.y.v + scratch->speed;
        obj->pos.l.y.v = y;
        scratch->speed += 0x3800;
        if ((int16_t)(y >> 16) != (int16_t)(scratch->home_y - 0x7D))
            break;
        obj->routine_sec += 2;
        FOR_EACH_CHARACTER(chr, who)
            Launcher_Launch(obj, chr, who);
        break;
    }
    default:
        break;
    }

    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 8, 9, old_x, NULL);
    RememberState(obj);
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
} Scratch_Spikeball;

enum { SpikeballRoutine_Init = 0, SpikeballRoutine_First = 2, SpikeballRoutine_Second = 4 };

// The sizes by subtype (loc_17F20): balls - 1, how far each way from the middle, and how far the balls start from it
static const struct { uint8_t more; uint8_t reach; int16_t first_x, second_x; } spikeball_sizes[3] = {
    { 0, 0x68, 0, 0 }, { 1, 0xE8, -0x18, 0x18 }, { 1, 0xA8, -0x58, -0x28 },
};

static void Spikeball_Setup(Object *ball) {
    Scratch_Spikeball *scratch = (Scratch_Spikeball *)&ball->scratch;
    ball->mappings = Mappings_OOZSpikeball;
    ball->tile = TILE_MAP(1, 2, 0, 0, 0x30C);
    ball->render.b = 0;
    ball->render.f.level_fg = true;
    ball->priority = 4;
    ball->width_pixels = 0x18;
    ball->col_type = 0xA5;
    scratch->centre_x = ball->pos.l.x.f.u;
}

static void Spikeball_Move(Object *ball) {
    Scratch_Spikeball *scratch = (Scratch_Spikeball *)&ball->scratch;
    if (!scratch->right_going) {
        ball->pos.l.x.f.u--;
        if (ball->pos.l.x.f.u == scratch->left)
            scratch->right_going = 1;
    } else {
        ball->pos.l.x.f.u++;
        if (ball->pos.l.x.f.u == scratch->right)
            scratch->right_going = 0;
    }
}

void Obj_OOZSpikeball(Object *obj) {
    Scratch_Spikeball *scratch = (Scratch_Spikeball *)&obj->scratch;

    switch (obj->routine) {
    case SpikeballRoutine_Init: {
        obj->routine += 2;
        const int kind = (scratch->subtype == 6) ? 1 : (scratch->subtype == 0xC) ? 2 : 0;
        const int more = spikeball_sizes[kind].more;
        const int16_t reach = spikeball_sizes[kind].reach;
        Object *last = obj;
        Spikeball_Setup(obj);
        for (int i = 0; i < more; i++) {
            Object *other = FindNextFreeObj(obj + 1);
            if (other == NULL)
                break;
            other->type = obj->type;
            other->routine = SpikeballRoutine_Second;
            other->pos.l.x.f.u = obj->pos.l.x.f.u;
            other->pos.l.y.f.u = obj->pos.l.y.f.u;
            ((Scratch_Spikeball *)&other->scratch)->right_going = 1;
            Spikeball_Setup(other);
            last = other;
        }
        ((Scratch_Spikeball *)&last->scratch)->partner = (uint8_t)(obj - objects);
        scratch->partner = (uint8_t)(last - objects);
        const int16_t left = (int16_t)(scratch->centre_x - reach);
        const int16_t right = (int16_t)(left + reach + reach);
        scratch->left = ((Scratch_Spikeball *)&last->scratch)->left = left;
        scratch->right = ((Scratch_Spikeball *)&last->scratch)->right = right;
        obj->pos.l.x.f.u += spikeball_sizes[kind].first_x;
        last->pos.l.x.f.u += spikeball_sizes[kind].second_x;
    }
        // Fallthrough
    case SpikeballRoutine_First:
        Spikeball_Move(obj);
        if (!IS_OFFSCREEN(scratch->left) || !IS_OFFSCREEN(scratch->right)) {
            DisplaySprite(obj);
            return;
        }
        if (scratch->partner != 0 && &objects[scratch->partner] != obj)
            ObjectDelete(&objects[scratch->partner]);
        ForgetIfRemembered(obj);
        ObjectDelete(obj);
        break;
    case SpikeballRoutine_Second: {
        Spikeball_Move(obj);
        // the balls turn round when they touch (this one's left side to the other's right)
        Object *other = &objects[scratch->partner];
        if (scratch->partner != 0 && other != obj && other->type == obj->type) {
            if ((int16_t)(other->pos.l.x.f.u + 0x18) == (int16_t)(obj->pos.l.x.f.u - 0x18)) {
                scratch->right_going ^= 1;
                ((Scratch_Spikeball *)&other->scratch)->right_going ^= 1;
            }
        }
        DisplaySprite(obj);
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
} Scratch_PushSpring;

enum { PushRoutine_Init = 0, PushRoutine_Spring = 2, PushRoutine_Pusher = 4 };

static void PushSpring_Launch(Object *obj, Scratch_PushSpring *scratch, Object *chr, int who) {
    const uint8_t bit = (uint8_t)(8 << who);
    if (!(obj->status.b & bit))
        return;
    obj->status.b &= (uint8_t)~bit;
    chr->ysp = scratch->power;
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (scratch->subtype & 0x80)
        chr->xsp = 0;
    ThrowTumble(chr, scratch->subtype, 4, 0, 1);
    PlaySound(sfx_Spring);
}

// loc_18478: whoever was pushing it (status bits 5 and 6) is thrown away from it, by how far it has been pushed
static void PushSpring_Fling(Object *obj, Scratch_PushSpring *scratch) {
    if (!(obj->status.b & 0x60))
        return;
    FOR_EACH_CHARACTER(chr, who) {
        const uint8_t bit = (uint8_t)(0x20 << who);
        if (!(obj->status.b & bit))
            continue;
        obj->status.b &= (uint8_t)~bit;
        int16_t d0 = (int16_t)(scratch->home_x - obj->pos.l.x.f.u);
        if (d0 < 0)
            d0 = -d0;
        d0 += 0xA;
        d0 = (int16_t)(d0 << 7);
        chr->xsp = (int16_t)-d0;
        chr->pos.l.x.f.u -= 4;
        chr->status.p.f.x_flip = true;
        if (!obj->status.o.f.x_flip) {
            chr->status.p.f.x_flip = false;
            chr->pos.l.x.f.u += 8;
            chr->xsp = (int16_t)-chr->xsp;
        }
        ((Scratch_Sonic *)&chr->scratch)->control_lock = 0xF;
        chr->inertia = chr->xsp;
        if (!chr->status.p.f.in_ball)
            chr->anim = SonAnimId_Walk;
        if (scratch->subtype & 0x80)
            chr->ysp = 0;
        ThrowTumble(chr, scratch->subtype, 8, 1, 3);
        chr->status.p.f.pushing = false;
        chr->prev_anim = 1;
        PlaySound(sfx_Spring);
    }
}

// loc_183E4: the pusher is pushed by `chr` (who has `penetration` pixels in it)
static void Pusher_Push(Object *obj, Scratch_PushSpring *scratch, Object *chr, int16_t penetration) {
    int16_t d0, d1;
    if (obj->status.o.f.x_flip) {
        if (chr->status.p.f.x_flip)
            return;
        if (penetration == 0) {
            if (chr->inertia > 0)
                scratch->moved = 1;
            return;
        }
        if ((int16_t)(scratch->home_x + 0x12) == obj->pos.l.x.f.u) {
            scratch->moved = 1;
            return;
        }
        obj->pos.l.x.f.u++;
        d0 = 1;
        d1 = 0x40;
    } else {
        if (!chr->status.p.f.x_flip)
            return;
        if (penetration == 0) {
            if (chr->inertia < 0)
                scratch->moved = 1;
            return;
        }
        if ((int16_t)(scratch->home_x - 0x12) == obj->pos.l.x.f.u) {
            scratch->moved = 1;
            return;
        }
        obj->pos.l.x.f.u--;
        d0 = -1;
        d1 = -0x40;
    }
    chr->pos.l.x.f.u += d0;
    chr->inertia = d1;
    chr->xsp = 0;
    d0 = (int16_t)(scratch->home_x - obj->pos.l.x.f.u);
    if (d0 < 0)
        d0 = -d0;
    obj->frame = (uint8_t)(d0 + 0xA);
    scratch->moved = 1;
}

void Obj_OOZPushSpring(Object *obj) {
    Scratch_PushSpring *scratch = (Scratch_PushSpring *)&obj->scratch;

    if (obj->routine == PushRoutine_Init) {
        obj->routine += 2;
        obj->mappings = Mappings_OOZPushSpring;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x3C5);
        obj->render.b |= 4; // ori.b #4,1(a0): the layout's facing stays
        obj->width_pixels = 0x10;
        obj->priority = 4;
        if (scratch->subtype & 0x10) { // the pusher
            obj->routine = PushRoutine_Pusher;
            obj->anim = 1;
            obj->frame = 0xA;
            obj->width_pixels = 0x14;
            scratch->home_x = obj->pos.l.x.f.u;
        }
        scratch->power = (scratch->subtype & 2) ? (int16_t)0xF600 : (int16_t)0xF000;
        RememberState(obj);
        return;
    }

    if (obj->routine == PushRoutine_Spring) {
        if (obj->status.b & 0x18) {
            if (scratch->press == 9) { // pressed all the way: it throws whoever stands on it
                FOR_EACH_CHARACTER(chr, who)
                    PushSpring_Launch(obj, scratch, chr, who);
                RememberState(obj);
                return;
            }
            scratch->press++;
        } else if (scratch->press) {
            scratch->press--;
        }
        obj->frame = scratch->press;
        const int16_t offset = (int16_t)(scratch->press * 2);
        obj->pos.l.y.f.u += offset; // (the top is lower, pressed)
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, 0x1B, 0x14, 0x14, obj->pos.l.x.f.u, NULL);
        obj->pos.l.y.f.u -= offset;
        RememberState(obj);
        return;
    }

    // the pusher (loc_18322)
    scratch->moved = 0;
    FOR_EACH_CHARACTER(chr, who) {
        const int16_t before = chr->pos.l.x.f.u;
        if (Solid_Character(obj, chr, who, 0x1F, 0xC, 0xD, obj->pos.l.x.f.u, NULL) != 1)
            continue;
        const int16_t penetration = (int16_t)(before - chr->pos.l.x.f.u);
        uint8_t d1 = obj->status.b;
        if (obj->pos.l.x.f.u >= chr->pos.l.x.f.u)
            d1 ^= 1;
        if (!(d1 & 1))
            Pusher_Push(obj, scratch, chr, penetration);
    }
    if (!scratch->moved && scratch->home_x != obj->pos.l.x.f.u) {
        if ((uint16_t)scratch->home_x >= (uint16_t)obj->pos.l.x.f.u) { // it is to the left of home: back to the right
            obj->frame -= 4;
            obj->pos.l.x.f.u += 4;
            if ((uint16_t)scratch->home_x < (uint16_t)obj->pos.l.x.f.u) {
                obj->frame = 0xA;
                obj->pos.l.x.f.u = scratch->home_x;
            }
        } else {
            obj->frame -= 4;
            obj->pos.l.x.f.u -= 4;
            if ((uint16_t)scratch->home_x >= (uint16_t)obj->pos.l.x.f.u) {
                obj->frame = 0xA;
                obj->pos.l.x.f.u = scratch->home_x;
            }
        }
        PushSpring_Fling(obj, scratch);
    }
    RememberState(obj);
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
} Scratch_SpringBall;

enum { BallRoutine_Init = 0, BallRoutine_Wait = 2, BallRoutine_Roll = 4, BallRoutine_Spring = 6 };

// loc_18C98: the ball's turning frames, by its speed ($14's high byte: the one the animation reads)
static void SpringBall_Animate(Object *obj) {
    if (obj->frame != 0) {
        obj->frame = 0;
        return;
    }
    int8_t speed = (int8_t)((uint16_t)obj->inertia >> 8);
    if (speed > 0) {
        if (--obj->frame_time.b < 0) {
            int d0 = -speed + 8;
            obj->frame_time.b = (int8_t)(d0 >= 0 ? d0 : 0);
            uint8_t turn = obj->anim_frame + 1;
            if (turn == 4)
                turn = 1;
            obj->anim_frame = turn;
        }
    } else if (speed < 0) {
        if (--obj->frame_time.b < 0) {
            int d0 = speed + 8;
            obj->frame_time.b = (int8_t)(d0 >= 0 ? d0 : 0);
            uint8_t turn = obj->anim_frame - 1;
            if (turn == 0)
                turn = 3;
            obj->anim_frame = turn;
        }
    }
    obj->frame = obj->anim_frame;
}

void Obj_OOZSpringBall(Object *obj) {
    Scratch_SpringBall *scratch = (Scratch_SpringBall *)&obj->scratch;

    switch (obj->routine) {
    case BallRoutine_Init: {
        if (obj->respawn_index) {
            objstate[obj->respawn_index] &= 0x7F;
            const bool used = (objstate[obj->respawn_index] & 1) != 0;
            objstate[obj->respawn_index] |= 1;
            if (used) {
                ObjectDelete(obj);
                return;
            }
        }
        obj->routine += 2;
        obj->y_rad = 0x0F;
        obj->x_rad = 0x0F;
        obj->mappings = Mappings_OOZSpringBall;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x354);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 3;
        scratch->home_x = obj->pos.l.x.f.u;
        scratch->home_y = obj->pos.l.y.f.u;
        obj->width_pixels = 0x10;
        obj->frame = 0;
        obj->inertia = 0;
        obj->anim_frame = 1; // (the frame the turning goes from: $1F)
        Object *spring = FindFreeObj();
        scratch->other = 0;
        if (spring != NULL) {
            spring->type = 0x46;
            spring->routine = BallRoutine_Spring;
            spring->pos.l.x.f.u = obj->pos.l.x.f.u;
            spring->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x12);
            spring->mappings = Mappings_OOZPushSpring;
            spring->tile = TILE_MAP(0, 2, 0, 0, 0x3C5);
            spring->render.b = 0;
            spring->render.b |= 4; // ori.b #4,1(a0): the layout's facing stays
            spring->width_pixels = 0x10;
            spring->priority = 4;
            spring->frame = 9;
            ((Scratch_SpringBall *)&spring->scratch)->other = (uint8_t)(obj - objects);
            scratch->other = (uint8_t)(spring - objects);
        }
    }
        // Fallthrough
    case BallRoutine_Wait: {
        const bool pad = (jpad2_press & JPAD_A) != 0;
        if (pad || f_switch[(scratch->subtype >> 4) & 0xF]) {
            obj->routine += 2;
            obj->status.b |= 2;
            obj->ysp = -0x300;
            obj->inertia = 0x100;
            if (scratch->other != 0) {
                Object *spring = &objects[scratch->other];
                ((Scratch_SpringBall *)&spring->scratch)->released = 1;
            }
            if (obj->status.o.f.x_flip)
                obj->inertia = -obj->inertia;
        }
        SpringBall_Animate(obj);
        RememberState(obj);
        break;
    }
    case BallRoutine_Roll: {
        const int16_t old_x = obj->pos.l.x.f.u;
        SpeedToPos(obj);
        bool gone = false;
        if (obj->status.b & 2) { // in the air
            obj->ysp += 0x18;
            if (obj->ysp >= 0) {
                if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)obj->pos.l.y.f.u) {
                    gone = true;
                } else {
                    const int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
                    if (d1 < 0) {
                        obj->pos.l.y.f.u += d1;
                        obj->ysp = 0;
                        obj->status.b &= (uint8_t)~2;
                        obj->xsp = 0x100;
                        if (obj->status.o.f.x_flip)
                            obj->xsp = -obj->xsp;
                    }
                }
            }
        } else { // on the ground
            const int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
            if (d1 >= 8)
                obj->status.b |= 2;
            else
                obj->pos.l.y.f.u += d1;
        }
        if (gone) {
            ForgetIfRemembered(obj);
            ObjectDelete(obj);
            return;
        }
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(obj, chr, who, (int16_t)(obj->width_pixels + 0xB), 0x10, 0x11, old_x, NULL);
        SpringBall_Animate(obj);
        RememberState(obj);
        break;
    }
    case BallRoutine_Spring:
        if (scratch->released) { // the spring lets go of the ball: its frame runs back to 0
            if (--obj->frame == 0)
                scratch->released = 0;
        }
        RememberState(obj);
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
} Scratch_Cannon;

enum { CannonState_Idle = 0, CannonState_Aim = 2, CannonState_Fly = 4, CannonState_Wait = 6 };

// The render flags and the frame the barrel starts at, by the subtype's low nibble (+4 when flipped): loc_19272
static const uint8_t cannon_render[8] = { 0x04, 0x06, 0x07, 0x05, 0x05, 0x04, 0x06, 0x07 };
static const uint8_t cannon_frame[8] = { 0x00, 0x07, 0x00, 0x07, 0x00, 0x07, 0x00, 0x07 };
// Where it fires, by direction: x and y speed (loc_1944E): up, right, down, left
static const int16_t cannon_fire[4][2] = { { 0x0000, -0x1000 }, { 0x1000, 0x0000 }, { 0x0000, 0x1000 }, { -0x1000, 0x0000 } };

static void Cannon_Char(Object *obj, Scratch_Cannon *scratch, Object *chr, int who) {
    uint8_t *state = (who == SolidChar_Sonic) ? &scratch->state_sonic : &scratch->state_tails;
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;

    switch (*state) {
    case CannonState_Idle: {
        if (debug_use)
            return;
        if ((uint16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u + 0x10) >= 0x20)
            return;
        if ((uint16_t)(chr->pos.l.y.f.u - obj->pos.l.y.f.u + 0x10) >= 0x20)
            return;
        if (chr->status.p.f.object_stand) { // (it was on another cannon: that one lets go of it)
            Object *prev = &objects[sscratch->standing_obj];
            if (prev != obj && prev->type == obj->type) {
                Scratch_Cannon *other = (Scratch_Cannon *)&prev->scratch;
                if (who == SolidChar_Sonic)
                    other->state_sonic = 0;
                else
                    other->state_tails = 0;
            }
        }
        sscratch->standing_obj = (uint8_t)(obj - objects);
        *state += 2;
        chr->pos.l.x.f.u = obj->pos.l.x.f.u;
        chr->pos.l.y.f.u = obj->pos.l.y.f.u;
        OBJ_CONTROL(chr) = 0x81;
        chr->anim = SonAnimId_Roll;
        chr->inertia = 0x1000;
        chr->xsp = 0;
        chr->ysp = 0;
        obj->status.b &= (uint8_t)~0x20; // (bit 5 for either: the prototype clears the first character's)
        chr->status.p.f.pushing = false;
        chr->status.p.f.in_air = true;
        chr->status.p.f.object_stand = true;
        obj->frame = scratch->base_frame;
        PlaySound(sfx_Roll);
        break;
    }
    case CannonState_Aim: {
        bool ready;
        if (!scratch->down) { // the barrel rises to frame 7
            ready = obj->frame == 7;
            if (!ready) {
                if (--obj->frame_time.w >= 0)
                    return;
                obj->frame_time.w = 7;
                obj->frame++;
                ready = obj->frame == 7;
            }
        } else { // ... or falls to frame 0
            ready = obj->frame == 0;
            if (!ready) {
                if (--obj->frame_time.w >= 0)
                    return;
                obj->frame_time.w = 7;
                obj->frame--;
                ready = obj->frame == 0;
            }
        }
        if (!ready)
            return;
        *state += 2;
        int dir = scratch->subtype + 1;
        if (obj->status.o.f.x_flip)
            dir -= 2;
        dir &= 3;
        chr->xsp = cannon_fire[dir][0];
        chr->ysp = cannon_fire[dir][1];
        obj->frame_time.w = 3;
        if (scratch->subtype & 0x80) { // let go to fly free
            OBJ_CONTROL(chr) = 0;
            chr->status.p.f.in_air = true;
            chr->status.p.f.object_stand = false;
            sscratch->jumping = 0;
            chr->routine = 2;
            *state = CannonState_Wait;
            scratch->wait = 7;
        }
        break;
    }
    case CannonState_Fly:
        if (scratch->state_sonic != CannonState_Aim && scratch->state_tails != CannonState_Aim) {
            if (--obj->frame_time.w < 0) {
                obj->frame_time.w = 1;
                if (scratch->down) {
                    if (obj->frame != 7)
                        obj->frame++;
                } else if (obj->frame != 0) {
                    obj->frame--;
                }
            }
        }
        chr->pos.l.x.v += (int32_t)chr->xsp << 8;
        chr->pos.l.y.v += (int32_t)chr->ysp << 8;
        break;
    case CannonState_Wait:
        if (--scratch->wait < 0)
            *state = CannonState_Idle;
        break;
    }
}

void Obj_OOZCannon(Object *obj) {
    Scratch_Cannon *scratch = (Scratch_Cannon *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_OOZCannon;
        obj->tile = TILE_MAP(0, 3, 0, 0, 0x368);
        int index = scratch->subtype & 0xF;
        if (obj->status.o.f.x_flip)
            index += 4;
        index &= 7;
        obj->render.b = cannon_render[index];
        scratch->base_frame = cannon_frame[index];
        if (scratch->base_frame)
            scratch->down = 1;
        obj->frame = scratch->base_frame;
        obj->width_pixels = 0x28;
        obj->priority = 1;
    }

    FOR_EACH_CHARACTER(chr, who)
        Cannon_Char(obj, scratch, chr, who);

    if (scratch->state_sonic + scratch->state_tails == 0) {
        RememberState(obj);
        return;
    }
    DisplaySprite(obj);
}
