// Hill Top's badniks for Sonic 2, as the alpha has them (see HTZBadniks.h). Each sets itself up from the alpha's table of badnik looks, which is here in the objects themselves (as in MTZBadniks.c). Their mappings have
// the tiles of their art in the pieces (the Spiker's $520, the Sol's $3DE and $3AE: its see-saw badnik's and the fireball's), so the objects themselves start from tile 0.
#include "Object/HTZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/Coconuts.h"
#include "Constants.h"

#include "Level.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Animation/Sol.h"
#include "Resource/Animation/SolFire.h"
#include "Resource/Animation/SpikerBadnik.h"
#include "Resource/Mappings/Rexon.h"
#include "Resource/Mappings/Sol.h"
#include "Resource/Mappings/SpikerBadnik.h"

// Where Sonic is to the object (Object_Check_Player_Position): the distances, x and y less his
static int16_t DistanceX(const Sst *self) {
    return (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Spiker (92) and its drill (93)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0;     // 0x29
    uint8_t timer;    // 0x2A: (the drill's: the slot of its Spiker, in the word the alpha has the address in)
    uint8_t fired;    // 0x2B: it has fired its drill and does not fire another
    uint8_t wait;     // 0x2C
    uint8_t saved;    // 0x2D: the routine it was in when it stopped to fire
    uint8_t pad1[2];
} Own_Spiker;

enum { SpikerRoutine_Init = 0, SpikerRoutine_Walk = 2, SpikerRoutine_Pause = 4, SpikerRoutine_Fire = 6 };

static void Spiker_SetUp(Sst *self) {
    self->mappings = Mappings_SpikerBadnik;
    self->art_tile = 0;
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 0x10;
    self->collision_flags = 0x12;
    self->routine += 2;
}

// It stops to fire when Sonic is within $20 pixels each side of it (and has not fired already). Returns that it did
static bool Spiker_Check(Sst *self, Own_Spiker *own) {
    if (own->fired)
        return false;
    if ((uint16_t)(DistanceX(self) + 0x20) >= 0x40)
        return false;
    own->saved = self->routine;
    self->routine = SpikerRoutine_Fire;
    own->wait = 0x10;
    return true;
}

static void Spiker_Turn(Sst *self) {
    self->x_vel = -self->x_vel;
    self->render_flags ^= SST_RENDER_X_FLIP;
    self->status ^= SST_STATUS_X_FLIP;
}

void Obj_Spiker(Sst *self) {
    Own_Spiker *own = (Own_Spiker *)self->own.u8;

    switch (self->routine) {
    case SpikerRoutine_Init:
        Spiker_SetUp(self);
        own->timer = 0x40;
        self->x_vel = 0x80;
        self->render_flags ^= SST_RENDER_X_FLIP;
        self->status ^= SST_STATUS_X_FLIP;
        break;

    case SpikerRoutine_Walk:
        Spiker_Check(self, own);
        if ((int8_t)--own->timer < 0) {
            self->routine += 2;
            own->timer = 0x10;
            SstOnScreenTest(self);
            break;
        }
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_SpikerBadnik);
        SstOnScreenTest(self);
        break;

    case SpikerRoutine_Pause:
        Spiker_Check(self, own);
        if ((int8_t)--own->timer < 0) {
            self->routine -= 2;
            own->timer = 0x40;
            Spiker_Turn(self);
        }
        SstOnScreenTest(self);
        break;

    case SpikerRoutine_Fire:
        if (own->wait != 8) {
            own->wait--;
            SstOnScreenTest(self);
            break;
        }
        {
            Sst *drill = Sst_AllocateAfter(self);
            if (drill != NULL) {
                own->fired = 0xFF;
                drill->code = sst_object_codes[ObjId_SpikerDrill];
                Own_Spiker *d = (Own_Spiker *)drill->own.u8;
                d->subtype = self->subtype;
                d->timer = (uint8_t)(self - sst);
                drill->x_pos.f.u = self->x_pos.f.u;
                drill->y_pos.f.u = self->y_pos.f.u;
                drill->collision_flags |= 0x80;
                drill->mapping_frame = 4;
                self->mapping_frame = 2;
                self->anim = 1;
            }
        }
        self->routine = own->saved;
        SstOnScreenTest(self);
        break;
    }
}

