// Hill Top's badniks for Sonic 2, as the alpha has them (see HTZBadniks.h). Each sets itself up from the alpha's table of badnik looks, which is here in the objects themselves (as in MTZBadniks.c). Their mappings have
// the tiles of their art in the pieces (the Spiker's $520, the Sol's $3DE and $3AE: its see-saw badnik's and the fireball's), so the objects themselves start from tile 0.
#include "Object/HTZBadniks.h"
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
static int16_t DistanceX(const Object *obj) {
    return (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
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
} Scratch_Spiker;

enum { SpikerRoutine_Init = 0, SpikerRoutine_Walk = 2, SpikerRoutine_Pause = 4, SpikerRoutine_Fire = 6 };

static void Spiker_SetUp(Object *obj) {
    obj->mappings = Mappings_SpikerBadnik;
    obj->tile = 0;
    obj->render.b |= SPRITE_CAM_FIELD;
    obj->priority = 4;
    obj->width_pixels = 0x10;
    obj->col_type = 0x12;
    obj->routine += 2;
}

// It stops to fire when Sonic is within $20 pixels each side of it (and has not fired already). Returns that it did
static bool Spiker_Check(Object *obj, Scratch_Spiker *scratch) {
    if (scratch->fired)
        return false;
    if ((uint16_t)(DistanceX(obj) + 0x20) >= 0x40)
        return false;
    scratch->saved = obj->routine;
    obj->routine = SpikerRoutine_Fire;
    scratch->wait = 0x10;
    return true;
}

static void Spiker_Turn(Object *obj) {
    obj->xsp = -obj->xsp;
    obj->render.f.x_flip ^= 1;
    obj->status.o.f.x_flip ^= 1;
}

void Obj_Spiker(Object *obj) {
    Scratch_Spiker *scratch = (Scratch_Spiker *)&obj->scratch;

    switch (obj->routine) {
    case SpikerRoutine_Init:
        Spiker_SetUp(obj);
        scratch->timer = 0x40;
        obj->xsp = 0x80;
        obj->render.f.x_flip ^= 1;
        obj->status.o.f.x_flip ^= 1;
        break;

    case SpikerRoutine_Walk:
        Spiker_Check(obj, scratch);
        if ((int8_t)--scratch->timer < 0) {
            obj->routine += 2;
            scratch->timer = 0x10;
            RememberState(obj);
            break;
        }
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_SpikerBadnik);
        RememberState(obj);
        break;

    case SpikerRoutine_Pause:
        Spiker_Check(obj, scratch);
        if ((int8_t)--scratch->timer < 0) {
            obj->routine -= 2;
            scratch->timer = 0x40;
            Spiker_Turn(obj);
        }
        RememberState(obj);
        break;

    case SpikerRoutine_Fire:
        if (scratch->wait != 8) {
            scratch->wait--;
            RememberState(obj);
            break;
        }
        {
            Object *drill = FindNextFreeObj(obj + 1);
            if (drill != NULL) {
                scratch->fired = 0xFF;
                drill->type = ObjId_SpikerDrill;
                Scratch_Spiker *d = (Scratch_Spiker *)&drill->scratch;
                d->subtype = scratch->subtype;
                d->timer = (uint8_t)(obj - objects);
                drill->pos.l.x.f.u = obj->pos.l.x.f.u;
                drill->pos.l.y.f.u = obj->pos.l.y.f.u;
                drill->col_type |= 0x80;
                drill->frame = 4;
                obj->frame = 2;
                obj->anim = 1;
            }
        }
        obj->routine = scratch->saved;
        RememberState(obj);
        break;
    }
}

void Obj_SpikerDrill(Object *obj) {
    Scratch_Spiker *scratch = (Scratch_Spiker *)&obj->scratch;

    if (obj->routine == 0) {
        Spiker_SetUp(obj);
        obj->render.b |= 0x80;
        const Object *spiker = &objects[scratch->timer];
        const uint8_t flips = spiker->render.b & 3;
        obj->render.b = (uint8_t)((obj->render.b & ~3) | flips);
        obj->ysp = (flips & 2) ? 0x200 : -0x200; // (up, or down if the Spiker is upside down)
        return;
    }
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    obj->render.f.x_flip ^= 1; // (it flickers)
    SpeedToPos(obj);
    RememberState(obj);
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
} Scratch_Sol;

