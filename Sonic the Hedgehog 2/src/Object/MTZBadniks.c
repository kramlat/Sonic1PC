// Metropolis's badniks for Sonic 2, as the alpha has them: the Shellcracker (object 9F), a crab that walks to and fro and, when Sonic is near, shoots out its claw as a chain of eight pieces (object A0); and
// the Asteron (A4), a starfish that drifts towards Sonic when he comes near, and then blows up into five spikes. Both set themselves up from the alpha's table of badnik looks (see Coconuts.c).
#include "Object/MTZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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

static Where WhereIsSonic(const Sst *self) {
    Where w;
    w.dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    w.dy = (int16_t)(self->y_pos.f.u - player->y_pos.f.u);
    w.d0 = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? 2 : 0;
    w.d1 = (uint16_t)self->y_pos.f.u < (uint16_t)player->y_pos.f.u ? 2 : 0;
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
} Own_Shell;

enum { ShellRoutine_Init = 0, ShellRoutine_Walk = 2, ShellRoutine_Wait = 4, ShellRoutine_Attack = 6 };
enum { ClawRoutine_Init = 0, ClawRoutine_Main = 2, ClawRoutine_Fall = 4 };

static bool InAttackReach(const Where *w) {
    return w->d0 == 0 && (uint16_t)(w->dx + 0x60) < 0xC0;
}

static void Shell_Stop(Sst *self, Own_Shell *own) {
    self->routine += 2;
    self->mapping_frame = 0;
    own->timer = 0x3B;
}

static void Shell_Attack(Sst *self, Own_Shell *own) {
    self->routine = ShellRoutine_Attack;
    self->mapping_frame = 0;
    own->timer = 8;
}

// The eight pieces of the claw, one after the other, all at the crab's front (Load_Sheelcracker_Craw_Obj)
static void Shell_MakeClaw(Sst *self) {
    for (uint16_t i = 0; i < 16; i += 2) {
        Sst *piece = Sst_AllocateAfter(self);
        if (piece == NULL)
            return;
        piece->code = sst_object_codes[ObjId_ShellcrackerClaw];
        Own_Shell *s = (Own_Shell *)piece->own.u8;
        piece->subtype = 0x26;
        piece->mapping_frame = 5;
        piece->priority = 4 * 0x80;
        s->timer = (uint16_t)(self - sst);
        s->index = i;
        piece->x_pos.f.u = (int16_t)(self->x_pos.f.u - 0x14);
        piece->y_pos.f.u = (int16_t)(self->y_pos.f.u - 8);
    }
}

void Obj_Shellcracker(Sst *self) {
    Own_Shell *own = (Own_Shell *)self->own.u8;

    switch (self->routine) {
    case ShellRoutine_Init:
        self->mappings = Mappings_Shellcracker;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Shellcracker);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 0x18;
        self->collision_flags = 0x0A;
        self->routine += 2;
        self->x_vel = -0x40;
        self->y_radius = 0x0C;
        self->x_radius = 0x18;
        own->timer = 0x140;
        break;

    case ShellRoutine_Walk: {
        const Where w = WhereIsSonic(self);
        if (InAttackReach(&w)) {
            Shell_Attack(self, own);
            SstOnScreenTest(self);
            break;
        }
        Sst_MoveSprite2(self);
        const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < -8 || d1 >= 0xC) { // the ledge or the wall: it turns, and waits
            self->x_vel = -self->x_vel;
            Shell_Stop(self, own);
            SstOnScreenTest(self);
            break;
        }
        self->y_pos.f.u += d1;
        if ((int16_t)--own->timer < 0) {
            Shell_Stop(self, own);
            SstOnScreenTest(self);
            break;
        }
        self->anim = 0;
        Sst_Animate(self, Animation_Shellcracker);
        SstOnScreenTest(self);
        break;
    }

    case ShellRoutine_Wait: {
        const Where w = WhereIsSonic(self);
        if (InAttackReach(&w)) {
            Shell_Attack(self, own);
        } else if ((int16_t)--own->timer < 0) {
            self->routine -= 2;
            own->timer = 0x140;
        }
        SstOnScreenTest(self);
        break;
    }

    case ShellRoutine_Attack:
        switch (self->routine_secondary) {
        case 0:
            if ((int16_t)--own->timer < 0) {
                self->routine_secondary += 2;
                self->mapping_frame = 3;
                Shell_MakeClaw(self);
            }
            break;
        case 2: // waits for the claw to come back
            if (own->index & 0xFF00) {
                self->routine_secondary += 2;
                own->timer = 0x20;
            }
            break;
        case 4:
            if ((int16_t)--own->timer < 0) {
                self->routine_secondary = 0;
                own->index = 0;
                self->routine = ShellRoutine_Walk;
                own->timer = 0x140;
            }
            break;
        }
        SstOnScreenTest(self);
        break;
    }
}

