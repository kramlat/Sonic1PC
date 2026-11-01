// Chemical Plant's badniks for Sonic 2, as the alpha has them: see CPZBadniks.h
#include "Object/CPZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/Coconuts.h"
#include "Constants.h"

#include "Level.h"

#include "Macros.h"

#include "Object/CharControl.h"
#include "Game.h"
#include "Resource/Animation/Grabber.h"
#include "Resource/Animation/SpinyHorizontal.h"
#include "Resource/Animation/SpinyShot.h"
#include "Resource/Animation/SpinyVertical.h"
#include "Resource/Mappings/Grabber.h"
#include "Resource/Mappings/GrabberStrand.h"
#include "Resource/Mappings/Spiny.h"

const uint8_t *Spiny_Mappings(void) {
    return Mappings_Spiny;
}

const uint8_t *Spiny_ShotAnimation(void) {
    return Animation_SpinyShot;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Spiny (A5 horizontal, A6 vertical): Object_Settings' entry $32 (mappings, art at $52D, render 4, priority 4, width 8, collision $0B).
// It keeps two counters in the bytes of the word at $2A: the first ($2A) counts every frame it walks and turns it round when it runs out (a whole 256), the second ($2B) is how long it waits (it starts at $80:
// it does not look for Sonic for that long) and counts down while it walks. When Sonic is within $60 either way (a third of a second after it last looked) it stops, and half a second on it throws its shot.
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype; // 0x28
    uint8_t pad0;    // 0x29
    uint8_t walk;    // 0x2A
    uint8_t wait;    // 0x2B (Obj_Player_Status)
} Own_Spiny;

enum { SpinyRoutine_Init = 0, SpinyRoutine_Walk = 2, SpinyRoutine_Throw = 4 };

static void Spiny_Shoot(Sst *self, bool horizontal) {
    Sst *shot = Sst_Allocate();
    if (shot == NULL)
        return;
    shot->code = sst_object_codes[ObjId_EnemyWeapon];
    ((Own_Weapon *)shot->own.u8)->weapon = Weapon_SpinyShot;
    shot->mapping_frame = 6;
    shot->x_pos.f.u = self->x_pos.f.u;
    shot->y_pos.f.u = self->y_pos.f.u;
    if (horizontal) { // thrown up and towards Sonic
        shot->y_vel = (int16_t)0xFD00;
        shot->x_vel = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? 0x100 : -0x100;
    } else { // thrown out of the wall, to the side that it faces
        shot->x_vel = ((self->render_flags & SST_RENDER_X_FLIP) != 0) ? -0x300 : 0x300;
    }
}

static void Spiny_Run(Sst *self, bool horizontal) {
    Own_Spiny *own = (Own_Spiny *)self->own.u8;

    switch (self->routine) {
    case SpinyRoutine_Init:
        self->mappings = Mappings_Spiny;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x52D);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 8;
        self->collision_flags = 0x0B;
        self->routine += 2;
        if (horizontal)
            self->x_vel = (int16_t)0xFFC0;
        else
            self->y_vel = (int16_t)0xFFC0;
        own->walk = 0;
        own->wait = 0x80;
        break;

    case SpinyRoutine_Walk: {
        bool walking = true;
        if (own->wait != 0) {
            own->wait--;
        } else {
            const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
            if ((uint16_t)(dx + 0x60) < 0xC0) { // Sonic is near: it stops, and its frame is the one that throws
                self->routine += 2;
                own->wait = 0x28;
                self->mapping_frame = horizontal ? 2 : 5;
                walking = false;
            }
        }
        if (walking) {
            if (--own->walk == 0) {
                own->walk = 0;
                own->wait = 0x80;
                if (horizontal)
                    self->x_vel = (int16_t)-self->x_vel;
                else
                    self->y_vel = (int16_t)-self->y_vel;
            }
            Sst_MoveSprite2(self);
            Sst_Animate(self, horizontal ? Animation_SpinyHorizontal : Animation_SpinyVertical);
        }
        SstOnScreenTest(self);
        break;
    }

    case SpinyRoutine_Throw:
        if ((int8_t)--own->wait < 0) {
            self->routine -= 2;
            own->wait = 0x40;
        } else if (own->wait == 0x14) {
            Spiny_Shoot(self, horizontal);
        }
        SstOnScreenTest(self);
        break;
    }
}