STATIC_ASSERT(sizeof(Scratch_Sol) == 0x18, "Scratch_Sol must fill the scratch memory");

enum { SolRoutine_Init = 0, SolRoutine_Wait = 2, SolRoutine_Plain = 4, SolRoutine_Ball = 6, SolRoutine_Shot = 8 };

static void Sol_Make(Object *obj, Scratch_Sol *scratch) {
    obj->mappings = Mappings_Sol;
    obj->tile = 0;
    obj->render.b |= SPRITE_CAM_FIELD;
    obj->priority = 4;
    obj->col_type = 0x0B;
    obj->width_pixels = 0x0C;

    uint8_t angle = 0;
    for (int i = 0; i < 4; i++, angle += 0x40) {
        Object *ball = FindNextFreeObj(obj + 1);
        if (ball == NULL)
            break;
        scratch->balls++;
        ball->type = ObjId_Sol;
        ball->routine = SolRoutine_Ball;
        ball->mappings = obj->mappings;
        ball->tile = obj->tile;
        ball->render.b |= SPRITE_CAM_FIELD;
        ball->priority = 4;
        ball->width_pixels = 8;
        ball->frame = 3;
        ball->col_type = 0x98;
        ball->angle = angle;
        ((Scratch_Sol *)&ball->scratch)->link = (uint8_t)(obj - objects);
    }
    scratch->spin = obj->status.o.f.x_flip ? -1 : 1;
    obj->routine = (uint8_t)(scratch->subtype + 2);
    obj->xsp = obj->status.o.f.x_flip ? 0x40 : -0x40;
}

void Obj_Sol(Object *obj) {
    Scratch_Sol *scratch = (Scratch_Sol *)&obj->scratch;

    switch (obj->routine) {
    case SolRoutine_Init:
        Sol_Make(obj, scratch);
        break;

    case SolRoutine_Wait: { // it ignites when Sonic is within $A0 pixels across and $50 up or down
        const uint16_t px = (uint16_t)player->pos.l.x.f.u, ox = (uint16_t)obj->pos.l.x.f.u;
        const uint16_t py = (uint16_t)player->pos.l.y.f.u, oy = (uint16_t)obj->pos.l.y.f.u;
        const uint16_t dx = px >= ox ? (uint16_t)(px - ox) : (uint16_t)(ox - px);
        const uint16_t dy = py >= oy ? (uint16_t)(py - oy) : (uint16_t)(oy - py);
        if (dx < 0xA0 && dy < 0x50 && !debug_use)
            obj->anim = 1;
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_Sol);
        obj->frame &= 3;
        RememberState(obj);
        break;
    }

    case SolRoutine_Plain:
        SpeedToPos(obj);
        obj->anim = 0;
        AnimateSprite(obj, Animation_SolFire);
        obj->frame &= 3;
        RememberState(obj);
        break;

    case SolRoutine_Ball: {
        AnimateSprite(obj, Animation_SolFire);
        Object *sol = &objects[scratch->link];
        if (sol->type != ObjId_Sol) {
            ObjectDelete(obj);
            return;
        }
        if (sol->frame == 2 && obj->angle == 0x40) { // it is thrown
            obj->routine += 2;
            obj->anim = 0;
            Scratch_Sol *s = (Scratch_Sol *)&sol->scratch;
            if (--s->balls == 0)
                sol->routine += 2;
            obj->xsp = sol->status.o.f.x_flip ? 0x200 : (int16_t)-0x200;
            DisplaySprite(obj);
            return;
        }
        int16_t sin, cos;
        CalcSine(obj->angle, &sin, &cos);
        obj->pos.l.x.f.u = (int16_t)((cos >> 4) + sol->pos.l.x.f.u);
        obj->pos.l.y.f.u = (int16_t)((sin >> 4) + sol->pos.l.y.f.u);
        obj->angle = (uint8_t)(obj->angle + ((Scratch_Sol *)&sol->scratch)->spin);
        DisplaySprite(obj);
        break;
    }

    case SolRoutine_Shot:
        SpeedToPos(obj);
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        AnimateSprite(obj, Animation_SolFire);
        DisplaySprite(obj);
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
} Scratch_RexonBody;

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
} Scratch_RexonHead;