void Obj_SpikerDrill(Sst *self) {
    Own_Spiker *own = (Own_Spiker *)self->own.u8;

    if (self->routine == 0) {
        Spiker_SetUp(self);
        self->render_flags |= 0x80;
        const Sst *spiker = &sst[own->timer];
        const uint8_t flips = spiker->render_flags & 3;
        self->render_flags = (uint8_t)((self->render_flags & ~3) | flips);
        self->y_vel = (flips & 2) ? 0x200 : -0x200; // (up, or down if the Spiker is upside down)
        return;
    }
    if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return;
    }
    self->render_flags ^= SST_RENDER_X_FLIP; // (it flickers)
    Sst_MoveSprite2(self);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Sol (95), and the fireballs (the same object, in its routines 6 and 8)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[0xD]; // 0x29-0x35
    int8_t spin;       // 0x36: the way its fireballs go round
    uint8_t balls;     // 0x37: how many it has (a fireball's: nothing)
    uint8_t pad1[4];   // 0x38-0x3B
    uint8_t link;      // 0x3C: (a fireball's) the slot of its Sol
    uint8_t pad2[3];
} Own_Sol;

STATIC_ASSERT(sizeof(Own_Sol) == 0x18, "Own_Sol must fill the scratch memory");

enum { SolRoutine_Init = 0, SolRoutine_Wait = 2, SolRoutine_Plain = 4, SolRoutine_Ball = 6, SolRoutine_Shot = 8 };

static void Sol_Make(Sst *self, Own_Sol *own) {
    self->mappings = Mappings_Sol;
    self->art_tile = 0;
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x0B;
    self->width_pixels = 0x0C;

    uint8_t angle = 0;
    for (int i = 0; i < 4; i++, angle += 0x40) {
        Sst *ball = Sst_AllocateAfter(self);
        if (ball == NULL)
            break;
        own->balls++;
        ball->code = sst_object_codes[ObjId_Sol];
        ball->routine = SolRoutine_Ball;
        ball->mappings = self->mappings;
        ball->art_tile = self->art_tile;
        ball->render_flags |= SST_RENDER_LEVEL;
        ball->priority = 4 * 0x80;
        ball->width_pixels = 8;
        ball->mapping_frame = 3;
        ball->collision_flags = 0x98;
        ball->angle = angle;
        ((Own_Sol *)ball->own.u8)->link = (uint8_t)(self - sst);
    }
    own->spin = ((self->status & SST_STATUS_X_FLIP) != 0) ? -1 : 1;
    self->routine = (uint8_t)(self->subtype + 2);
    self->x_vel = ((self->status & SST_STATUS_X_FLIP) != 0) ? 0x40 : -0x40;
}

