// Metropolis's badniks for Sonic 2, as the alpha has them: the Shellcracker (object 9F), a crab that walks to and fro and, when Sonic is near, shoots out its claw as a chain of eight pieces (object A0); and
// the Asteron (A4), a starfish that drifts towards Sonic when he comes near, and then blows up into five spikes. Both set themselves up from the alpha's table of badnik looks (see Coconuts.c).
#include "Object/MTZBadniks.h"
#include "Object/Coconuts.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/Asteron.h"
#include "Resource/Animation/Shellcracker.h"
#include "Resource/Animation/Slicer.h"
#include "Resource/Animation/SlicerPincers.h"
#include "Resource/Mappings/Asteron.h"
#include "Resource/Mappings/Shellcracker.h"
#include "Resource/Mappings/Slicer.h"

#define ArtTile_Shellcracker 0x30F // ($61E0 in the alpha's Metropolis list)
#define ArtTile_Asteron      0x368 // ($6D00)
#define ArtTile_Slicer       0x43C // ($8780)

const uint8_t *Asteron_Mappings(void) {
    return Mappings_Asteron;
}

// Object_Check_Player_Position: where Sonic is to the object (the word that the original returns in D0 is 2 when the object is to the left, in D1 when it is above) and how far (its x and y less his, in D2 and D3)
typedef struct {
    int d0, d1;
    int16_t dx, dy;
} Where;

static Where WhereIsSonic(const Object *obj) {
    Where w;
    w.dx = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
    w.dy = (int16_t)(obj->pos.l.y.f.u - player->pos.l.y.f.u);
    w.d0 = (uint16_t)obj->pos.l.x.f.u < (uint16_t)player->pos.l.x.f.u ? 2 : 0;
    w.d1 = (uint16_t)obj->pos.l.y.f.u < (uint16_t)player->pos.l.y.f.u ? 2 : 0;
    return w;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Shellcracker (9F) and its claw (A0)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad;      // 0x29
    uint16_t timer;   // 0x2A: (the claw's) the slot of the Shellcracker
    uint16_t index;   // 0x2C: (the Shellcracker's: its high byte is $FF when the last claw has come back) (the claw's: which piece, 0 to 14 in twos)
    uint8_t count_a;  // 0x2E
    uint8_t count_b;  // 0x2F
} Scratch_Shell;

enum { ShellRoutine_Init = 0, ShellRoutine_Walk = 2, ShellRoutine_Wait = 4, ShellRoutine_Attack = 6 };
enum { ClawRoutine_Init = 0, ClawRoutine_Main = 2, ClawRoutine_Fall = 4 };

static bool InAttackReach(const Where *w) {
    return w->d0 == 0 && (uint16_t)(w->dx + 0x60) < 0xC0;
}

static void Shell_Stop(Object *obj, Scratch_Shell *scratch) {
    obj->routine += 2;
    obj->frame = 0;
    scratch->timer = 0x3B;
}

static void Shell_Attack(Object *obj, Scratch_Shell *scratch) {
    obj->routine = ShellRoutine_Attack;
    obj->frame = 0;
    scratch->timer = 8;
}

// The eight pieces of the claw, one after the other, all at the crab's front (Load_Sheelcracker_Craw_Obj)
static void Shell_MakeClaw(Object *obj) {
    for (uint16_t i = 0; i < 16; i += 2) {
        Object *piece = FindNextFreeObj(obj + 1);
        if (piece == NULL)
            return;
        piece->type = ObjId_ShellcrackerClaw;
        Scratch_Shell *s = (Scratch_Shell *)&piece->scratch;
        s->subtype = 0x26;
        piece->frame = 5;
        piece->priority = 4;
        s->timer = (uint16_t)(obj - objects);
        s->index = i;
        piece->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u - 0x14);
        piece->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u - 8);
    }
}