STATIC_ASSERT(sizeof(Scratch_RexonBody) <= 0x18, "Scratch_RexonBody must fit the scratch memory");
STATIC_ASSERT(sizeof(Scratch_RexonHead) <= 0x18, "Scratch_RexonHead must fit the scratch memory");

enum { RexonRoutine_Init = 0, RexonRoutine_Walk = 2, RexonRoutine_Still = 4, RexonRoutine_Reared = 6 };
enum { HeadRoutine_Init = 0, HeadRoutine_Wait = 2, HeadRoutine_Rise = 4, HeadRoutine_Sway = 6, HeadRoutine_Fall = 8 };

static Object *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
}

static void RexonSolid(Object *obj, int16_t d2, int16_t d3, int16_t old_x) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = Character(who);
        if (chr != NULL)
            Solid_Character(obj, chr, who, 0x1B, d2, d3, old_x, NULL);
    }
}

// It looks at Sonic, stops, and sends up its five heads (Load_Rexon_Head_Obj)
static void Rexon_RearUp(Object *obj, Scratch_RexonBody *scratch) {
    obj->routine = RexonRoutine_Reared;
    obj->render.f.x_flip = (uint16_t)obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u;
    obj->xsp = 0;
    obj->ysp = 0;
    for (int i = 0; i < 5; i++) {
        Object *head = FindFreeObj();
        if (head == NULL)
            return;
        head->type = ObjId_RexonHead;
        head->render.b = obj->render.b;
        Scratch_RexonHead *h = (Scratch_RexonHead *)&head->scratch;
        h->subtype = scratch->subtype;
        h->body = (uint8_t)(obj - objects);
        scratch->heads[i] = (uint8_t)(head - objects);
        h->index = (uint8_t)(i * 2);
        head->pos.l.x.f.u = obj->pos.l.x.f.u;
        head->pos.l.y.f.u = obj->pos.l.y.f.u;
    }
}

