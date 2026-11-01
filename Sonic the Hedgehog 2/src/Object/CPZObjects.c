// Chemical Plant's own objects, the Simon Wai prototype's: the elevator (19, Obj_0x19_Elevator at loc_1621C), the speed booster (1B, Obj1B), the section of pipe that tips you off (0B, Obj0B), the moving blocks
// that make its staircases (6B, Obj_0x6B_Mz_Platform at loc_1BCEC), the one way barrier (2D), the tube cover (32), the rotating platforms (78), the sliding platforms (7A), the invisible block (74)
// and the droplet chain badnik (1D). Each is a port of its routine in the prototype's disassembly.
#include "Object/CPZObjects.h"
#include "Object/CharControl.h"
void Obj_Points(Sst *self);
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Camera.h"
#include "Game.h"
#include "Level.h"
#include "SstLoad.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "MathUtil.h"
#include "Oscillatory Routines.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/Booster.h"
#include "Resource/Mappings/CPZBarrier.h"
#include "Resource/Mappings/SSZBarrier.h"
#include "Resource/Mappings/CPZBlock.h"
#include "Resource/Mappings/CPZInvisibleBlock.h"
#include "Resource/Mappings/CPZSlider.h"
#include "Resource/Mappings/CPZWorm.h"
#include "Resource/Mappings/FloatingPlatform.h"
#include "Resource/Mappings/OOZElevator.h"
#include "Resource/Mappings/PipeTipper.h"
#include "Resource/Mappings/TubeCover.h"
#include "Resource/Mappings/HTZRock.h"
#include "Resource/Mappings/SSZRock.h"
#include "Resource/Mappings/SSZPlatform.h"

extern const uint8_t Mappings_MTZPlatformA[]; // (defined with MTZObjects.c, whose platforms they are)

extern Oscillatory oscillatory;