void Obj_Sol(Sst *self) {
    Own_Sol *own = (Own_Sol *)self->own.u8;

    switch (self->routine) {
    case SolRoutine_Init:
        Sol_Make(self, own);
        break;

    case SolRoutine_Wait: { // it ignites when Sonic is within $A0 pixels across and $50 up or down
        const uint16_t px = (uint16_t)player->x_pos.f.u, ox = (uint16_t)self->x_pos.f.u;
        const uint16_t py = (uint16_t)player->y_pos.f.u, oy = (uint16_t)self->y_pos.f.u;
        const uint16_t dx = px >= ox ? (uint16_t)(px - ox) : (uint16_t)(ox - px);
        const uint16_t dy = py >= oy ? (uint16_t)(py - oy) : (uint16_t)(oy - py);
        if (dx < 0xA0 && dy < 0x50 && !debug_use)
            self->anim = 1;
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_Sol);
        self->mapping_frame &= 3;
        SstOnScreenTest(self);
        break;
    }

    case SolRoutine_Plain:
        Sst_MoveSprite2(self);
        self->anim = 0;
        Sst_Animate(self, Animation_SolFire);
        self->mapping_frame &= 3;
        SstOnScreenTest(self);
        break;

    case SolRoutine_Ball: {
        Sst_Animate(self, Animation_SolFire);
        Sst *sol = &sst[own->link];
        if (sol->code != sst_object_codes[ObjId_Sol]) {
            Sst_Delete(self);
            return;
        }
        if (sol->mapping_frame == 2 && self->angle == 0x40) { // it is thrown
            self->routine += 2;
            self->anim = 0;
            Own_Sol *s = (Own_Sol *)sol->own.u8;
            if (--s->balls == 0)
                sol->routine += 2;
            self->x_vel = ((sol->status & SST_STATUS_X_FLIP) != 0) ? 0x200 : (int16_t)-0x200;
            Sst_Draw(self);
            return;
        }
        int16_t sin, cos;
        CalcSine(self->angle, &sin, &cos);
        self->x_pos.f.u = (int16_t)((cos >> 4) + sol->x_pos.f.u);
        self->y_pos.f.u = (int16_t)((sin >> 4) + sol->y_pos.f.u);
        self->angle = (uint8_t)(self->angle + ((Own_Sol *)sol->own.u8)->spin);
        Sst_Draw(self);
        break;
    }

    case SolRoutine_Shot:
        Sst_MoveSprite2(self);
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        Sst_Animate(self, Animation_SolFire);
        Sst_Draw(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Rexon: a snake of lava, a body (96, the same code as 94) that is solid and walks to and fro, and when Sonic comes near rears up into five heads (97) in a chain, the last of which, its head, throws fireballs
// ---------------------------------------------------------------------------------------------------------------------------------------
const uint8_t *Rexon_Mappings(void) {
    return Mappings_Rexon;
}

#define ArtTile_Rexon 0x37E // ($6FC0)

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0;      // 0x29
    uint8_t timer;     // 0x2A
    uint8_t pad1;      // 0x2B
    uint8_t heads[5];  // 0x2C...: (the body's) the slots of its five heads
    uint8_t pad2[3];
} Own_RexonBody;

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0;      // 0x29
    uint8_t timer;     // 0x2A
    uint8_t wobble;    // 0x2B: where it is in its sway (the alpha's Obj_Player_Status)
    uint8_t body;      // 0x2C: the slot of the body
    uint8_t index;     // 0x2E: which head, 0 to 8 in twos (8: the one with the face)
    uint8_t next;      // 0x30: the slot of the next head (0: none)
    uint8_t last;      // 0x32: the slot of the last head
    int8_t sway;       // 0x38: which way it sways
    uint8_t phase;     // 0x39: how far it has gone
    uint8_t pad[6];
} Own_RexonHead;

STATIC_ASSERT(sizeof(Own_RexonBody) <= 0x18, "Own_RexonBody must fit the scratch memory");
STATIC_ASSERT(sizeof(Own_RexonHead) <= 0x18, "Own_RexonHead must fit the scratch memory");

enum { RexonRoutine_Init = 0, RexonRoutine_Walk = 2, RexonRoutine_Still = 4, RexonRoutine_Reared = 6 };
enum { HeadRoutine_Init = 0, HeadRoutine_Wait = 2, HeadRoutine_Rise = 4, HeadRoutine_Sway = 6, HeadRoutine_Fall = 8 };

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

static void RexonSolid(Sst *self, int16_t d2, int16_t d3, int16_t old_x) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x1B, d2, d3, old_x, NULL);
    }
}

// It looks at Sonic, stops, and sends up its five heads (Load_Rexon_Head_Obj)
static void Rexon_RearUp(Sst *self, Own_RexonBody *own) {
    self->routine = RexonRoutine_Reared;
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u);
    self->x_vel = 0;
    self->y_vel = 0;
    for (int i = 0; i < 5; i++) {
        Sst *head = Sst_Allocate();
        if (head == NULL)
            return;
        head->code = sst_object_codes[ObjId_RexonHead];
        head->render_flags = self->render_flags;
        Own_RexonHead *h = (Own_RexonHead *)head->own.u8;
        h->subtype = self->subtype;
        h->body = (uint8_t)(self - sst);
        own->heads[i] = (uint8_t)(head - sst);
        h->index = (uint8_t)(i * 2);
        head->x_pos.f.u = self->x_pos.f.u;
        head->y_pos.f.u = self->y_pos.f.u;
    }
}