static const uint8_t claw_delays[8] = { 0x00, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F };
static const uint8_t claw_lengths[8] = { 0x0D, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x00 };

static bool Claw_Counted(uint8_t *counter) { // (subq.b #1: it goes on when the count is 0 or below)
    return (int8_t)--*counter <= 0;
}

void Obj_ShellcrackerClaw(Sst *self) {
    Own_Shell *own = (Own_Shell *)self->own.u8;

    switch (self->routine) {
    case ClawRoutine_Init:
        self->mappings = Mappings_Shellcracker;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Shellcracker);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0xC;
        self->collision_flags = 0x9A;
        self->routine += 2;
        if (own->index != 0) {
            self->mapping_frame = 4;
            self->x_pos.f.u += 6;
            self->y_pos.f.u += 6;
        }
        own->count_a = claw_delays[own->index >> 1];
        SstOnScreenTest(self);
        break;

    case ClawRoutine_Main: {
        Sst *shell = &sst[own->timer & 0xFF];
        if (shell->code != sst_object_codes[ObjId_Shellcracker]) { // its crab is gone: the piece falls
            self->routine = ClawRoutine_Fall;
            own->count_a = 1; // (the word at $2E: $100)
            own->count_b = 0;
            SstOnScreenTest(self);
            break;
        }
        switch (self->routine_secondary) {
        case 0:
            if (!Claw_Counted(&own->count_a))
                break;
            self->routine_secondary += 2;
            if (own->index < 0xE) {
                self->x_vel = -0x400;
                own->count_a = own->count_b = claw_lengths[own->index >> 1];
            } else {
                own->count_a = 0;
                own->count_b = 0x0B;
            }
            break;
        case 2:
            Sst_MoveSprite2(self);
            if (Claw_Counted(&own->count_a)) {
                self->routine_secondary += 2;
                own->count_a = 8;
            }
            break;
        case 4:
            if (Claw_Counted(&own->count_a)) {
                self->routine_secondary += 2;
                self->x_vel = -self->x_vel;
            }
            break;
        case 6:
            Sst_MoveSprite2(self);
            if (Claw_Counted(&own->count_b)) {
                if (own->index == 0) { // the first piece is the last back: the crab shuts its claw
                    shell->mapping_frame = 0;
                    ((Own_Shell *)shell->own.u8)->index = 0xFF00;
                }
                Sst_Delete(self);
                return;
            }
            break;
        }
        SstOnScreenTest(self);
        break;
    }

    case ClawRoutine_Fall: {
        Sst_MoveSprite(self);
        const int16_t word = (int16_t)((((uint16_t)own->count_a << 8) | own->count_b) - 1);
        own->count_a = (uint8_t)((uint16_t)word >> 8);
        own->count_b = (uint8_t)word;
        if (word < 0) {
            Sst_Delete(self);
            return;
        }
        SstOnScreenTest(self);
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
} Own_Asteron;

// Its spikes (Enemy_Weapon_Data): where from its centre, how fast, which frame and whether flipped
static const struct { int8_t dx, dy, xsp, ysp, frame, flip; } spikes[5] = {
    { 0x00, -8, 0x00, -4, 2, 0 },
    { 0x08, -4, 0x03, -1, 3, 1 },
    { 0x08, 0x08, 0x03, 0x03, 4, 1 },
    { -8, 0x08, -3, 0x03, 4, 0 },
    { -8, -4, -3, -1, 3, 0 },
};

static void Asteron_Explode(Sst *self) {
    self->code = sst_object_codes[0x27]; // (an explosion, and no animal)
    self->routine = 2;
    for (int i = 0; i < 5; i++) {
        Sst *spike = Sst_AllocateAfter(self);
        if (spike == NULL)
            return;
        spike->code = sst_object_codes[ObjId_EnemyWeapon];
        ((Own_Weapon *)spike->own.u8)->weapon = Weapon_AsteronSpike;
        spike->x_pos.f.u = (int16_t)(self->x_pos.f.u + spikes[i].dx);
        spike->y_pos.f.u = (int16_t)(self->y_pos.f.u + spikes[i].dy);
        spike->x_vel = (int16_t)(spikes[i].xsp << 8);
        spike->y_vel = (int16_t)(spikes[i].ysp << 8);
        spike->mapping_frame = (uint8_t)spikes[i].frame;
        spike->render_flags = (uint8_t)spikes[i].flip;
    }
}

void Obj_Asteron(Sst *self) {
    Own_Asteron *own = (Own_Asteron *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->mappings = Mappings_Asteron;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Asteron);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x0B;
        self->routine += 2;
        break;

    case 2: { // it sets off towards Sonic when he is $10 to $60 pixels away, on either axis
        static const int16_t speeds[2] = { -0x40, 0x40 };
        Where w = WhereIsSonic(self);
        int16_t dx = w.dx < 0 ? (int16_t)-w.dx : w.dx;
        int16_t dy = w.dy < 0 ? (int16_t)-w.dy : w.dy;
        if (dx >= 0x10 && dx < 0x60) {
            self->x_vel = speeds[w.d0 >> 1];
            self->routine = 4;
            own->timer = 0x40;
        }
        if (dy >= 0x10 && dy < 0x60) {
            self->y_vel = speeds[w.d1 >> 1];
            self->routine = 4;
            own->timer = 0x40;
        }
        SstOnScreenTest(self);
        break;
    }

    case 4:
        if ((int8_t)--own->timer < 0) {
            Asteron_Explode(self);
            SstOnScreenTest(self);
            break;
        }
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_Asteron);
        SstOnScreenTest(self);
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
} Own_Slicer;