// What the object does for each of the two characters: `chr` is NULL for a Tails who is not there
#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// The object is out of range: it goes, and forgets that it was loaded so that it can come back (MarkObjGone)
static void GoneIfOffscreen(Sst *self, int16_t x) {
    if (SstOutOfRange(x)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        SstLoad_Forget(self);
        Sst_Delete(self);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 19: the elevator, a platform that moves by its subtype
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 4-7 which size, bits 0-3 how it moves (the low four are the movement's step too: some kinds hand on to the next)
    uint8_t pad[7];
    int16_t base_x;   // 0x30
    int16_t base_y;   // 0x32
} Own_Elevator;

// loc_1622E: the width and the mapping frame of each size
// (the alpha's, for Chemical Plant and Oil Ocean: the third frame is a platform of $80 pixels; the prototype's was $40)
// Sand Shower's: its four frames of the staged mappings, 64, 48, 128 and 64 pixels wide ($20, $18, $40 and $20 either side)
static const uint8_t elevator_sizes_ssz[4][2] = { { 0x20, 0 }, { 0x18, 1 }, { 0x40, 2 }, { 0x20, 3 } };
static const uint8_t elevator_sizes_alpha[5][2] = { { 0x20, 0 }, { 0x18, 1 }, { 0x40, 2 }, { 0x40, 3 }, { 0x30, 4 } };

// The oscillators of the prototype's Oscillating_Data, by their offset (4 bytes each): the high byte of the value
static int16_t Osc(int offset) {
    return (int16_t)(uint8_t)(oscillatory.state[offset >> 2][0] >> 8);
}

static void Elevator_Move(Sst *self, Own_Elevator *own) {
    const int kind = self->subtype & 0xF;
    switch (kind) {
    case 0:
    case 1: { // slides along x
        int16_t d0 = kind == 0 ? Osc(8) : Osc(0xC), d1 = kind == 0 ? 0x40 : 0x60;
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(-d0 + d1);
        self->x_pos.f.u = (int16_t)(own->base_x - d0);
        break;
    }
    case 2: { // slides along y
        int16_t d0 = Osc(0x1C), d1 = 0x80;
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(-d0 + d1);
        self->y_pos.f.u = (int16_t)(own->base_y - d0);
        break;
    }
    case 3: // waits for someone to stand on it
        if (self->status & 0x18)
            self->subtype++;
        break;
    case 4: // then rises, slowing down to a stop at the top
    case 6:
    case 7: { // (6 and 7 never stop: they bob up and down above their start)
        Sst_MoveSprite2(self);
        int16_t step = 8;
        int16_t top = (int16_t)(own->base_y - 0x60);
        if ((uint16_t)top < (uint16_t)self->y_pos.f.u)
            step = -step;
        self->y_vel = (int16_t)(self->y_vel + step);
        if (kind == 4 && self->y_vel == 0)
            self->subtype++;
        break;
    }
    case 5: // (stopped)
        break;
    default: { // 8-15: round in a circle (the two oscillators a quarter of a turn apart), one way or the other and by which side it starts
        int16_t d1 = (int16_t)(Osc(0x38) - 0x40), d2 = (int16_t)(Osc(0x3C) - 0x40);
        if (kind & 4) {
            d1 = (int16_t)-d1;
            d2 = (int16_t)-d2;
        }
        if (kind & 2) {
            d1 = (int16_t)-d1;
            int16_t t = d1;
            d1 = d2;
            d2 = t;
        }
        if (kind >= 12)
            d1 = (int16_t)-d1;
        self->x_pos.f.u = (int16_t)(own->base_x + d1);
        self->y_pos.f.u = (int16_t)(own->base_y + d2);
        break;
    }
    }
}

void Obj_CPZElevator(Sst *self) {
    Own_Elevator *own = (Own_Elevator *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        const bool oil = LEVEL_ZONE(level_id) == ZoneId_OOZ;
        const bool sand = LEVEL_ZONE(level_id) == ZoneId_SSZ; // (Sand Shower's, the staged objpatch-19.asm: its own mappings, drawn from the level's tiles)
        self->mappings = sand ? Mappings_SSZPlatform : Mappings_OOZElevator; // (the alpha's, for both zones: five frames)
        self->art_tile = sand ? TILE_MAP(0, 0, 0, 0, 0) : TILE_MAP(0, 3, 0, 0, oil ? 0x2F4 : 0x3A0); // (the alpha's art list puts Oil Ocean's at $5E80)
        self->render_flags = SST_RENDER_LEVEL;
        int size = (self->subtype >> 4) & 0xF;
        if (size > (sand ? 3 : 4))
            size = sand ? 3 : 4;
        self->width_pixels = sand ? elevator_sizes_ssz[size][0] : elevator_sizes_alpha[size][0];
        self->mapping_frame = sand ? elevator_sizes_ssz[size][1] : elevator_sizes_alpha[size][1];
        self->priority = 4 * 0x80;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        self->subtype &= 0xF;
        if (self->subtype == 7)
            self->y_pos.f.u -= 0xC0;
    }

    int16_t old_x = self->x_pos.f.u;
    Elevator_Move(self, own);
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(self, chr, who, self->width_pixels, 0x10, old_x);

    if (SstOutOfRange(own->base_x)) {
        GoneIfOffscreen(self, own->base_x);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 1B: the speed booster
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bit 1 the slower one
    uint8_t pad[7];
    int16_t speed;    // 0x30
} Own_Booster;

#define ARTTILE_BOOSTER 0x39C // ($7380 in the zone's art list)

static void Booster_Give(Sst *self, Sst *chr, int16_t speed) {
    int16_t going = chr->x_vel; // (the pre-beta's: how fast he goes the way the booster faces; the alpha's looked at the size of his speed, either way)
    if (self->status & SST_STATUS_X_FLIP)
        going = (int16_t)-going;
    if (going >= 0x1000) { // (a character who is going that fast already is left as he is, and the booster only sounds)
        PlaySound(sfx_Spring);
        return;
    }
    chr->x_vel = speed; // (goes super fast)
    chr->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        chr->status |= SST_PSTATUS_X_FLIP;
        chr->x_vel = (int16_t)-chr->x_vel;
    }
    ((Scratch_Sonic *)chr->own.u8)->control_lock = 0xF; // (not to turn round for a few frames)
    chr->ground_vel = chr->x_vel;
    self->status &= (uint8_t)~((1 << 5) | (1 << 6));
    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    PlaySound(sfx_Spring);
}

void Obj_CPZBooster(Sst *self) {
    Own_Booster *own = (Own_Booster *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_Booster;
        self->art_tile = TILE_MAP(1, 3, 0, 0, ARTTILE_BOOSTER);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->priority = 1 * 0x80;
        own->speed = (self->subtype & 2) ? 0xA00 : 0x1000;
    }

    self->mapping_frame = (uint8_t)(frame_count & 2);
    const int16_t x0 = (int16_t)(self->x_pos.f.u - 0x10), x1 = (int16_t)(self->x_pos.f.u + 0x10);
    const int16_t y0 = (int16_t)(self->y_pos.f.u - 0x10), y1 = (int16_t)(self->y_pos.f.u + 0x10);
    FOR_EACH_CHARACTER(chr, who) {
        (void)who;
        if (((chr->status & SST_PSTATUS_IN_AIR) != 0))
            continue;
        uint16_t cx = (uint16_t)chr->x_pos.f.u, cy = (uint16_t)chr->y_pos.f.u;
        if (cx >= (uint16_t)x0 && cx < (uint16_t)x1 && cy >= (uint16_t)y0 && cy < (uint16_t)y1)
            Booster_Give(self, chr, own->speed);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 0B: the section of pipe that tips you off
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 4-7 how long it stays (in 8ths of a second), bits 0-3 when it first turns
    uint8_t pad[7];
    int16_t wait;     // 0x30
    int16_t wait2;    // 0x32
    uint8_t offset;   // 0x36
} Own_PipeTipper;

#define ARTTILE_PIPE_TIPPER 0x3B0 // ($7600 in the zone's art list)

// Ani_obj0B: it tips over to its fifth frame and back
static const uint8_t Animation_PipeTipper[] = {
    0x00, 0x04, 0x00, 0x0C,
    0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0xFE, 0x01,
    0x07, 0x04, 0x03, 0x02, 0x01, 0x00, 0xFE, 0x01,
};

void Obj_CPZPipeTipper(Sst *self) {
    Own_PipeTipper *own = (Own_PipeTipper *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_PipeTipper;
        self->art_tile = TILE_MAP(1, 3, 0, 0, ARTTILE_PIPE_TIPPER);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        int d0 = (self->subtype & 0xF0) + 0x10;
        own->wait = (int16_t)(d0 - 1);
        own->wait2 = (int16_t)(d0 - 1);
        own->offset = (uint8_t)(((self->subtype & 0xF) + 1) << 4);
    }

    if (self->routine == 2) { // waits until the frame counter and its offset make 0
        if ((uint8_t)(frame_count + own->offset) != 0) {
            SstOnScreenTest(self);
            return;
        }
        self->routine += 2;
    }
    // Turn
    if (--own->wait < 0) {
        own->wait = 0x7F;
        if (self->anim != 0)
            own->wait = own->wait2;
        self->anim ^= 1;
        self->prev_anim = 0xFF; // (the animation starts again)
    }
    Sst_Animate(self, Animation_PipeTipper);

    if (self->mapping_frame == 0) { // upright: a platform
        FOR_EACH_CHARACTER(chr, who)
            Solid_Platform(self, chr, who, 0x10, 0x11, self->x_pos.f.u);
    } else { // tipped: whoever stands on it falls
        FOR_EACH_CHARACTER(chr, who) {
            if (self->status & (1 << (3 + who))) {
                chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                self->status &= (uint8_t)~(1 << (3 + who));
            }
        }
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 6B: the moving block (the staircase): a solid block that slides, waits to be stood on and then falls, or travels round a square, by its subtype
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bit 4 the size, bits 0-3 how it moves
    uint8_t pad[7];
    int16_t base_y;   // 0x30
    int16_t base_x;   // 0x34
    uint8_t started;  // 0x38: (kind 7) it has been stood on
} Own_CPZBlock;

#define ARTTILE_CPZ_BLOCK 0x418 // ($8300 in the zone's art list)

// What the block does (loc_1BDC8): 0 nothing (7 is the alpha's new one, below), 1 and 2 slide along x, 3 and 4 along y, 5 waits to be stood on and goes on to 6, which falls; 8-11 go round a square of side 2x$10, $30, $50 or $70, as the
// oscillator that the kind names goes
static void CPZBlock_Move(Sst *self, Own_CPZBlock *own) {
    const int kind = self->subtype & 0xF;
    switch (kind) {
    case 1:
    case 2:
    case 3:
    case 4: {
        int16_t d1 = (kind == 1 || kind == 3) ? 0x40 : 0x80;
        int16_t d0 = (kind == 1 || kind == 3) ? Osc(8) : Osc(0x1C);
        if (((self->status & SST_STATUS_X_FLIP) != 0))
            d0 = (int16_t)(-d0 + d1);
        if (kind <= 2)
            self->x_pos.f.u = (int16_t)(own->base_x - d0);
        else
            self->y_pos.f.u = (int16_t)(own->base_y - d0);
        break;
    }
    case 5:
        self->y_pos.f.u = (int16_t)(own->base_y + (Osc(0) >> 1));
        if (self->status & 0x18)
            self->subtype++;
        break;
    case 6:
        self->y_pos.v += self->y_vel * 256;
        self->y_vel = (int16_t)(self->y_vel + 8);
        if (!((uint16_t)(limit_btm1 + 0xE0) >= (uint16_t)self->y_pos.f.u))
            self->subtype = 0;
        break;
    case 7: // (the alpha's: waits to be stood on, then drops $70 pixels as a pendulum would, speeding up and slowing down by 8, and stops where its speed is 0 again)
        if (!own->started) {
            if (!(self->status & 0x18))
                break;
            own->started = 1;
        }
        Sst_MoveSprite2(self);
        {
            int16_t accel = 8;
            if ((uint16_t)(own->base_y + 0x70) < (uint16_t)self->y_pos.f.u)
                accel = -8;
            self->y_vel = (int16_t)(self->y_vel + accel);
            if (self->y_vel == 0)
                self->subtype = 0;
        }
        break;
    case 8:
    case 9:
    case 10:
    case 11: {
        static const int16_t sides[4] = { 0x10, 0x30, 0x50, 0x70 };
        const int osc = 10 + (kind - 8) * 1; // the oscillator pairs (Oscillating_Data+$28, +$2C, +$30, +$34: entries 10, 11, 12, 13)
        int16_t d1 = sides[kind - 8];
        int16_t d0 = (int16_t)(oscillatory.state[osc][0] >> 8);
        if (kind == 8)
            d0 = (int16_t)(d0 >> 1);
        int16_t d3 = (int16_t)oscillatory.state[osc][1];
        if (d3 == 0)
            self->status = (uint8_t)((self->status & ~3) | ((self->status + 1) & 3)); // (the phase: which side of the square)
        switch (self->status & 3) {
        case 0:
            self->x_pos.f.u = (int16_t)(own->base_x + d0 - d1);
            self->y_pos.f.u = (int16_t)(own->base_y - d1);
            break;
        case 1:
            self->y_pos.f.u = (int16_t)(own->base_y - (d0 - (d1 - 1)));
            self->x_pos.f.u = (int16_t)(own->base_x + d1);
            break;
        case 2:
            self->x_pos.f.u = (int16_t)(own->base_x - (d0 - (d1 - 1)));
            self->y_pos.f.u = (int16_t)(own->base_y + d1);
            break;
        default:
            self->y_pos.f.u = (int16_t)(own->base_y + d0 - d1);
            self->x_pos.f.u = (int16_t)(own->base_x - d1);
            break;
        }
        break;
    }
    default:
        break;
    }
}

void Obj_CPZBlock(Sst *self) {
    Own_CPZBlock *own = (Own_CPZBlock *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        const bool cpz = LEVEL_ZONE(level_id) == ZoneId_CPZ; // (loc_1BD06: Metropolis's platforms are the slide-out platform's: object 65's mappings)
        self->mappings = cpz ? Mappings_CPZBlock : Mappings_MTZPlatformA;
        self->art_tile = TILE_MAP(0, 3, 0, 0, cpz ? ARTTILE_CPZ_BLOCK : 0);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        const bool small = (self->subtype >> 4) & 1; // (loc_1BCFE: a wide block of 12 high, or a square one of 16)
        self->width_pixels = small ? 0x10 : 0x20;
        self->y_radius = small ? 0x10 : 0x0C;
        self->mapping_frame = small ? 0 : 1;
        own->base_x = self->x_pos.f.u;
        own->base_y = self->y_pos.f.u;
        const int kind = self->subtype & 0xF;
        if (kind >= 8 && (int16_t)oscillatory.state[10 + (kind - 8)][1] < 0)
            self->status ^= 1; // (it starts on the other side of its square when its oscillator is going down)
    }

    int16_t old_x = self->x_pos.f.u;
    CPZBlock_Move(self, own);
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), old_x, NULL);
    }
    if (SstOutOfRange(own->base_x)) {
        GoneIfOffscreen(self, own->base_x);
        return;
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 2D: the one way barrier, a door that rises while a character is within reach of it on its open side, and shuts behind them
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: the frame
    uint8_t pad0[7];
    int16_t height;   // 0x30: how far it has risen
    int16_t base_y;   // 0x32
    int16_t wait;     // 0x34: (Sand Shower's) how long it stays open after the button it answers is let go
    uint8_t latched;  // 0x36: (the switch door) its switch has been pressed and nobody has gone through since
    uint8_t passing;  // 0x37: and someone is going through
    int16_t left;     // 0x38: the strip in which it opens
    int16_t right;    // 0x3A
} Own_CPZBarrier;

#define ARTTILE_CPZ_STRIPES 0x394 // ($7280 in the zone's art list)
#define ARTTILE_HTZ_VALVE_BARRIER 0x426 // ($84C0 in Hill Top's second list)

void Obj_CPZBarrier(Sst *self) {
    Own_CPZBarrier *own = (Own_CPZBarrier *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        const bool ssz = LEVEL_ZONE(level_id) == ZoneId_SSZ; // (Sand Shower's door: its own mappings, one frame, drawn from the level's tiles on palette row 2)
        self->mappings = ssz ? Mappings_SSZBarrier : Mappings_CPZBarrier;
        // (Hill Top's valve barrier has art and a width of its own: the default of Obj2D_Init; Chemical Plant's is the stripes)
        const bool cpz = LEVEL_ZONE(level_id) == ZoneId_CPZ;
        const bool mtz = LEVEL_ZONE(level_id) == ZoneId_MTZ; // (Metropolis's is the first tile of its art, and as wide as Chemical Plant's)
        self->art_tile = ssz ? TILE_MAP(0, 2, 0, 0, 0) : mtz ? TILE_MAP(0, 3, 0, 0, 0) : TILE_MAP(0, 1, 0, 0, cpz ? ARTTILE_CPZ_STRIPES : ARTTILE_HTZ_VALVE_BARRIER);
        self->width_pixels = ssz ? 0x10 : 8; // (Sand Shower's runs from x to x+32: its solid box is centred 16 to the right; the pre-beta's Chemical Plant and Metropolis doors are 8 wide too: the alpha's were 12)
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->base_y = self->y_pos.f.u;
        self->mapping_frame = ssz ? 0 : self->subtype;
        int16_t d2 = (int16_t)(self->x_pos.f.u - 0x200), d3 = (int16_t)(self->x_pos.f.u + 0x18);
        if (((self->status & SST_STATUS_X_FLIP) != 0)) {
            d2 = (int16_t)(d2 + 0x1E8);
            d3 = (int16_t)(d3 + 0x1E8);
        }
        own->left = d2;
        own->right = d3;
    }

    int16_t d2, d3;
    const int16_t x = self->x_pos.f.u;
    if (LEVEL_ZONE(level_id) == ZoneId_SSZ) {
        // Sand Shower's door, in three kinds by the subtype (bits 4-5: the kind; bits 0-3: the switch it answers; bits 6-7: the time of the third kind). It is drawn from x to x+32, and opens to the side that it faces:
        // the open side is to the left, to the right if the object is flipped in X.
        //  0 the door: opens as a character comes up to it on its open side, and shuts behind him
        //  1 the switch door: the same, but it does not open until its switch is pressed, and then it waits for someone to go through it, and shuts behind him
        //  2 the timed switch door: faces nothing: its switch opens it, it stays up for the time the subtype gives (3, 5, 8 or 12 seconds) and shuts, on someone in its way too (he is crushed)
        const unsigned kind = (self->subtype >> 4) & 3;
        const bool pressed = (f_switch[self->subtype & 0xF] & 1) != 0;
        const bool flipped = ((self->status & SST_STATUS_X_FLIP) != 0);
        bool in_way = false;   // (in the door's place: it must not shut on him)
        FOR_EACH_CHARACTER(chr, who) {
            const int16_t cx = chr->x_pos.f.u, cy = chr->y_pos.f.u;
            if (cx >= x - 8 && cx < x + 40 && cy >= own->base_y - 0x60 && cy < own->base_y + 0x30)
                in_way = true;
        }
        bool open;
        if (kind == 2) {
            static const int16_t seconds[4] = { 3, 5, 8, 12 };
            if (pressed)
                own->wait = (int16_t)(seconds[(self->subtype >> 6) & 3] * 60);
            open = own->wait > 0;
            if (own->wait > 0)
                own->wait--;
            // (nothing holds it for someone in its way: when the time is up it shuts, and a character who is still under it is crushed, as the solid test has it. That is the puzzle)
        } else {
            // the strip in which a character opens it, as the barrier has it, wider for this one's width: the open side's reach, and the far edge that he has to be past for it to shut (x+$38, or x-$18 flipped)
            const int16_t d2 = flipped ? (self->routine_secondary ? x - 0x18 : x + 0x20) : x - 0x200;
            const int16_t d3 = flipped ? x + 0x200 : (self->routine_secondary ? x + 0x38 : x);
            bool in_strip = false;
            FOR_EACH_CHARACTER(chr, who) {
                const int16_t cx = chr->x_pos.f.u;
                if (cx >= d2 && cx < d3)
                    in_strip = true;
            }
            if (kind == 0) {
                open = in_strip || in_way;
            } else {
                if (pressed)
                    own->latched = 1;
                if (own->latched && in_way)
                    own->passing = 1;
                if (own->latched && own->passing && !in_way)
                    own->latched = own->passing = 0; // (someone has gone through: it shuts behind him)
                open = own->latched != 0;
            }
        }
        self->routine_secondary = open ? 2 : 0;
    } else {
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        d2 = own->left;
        d3 = self->routine_secondary ? own->right : x;
    } else {
        d2 = self->routine_secondary ? own->left : x;
        d3 = own->right;
    }
    self->routine_secondary = 0; // (down, unless a character is in the strip)
    FOR_EACH_CHARACTER(chr, who) {
        const int16_t cx = chr->x_pos.f.u, cy = chr->y_pos.f.u; // (the pre-beta's: the strip is a box, $20 above and below the door's base, and a character whom an object holds with his collisions off does not open it)
        if (cx >= d2 && (uint16_t)cx < (uint16_t)d3 && (uint16_t)cy >= (uint16_t)(own->base_y - 0x20) && (uint16_t)cy < (uint16_t)(own->base_y + 0x20) && !(CharObjControl(chr) & 0x80))
            self->routine_secondary = 2;
    }
    }
    if (self->routine_secondary) {
        if (own->height != 0x40) {
            own->height += 8;
            self->y_pos.f.u = (int16_t)(own->base_y - own->height);
        }
    } else if (own->height != 0) {
        own->height -= 8;
        self->y_pos.f.u = (int16_t)(own->base_y - own->height);
    }

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        // (Sand Shower's door is drawn from its x to x+32: the solid test measures from the object's own x, so it is moved to the door's centre for it and back)
        const int16_t shift = (LEVEL_ZONE(level_id) == ZoneId_SSZ) ? 0x10 : 0;
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u + shift);
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 0x20, 0x21, (int16_t)(x + shift), NULL);
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u - shift);
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 32: the tube cover (in Hill Top the rock): solid, until a character rolls into it from above: it breaks into four pieces and throws him up
// ---------------------------------------------------------------------------------------------------------------------------------------
#define ARTTILE_TUBE_COVER 0x430 // ($8600 in the zone's art list)
#define ARTTILE_HTZ_ROCK 0x3B2 // ($7640 in Hill Top's)