void Obj_Spiny(Sst *self) {
    Spiny_Run(self, true);
}

void Obj_SpinyVertical(Sst *self) {
    Spiny_Run(self, false);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Grabber (A7; its parts A8 the claw, A9 the head, AA the thread, AB unused): the pre-beta's spider, which no layout of it places. It hangs from a thread and goes to and fro for 256 frames each way
// ($40 a frame in 1/256 of a pixel: a quarter of a pixel), and when a character is under it, within $40 either side and not more than $7F below, it stops, waits $10 frames and goes down $20 frames and up again.
// The claw (collision $D7, the special kind: it counts the touch, a bit for each character as the bumper does) takes whoever it touches on the way down; the Grabber then goes up with him (his frame is
// the claw's place) and starts to flash, quicker and quicker (every $10 frames, then $F, $E and so on down to 2: about 135 frames), and explodes, letting go. The character gets free by pressing left and
// right against each other: in the 32 frames that start with the first press, four changes of side let him go (the Grabber goes on flashing, and then explodes, with nobody in it).
// Each part is the Grabber's: it is gone when the body is not a Grabber any more (a destroyed one is an explosion).
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0;        // 0x29
    int16_t patrol;      // 0x2A: frames to go on this way
    uint8_t blink;       // (Obj_Timer: frames to the next flash) the flashing starts when it takes somebody
    uint8_t blinks;      // (Obj_Player_Status: how long that is, counted down)
    uint8_t timer;       // 0x2C (var_00)
    int16_t saved_speed; // 0x2E (var_02): the way it goes when it is not waiting
    uint8_t grabbed;     // 0x30 (var_04): the claw has somebody
    uint8_t mashing;     // 0x31 (var_05): a window of 32 frames has started
    uint8_t held;        // 0x32 (var_06): the slot of the character it holds, and 1
    uint8_t last_side;   // 0x36 (var_0A): the last of left and right pressed
    uint8_t window;      // 0x37 (var_0B): frames to the end of the window
    uint8_t changes;     // 0x38 (var_0C): changes of side in it
    uint8_t strand;      // 0x3A (var_0E): the slot of the thread
    uint8_t claw;        // 0x3C (var_10)
    uint8_t head;        // 0x3E (var_12)
} Own_Grabber;

void Obj_Explosion(Sst *self);

enum { GrabberRoutine_Init = 0, GrabberRoutine_Run = 2 };
enum { GrabberState_Patrol = 0, GrabberState_Wait = 2, GrabberState_Down = 4, GrabberState_Up = 6, GrabberState_Top = 8, GrabberState_Done = 10 };

#define GRABBER_ART TILE_MAP(1, 1, 0, 0, 0x500)

static Sst *Grabber_Part(Sst *body, SstCode code, uint8_t *slot) {
    Sst *part = Sst_AllocateAfter(body);
    if (part == NULL)
        return NULL;
    *slot = (uint8_t)(part - sst);
    part->code = code;
    part->parent = (uint16_t)(body - sst + 1);
    part->x_pos.f.u = body->x_pos.f.u;
    part->y_pos.f.u = body->y_pos.f.u;
    return part;
}

// The body that a part belongs to, or NULL when it is not a Grabber any more
static Sst *Grabber_Body(const Sst *part) {
    if (part->parent == 0)
        return NULL;
    Sst *body = &sst[part->parent - 1];
    return body->code == Obj_Grabber ? body : NULL;
}

// A character is let go: he is in the air and under his own control
static void Grabber_Release(Sst *body, bool walk) {
    Own_Grabber *own = (Own_Grabber *)body->own.u8;
    if (own->held == 0)
        return;
    Sst *chr = &sst[own->held - 1];
    OBJ_CONTROL(chr) = 0;
    chr->status |= SST_PSTATUS_IN_AIR;
    if (walk)
        chr->anim = 0;
    own->held = 0;
}

// The flashing, quicker and quicker, and the explosion at the end of it (Obj_0xA7_Sub_01)
static void Grabber_Flash(Sst *self, Own_Grabber *own) {
    if (--own->blink != 0)
        return;
    own->blink = own->blinks;
    if (--own->blinks != 0) {
        self->art_tile ^= 0x2000;
        return;
    }
    self->code = Obj_Explosion;
    self->routine = 2;
    self->art_tile |= 0x2000;
    if (own->held != 0) {
        Grabber_Release(self, false);
        self->collision_flags = 0x0B;
    }
}

