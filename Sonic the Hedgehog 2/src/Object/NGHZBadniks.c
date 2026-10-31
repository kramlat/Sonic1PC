// Neo Green Hill's badniks for Sonic 2, as the alpha has them: see NGHZBadniks.h. Each sets itself up from the alpha's table of looks (Object_Settings), whose entries are written out below.
#include "Object/NGHZBadniks.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

#include "Macros.h"

#include "Resource/Animation/ChopChop.h"
#include "Resource/Animation/GrounderEmerge.h"
#include "Resource/Animation/GrounderWalk.h"
#include "Resource/Animation/Whisp.h"
#include "Resource/Mappings/ChopChop.h"
#include "Resource/Mappings/Grounder.h"
#include "Resource/Mappings/GrounderRock.h"
#include "Resource/Mappings/GrounderWall.h"
#include "Resource/Mappings/Whisp.h"

#define ObjId_Grounder 0x8E
#define ObjId_GrounderWall 0x8F
#define ObjId_GrounderRock 0x90

// The Animation scripts of the alpha: a word (the offset of the script, 2), the delay and then frames up to $FF (again) or $FC (moves the routine on, as the final's, and steps past the end of the script)
static void AlphaAnimate(Sst *self, const uint8_t *table) {
    const uint8_t *script = table + 2;
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, ((self->status & SST_STATUS_X_FLIP) != 0)); // (the flips of the status are those of the render)
    if (--self->anim_frame_timer >= 0)
        return;
    self->anim_frame_timer = (int8_t)script[0];
    uint8_t frame = script[1 + self->anim_frame];
    if (frame == 0xFF) {
        self->anim_frame = 0;
        frame = script[1];
    } else if (frame == 0xFC) {
        self->routine += 2;
        self->anim_frame_timer = 0;
        self->anim_frame++;
        return;
    }
    self->mapping_frame = frame & 0x7F;
    self->anim_frame++;
}

// Where Sonic is from the object (Object_Check_Player_Position): the side (0 left, 2 right) and above/below (0 above, 2 below), and how far (object minus Sonic)
typedef struct {
    int side, below;
    int16_t dx, dy;
} Where;

static Where WhereIsSonic(const Sst *self) {
    Where w;
    w.dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    w.dy = (int16_t)(self->y_pos.f.u - player->y_pos.f.u);
    w.side = (uint16_t)self->x_pos.f.u < (uint16_t)player->x_pos.f.u ? 2 : 0;
    w.below = (uint16_t)self->y_pos.f.u < (uint16_t)player->y_pos.f.u ? 2 : 0;
    return w;
}

static void SetFlip(Sst *self, bool flip) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, flip);
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, flip);
}

static void Settings(Sst *self, const void *mappings, uint16_t tile, uint8_t render, uint8_t priority, uint8_t width, uint8_t col) {
    self->mappings = mappings;
    self->art_tile = tile;
    self->render_flags |= render | SPRITE_CAM_FIELD;
    self->priority = (uint16_t)(priority * 0x80);
    self->width_pixels = width;
    self->collision_flags = col;
    self->routine += 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Whisp (8C): a fly that flutters at Sonic. After a short wait it goes up (it does that up to 7 times: the count is at $2B, the wait at $2A), follows Sonic for $60 frames with a speed that is kept to $200 either
// way and then waits again; the eighth time it flies off to the left
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint8_t timer; // 0x2A
    uint8_t count; // 0x2B
} Own_Whisp;

enum { WhispRoutine_Init = 0, WhispRoutine_Wait = 2, WhispRoutine_Chase = 4, WhispRoutine_Leave = 6 };