// loc_17808: the speed of each piece (x, y); and Hill Top's rock's (loc_177F0), which is in six
static const int16_t cover_pieces[4][2] = { { -0x100, -0x200 }, { 0x100, -0x200 }, { -0xC0, -0x1C0 }, { 0xC0, -0x1C0 } };
static const int16_t rock_pieces[6][2] = { { -0x200, -0x200 }, { 0, -0x280 }, { 0x200, -0x200 }, { -0x1C0, -0x1C0 }, { 0, -0x200 }, { 0x1C0, -0x1C0 } };

// loc_17778: a rolling character bounces off what he has broken, curled up
static void Cover_Bounce(Sst *chr) {
    chr->status |= SST_PSTATUS_IN_BALL;
    chr->y_radius = 0xE;
    chr->x_radius = 7;
    chr->anim = SonAnimId_Roll;
    chr->y_vel = -0x300;
}

// loc_17796 (and loc_17772 above it): he is in the air, and no longer on the cover
static void Cover_Launch(Sst *chr, bool rolling) {
    if (rolling)
        Cover_Bounce(chr);
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
}

// loc_17818: the score for it, as for a badnik (Obj29, 10, 20, 50 then 100 and, after fifteen in a row, 1000)
void ObjectChainScore(Sst *self) {
    Sst *points = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
    if (points == NULL)
        return;
    static const uint16_t scores[4] = { 10, 20, 50, 100 };
    uint16_t d2 = item_bonus;
    item_bonus += 2;
    if (d2 >= 6)
        d2 = 6;
    uint16_t score = scores[d2 >> 1];
    if (item_bonus >= 0x20) {
        score = 1000;
        d2 = 10;
    }
    AddPoints(score);
    points->mapping_frame = (uint8_t)(d2 >> 1);
}

