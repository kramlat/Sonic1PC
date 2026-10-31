// Dust Hill's objects for Sonic 2 (the Simon Wai prototype's 2A, 47, 6A, 75, 76 and 77; the collapsing platform, 1F, is in CollapsingPlatform.c): the stomper that rises slowly and drops (2A), the switch that is stood
// on (47, shared with Oil Ocean and Metropolis), the boxes that move round a path (6A), the spiked ball on a chain that circles (75), the platform with spikes on its sides that slides away from whoever
// comes near (76) and the drawbridge that a switch lowers (77). All are solid for Sonic and Tails.
#include "Object/DHZObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Tails.h"
#include "SolidSst.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/DHZGate.h"
#include "Resource/Mappings/DHZStomper.h"
#include "Resource/Mappings/PlatformSpikes.h"
#include "Resource/Mappings/RotatingBoxes.h"
#include "Resource/Mappings/SpikeballChain.h"
#include "Resource/Mappings/Switch.h"
#include "Resource/Mappings/SSZSwitch.h"

extern const uint8_t Mappings_MTZPlatformA[]; // (defined with MTZObjects.c, whose platforms they are)

// (the spikes' hurt, Touch_ChkHurt2: Spikes.c)
void Spikes_HurtCharacter(Sst *self, Sst *chr, int who);

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// MarkObjGone2: gone, and forgotten (so that it can come back), when x is out of sight
static void GoneIfOffscreen(Sst *self, int16_t x) {
    if (SstOutOfRange(x)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2A: the stomper. It rises $60 pixels a frame at a time and then drops 8 a frame, over and over
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t pad0[8];   // 0x28-0x2F
    int16_t lift;      // 0x30: how far it is raised
    int16_t base_y;    // 0x32
} Own_Stomper;

void Obj_DHZStomper(Sst *self) {
    Own_Stomper *own = (Own_Stomper *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_DHZStomper;
        self->art_tile = TILE_MAP(0, 2, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        own->base_y = self->y_pos.f.u;
        self->y_radius = 0x50;
        self->height_pixels = self->y_radius;
    }

    if (self->routine_secondary == 0) { // rising
        own->lift++;
        if (own->lift == 0x60)
            self->routine_secondary = 2;
    } else { // dropping
        own->lift -= 8;
        if (own->lift <= 0) {
            own->lift = 0;
            self->routine_secondary = 0;
        }
    }
    self->y_pos.f.u = (int16_t)(own->base_y - own->lift);

    FOR_EACH_CHARACTER(chr, who)
        SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 0x40, 0x41, self->x_pos.f.u, NULL);
    if (SstOutOfRange(self->x_pos.f.u)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 47: the switch. A character standing on it sets a bit in the level's table of switches (the subtype's low nibble says which, bit 6 of it the bit: 0 or 7)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_Switch(Sst *self) {
    const uint8_t subtype = self->subtype;

    if (self->routine == 0) {
        self->routine += 2;
        const bool sand = LEVEL_ZONE(level_id) == ZoneId_SSZ; // (Sand Shower's: the user's mappings of the level's own tiles, on palette row 2)
        self->mappings = sand ? Mappings_SSZSwitch : Mappings_Switch;
        self->art_tile = sand ? TILE_MAP(0, 2, 0, 0, 0) : TILE_MAP(0, 0, 0, 0, 0x424);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        self->y_pos.f.u += 4;
        if (sand) { // (two sprites: the button, and the base that it sits in, which is drawn in front: the first child is the VDP-front one)
            self->render_flags |= SST_RENDER_MULTI_DRAW;
            self->child_count = 2;
            for (int i = 0; i < 2; i++) {
                self->children[i].x = self->x_pos.f.u;
                self->children[i].y = self->y_pos.f.u;
            }
            self->children[0].frame = 1;
            self->children[1].frame = 0;
        }
    }

    const bool sand_shower = LEVEL_ZONE(level_id) == ZoneId_SSZ;
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who)
            SolidSst_Character(self, chr, who, 0x1B, 4, 5, self->x_pos.f.u, NULL);
        self->mapping_frame = 0;
        if (sand_shower)
            self->children[1].frame = 0;
        uint8_t *flag = &f_switch[subtype & 0xF];
        const uint8_t bit = (subtype & 0x40) ? 0x80 : 0x01;
        if (!(self->status & 0x18)) {
            *flag &= (uint8_t)~bit;
        } else {
            if (*flag == 0)
                PlaySound(sfx_Switch);
            *flag |= bit;
            if (sand_shower)
                self->children[1].frame = 2; // (Sand Shower's pressed look is its third frame: the top row lowered; the base, frame 1, is drawn too)
            else
                self->mapping_frame = 1;
        }
    }
    if (SstOutOfRange(self->x_pos.f.u)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6A: the boxes. They move along a path of steps (a speed in each direction and for how long), solid, forever; the subtype is where in the path they start, and $18 makes
// two more (at $40 right and left of it and $40 below, starting a step ahead of it and a step behind) with it
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t base_y;    // 0x30
    int16_t base_x;    // 0x32: where it is, for when it is forgotten
    int16_t timer;     // 0x34: frames left of the step
    uint16_t moving;   // 0x36: Metropolis: it is on its way
    uint8_t step;      // 0x38: the index of the next step in the path
    uint8_t pad2[3];   // 0x39-0x3B
    uint8_t stood;     // 0x3C: Metropolis: who was standing on it (its status as of last frame)
} Own_Boxes;

// The path (loc_1BC74; loc_1BC92 for a box that is flipped, which goes the other way): x speed, y speed and frames of each step. Its fifth step is not in the cycle: it is only where a box that starts
// with subtype $18 begins
static const int16_t boxes_path[5][3] = {
    { 0x0000, 0x0100, 0x0040 }, { -0x0100, 0x0000, 0x0080 }, { 0x0000, -0x0100, 0x0040 }, { 0x0100, 0x0000, 0x0080 }, { 0x0100, 0x0000, 0x0040 },
};
static const int16_t boxes_path_flipped[5][3] = {
    { 0x0000, 0x0100, 0x0040 }, { 0x0100, 0x0000, 0x0080 }, { 0x0000, -0x0100, 0x0040 }, { -0x0100, 0x0000, 0x0080 }, { -0x0100, 0x0000, 0x0040 },
};

// Metropolis's (loc_1BC5C): down, right and up, down, left and up
static const int16_t boxes_path_mtz[4][3] = {
    { 0x0000, 0x0400, 0x0010 }, { 0x0400, -0x0200, 0x0020 }, { 0x0000, 0x0400, 0x0010 }, { -0x0400, -0x0200, 0x0020 },
};

// loc_1BC22: the next step
static void Boxes_NextStep(Sst *self, Own_Boxes *own) {
    int index = own->step / 6;
    if (index > 4)
        index = 0; // (the prototype reads past its path here; no layout starts a box so)
    const int16_t *step = LEVEL_ZONE(level_id) != ZoneId_MCZ ? boxes_path_mtz[index & 3] : ((self->status & SST_STATUS_X_FLIP) != 0) ? boxes_path_flipped[index] : boxes_path[index];
    self->x_vel = step[0];
    self->y_vel = step[1];
    own->timer = step[2];
    own->step += 6;
    if (own->step >= 0x18)
        own->step = 0;
}

void Obj_RotatingBoxes(Sst *self) {
    Own_Boxes *own = (Own_Boxes *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        const bool mtz = LEVEL_ZONE(level_id) != ZoneId_MCZ;
        if (!mtz)
            self->routine += 2; // (Metropolis's boxes wait for a character to step off them: routine 2)
        self->mappings = mtz ? Mappings_MTZPlatformA : Mappings_RotatingBoxes;
        self->art_tile = TILE_MAP(0, 3, 0, 0, mtz ? 0 : 0x3D4);
        self->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
        self->priority = 4 * 0x80;
        self->width_pixels = 0x20;
        self->y_radius = mtz ? 0x0C : 0x20;
        self->mapping_frame = mtz ? 1 : 0;
        if (mtz) {
            own->base_x = self->x_pos.f.u;
            own->base_y = self->y_pos.f.u;
        } else if (self->subtype == 0x18) {
            const bool flipped = ((self->status & SST_STATUS_X_FLIP) != 0);
            for (int i = 0; i < 2; i++) {
                Sst *box = Sst_AllocateAfter(self);
                if (box == NULL)
                    break;
                box->code = self->code;
                box->x_pos.f.u = self->x_pos.f.u + (i == 0 ? 0x40 : -0x40);
                box->y_pos.f.u = self->y_pos.f.u + 0x40;
                Own_Boxes *other = (Own_Boxes *)box->own.u8;
                other->base_x = self->x_pos.f.u;
                other->base_y = self->y_pos.f.u;
                box->status = self->status;
                box->subtype = (uint8_t)((i == 0) != flipped ? 6 : 0xC);
            }
            own->base_x = self->x_pos.f.u;
            own->base_y = self->y_pos.f.u;
        }
        own->step = self->subtype;
        Boxes_NextStep(self, own);
        own->moving = 0;
        return; // (the first frame only sets it up: loc_1BC22 ends the routine)
    }

    if (self->routine == 2) { // Metropolis: it sets off when whoever stood on it steps off, and stops after a step
        const int16_t old_x = self->x_pos.f.u;
        if (own->moving) {
            Sst_MoveSprite2(self);
            if (--own->timer == 0) {
                Boxes_NextStep(self, own);
                own->moving = 0;
            }
        } else {
            const uint8_t status = self->status, before = own->stood;
            if (status & 8)
                own->stood = status;
            else if (before & 8) {
                own->moving = 1;
                own->stood = 0;
            }
            if (status & 8 || !(before & 8)) {
                if (status & 0x10)
                    own->stood = status;
                else if (before & 0x10) {
                    own->moving = 1;
                    own->stood = 0;
                }
            }
        }
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            FOR_EACH_CHARACTER(chr, who)
                SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
        }
        GoneIfOffscreen(self, own->base_x);
        return;
    }

    const int16_t old_x = self->x_pos.f.u;
    Sst_MoveSprite2(self);
    if (--own->timer == 0)
        Boxes_NextStep(self, own);

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who)
            SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
    }
    GoneIfOffscreen(self, own->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 75: the spiked ball on a chain. It circles its pivot at the speed in the high nibble of the subtype (times 8, either way) through as many links as the low nibble says
// (frame 1 each, drawn as child sprites of an object of its own); a subtype whose low nibble is $F is a solid block (frame 2) instead. The object's flipping gives the angle it starts at
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t pivot_x;   // 0x30
    int16_t pivot_y;   // 0x32
    int16_t speed;     // 0x34
    uint16_t angle;    // 0x36: the high byte is the angle
    uint8_t pad1[4];   // 0x38-0x3B
    uint8_t chain;     // 0x3C: the slot of the object that draws the chain
} Own_SpikeballChain;