void Obj_Shellcracker(Object *obj) {
    Scratch_Shell *scratch = (Scratch_Shell *)&obj->scratch;

    switch (obj->routine) {
    case ShellRoutine_Init:
        obj->mappings = Mappings_Shellcracker;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Shellcracker);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 5;
        obj->width_pixels = 0x18;
        obj->col_type = 0x0A;
        obj->routine += 2;
        obj->xsp = -0x40;
        obj->y_rad = 0x0C;
        obj->x_rad = 0x18;
        scratch->timer = 0x140;
        break;

    case ShellRoutine_Walk: {
        const Where w = WhereIsSonic(obj);
        if (InAttackReach(&w)) {
            Shell_Attack(obj, scratch);
            RememberState(obj);
            break;
        }
        SpeedToPos(obj);
        const int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d1 < -8 || d1 >= 0xC) { // the ledge or the wall: it turns, and waits
            obj->xsp = -obj->xsp;
            Shell_Stop(obj, scratch);
            RememberState(obj);
            break;
        }
        obj->pos.l.y.f.u += d1;
        if ((int16_t)--scratch->timer < 0) {
            Shell_Stop(obj, scratch);
            RememberState(obj);
            break;
        }
        obj->anim = 0;
        AnimateSprite(obj, Animation_Shellcracker);
        RememberState(obj);
        break;
    }

    case ShellRoutine_Wait: {
        const Where w = WhereIsSonic(obj);
        if (InAttackReach(&w)) {
            Shell_Attack(obj, scratch);
        } else if ((int16_t)--scratch->timer < 0) {
            obj->routine -= 2;
            scratch->timer = 0x140;
        }
        RememberState(obj);
        break;
    }

    case ShellRoutine_Attack:
        switch (obj->routine_sec) {
        case 0:
            if ((int16_t)--scratch->timer < 0) {
                obj->routine_sec += 2;
                obj->frame = 3;
                Shell_MakeClaw(obj);
            }
            break;
        case 2: // waits for the claw to come back
            if (scratch->index & 0xFF00) {
                obj->routine_sec += 2;
                scratch->timer = 0x20;
            }
            break;
        case 4:
            if ((int16_t)--scratch->timer < 0) {
                obj->routine_sec = 0;
                scratch->index = 0;
                obj->routine = ShellRoutine_Walk;
                scratch->timer = 0x140;
            }
            break;
        }
        RememberState(obj);
        break;
    }
}

static const uint8_t claw_delays[8] = { 0x00, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F };
static const uint8_t claw_lengths[8] = { 0x0D, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00 };

static bool Claw_Counted(uint8_t *counter) { // (subq.b #1: it goes on when the count is 0 or below)
    return (int8_t)--*counter <= 0;
}

void Obj_ShellcrackerClaw(Object *obj) {
    Scratch_Shell *scratch = (Scratch_Shell *)&obj->scratch;

    switch (obj->routine) {
    case ClawRoutine_Init:
        obj->mappings = Mappings_Shellcracker;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Shellcracker);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0xC;
        obj->col_type = 0x9A;
        obj->routine += 2;
        if (scratch->index != 0) {
            obj->frame = 4;
            obj->pos.l.x.f.u += 6;
            obj->pos.l.y.f.u += 6;
        }
        scratch->count_a = claw_delays[scratch->index >> 1];
        RememberState(obj);
        break;

    case ClawRoutine_Main: {
        Object *shell = &objects[scratch->timer & 0xFF];
        if (shell->type != ObjId_Shellcracker) { // its crab is gone: the piece falls
            obj->routine = ClawRoutine_Fall;
            scratch->count_a = 1; // (the word at $2E: $100)
            scratch->count_b = 0;
            RememberState(obj);
            break;
        }
        switch (obj->routine_sec) {
        case 0:
            if (!Claw_Counted(&scratch->count_a))
                break;
            obj->routine_sec += 2;
            if (scratch->index < 0xE) {
                obj->xsp = -0x400;
                scratch->count_a = scratch->count_b = claw_lengths[scratch->index >> 1];
            } else {
                scratch->count_a = 0;
                scratch->count_b = 0x0B;
            }
            break;
        case 2:
            SpeedToPos(obj);
            if (Claw_Counted(&scratch->count_a)) {
                obj->routine_sec += 2;
                scratch->count_a = 8;
            }
            break;
        case 4:
            if (Claw_Counted(&scratch->count_a)) {
                obj->routine_sec += 2;
                obj->xsp = -obj->xsp;
            }
            break;
        case 6:
            SpeedToPos(obj);
            if (Claw_Counted(&scratch->count_b)) {
                if (scratch->index == 0) { // the first piece is the last back: the crab shuts its claw
                    shell->frame = 0;
                    ((Scratch_Shell *)&shell->scratch)->index = 0xFF00;
                }
                ObjectDelete(obj);
                return;
            }
            break;
        }
        RememberState(obj);
        break;
    }

    case ClawRoutine_Fall: {
        ObjectFall(obj);
        const int16_t word = (int16_t)((((uint16_t)scratch->count_a << 8) | scratch->count_b) - 1);
        scratch->count_a = (uint8_t)((uint16_t)word >> 8);
        scratch->count_b = (uint8_t)word;
        if (word < 0) {
            ObjectDelete(obj);
            return;
        }
        RememberState(obj);
        break;
    }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Asteron (A4)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad;     // 0x29
    uint8_t timer;   // 0x2A
} Scratch_Asteron;