// BreakObjectToPieces: the object is its first piece, and each other piece of the frame is a new object of the same id, in routine 4, that drifts away on the speed its piece is given
void ObjectBreakToPieces(Sst *self, const int16_t (*speeds)[2], int count) {
    const uint8_t *piece;
    Mappings_FramePieces((const uint8_t *)self->mappings, self->mapping_frame, &piece);
    self->render_flags |= SST_RENDER_STATIC;
    Sst *part = self;
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            part = Sst_AllocateAfter(self);
            if (part == NULL)
                break;
            piece += SPRITE_PIECE_SIZE;
            part->code = self->code;
            part->render_flags = self->render_flags;
            part->art_tile = self->art_tile;
            part->priority = self->priority;
            part->width_pixels = self->width_pixels;
            part->x_pos.f.u = self->x_pos.f.u;
            part->y_pos.f.u = self->y_pos.f.u;
        }
        part->routine = 4;
        part->mappings = piece;
        part->x_vel = speeds[i][0];
        part->y_vel = speeds[i][1];
    }
    PlaySound(sfx_WallSmash);
}

void Obj_TubeCover(Sst *self) {
    if (self->routine == 0) {
        self->routine += 2;
        const bool rock = LEVEL_ZONE(level_id) != ZoneId_CPZ; // (Hill Top's rock: Obj_0x32 is the same object there, with its own art, width and pieces)
        const bool sand = LEVEL_ZONE(level_id) == ZoneId_SSZ; // (Sand Shower's rock: its own mappings, art at $3B2, 16 pixels wide, six pieces like Hill Top's; the third row of the palette (line 2 counting from 0): the mapping's words already say line 2 and high priority ($C0xx) and the engine adds the object's tile word to them, so the object adds no line of its own (a line on top of the mapping's 2 made it the wrong row))
        self->mappings = sand ? Mappings_SSZRock : rock ? Mappings_HTZRock : Mappings_TubeCover;
        self->art_tile = sand ? TILE_MAP(0, 0, 0, 0, 0x3BA) : rock ? TILE_MAP(0, 2, 0, 0, ARTTILE_HTZ_ROCK) : TILE_MAP(0, 3, 0, 0, ARTTILE_TUBE_COVER);
        self->width_pixels = rock && !sand ? 0x18 : 0x10;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = (sand ? 2 : 4) * 0x80; // (Sand Shower: in front of monitors, which are 3, and so of the switch, 4: a monitor can be hidden under a rock and a switch under a monitor. Sonic and Tails are 2 too, but come first in the queue, so they stay in front)
    }

    if (self->routine == 4) { // a piece
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        if (!(self->render_flags & SST_RENDER_ON_SCREEN))
            Sst_Delete(self);
        else
            Sst_Draw(self);
        return;
    }

    Sst *sidekick = TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL;
    const bool sonic_rolls = player->anim == SonAnimId_Roll, tails_rolls = sidekick != NULL && sidekick->anim == SonAnimId_Roll;
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 0x10, 0x11, self->x_pos.f.u, NULL);
    }
    const uint8_t stand = self->status & 0x18;
    bool broken = false;
    if (stand == 0x18) { // both are on it
        if (sonic_rolls || tails_rolls) {
            Cover_Launch(player, sonic_rolls);
            if (sidekick != NULL)
                Cover_Launch(sidekick, tails_rolls);
            broken = true;
        }
    } else if (stand & 0x08) {
        if (sonic_rolls) {
            Cover_Launch(player, true);
            broken = true;
        }
    } else if (stand & 0x10) {
        if (sidekick != NULL && tails_rolls) {
            Cover_Launch(sidekick, true);
            broken = true;
        }
    }
    if (!broken) {
        SstOnScreenTest(self);
        return;
    }
    self->status &= 0xE7;
    if (self->mappings == Mappings_HTZRock || self->mappings == Mappings_SSZRock)
        ObjectBreakToPieces(self, rock_pieces, 6);
    else
        ObjectBreakToPieces(self, cover_pieces, 4);
    ObjectChainScore(self);
    // (it goes on as its first piece, this very frame)
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 78: the rotating platforms: four blocks side by side that hang from the first and sink in a staircase when they are stood on (or bumped from below)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 0-2 what they do, and the next kind once they have started
    uint8_t pad0[3];
    int16_t wait;     // 0x2C
    uint8_t touched;  // 0x2E: what has touched the first (bit 4/5: stood on by Sonic/Tails, bit 2/3: bumped from below by them), it stays set
    uint8_t index;    // 0x2F: which of the four it is, as the offset into the first's table
    int16_t base_x;   // 0x30
    int16_t base_y;   // 0x32
    int16_t off[4];   // 0x34: (the first's) how far down each of the four is
    uint8_t parent;   // 0x3C: the slot of the first
    uint8_t pad1[3];
} Own_CPZRotor;