void Obj_Whisp(Sst *self) {
    Own_Whisp *own = (Own_Whisp *)self->own.u8;

    switch (self->routine) {
    case WhispRoutine_Init:
        Settings(self, Mappings_Whisp, TILE_MAP(0, 1, 0, 0, 0x500), 4, 4, 0x0C, 0x0B);
        own->timer = 0x10;
        own->count = 6;
        return;

    case WhispRoutine_Wait:
        if ((int8_t)--own->timer < 0) {
            if ((int8_t)--own->count >= 0) { // up it goes
                self->routine += 2;
                self->y_vel = (int16_t)0xFF00;
                own->timer = 0x60;
            } else { // the last: away to the left
                self->routine = WhispRoutine_Leave;
                SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
                self->x_vel = (int16_t)0xFE00;
                self->y_vel = 0;
                Sst_MoveSprite2(self);
                AlphaAnimate(self, Animation_Whisp);
                SstOnScreenTest(self);
                return;
            }
            // (it goes on with the chase's code in the same frame)
        } else {
            AlphaAnimate(self, Animation_Whisp);
            SstOnScreenTest(self);
            return;
        }
        // fallthrough
    case WhispRoutine_Chase: {
        if ((int8_t)--own->timer < 0) { // done: stops, waits $10 and starts again
            self->routine -= 2;
            own->timer = 0x10;
            self->x_vel = 0;
            self->y_vel = 0;
            AlphaAnimate(self, Animation_Whisp);
            SstOnScreenTest(self);
            return;
        }
        const Where w = WhereIsSonic(self);
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, w.side != 0);
        self->x_vel += w.side ? 0x10 : -0x10;
        self->y_vel += w.below ? 0x10 : -0x10;
        if (self->x_vel > 0x200)
            self->x_vel = 0x200;
        else if (self->x_vel < -0x200)
            self->x_vel = -0x200;
        if (self->y_vel > 0x200)
            self->y_vel = 0x200;
        else if (self->y_vel < -0x200)
            self->y_vel = -0x200;
        Sst_MoveSprite2(self);
        AlphaAnimate(self, Animation_Whisp);
        SstOnScreenTest(self);
        return;
    }

    case WhispRoutine_Leave:
        Sst_MoveSprite2(self);
        AlphaAnimate(self, Animation_Whisp);
        SstOnScreenTest(self);
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Grounder (8D in its wall of bricks, 8E in the open): it sits, and when Sonic comes within $60 it rises (the bricks, 8F, fly apart and the rocks, 90, spring from it), then walks to Sonic's side along the
// ground until the floor ends, waits a second and turns round
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint8_t timer;  // 0x2A
    uint8_t active; // 0x2B: set when it has been woken (the bricks and the rocks look at it)
} Own_Grounder;

enum { GrounderRoutine_Init = 0, GrounderRoutine_Sit = 2, GrounderRoutine_Emerge = 4, GrounderRoutine_Face = 6, GrounderRoutine_Walk = 8, GrounderRoutine_Wait = 0xA };

typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint8_t parent; // 0x2A: the slot of the Grounder (a word in the alpha: its address)
    uint8_t pad2;
    uint16_t index; // 0x2C
} Own_GrounderPart;

// Offset_0x02823C: the rocks (5)
static void Grounder_Rocks(Sst *self) {
    uint16_t index = 0;
    for (int i = 0; i < 5; i++) {
        Sst *rock = Sst_Allocate();
        if (rock == NULL)
            return;
        rock->code = sst_object_codes[ObjId_GrounderRock];
        Own_GrounderPart *part = (Own_GrounderPart *)rock->own.u8;
        part->subtype = 6;
        part->parent = (uint8_t)(self - sst);
        part->index = index;
        rock->x_pos.f.u = self->x_pos.f.u;
        rock->y_pos.f.u = self->y_pos.f.u;
        index += 2;
    }
}

// Offset_0x028274: the bricks (4) round it
static void Grounder_Bricks(Sst *self) {
    static const int8_t offsets[8] = { 0x00, (int8_t)0xEC, 0x10, 0x00, 0x00, 0x0C, (int8_t)0xF0, 0x00 };
    uint16_t index = 0;
    for (int i = 0; i < 4; i++) {
        Sst *brick = Sst_Allocate();
        if (brick == NULL)
            return;
        brick->code = sst_object_codes[ObjId_GrounderWall];
        Own_GrounderPart *part = (Own_GrounderPart *)brick->own.u8;
        part->subtype = 4;
        part->parent = (uint8_t)(self - sst);
        part->index = index;
        brick->x_pos.f.u = (int16_t)(self->x_pos.f.u + offsets[index]);
        brick->y_pos.f.u = (int16_t)(self->y_pos.f.u + offsets[index + 1]);
        index += 2;
    }
}