enum { ChainRoutine_Init = 0, ChainRoutine_Circle = 2, ChainRoutine_Block = 4 };

static Sst *SpikeballChain_Links(Sst *self, Own_SpikeballChain *own) {
    Sst *chain = (own->chain != 0) ? &sst[own->chain] : NULL;
    if (chain == NULL || chain == self || chain->code != self->code || !(chain->render_flags & SST_RENDER_MULTI_DRAW))
        return NULL;
    return chain;
}

void Obj_SpikeballChain(Sst *self) {
    Own_SpikeballChain *own = (Own_SpikeballChain *)self->own.u8;

    if ((self->render_flags & SST_RENDER_MULTI_DRAW)) { // the object that draws the chain: its links are set by the ball's
        Sst_Draw(self);
        return;
    }

    switch (self->routine) {
    case ChainRoutine_Init: {
        self->routine += 2;
        self->mappings = Mappings_SpikeballChain;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->width_pixels = 0x10;
        own->pivot_x = self->x_pos.f.u;
        own->pivot_y = self->y_pos.f.u;
        const int links = self->subtype & 0xF;
        own->speed = (int16_t)(((int16_t)(int8_t)(self->subtype & 0xF0)) * 8);
        own->angle = (uint16_t)(((self->status << 6) | (self->status >> 2)) & 0xC0) << 8; // (ror.b #2, and $C0: the flip bits)
        own->chain = 0;
        if (links == 0xF) { // a solid block
            self->routine += 2;
            self->priority = 4 * 0x80;
            self->mapping_frame = 2;
            return;
        }
        self->collision_flags = 0x9A;
        Sst *chain = Sst_AllocateAfter(self);
        if (chain != NULL) {
            chain->code = self->code;
            chain->mappings = self->mappings;
            chain->art_tile = self->art_tile;
            chain->render_flags = SST_RENDER_LEVEL;
            chain->render_flags |= SST_RENDER_MULTI_DRAW;
            chain->height_pixels = chain->y_radius;
            chain->width_pixels = 0x40;
            chain->y_radius = 0x40;
            chain->priority = 5 * 0x80;
            chain->mapping_frame = 0;
            chain->child_count = (uint8_t)links;
            for (int i = 0; i < links && i < OBJECT_CHILDREN; i++) {
                chain->children[i].x = self->x_pos.f.u;
                chain->children[i].y = self->y_pos.f.u;
                chain->children[i].frame = 1;
            }
            chain->x_pos.f.u = self->x_pos.f.u;
            chain->y_pos.f.u = self->y_pos.f.u;
            own->chain = (uint8_t)(chain - sst);
        }
    }
        // Fallthrough
    case ChainRoutine_Circle: {
        own->angle = (uint16_t)(own->angle + own->speed);
        int16_t sin, cos;
        CalcSine((uint8_t)(own->angle >> 8), &sin, &cos);
        Sst *chain = SpikeballChain_Links(self, own);
        int links = chain ? chain->child_count : 0;
        // (a link a step of 16 pixels along the angle from the pivot, as 16.16 sums that are cut to whole pixels each)
        const int32_t step_y = (int32_t)(int16_t)(sin * 16) << 8, step_x = (int32_t)(int16_t)(cos * 16) << 8;
        int32_t acc_y = 0, acc_x = 0;
        for (int i = 0; i < links && i < OBJECT_CHILDREN; i++) {
            chain->children[i].x = (int16_t)((acc_x >> 16) + own->pivot_x);
            chain->children[i].y = (int16_t)((acc_y >> 16) + own->pivot_y);
            acc_x += step_x;
            acc_y += step_y;
        }
        if (chain != NULL) {
            self->x_pos.f.u = (int16_t)((acc_x >> 16) + own->pivot_x); // (the ball is where the next link would be)
            self->y_pos.f.u = (int16_t)((acc_y >> 16) + own->pivot_y);
            chain->x_pos.f.u = chain->children[4].x; // (the chain is where its fifth link is: that is what culls it)
            chain->y_pos.f.u = chain->children[4].y;
        }
        if (camera_split || !SstOutOfRange(own->pivot_x)) {
            Sst_Draw(self);
            return;
        }
        if (chain != NULL)
            Sst_Delete(chain);
        Sst_Delete(self);
        break;
    }
    case ChainRoutine_Block:
        FOR_EACH_CHARACTER(chr, who)
            SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 0x10, 0x11, self->x_pos.f.u, NULL);
        if (SstOutOfRange(self->x_pos.f.u)) {
            if (self->respawn)
                (*SstLoad_State(self)) &= 0x7F;
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 76: the platform with spikes at its sides. It waits until a character who is not in the air comes within reach at its level, then slides $80 pixels (to the left; to the right if
// flipped). Anyone who pushes against its sides is hurt
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t state;     // 0x28: 0 waiting, 2 sliding
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t pad1;      // 0x30
    int16_t pad2;      // 0x32
    int16_t base_x;    // 0x34
    int16_t slide;     // 0x36: frames of sliding left
} Own_PlatformSpikes;

void Obj_PlatformSpikes(Sst *self) {
    Own_PlatformSpikes *own = (Own_PlatformSpikes *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_PlatformSpikes;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
        self->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
        self->priority = 4 * 0x80;
        // The size by the subtype's high nibble (loc_1D08A: only the first of its four bytes' rows is data in the prototype): $40 wide, $10 high, frame 0
        self->width_pixels = 0x40;
        self->y_radius = 0x10;
        self->mapping_frame = 0;
        own->base_x = self->x_pos.f.u;
        own->state &= 0xF;
    }

    const int16_t old_x = self->x_pos.f.u;
    if (own->state == 0) {
        FOR_EACH_CHARACTER(chr, who) {
            if (((chr->status & SST_PSTATUS_IN_AIR) != 0))
                continue;
            int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0xC0);
            if (((self->status & SST_STATUS_X_FLIP) != 0))
                d0 -= 0x100;
            if ((uint16_t)d0 >= 0x80)
                continue;
            if ((uint16_t)(chr->y_pos.f.u - self->y_pos.f.u + 0x10) >= 0x20)
                continue;
            own->state = 2;
            own->slide = 0x80;
        }
    } else if (own->slide != 0) {
        own->slide--;
        self->x_pos.f.u += ((self->status & SST_STATUS_X_FLIP) != 0) ? 1 : -1;
    }

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who) {
            int32_t side = SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
            if (side == 1) {
                Spikes_HurtCharacter(self, chr, who);
                self->status &= (uint8_t)~(1 << (5 + who));
            }
        }
    }
    GoneIfOffscreen(self, own->base_x);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 77: the drawbridge. The first time its switch (the subtype) is down it lowers, or raises (flat it is a platform; up it is not, and lets go of whoever stands on it)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;   // 0x28: the switch
    uint8_t pad0[0xB]; // 0x29-0x33
    uint8_t moved;     // 0x34: it has been triggered (once)
} Own_Gate;