// loc_1D4BC .. loc_1D552: what the first does for each kind (0 and 4 wait to be stood on, 2 and 6 to be bumped, then 1, 3, 5, 7 sink or rise)
static void Rotor_Run(Sst *self, Own_CPZRotor *own) {
    int16_t *off = own->off;
    switch (self->subtype & 7) {
    case 0:
    case 4:
        if (own->wait == 0) {
            if (own->touched & 0x30)
                own->wait = 0x1E;
        } else if (--own->wait == 0) {
            self->subtype++;
        }
        break;
    case 2:
    case 6:
        if (own->wait == 0) {
            if (own->touched & 0x0C)
                own->wait = 0x3C;
        } else if (--own->wait == 0) {
            self->subtype++;
        } else { // shakes: the four go up and down a pixel, in turns
            int16_t d0 = (int16_t)((own->wait >> 2) & 1);
            off[0] = d0;
            off[1] = d0 ^ 1;
            off[2] = d0;
            off[3] = d0 ^ 1;
        }
        break;
    case 1:
    case 3:
        if (off[0] != 0x80) {
            off[0]++;
            off[1] = (int16_t)(off[0] * 3 >> 2);
            off[2] = (int16_t)(off[0] >> 1);
            off[3] = (int16_t)(off[0] >> 2);
        }
        break;
    default: // 5, 7
        if (off[0] != -0x80) {
            off[0]--;
            off[1] = (int16_t)(off[0] * 3 >> 2);
            off[2] = (int16_t)(off[0] >> 1);
            off[3] = (int16_t)(off[0] >> 2);
        }
        break;
    }
}