void Obj_Grounder(Sst *self) {
    Own_Grounder *own = (Own_Grounder *)self->own.u8;

    switch (self->routine) {
    case GrounderRoutine_Init:
        Settings(self, Mappings_Grounder, TILE_MAP(0, 1, 0, 0, 0x509), 4, 5, 0x10, 0x02);
        self->y_radius = 0x14;
        self->x_radius = 0x10;
        {
            const int16_t dist = SstFloorDist(self, self->x_pos.f.u);
            if (dist < 0) {
                self->y_pos.f.u += dist;
                self->y_vel = 0;
            }
        }
        if (self->code == sst_object_codes[0x8D]) { // (hidden: its bricks first)
            Grounder_Bricks(self);
        } else {
            self->routine = GrounderRoutine_Face;
        }
        return;

    case GrounderRoutine_Sit: {
        int16_t dx = WhereIsSonic(self).dx;
        if (dx < 0)
            dx = (int16_t)-dx;
        if ((uint16_t)dx <= 0x60) {
            self->routine += 2;
            own->active = 0xFF;
            Grounder_Rocks(self);
        }
        SstOnScreenTest(self);
        return;
    }

    case GrounderRoutine_Emerge:
        AlphaAnimate(self, Animation_GrounderEmerge);
        SstOnScreenTest(self);
        return;

    case GrounderRoutine_Face: {
        self->routine += 2;
        const Where w = WhereIsSonic(self);
        self->x_vel = w.side ? 0x100 : -0x100;
        SetFlip(self, w.side != 0);
        SstOnScreenTest(self);
        return;
    }

    case GrounderRoutine_Walk: {
        Sst_MoveSprite2(self);
        const int16_t dist = SstFloorDist(self, self->x_pos.f.u);
        if (dist >= -2 && dist < 0x0C) {
            self->y_pos.f.u += dist;
            AlphaAnimate(self, Animation_GrounderWalk);
        } else {
            self->routine += 2;
            own->timer = 0x3B;
        }
        SstOnScreenTest(self);
        return;
    }

    case GrounderRoutine_Wait:
        if ((int8_t)--own->timer < 0) {
            self->routine = GrounderRoutine_Walk;
            self->x_vel = (int16_t)-self->x_vel;
            SetFlip(self, !((self->status & SST_STATUS_X_FLIP) != 0));
        }
        SstOnScreenTest(self);
        return;
    }
}

// The bricks (8F): they wait for the Grounder to wake and then break away, up and out, by one of four speeds
void Obj_GrounderWall(Sst *self) {
    Own_GrounderPart *own = (Own_GrounderPart *)self->own.u8;
    static const int8_t speeds[8] = { 0x01, (int8_t)0xFE, 0x01, (int8_t)0xFF, (int8_t)0xFF, (int8_t)0xFE, (int8_t)0xFF, (int8_t)0xFF };

    switch (self->routine) {
    case 0:
        Settings(self, Mappings_GrounderWall, TILE_MAP(0, 1, 0, 0, 0x434), 0x84, 4, 0x10, 0);
        return;

    case 2:
        if (((Own_Grounder *)sst[own->parent].own.u8)->active != 0) {
            self->routine += 2;
            self->x_vel = (int16_t)(speeds[own->index] << 8);
            self->y_vel = (int16_t)(speeds[own->index + 1] << 8);
        }
        SstOnScreenTest(self);
        return;

    default: // 4: falls until it is out of sight
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        Sst_MoveSprite(self);
        SstOnScreenTest(self);
        return;
    }
}