// Four changes of side in 32 frames (Obj_0xA7_Sub)
static void Grabber_Struggle(Sst *self, Own_Grabber *own) {
    if (own->held == 0)
        return;
    const uint8_t pad = (uint8_t)((own->held == 1 ? jpad1_hold2 : jpad2_hold) & (JPAD_LEFT | JPAD_RIGHT));
    if (!own->mashing) {
        if (pad != 0) {
            own->mashing = 0xFF;
            own->last_side = pad;
        }
        return;
    }
    if (--own->window != 0) {
        if (pad != 0 && pad != own->last_side) {
            own->last_side = pad;
            own->changes++;
        }
        return;
    }
    if (own->changes >= 4) { // he is free, and the Grabber has nobody
        self->routine_secondary = GrabberState_Done;
        self->y_vel = 0;
        self->collision_flags = 0;
        Grabber_Release(self, true);
    }
    own->window = 0x20;
    own->mashing = 0;
    own->changes = 0;
}

static void Grabber_Seize(Sst *self, Own_Grabber *own) {
    self->routine_secondary += 2;
    Sst *chr = &sst[own->held - 1];
    OBJ_CONTROL(chr) = 0x81;
    chr->x_vel = 0;
    chr->y_vel = 0;
    chr->anim = 0x0E;
    self->mapping_frame = 1;
    if (self->y_vel >= 0) { // it was on the way down: the same time up
        self->y_vel = (int16_t)-self->y_vel;
        own->timer = (uint8_t)(1 - (uint8_t)(own->timer - 0x40));
    }
    own->blink = 1;
    own->blinks = 0x10;
    own->window = 0x20;
}

static void Grabber_Parts(Sst *self, Own_Grabber *own) {
    Sst *claw = &sst[own->claw], *head = &sst[own->head], *strand = &sst[own->strand];
    if (claw->code == Obj_GrabberClaw) {
        claw->x_pos.f.u = self->x_pos.f.u;
        claw->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x10);
    }
    if (head->code == Obj_GrabberHead)
        head->x_pos.f.u = self->x_pos.f.u;
    if (strand->code == Obj_GrabberStrand)
        strand->x_pos.f.u = self->x_pos.f.u;
}

void Obj_Grabber(Sst *self) {
    Own_Grabber *own = (Own_Grabber *)self->own.u8;

    if (self->routine == GrabberRoutine_Init) {
        self->mappings = Mappings_Grabber;
        self->art_tile = GRABBER_ART;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x0B;
        self->routine += 2;
        self->x_vel = (int16_t)(((self->render_flags & SST_RENDER_X_FLIP) != 0) ? 0x40 : -0x40);
        own->patrol = 0xFF;
        own->claw = own->head = own->strand = 0;
        Grabber_Part(self, Obj_GrabberHead, &own->head);
        Grabber_Part(self, Obj_GrabberClaw, &own->claw);
        Grabber_Part(self, Obj_GrabberStrand, &own->strand);
        return;
    }

    switch (self->routine_secondary) {
    case GrabberState_Patrol: {
        const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
        const int16_t dy = (int16_t)(self->y_pos.f.u - player->y_pos.f.u);
        if ((uint16_t)(dx + 0x40) < 0x80 && (uint16_t)dy > 0xFF80) { // a character is under it
            self->routine_secondary += 2;
            own->saved_speed = self->x_vel;
            self->x_vel = 0;
            own->timer = 0x10;
        } else if (--own->patrol < 0) {
            own->patrol = 0xFF;
            self->x_vel = (int16_t)-self->x_vel;
            self->render_flags ^= SST_RENDER_X_FLIP;
            self->status ^= SST_STATUS_X_FLIP;
        }
        break;
    }
    case GrabberState_Wait:
        if ((int8_t)--own->timer < 0) {
            self->routine_secondary += 2;
            self->y_vel = 0x200;
            own->timer = 0x40;
        }
        break;
    case GrabberState_Down:
        if (own->grabbed) {
            Grabber_Seize(self, own);
        } else {
            if (--own->timer == 0) {
                self->routine_secondary = GrabberState_Patrol;
                self->y_vel = 0;
                self->x_vel = own->saved_speed;
                self->mapping_frame = 0;
            } else {
                if (own->timer == 0x20)
                    self->y_vel = (int16_t)-self->y_vel;
                Sst_Animate(self, Animation_Grabber);
            }
        }
        break;
    case GrabberState_Up:
        Grabber_Flash(self, own);
        Grabber_Struggle(self, own);
        if (--own->timer == 0) {
            self->routine_secondary += 2;
            self->y_vel = 0;
        }
        break;
    case GrabberState_Top:
        Grabber_Flash(self, own);
        Grabber_Struggle(self, own);
        break;
    case GrabberState_Done:
        Grabber_Flash(self, own);
        break;
    }

    Sst_MoveSprite2(self);
    Grabber_Parts(self, own);

    if (SstOutOfRange(self->x_pos.f.u)) { // (the parts go with it, and so does whoever it holds)
        Grabber_Release(self, false);
        if (own->claw != 0)
            Sst_Delete(&sst[own->claw]);
        if (own->head != 0)
            Sst_Delete(&sst[own->head]);
        if (own->strand != 0)
            Sst_Delete(&sst[own->strand]);
    }
    SstOnScreenTest(self);
}