static void Slicer_SetTimer(Sst *self, uint8_t v) {
    ((Own_Slicer *)self->own.u8)->timer = (uint16_t)((v << 8) | (((Own_Slicer *)self->own.u8)->timer & 0xFF)); // (the byte at $2A is the high one of the word)
}

static uint8_t Slicer_Timer(const Sst *self) {
    return (uint8_t)(((const Own_Slicer *)self->own.u8)->timer >> 8);
}

enum { SlicerRoutine_Init = 0, SlicerRoutine_Walk = 2, SlicerRoutine_Turn = 4, SlicerRoutine_Raise = 6, SlicerRoutine_Throw = 8 };

static void Slicer_MakePincers(Sst *self) {
    for (uint16_t i = 0; i < 4; i += 2) {
        Sst *pincer = Sst_AllocateAfter(self);
        if (pincer == NULL)
            return;
        pincer->code = sst_object_codes[ObjId_SlicerPincers];
        Own_Slicer *s = (Own_Slicer *)pincer->own.u8;
        pincer->subtype = 0x2A;
        pincer->mapping_frame = 5;
        pincer->priority = 4 * 0x80;
        s->timer = (uint16_t)(self - sst);
        s->index = i;
        pincer->x_pos.f.u = self->x_pos.f.u;
        pincer->y_pos.f.u = self->y_pos.f.u;
    }
}