// The rocks (90): they spring out by one of five speeds, and fall until they are out of sight
void Obj_GrounderRock(Sst *self) {
    Own_GrounderPart *own = (Own_GrounderPart *)self->own.u8;
    static const uint8_t frames[6] = { 0, 2, 0, 1, 0, 0 };
    static const int8_t speeds[10] = { (int8_t)0xFF, (int8_t)0xFC, 0x04, (int8_t)0xFD, 0x02, 0x00, (int8_t)0xFD, (int8_t)0xFF, (int8_t)0xFD, (int8_t)0xFD };

    if (self->routine == 0) {
        Settings(self, Mappings_GrounderRock, TILE_MAP(0, 2, 0, 0, 0x509), 0x84, 4, 0x08, 0);
        self->x_vel = (int16_t)(speeds[own->index] << 8);
        self->y_vel = (int16_t)(speeds[own->index + 1] << 8);
        self->mapping_frame = frames[own->index >> 1];
        SstOnScreenTest(self);
        return;
    }
    if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return;
    }
    Sst_MoveSprite(self);
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Chop Chop (91): a fish that swims to and fro ($200 frames each way), spits a bubble (object 0A, subtype 6) every $50, and when Sonic is ahead of it and near (within $20 to $A0 across, $20 up or down) it stops,
// waits $10 and dashes at him
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad;
    int16_t timer;   // 0x2A
    uint8_t bubbles; // 0x2C: the high byte of the word that is set to $50 (the code counts the byte at $2C, which is 0: so it counts a whole 256 frames)
    uint8_t pad2;
} Own_ChopChop;

enum { ChopRoutine_Init = 0, ChopRoutine_Swim = 2, ChopRoutine_Stop = 4, ChopRoutine_Dash = 6 };

static void ChopChop_Bubble(Sst *self) {
    Own_ChopChop *own = (Own_ChopChop *)self->own.u8;
    own->bubbles = 0;
    Sst *bubble = Sst_Allocate();
    if (bubble == NULL)
        return;
    bubble->code = sst_object_codes[0x0A];
    bubble->subtype = 6;
    bubble->x_pos.f.u = (int16_t)(self->x_pos.f.u + (((self->render_flags & SST_RENDER_X_FLIP) != 0) ? -0x14 : 0x14));
    bubble->y_pos.f.u = (int16_t)(self->y_pos.f.u + 6);
}

// Offset_0x0284C2: is Sonic ahead of it, within reach? (D2 across, D3 up and down)
static bool ChopChop_SeesSonic(const Sst *self, const Where *w) {
    uint16_t dy = (uint16_t)(w->dy + 0x20);
    if (dy >= 0x40)
        return false;
    int16_t dx = w->dx;
    if (dx >= 0) { // Sonic is to the left
        if (self->x_vel >= 0)
            return false;
    } else { // to the right
        if (self->x_vel < 0)
            return false;
        dx = (int16_t)-dx;
    }
    return (uint16_t)dx >= 0x20 && (uint16_t)dx < 0xA0;
}

void Obj_ChopChop(Sst *self) {
    Own_ChopChop *own = (Own_ChopChop *)self->own.u8;

    switch (self->routine) {
    case ChopRoutine_Init:
        Settings(self, Mappings_ChopChop, TILE_MAP(0, 1, 0, 0, 0x53B), 4, 4, 0x10, 0x02);
        own->timer = 0x200;
        own->bubbles = 0;
        self->x_vel = ((self->status & SST_STATUS_X_FLIP) != 0) ? 0x40 : -0x40;
        return;

    case ChopRoutine_Swim: {
        if (--own->bubbles == 0)
            ChopChop_Bubble(self);
        if (--own->timer < 0) { // (a word: it turns round every $200 frames)
            own->timer = 0x200;
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0));
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, !((self->render_flags & SST_RENDER_X_FLIP) != 0));
            self->x_vel = (int16_t)-self->x_vel;
        }
        Sst_MoveSprite2(self);
        const Where w = WhereIsSonic(self);
        if (ChopChop_SeesSonic(self, &w)) {
            self->routine += 2;
            own->timer = 0x10;
            self->x_vel = 0;
        }
        SstOnScreenTest(self);
        return;
    }

    case ChopRoutine_Stop:
        if ((int8_t)--own->timer < 0) { // dashes (the speed from the side, and the byte at $13 from above or below)
            self->routine += 2;
            const Where w = WhereIsSonic(self);
            self->x_vel = w.side ? 0x200 : -0x200;
            if ((uint16_t)(w.dy + 0x10) >= 0x20)
                self->y_vel = (int16_t)((self->y_vel & 0xFF00) | 0x80); // (its low byte, from a table of $80 and $80: it always sinks)
        }
        AlphaAnimate(self, Animation_ChopChop);
        SstOnScreenTest(self);
        return;

    case ChopRoutine_Dash:
        Sst_MoveSprite2(self);
        AlphaAnimate(self, Animation_ChopChop);
        SstOnScreenTest(self);
        return;
    }
}