// The claw: it has the Grabber's flips and its frame, plus 3 (the claw open, or closed)
void Obj_GrabberClaw(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_Grabber;
        self->art_tile = GRABBER_ART;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0xD7;
        self->routine += 2;
        self->mapping_frame = 3;
        return;
    }
    Sst *body = Grabber_Body(self);
    Own_Grabber *own = body != NULL ? (Own_Grabber *)body->own.u8 : NULL;
    switch (self->routine) {
    case 2:
        if (body == NULL) {
            Sst_Delete(self);
            return;
        }
        self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (body->render_flags & 3));
        self->status = (uint8_t)((self->status & 0xFC) | (body->render_flags & 3));
        self->mapping_frame = (uint8_t)(body->mapping_frame + 3);
        if (self->collision_property != 0 && body->routine_secondary == GrabberState_Down) { // it has him: the character's slot is by the bit that touched
            self->routine += 2;
            self->collision_flags = 0;
            own->grabbed = 0xFF;
            own->held = (uint8_t)((self->collision_property & 3) == 2 ? 2 : 1);
        }
        Sst_Draw(self);
        break;
    case 4: // it carries him
        if (body == NULL) { // (the body is gone: he is let go with the claw)
            Sst *chr = NULL;
            if (self->parent != 0 && self->own.u8[0] != 0)
                chr = &sst[self->own.u8[0] - 1];
            if (chr != NULL) {
                OBJ_CONTROL(chr) = 0;
                chr->status |= SST_PSTATUS_IN_AIR;
            }
            Sst_Delete(self);
            return;
        }
        self->own.u8[0] = own->held;
        if (own->held == 0) {
            self->routine += 2;
        } else {
            Sst *chr = &sst[own->held - 1];
            chr->x_pos.f.u = self->x_pos.f.u;
            chr->y_pos.f.u = self->y_pos.f.u;
        }
        Sst_Draw(self);
        break;
    case 6:
        if (body == NULL) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

// The head, at the top of the thread
void Obj_GrabberHead(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_Grabber;
        self->art_tile = GRABBER_ART;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 4;
        self->collision_flags = 0;
        self->routine += 2;
        self->mapping_frame = 2;
        self->y_pos.f.u -= 0x0C;
        return;
    }
    if (Grabber_Body(self) == NULL) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// The thread: a link of it for each $10 pixels between the head and the body
void Obj_GrabberStrand(Sst *self) {
    if (self->routine == 0) {
        self->mappings = Mappings_GrabberStrand;
        self->art_tile = GRABBER_ART;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 4;
        self->collision_flags = 0;
        self->routine += 2;
        self->y_pos.f.u -= 8;
        return;
    }
    Sst *body = Grabber_Body(self);
    if (body == NULL) {
        Sst_Delete(self);
        return;
    }
    const int16_t length = (int16_t)(body->y_pos.f.u - self->y_pos.f.u);
    if (length >= 0)
        self->mapping_frame = (uint8_t)((uint16_t)length >> 4);
    Sst_Draw(self);
}

// (the pre-beta's object AB sets itself up with the table's last look and stays: nothing places it)
void Obj_GrabberUnused(Sst *self) {
    SstOnScreenTest(self);
}