static const uint8_t gate_animations[] = { // (the alpha's DHz_Bridge_Animate_Data: five frames)
    0x00, 0x04, 0x00, 0x0C,
    0x03, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFE, 0x01, // (to flat)
    0x03, 0x00, 0x01, 0x02, 0x03, 0x04, 0xFE, 0x01, // (to raised)
};

void Obj_DHZGate(Sst *self) {
    Own_Gate *own = (Own_Gate *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_DHZGate;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x43C);
        self->render_flags |= SST_RENDER_LEVEL; // ori.b #4,1(a0): the layout's facing stays
        self->width_pixels = 0x80;
        self->priority = 0 * 0x80;
    }

    if (!own->moved && (f_switch[self->subtype & 0xF] & 1)) {
        own->moved = 1;
        self->anim ^= 1;
        if ((self->render_flags & SST_RENDER_ON_SCREEN))
            PlaySound(sfx_Door);
    }
    Sst_Animate(self, gate_animations);

    if (self->mapping_frame == 0) {
        FOR_EACH_CHARACTER(chr, who)
            SolidSst_Character(self, chr, who, 0x4B, 8, 9, self->x_pos.f.u, NULL);
    } else {
        FOR_EACH_CHARACTER(chr, who) {
            if (self->status & (1 << (3 + who)))
                chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        }
        self->status &= (uint8_t)~0x18;
    }
    if (SstOutOfRange(self->x_pos.f.u)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}