// Its spikes (Enemy_Weapon_Data): where from its centre, how fast, which frame and whether flipped
static const struct { int8_t dx, dy, xsp, ysp, frame, flip; } spikes[5] = {
    { 0x00, -8, 0x00, -4, 2, 0 },
    { 0x08, -4, 0x03, -1, 3, 1 },
    { 0x08, 0x08, 0x03, 0x03, 4, 1 },
    { -8, 0x08, -3, 0x03, 4, 0 },
    { -8, -4, -3, -1, 3, 0 },
};

static void Asteron_Explode(Object *obj) {
    obj->type = 0x27; // (an explosion, and no animal)
    obj->routine = 2;
    for (int i = 0; i < 5; i++) {
        Object *spike = FindNextFreeObj(obj + 1);
        if (spike == NULL)
            return;
        spike->type = ObjId_EnemyWeapon;
        ((Scratch_Weapon *)&spike->scratch)->weapon = Weapon_AsteronSpike;
        spike->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + spikes[i].dx);
        spike->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + spikes[i].dy);
        spike->xsp = (int16_t)(spikes[i].xsp << 8);
        spike->ysp = (int16_t)(spikes[i].ysp << 8);
        spike->frame = (uint8_t)spikes[i].frame;
        spike->render.b = (uint8_t)spikes[i].flip;
    }
}

void Obj_Asteron(Object *obj) {
    Scratch_Asteron *scratch = (Scratch_Asteron *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->mappings = Mappings_Asteron;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Asteron);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->col_type = 0x0B;
        obj->routine += 2;
        break;

    case 2: { // it sets off towards Sonic when he is $10 to $60 pixels away, on either axis
        static const int16_t speeds[2] = { -0x40, 0x40 };
        Where w = WhereIsSonic(obj);
        int16_t dx = w.dx < 0 ? (int16_t)-w.dx : w.dx;
        int16_t dy = w.dy < 0 ? (int16_t)-w.dy : w.dy;
        if (dx >= 0x10 && dx < 0x60) {
            obj->xsp = speeds[w.d0 >> 1];
            obj->routine = 4;
            scratch->timer = 0x40;
        }
        if (dy >= 0x10 && dy < 0x60) {
            obj->ysp = speeds[w.d1 >> 1];
            obj->routine = 4;
            scratch->timer = 0x40;
        }
        RememberState(obj);
        break;
    }

    case 4:
        if ((int8_t)--scratch->timer < 0) {
            Asteron_Explode(obj);
            RememberState(obj);
            break;
        }
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_Asteron);
        RememberState(obj);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Slicer (A1) and its pincers (A2)
// ---------------------------------------------------------------------------------------------------------------------------------------
// A praying mantis that walks to and fro, turns at ledges, and when Sonic is in front of it within $80 pixels raises its blades and lets two pincers fly at him
typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad;      // 0x29
    uint16_t timer;   // 0x2A: (its) a count in a byte; (a pincer's) the slot of its Slicer
    uint16_t index;   // 0x2C: (a pincer's) which one, 0 or 2
    uint16_t life;    // 0x2E: (a pincer's) how many frames it flies for
} Scratch_Slicer;

static void Slicer_SetTimer(Object *obj, uint8_t v) {
    ((Scratch_Slicer *)&obj->scratch)->timer = (uint16_t)((v << 8) | (((Scratch_Slicer *)&obj->scratch)->timer & 0xFF)); // (the byte at $2A is the high one of the word)
}

static uint8_t Slicer_Timer(const Object *obj) {
    return (uint8_t)(((const Scratch_Slicer *)&obj->scratch)->timer >> 8);
}

enum { SlicerRoutine_Init = 0, SlicerRoutine_Walk = 2, SlicerRoutine_Turn = 4, SlicerRoutine_Raise = 6, SlicerRoutine_Throw = 8 };

static void Slicer_MakePincers(Object *obj) {
    for (uint16_t i = 0; i < 4; i += 2) {
        Object *pincer = FindNextFreeObj(obj + 1);
        if (pincer == NULL)
            return;
        pincer->type = ObjId_SlicerPincers;
        Scratch_Slicer *s = (Scratch_Slicer *)&pincer->scratch;
        s->subtype = 0x2A;
        pincer->frame = 5;
        pincer->priority = 4;
        s->timer = (uint16_t)(obj - objects);
        s->index = i;
        pincer->pos.l.x.f.u = obj->pos.l.x.f.u;
        pincer->pos.l.y.f.u = obj->pos.l.y.f.u;
    }
}