void Obj_RexonBody(Sst *self) {
    Own_RexonBody *own = (Own_RexonBody *)self->own.u8;

    switch (self->routine) {
    case RexonRoutine_Init:
        self->mappings = Mappings_Rexon;
        self->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_Rexon);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
        self->routine += 2;
        self->mapping_frame = 2;
        self->x_vel = -0x20;
        own->timer = 0x80;
        break;

    case RexonRoutine_Walk: {
        if ((uint16_t)(DistanceX(self) + 0x60) < 0x100)
            Rexon_RearUp(self, own);
        const int16_t old_x = self->x_pos.f.u;
        if ((int8_t)--own->timer < 0) {
            own->timer = 0x80;
            self->x_vel = -self->x_vel;
            self->render_flags ^= SST_RENDER_X_FLIP;
        }
        Sst_MoveSprite2(self);
        RexonSolid(self, 8, 0x11, old_x);
        SstOnScreenTest(self);
        break;
    }

    case RexonRoutine_Still:
        if ((uint16_t)(DistanceX(self) + 0x60) < 0x100)
            Rexon_RearUp(self, own);
        RexonSolid(self, 8, 8, self->x_pos.f.u);
        SstOnScreenTest(self);
        break;

    default: // reared up (or anything else): only solid
        RexonSolid(self, 8, 8, self->x_pos.f.u);
        SstOnScreenTest(self);
        break;
    }
}

// How the heads sway: the offsets of the next head from this one, by the sway (Offset_0x028CA4)
static const int8_t rexon_sway[64] = {
    0x0F, 0x00, 0x0F, -1, 0x0F, -1, 0x0F, -2, 0x0F, -3, 0x0F, -4, 0x0E, -4, 0x0E, -5,
    0x0E, -6, 0x0E, -6, 0x0D, -7, 0x0D, -8, 0x0C, -8, 0x0C, -9, 0x0C, -10, 0x0B, -10,
    0x0B, -11, 0x0A, -11, 0x0A, -12, 0x09, -12, 0x08, -12, 0x08, -13, 0x07, -13, 0x06, -14,
    0x06, -14, 0x05, -14, 0x04, -14, 0x04, -15, 0x03, -15, 0x02, -15, 0x01, -15, 0x01, -15,
};

static const uint8_t rexon_delays[6] = { 0x1E, 0x18, 0x12, 0x0C, 0x06, 0x00 };
static const int16_t rexon_fall_speeds[5] = { 0x0080, -0x100, 0x0100, -0x80, 0x0080 };

// If the last head is gone the rest come apart (Offset_0x028B08)
static void Head_CheckAlive(Sst *self, Own_RexonHead *h) {
    if (sst[h->last].code != sst_object_codes[ObjId_RexonHead]) {
        self->routine = HeadRoutine_Fall;
        self->x_vel = rexon_fall_speeds[h->index >> 1];
    }
}

static void Head_Fireball(Sst *self, Own_RexonHead *h) {
    h->timer = 0x7F;
    Sst *ball = Sst_AllocateAfter(self);
    if (ball == NULL)
        return;
    ball->code = sst_object_codes[ObjId_EnemyWeapon];
    ball->mapping_frame = 3;
    ((Own_Weapon *)ball->own.u8)->weapon = Weapon_RexonFireball;
    ball->x_pos.f.u = self->x_pos.f.u;
    ball->y_pos.f.u = self->y_pos.f.u;
    int16_t speed = 1, reach = 0x10;
    if (!((self->render_flags & SST_RENDER_X_FLIP) != 0)) {
        speed = -speed;
        reach = -reach;
    }
    ball->x_vel = (int16_t)(speed << 8);
    ball->x_pos.f.u += reach;
    ball->y_pos.f.u += 4;
    ball->y_vel = (int16_t)((ball->y_vel & 0xFF00) | 0x80);
}