void Obj_CPZRotor(Sst *self) {
    Own_CPZRotor *own = (Own_CPZRotor *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        int d3 = 0x34, d4 = 2;
        if (((self->status & SST_STATUS_X_FLIP) != 0)) {
            d3 = 0x3A;
            d4 = -2;
        }
        int16_t d2 = self->x_pos.f.u;
        Sst *part = self;
        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                part = Sst_AllocateAfter(self);
                if (part == NULL)
                    break;
                part->routine = 4;
            }
            Own_CPZRotor *ps = (Own_CPZRotor *)part->own.u8;
            part->code = self->code;
            part->mappings = Mappings_CPZBlock;
            part->art_tile = TILE_MAP(0, 3, 0, 0, ARTTILE_CPZ_BLOCK);
            part->render_flags = 4;
            part->priority = 3 * 0x80;
            part->width_pixels = 0x10;
            part->subtype = self->subtype;
            part->x_pos.f.u = d2;
            part->y_pos.f.u = self->y_pos.f.u;
            ps->base_x = self->x_pos.f.u;
            ps->base_y = part->y_pos.f.u;
            d2 = (int16_t)(d2 + 0x20);
            ps->index = (uint8_t)d3;
            ps->parent = (uint8_t)(self - sst);
            d3 += d4;
        }
    }

    if (self->routine == 2) {
        Rotor_Run(self, own);
    }
    Sst *first = &sst[own->parent];
    Own_CPZRotor *fs = (Own_CPZRotor *)first->own.u8;
    self->y_pos.f.u = (int16_t)(own->base_y + fs->off[(own->index - 0x34) >> 1]);
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        FOR_EACH_CHARACTER(chr, who) {
            int32_t hit = Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), 0x10, 0x11, self->x_pos.f.u, NULL);
            if (hit == -1)
                fs->touched |= (uint8_t)(0x10 << who);
            else if (hit == -2)
                fs->touched |= (uint8_t)(4 << who);
        }
    }
    GoneIfOffscreen(self, own->base_x);
    if (self->code != NULL)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 7A: the sliding platforms: one platform, or two, that go to and fro along a row and turn round when they meet
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: which row (the offset into loc_1D5A8)
    uint8_t pad0[7];
    int16_t base_x;   // 0x30
    int16_t left;     // 0x32: the ends of the row
    int16_t right;    // 0x34
    uint8_t going_left; // 0x36
    uint8_t pad1[5];
    uint8_t partner;  // 0x3C: the slot of the other platform
    uint8_t pad2[3];
} Own_CPZSlider;