void Obj_Slicer(Sst *self) {
    switch (self->routine) {
    case SlicerRoutine_Init:
        self->mappings = Mappings_Slicer;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Slicer); // ($243C: palette line 2)
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x06;
        self->routine += 2;
        self->x_vel = -0x40;
        self->y_radius = 0x10;
        self->x_radius = 0x10;
        break;

    case SlicerRoutine_Walk: {
        const Where w = WhereIsSonic(self);
        if (w.d0 == 0 && (uint16_t)(w.dx + 0x80) < 0x100) { // Sonic in front of it
            self->routine += 4;
            self->mapping_frame = 3;
            Slicer_SetTimer(self, 8);
            SstOnScreenTest(self);
            break;
        }
        Sst_MoveSprite2(self);
        const int16_t d1 = SstFloorDist(self, self->x_pos.f.u);
        if (d1 < -8 || d1 >= 0xC) { // a ledge or a wall
            self->routine += 2;
            Slicer_SetTimer(self, 0x3B);
            SstOnScreenTest(self);
            break;
        }
        self->y_pos.f.u += d1;
        Sst_Animate(self, Animation_Slicer);
        SstOnScreenTest(self);
        break;
    }

    case SlicerRoutine_Turn:
        if ((int8_t)(Slicer_Timer(self) - 1) < 0) {
            self->routine -= 2;
            self->x_vel = -self->x_vel;
            self->status ^= SST_STATUS_X_FLIP;
        } else {
            Slicer_SetTimer(self, (uint8_t)(Slicer_Timer(self) - 1));
        }
        SstOnScreenTest(self);
        break;

    case SlicerRoutine_Raise:
        if ((int8_t)(Slicer_Timer(self) - 1) < 0) {
            self->routine += 2;
            self->mapping_frame = 4;
            Slicer_MakePincers(self);
        } else {
            Slicer_SetTimer(self, (uint8_t)(Slicer_Timer(self) - 1));
        }
        SstOnScreenTest(self);
        break;

    case SlicerRoutine_Throw:
        SstOnScreenTest(self);
        break;
    }
}

enum { PincerRoutine_Init = 0, PincerRoutine_Fly = 2, PincerRoutine_Fall = 4 };

void Obj_SlicerPincers(Sst *self) {
    Own_Slicer *own = (Own_Slicer *)self->own.u8;

    switch (self->routine) {
    case PincerRoutine_Init: {
        static const int8_t offsets[2][2] = { { 6, 0 }, { -0x10, 0 } };
        self->mappings = Mappings_Slicer;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Slicer);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x9A;
        self->routine += 2;
        self->x_vel = -0x200;
        own->life = 0x200;
        self->x_pos.f.u += offsets[own->index >> 1][0];
        self->y_pos.f.u += offsets[own->index >> 1][1];
        break;
    }

    case PincerRoutine_Fly: {
        Sst *slicer = &sst[own->timer & 0xFF];
        if ((int16_t)--own->life >= 0 && slicer->code == sst_object_codes[ObjId_Slicer]) {
            // it homes in on Sonic, up to $200 each way
            const Where w = WhereIsSonic(self);
            static const int16_t accel[2] = { -0x10, 0x10 };
            self->x_vel = (int16_t)(self->x_vel + accel[w.d0 >> 1]);
            self->y_vel = (int16_t)(self->y_vel + accel[w.d1 >> 1]);
            if (self->x_vel > 0x200)
                self->x_vel = 0x200;
            if (self->x_vel < -0x200)
                self->x_vel = -0x200;
            if (self->y_vel > 0x200)
                self->y_vel = 0x200;
            if (self->y_vel < -0x200)
                self->y_vel = -0x200;
            Sst_MoveSprite2(self);
            Sst_Animate(self, Animation_SlicerPincers);
            SstOnScreenTest(self);
            break;
        }
        self->routine += 2; // (its Slicer is gone or it has flown long enough: it falls)
        own->life = 0x80;
    }
    // Fallthrough
    case PincerRoutine_Fall:
        if ((int16_t)--own->life < 0) {
            Sst_Delete(self);
            return;
        }
        Sst_MoveSprite(self);
        Sst_Animate(self, Animation_SlicerPincers);
        SstOnScreenTest(self);
        break;
    }
}