// The sway: it goes on by its direction each time and turns at the ends of $18 to $27 (Offset_0x028B86)
static void Head_Sway(Own_RexonHead *h) {
    h->wobble = (uint8_t)(h->wobble + h->sway);
    const uint8_t d0 = (uint8_t)(h->wobble - 0x18);
    if (h->wobble < 0x18 || d0 == 0 || d0 >= 0x10)
        h->sway = (int8_t)-h->sway;
}

// The next head is put where this one's sway puts it (Offset_0x028C00)
static void Head_PlaceNext(Sst *self, Own_RexonHead *h) {
    if (h->next == 0)
        return;
    Sst *next = &sst[h->next];
    const uint8_t w = h->wobble & 0x7F;
    int16_t d2 = rexon_sway[(w & 0x1F) * 2], d3 = rexon_sway[(w & 0x1F) * 2 + 1];
    switch ((w >> 4) & 6) {
    case 0:
        break;
    case 2: {
        const int16_t t = d2;
        d2 = d3;
        d3 = (int16_t)-t;
        break;
    }
    case 4:
        d2 = (int16_t)-d2;
        d3 = (int16_t)-d3;
        break;
    default: {
        const int16_t t = d2;
        d2 = (int16_t)-d3;
        d3 = t;
        break;
    }
    }
    next->x_pos.f.u = (int16_t)(self->x_pos.f.u + d2);
    // (only the low byte of its height is set, as the alpha does)
    next->y_pos.f.u = (int16_t)((next->y_pos.f.u & 0xFF00) | (uint8_t)(self->y_pos.f.u + d3));
}

void Obj_RexonHead(Sst *self) {
    Own_RexonHead *h = (Own_RexonHead *)self->own.u8;

    switch (self->routine) {
    case HeadRoutine_Init: {
        self->mappings = Mappings_Rexon;
        self->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_Rexon);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 8;
        self->routine += 2;
        self->x_pos.f.u += (((self->render_flags & SST_RENDER_X_FLIP) != 0) ? 0x28 : -0x18);
        self->y_pos.f.u += 0x10;
        h->sway = 1;
        self->collision_flags = 0x0B;
        const Own_RexonBody *body = (const Own_RexonBody *)sst[h->body].own.u8;
        if (h->index != 8) {
            self->mapping_frame = 1;
            self->collision_flags = 0x8B;
            h->next = body->heads[(h->index >> 1) + 1];
        }
        h->last = body->heads[4];
        h->timer = rexon_delays[h->index >> 1];
        h->phase = (uint8_t)(h->index >> 1);
        break;
    }

    case HeadRoutine_Wait:
        Head_CheckAlive(self, h);
        if ((int8_t)--h->timer < 0) {
            self->routine += 2;
            self->x_vel = (int16_t)0xFEE0;
            self->y_vel = (int16_t)0xFE00;
            h->timer = rexon_delays[(8 - h->index) >> 1];
        }
        SstOnScreenTest(self);
        break;

    case HeadRoutine_Rise:
        Head_CheckAlive(self, h);
        self->x_vel += 0x10;
        if ((int8_t)--h->timer < 0) {
            self->routine += 2;
            self->x_vel = 0;
            self->y_vel = 0;
            h->timer = 0x20;
            static const uint8_t starts[5] = { 0x24, 0x20, 0x1C, 0x1A, 0x61 }; // (the last is the byte after the table in the alpha: the first of its next routine's code)
            h->wobble = starts[h->index >> 1];
        } else {
            Sst_MoveSprite2(self);
        }
        SstOnScreenTest(self);
        break;

    case HeadRoutine_Sway:
        Head_CheckAlive(self, h);
        if (h->index == 8 && (int8_t)--h->timer < 0)
            Head_Fireball(self, h);
        h->phase++;
        if ((h->phase & 3) == 0) {
            Head_Sway(h);
            Head_PlaceNext(self, h);
        }
        SstOnScreenTest(self);
        break;

    default: // the rest, with the head gone: they are deleted at once in the alpha's own test (Offset_0x028AF0 compares the wrong way), unless they are below the level
        if ((uint16_t)(limit_btm2 + 0xE0) >= (uint16_t)self->y_pos.f.u) {
            Sst_Delete(self);
            return;
        }
        Sst_MoveSprite(self);
        SstOnScreenTest(self);
        break;
    }
}