#define ARTTILE_CPZ_SLIDER ARTTILE_CPZ_BLOCK

// loc_1D5A8: for each kind, how many platforms there are less one, how far the row reaches each side of its place, and where each platform starts in it
typedef struct {
    uint8_t count, reach;
    int16_t start0, start1;
} SliderRow;
// The alpha's rows (its platforms are 48 pixels wide, 8 more than the prototype's, so its rows are 8 shorter, and there is a fourth one): Chemical Plant's and Dust Hill's alike
static const SliderRow slider_rows_alpha[4] = { { 0, 0x68, -0x68, 0 }, { 1, 0xA8, -0xB0, 0x40 }, { 1, 0xE8, -0x80, 0x80 }, { 0, 0x68, 0x67, 0 } };

static bool SliderIsDustHill(void) {
    return LEVEL_ZONE(level_id) == ZoneId_MCZ;
}

const void *DHZSwingMappings(void); // (NGHZObjects.c holds the resource)

static void Slider_Move(Sst *self, Own_CPZSlider *own) {
    const int16_t old_x = self->x_pos.f.u;
    int16_t x = old_x;
    if (own->going_left) {
        x--;
        if (x == own->left)
            own->going_left = 0;
    } else {
        x++;
        if (x == own->right)
            own->going_left = 1;
    }
    self->x_pos.f.u = x;
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(self, chr, who, self->width_pixels, 8, old_x);
}

void Obj_CPZSlider(Sst *self) {
    Own_CPZSlider *own = (Own_CPZSlider *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        const bool dust_hill = SliderIsDustHill();
        const int row = self->subtype / 6 > 3 ? 3 : self->subtype / 6;
        const SliderRow *conf = &slider_rows_alpha[row];
        Sst *other = self;
        for (int i = 0; i <= conf->count; i++) {
            if (i > 0) {
                other = Sst_AllocateAfter(self);
                if (other == NULL)
                    break;
                other->code = self->code;
                other->routine = 4;
                other->x_pos.f.u = self->x_pos.f.u;
                other->y_pos.f.u = self->y_pos.f.u;
            }
            other->mappings = dust_hill ? DHZSwingMappings() : Mappings_CPZSlider;
            other->art_tile = dust_hill ? TILE_MAP(0, 0, 0, 0, 0) : TILE_MAP(1, 3, 0, 0, ARTTILE_CPZ_SLIDER);
            other->render_flags = 4;
            other->priority = 4 * 0x80;
            other->width_pixels = 0x18;
            ((Own_CPZSlider *)other->own.u8)->base_x = self->x_pos.f.u;
        }
        ((Own_CPZSlider *)other->own.u8)->partner = (uint8_t)(self - sst);
        own->partner = (uint8_t)(other - sst);
        if (self->subtype == 0xC)
            own->going_left = 1;
        const int16_t reach = conf->reach;
        const int16_t left = (int16_t)(own->base_x - reach), right = (int16_t)(left + reach * 2);
        Own_CPZSlider *os = (Own_CPZSlider *)other->own.u8;
        own->left = os->left = left;
        own->right = os->right = right;
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u + conf->start0);
        other->x_pos.f.u = (int16_t)(other->x_pos.f.u + conf->start1);
    }

    if (self->routine == 2) { // the first: it is the one that goes when the row is out of range
        Slider_Move(self, own);
        if (SstOutOfRange(own->left) && SstOutOfRange(own->right)) {
            Sst *other = &sst[own->partner];
            if (other != self && other->code == self->code)
                Sst_Delete(other);
            if (self->respawn)
                (*SstLoad_State(self)) &= 0x7F;
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
    } else { // the second: it turns the first round when they meet
        Slider_Move(self, own);
        Sst *other = &sst[own->partner];
        if (other->code == self->code && (int16_t)(self->x_pos.f.u - self->width_pixels) == (int16_t)(other->x_pos.f.u + self->width_pixels)) {
            own->going_left ^= 1;
            ((Own_CPZSlider *)other->own.u8)->going_left ^= 1;
        }
    }
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 74: the invisible block, a solid that is only there while it is on the screen (and only shows in debug mode)
// ---------------------------------------------------------------------------------------------------------------------------------------
void Obj_CPZInvisibleBlock(Sst *self) {
    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CPZInvisibleBlock;
        self->art_tile = TILE_MAP(1, 0, 0, 0, 0x680);
        self->render_flags |= SST_RENDER_LEVEL;
        const uint8_t subtype = self->subtype;
        self->width_pixels = (uint8_t)((((subtype & 0xF0) + 0x10)) >> 1);
        self->y_radius = (int8_t)(((subtype & 0xF) + 1) << 3);
    }

    const int16_t dx = (int16_t)(self->x_pos.f.u - scrpos_x.f.u), dy = (int16_t)(self->y_pos.f.u - scrpos_y.f.u);
    if (dx >= 0 && dx < 0x140 && dy >= 0 && dy < 0xE0) {
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), self->x_pos.f.u, NULL);
    }
    if (SstOutOfRange(self->x_pos.f.u)) {
        GoneIfOffscreen(self, self->x_pos.f.u);
        return;
    }
    if (debug_use)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 1D: the droplet chain, a row of drops that jump up out of the floor one after another (it hurts), at the same place each time or, in