void Obj_RexonBody(Object *obj) {
    Scratch_RexonBody *scratch = (Scratch_RexonBody *)&obj->scratch;

    switch (obj->routine) {
    case RexonRoutine_Init:
        obj->mappings = Mappings_Rexon;
        obj->tile = TILE_MAP(0, 3, 0, 0, ArtTile_Rexon);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->col_type = 0;
        obj->routine += 2;
        obj->frame = 2;
        obj->xsp = -0x20;
        scratch->timer = 0x80;
        break;

    case RexonRoutine_Walk: {
        if ((uint16_t)(DistanceX(obj) + 0x60) < 0x100)
            Rexon_RearUp(obj, scratch);
        const int16_t old_x = obj->pos.l.x.f.u;
        if ((int8_t)--scratch->timer < 0) {
            scratch->timer = 0x80;
            obj->xsp = -obj->xsp;
            obj->render.f.x_flip ^= 1;
        }
        SpeedToPos(obj);
        RexonSolid(obj, 8, 0x11, old_x);
        RememberState(obj);
        break;
    }

    case RexonRoutine_Still:
        if ((uint16_t)(DistanceX(obj) + 0x60) < 0x100)
            Rexon_RearUp(obj, scratch);
        RexonSolid(obj, 8, 8, obj->pos.l.x.f.u);
        RememberState(obj);
        break;

    default: // reared up (or anything else): only solid
        RexonSolid(obj, 8, 8, obj->pos.l.x.f.u);
        RememberState(obj);
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
static void Head_CheckAlive(Object *obj, Scratch_RexonHead *h) {
    if (objects[h->last].type != ObjId_RexonHead) {
        obj->routine = HeadRoutine_Fall;
        obj->xsp = rexon_fall_speeds[h->index >> 1];
    }
}

static void Head_Fireball(Object *obj, Scratch_RexonHead *h) {
    h->timer = 0x7F;
    Object *ball = FindNextFreeObj(obj + 1);
    if (ball == NULL)
        return;
    ball->type = ObjId_EnemyWeapon;
    ball->frame = 3;
    ((Scratch_Weapon *)&ball->scratch)->weapon = Weapon_RexonFireball;
    ball->pos.l.x.f.u = obj->pos.l.x.f.u;
    ball->pos.l.y.f.u = obj->pos.l.y.f.u;
    int16_t speed = 1, reach = 0x10;
    if (!obj->render.f.x_flip) {
        speed = -speed;
        reach = -reach;
    }
    ball->xsp = (int16_t)(speed << 8);
    ball->pos.l.x.f.u += reach;
    ball->pos.l.y.f.u += 4;
    ball->ysp = (int16_t)((ball->ysp & 0xFF00) | 0x80);
}

// The sway: it goes on by its direction each time and turns at the ends of $18 to $27 (Offset_0x028B86)
static void Head_Sway(Scratch_RexonHead *h) {
    h->wobble = (uint8_t)(h->wobble + h->sway);
    const uint8_t d0 = (uint8_t)(h->wobble - 0x18);
    if (h->wobble < 0x18 || d0 == 0 || d0 >= 0x10)
        h->sway = (int8_t)-h->sway;
}

// The next head is put where this one's sway puts it (Offset_0x028C00)
static void Head_PlaceNext(Object *obj, Scratch_RexonHead *h) {
    if (h->next == 0)
        return;
    Object *next = &objects[h->next];
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
    next->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + d2);
    // (only the low byte of its height is set, as the alpha does)
    next->pos.l.y.f.u = (int16_t)((next->pos.l.y.f.u & 0xFF00) | (uint8_t)(obj->pos.l.y.f.u + d3));
}

void Obj_RexonHead(Object *obj) {
    Scratch_RexonHead *h = (Scratch_RexonHead *)&obj->scratch;

    switch (obj->routine) {
    case HeadRoutine_Init: {
        obj->mappings = Mappings_Rexon;
        obj->tile = TILE_MAP(0, 3, 0, 0, ArtTile_Rexon);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 8;
        obj->routine += 2;
        obj->pos.l.x.f.u += (obj->render.f.x_flip ? 0x28 : -0x18);
        obj->pos.l.y.f.u += 0x10;
        h->sway = 1;
        obj->col_type = 0x0B;
        const Scratch_RexonBody *body = (const Scratch_RexonBody *)&objects[h->body].scratch;
        if (h->index != 8) {
            obj->frame = 1;
            obj->col_type = 0x8B;
            h->next = body->heads[(h->index >> 1) + 1];
        }
        h->last = body->heads[4];
        h->timer = rexon_delays[h->index >> 1];
        h->phase = (uint8_t)(h->index >> 1);
        break;
    }

    case HeadRoutine_Wait:
        Head_CheckAlive(obj, h);
        if ((int8_t)--h->timer < 0) {
            obj->routine += 2;
            obj->xsp = (int16_t)0xFEE0;
            obj->ysp = (int16_t)0xFE00;
            h->timer = rexon_delays[(8 - h->index) >> 1];
        }
        RememberState(obj);
        break;

    case HeadRoutine_Rise:
        Head_CheckAlive(obj, h);
        obj->xsp += 0x10;
        if ((int8_t)--h->timer < 0) {
            obj->routine += 2;
            obj->xsp = 0;
            obj->ysp = 0;
            h->timer = 0x20;
            static const uint8_t starts[5] = { 0x24, 0x20, 0x1C, 0x1A, 0x61 }; // (the last is the byte after the table in the alpha: the first of its next routine's code)
            h->wobble = starts[h->index >> 1];
        } else {
            SpeedToPos(obj);
        }
        RememberState(obj);
        break;

    case HeadRoutine_Sway:
        Head_CheckAlive(obj, h);
        if (h->index == 8 && (int8_t)--h->timer < 0)
            Head_Fireball(obj, h);
        h->phase++;
        if ((h->phase & 3) == 0) {
            Head_Sway(h);
            Head_PlaceNext(obj, h);
        }
        RememberState(obj);
        break;

    default: // the rest, with the head gone: they are deleted at once in the alpha's own test (Offset_0x028AF0 compares the wrong way), unless they are below the level
        if ((uint16_t)(limit_btm2 + 0xE0) >= (uint16_t)obj->pos.l.y.f.u) {
            ObjectDelete(obj);
            return;
        }
        ObjectFall(obj);
        RememberState(obj);
        break;
    }
}