void Obj_Slicer(Object *obj) {
    switch (obj->routine) {
    case SlicerRoutine_Init:
        obj->mappings = Mappings_Slicer;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Slicer); // ($243C: palette line 2)
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 5;
        obj->width_pixels = 0x10;
        obj->col_type = 0x06;
        obj->routine += 2;
        obj->xsp = -0x40;
        obj->y_rad = 0x10;
        obj->x_rad = 0x10;
        break;

    case SlicerRoutine_Walk: {
        const Where w = WhereIsSonic(obj);
        if (w.d0 == 0 && (uint16_t)(w.dx + 0x80) < 0x100) { // Sonic in front of it
            obj->routine += 4;
            obj->frame = 3;
            Slicer_SetTimer(obj, 8);
            RememberState(obj);
            break;
        }
        SpeedToPos(obj);
        const int16_t d1 = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (d1 < -8 || d1 >= 0xC) { // a ledge or a wall
            obj->routine += 2;
            Slicer_SetTimer(obj, 0x3B);
            RememberState(obj);
            break;
        }
        obj->pos.l.y.f.u += d1;
        AnimateSprite(obj, Animation_Slicer);
        RememberState(obj);
        break;
    }

    case SlicerRoutine_Turn:
        if ((int8_t)(Slicer_Timer(obj) - 1) < 0) {
            obj->routine -= 2;
            obj->xsp = -obj->xsp;
            obj->status.o.f.x_flip ^= 1;
        } else {
            Slicer_SetTimer(obj, (uint8_t)(Slicer_Timer(obj) - 1));
        }
        RememberState(obj);
        break;

    case SlicerRoutine_Raise:
        if ((int8_t)(Slicer_Timer(obj) - 1) < 0) {
            obj->routine += 2;
            obj->frame = 4;
            Slicer_MakePincers(obj);
        } else {
            Slicer_SetTimer(obj, (uint8_t)(Slicer_Timer(obj) - 1));
        }
        RememberState(obj);
        break;

    case SlicerRoutine_Throw:
        RememberState(obj);
        break;
    }
}

enum { PincerRoutine_Init = 0, PincerRoutine_Fly = 2, PincerRoutine_Fall = 4 };

void Obj_SlicerPincers(Object *obj) {
    Scratch_Slicer *scratch = (Scratch_Slicer *)&obj->scratch;

    switch (obj->routine) {
    case PincerRoutine_Init: {
        static const int8_t offsets[2][2] = { { 6, 0 }, { -0x10, 0 } };
        obj->mappings = Mappings_Slicer;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Slicer);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->col_type = 0x9A;
        obj->routine += 2;
        obj->xsp = -0x200;
        scratch->life = 0x200;
        obj->pos.l.x.f.u += offsets[scratch->index >> 1][0];
        obj->pos.l.y.f.u += offsets[scratch->index >> 1][1];
        break;
    }

    case PincerRoutine_Fly: {
        Object *slicer = &objects[scratch->timer & 0xFF];
        if ((int16_t)--scratch->life >= 0 && slicer->type == ObjId_Slicer) {
            // it homes in on Sonic, up to $200 each way
            const Where w = WhereIsSonic(obj);
            static const int16_t accel[2] = { -0x10, 0x10 };
            obj->xsp = (int16_t)(obj->xsp + accel[w.d0 >> 1]);
            obj->ysp = (int16_t)(obj->ysp + accel[w.d1 >> 1]);
            if (obj->xsp > 0x200)
                obj->xsp = 0x200;
            if (obj->xsp < -0x200)
                obj->xsp = -0x200;
            if (obj->ysp > 0x200)
                obj->ysp = 0x200;
            if (obj->ysp < -0x200)
                obj->ysp = -0x200;
            SpeedToPos(obj);
            AnimateSprite(obj, Animation_SlicerPincers);
            RememberState(obj);
            break;
        }
        obj->routine += 2; // (its Slicer is gone or it has flown long enough: it falls)
        scratch->life = 0x80;
    }
    // Fallthrough
    case PincerRoutine_Fall:
        if ((int16_t)--scratch->life < 0) {
            ObjectDelete(obj);
            return;
        }
        ObjectFall(obj);
        AnimateSprite(obj, Animation_SlicerPincers);
        RememberState(obj);
        break;
    }
}