// the other kind, hop sideways at the top
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 0-3 how many drops, bit 4-7 the hopping kind
    uint8_t pad0[7];
    int16_t base_y;   // 0x30
    int16_t wait;     // 0x32: frames to wait before the jump
    int16_t jump;     // 0x34: the speed it jumps at
    int16_t drift;    // 0x36: the speed it adds to xsp each frame
    int16_t base_x;   // 0x38
    int16_t hop;      // 0x3A: how far it hops at the top of the jump (the hopping kind)
} Own_CPZWorm;

#define ARTTILE_CPZ_WORM 0x43C // ($8780 in the zone's second list)

void Obj_CPZWorm(Sst *self) {
    Own_CPZWorm *own = (Own_CPZWorm *)self->own.u8;

    switch (self->routine) {
    case 0: {
        const int count = self->subtype & 0xF;
        const uint8_t routine = (self->subtype & 0xF0) ? 6 : 2;
        const bool flip = ((self->status & SST_STATUS_X_FLIP) != 0);
        Sst *drop = self;
        for (int i = 0; i <= count; i++) {
            if (i > 0) {
                drop = Sst_AllocateAfter(self);
                if (drop == NULL)
                    break;
            }
            Own_CPZWorm *ds = (Own_CPZWorm *)drop->own.u8;
            drop->code = self->code;
            drop->routine = routine;
            drop->x_pos.f.u = self->x_pos.f.u;
            drop->y_pos.f.u = self->y_pos.f.u;
            drop->mappings = Mappings_CPZWorm;
            drop->art_tile = TILE_MAP(0, 3, 0, 0, ARTTILE_CPZ_WORM);
            drop->render_flags = 4;
            drop->priority = 3 * 0x80;
            drop->collision_flags = 0x8B;
            ds->base_x = drop->x_pos.f.u;
            ds->base_y = drop->y_pos.f.u;
            drop->y_vel = -0x480;
            ds->jump = drop->y_vel;
            drop->width_pixels = 8;
            ds->hop = flip ? -0x60 : 0x60;
            ds->drift = flip ? -0x0B : 0x0B;
            ds->wait = (int16_t)(i * 3);
        }
        return;
    }
    case 2:
    case 6: // waiting to jump
        if (--own->wait < 0) {
            self->routine += 2;
            own->wait = 0x3B;
            PlaySound(sfx_Gloop); // (the alpha's $DA: the prototype's was the fireball's $AE)
        }
        break;
    case 4: // jumping, and drifting to one side and then to the other
        Sst_MoveSprite2(self);
        self->x_vel += own->drift;
        self->y_vel += 0x18;
        if (self->y_vel == 0)
            own->drift = (int16_t)-own->drift;
        if (!((uint16_t)own->base_y > (uint16_t)self->y_pos.f.u)) {
            self->y_vel = own->jump;
            self->x_vel = 0;
            self->routine -= 2;
        }
        break;
    default: // 8: jumping, and hopping sideways at the top
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        if (self->y_vel == 0)
            self->x_pos.f.u = (int16_t)(own->hop + own->base_x);
        if (self->y_vel == 0x180) // (the pre-beta's sounds, the splashes of the hopping worm: on its way down, and as it lands)
            PlaySound(sfx_Gloop);
        if (!((uint16_t)own->base_y > (uint16_t)self->y_pos.f.u)) {
            self->y_vel = own->jump;
            self->x_pos.f.u = own->base_x;
            PlaySound(sfx_Gloop);
        }
        break;
    }
    SstOnScreenTest(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object 0C: the small floating platform (Obj0C, Nick Arcade's Chemical Plant platform, kept in the prototype's object table but with none in its levels): it bobs on a sine,
// and every 1024 frames it starts a longer, slower swing, by its subtype
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;  // 0x28: bits 0-3 how many times it pauses at the bottom of the swing
    uint8_t pad0[0x11];
    int16_t base_y;   // 0x3A
    uint8_t phase;    // 0x3C
    uint8_t swing;    // 0x3D
    uint8_t pauses;   // 0x3E
    uint8_t reload;   // 0x3F
} Own_FloatingPlatform;

void Obj_FloatingPlatform(Sst *self) {
    Own_FloatingPlatform *own = (Own_FloatingPlatform *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_FloatingPlatform;
        self->art_tile = TILE_MAP(1, 3, 0, 0, ARTTILE_CPZ_BLOCK);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        own->base_y = (int16_t)(self->y_pos.f.u - 0x10);
        own->pauses = own->reload = self->subtype & 0xF;
    }

    int16_t sin, cos;
    const uint8_t angle = own->phase;
    bool bob = true; // (loc_14AC0) the usual bob; the swing (loc_14A8E) is the other
    if (angle == 0) {
        if ((frame_count & 0x3FF) == 0) {
            own->swing = 1;
            own->phase++;
        }
    } else if (angle != 0x80) {
        own->phase++;
    } else {
        const uint8_t d1 = own->swing;
        if (d1 != 0 || (int8_t)--own->pauses >= 0) {
            own->swing++;
            CalcSine(d1, &sin, &cos);
            self->y_pos.f.u = (int16_t)(own->base_y + (((sin + 8) >> 6) - 0x10));
            bob = false;
        } else {
            own->pauses = own->reload;
            own->phase++;
        }
    }
    if (bob) {
        CalcSine(angle, &sin, &cos);
        self->y_pos.f.u = (int16_t)(own->base_y + ((cos + 8) >> 4));
    }
    FOR_EACH_CHARACTER(chr, who)
        Solid_Platform(self, chr, who, self->width_pixels, 9, self->x_pos.f.u);
    SstOnScreenTest(self);
}
