// Casino Night's slot machine of the Sonic 2 pre-beta (September 14, 1992): the starter (object $D6, Data/Objects/Obj_0xD6.asm) and the reels (Process_Slot_Machine, S2140992.asm at $026810 to $026F10).
// The machine is a state machine of $FF4E (here `m`, 18 bytes, the pre-beta's own layout so that the translation stays line for line):
//   m[0] state (0 init, 4 first draw, $38 resting, 8 start ... $34 last), m[1] how many full drawings to wait, m[2] a wait in frames, m[3] which reel is drawn next (0, 4, 8),
//   m[4], m[5] the stops the spin was set up for (the first reel, then the second and third as nibbles) and, when it ends, the reward word,
//   words at 6, 8, A, C, E and $10: the three reels' positions and speeds.
#include "Object/CNZSlots.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Video.h"
#include "Backend/VDP.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZSlotStarter.h"
#include "Resource/Art/CNZSlotCheck.h"
#include "Resource/Mappings/CNZSlotStarter.h"

void Obj_Points(Sst *self);

#define ArtTile_CNZSlotStarter 0x388 // ($7100: the zone's list)

static uint8_t m[0x12];

static uint16_t W(int off) { return (uint16_t)((m[off] << 8) | m[off + 1]); }
static void SetW(int off, uint16_t v) {
    m[off] = (uint8_t)(v >> 8);
    m[off + 1] = (uint8_t)v;
}

void SlotMachine_Reset(void) {
    memset(m, 0, sizeof(m));
}

uint8_t SlotMachine_State(void) {
    return m[0];
}

void SlotMachine_Start(void) { // (the starter: a character was taken, and the machine was resting)
    if (m[0] == 0x38)
        m[0] = 0x08;
}

uint16_t SlotMachine_Result(void) {
    return W(4);
}

// the symbols of each reel's strips (Offset_0x026EFF, 026F07 and 026F0F)
static const uint8_t reel_symbols[3][8] = {
    { 3, 0, 1, 4, 2, 5, 4, 1 },
    { 3, 0, 1, 4, 2, 5, 0, 2 },
    { 3, 0, 1, 4, 2, 5, 4, 1 },
};
// Offset_0x026EEA: the stops a spin is set up for, by the frame counter: it counts down the thresholds; 0F, FF is "any"
static const uint8_t spin_choices[7][3] = { { 0x08, 0x03, 0x33 }, { 0x12, 0x00, 0x00 }, { 0x12, 0x01, 0x11 }, { 0x20, 0x02, 0x22 }, { 0x20, 0x04, 0x44 }, { 0x28, 0x05, 0x55 }, { 0xFF, 0x0F, 0xFF } };
// Offset_0x026EDE: the reward for three of each symbol (BCD; $FFFF is the jackpot)
static const uint16_t rewards[6] = { 0x0030, 0x0025, 0xFFFF, 0x0150, 0x0010, 0x0020 };

static uint8_t frame_lo(void) {
    return (uint8_t)frame_count;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The drawing of a reel (Offset_0x026C76): one reel for each call, in turn; its strip is made into 16 tiles of the 4 columns of the symbols that are at its position
// ---------------------------------------------------------------------------------------------------------------------------------------
static const uint8_t *StripSource(const uint8_t *table, uint16_t d3) { // Offset_0x026D68
    const unsigned symbol = table[(d3 >> 8) & 7] & 7;
    return Art_CNZSlotCheck + (symbol << 9) + ((d3 & 0xF8) >> 1);
}

static void DrawReel(int pos_off, int speed_off, const uint8_t *table, uint16_t vram) {
    const uint16_t old = W(pos_off);
    SetW(pos_off, (uint16_t)(old - W(speed_off)));
    uint16_t d3 = W(pos_off) & 0x7F8;
    if (d3 == (old & 0x7F8))
        return;
    static uint8_t buffer[0x200];
    const uint8_t *src = StripSource(table, d3);
    for (int i = 0; i < 0x20; i++) {
        for (int k = 1; k < 4; k++)
            memcpy(&buffer[k * 0x80 + i * 4], src + k * 0x80, 4);
        memcpy(&buffer[i * 4], src, 4);
        src += 4;
        d3 = (uint16_t)((d3 & 0xFF00) | ((d3 + 8) & 0xFF));
        if ((d3 & 0xFF) == 0) {
            d3 = (uint16_t)((d3 + 0x100) & 0x700);
            src = StripSource(table, d3);
        }
    }
    VDP_SeekVRAM(vram);
    VDP_WriteVRAM(buffer, sizeof(buffer));
}

static void Draw(void) {
    switch (m[3]) {
    case 0:
        m[3] += 4;
        DrawReel(6, 8, reel_symbols[0], 0xAA00);
        break;
    case 4:
        m[3] += 4;
        DrawReel(0xA, 0xC, reel_symbols[1], 0xAC00);
        break;
    default: // 8
        m[3] = 0;
        m[1]--;
        DrawReel(0xE, 0x10, reel_symbols[2], 0xAE00);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The result (Offset_0x026D94 and what it calls): three of a kind pay by the symbol, a symbol 3 (the wild one) with two others pays on, and fives alone are counted
// ---------------------------------------------------------------------------------------------------------------------------------------
static uint16_t Bcd(uint16_t v) { // (three digits)
    return (uint16_t)(((v / 100) << 8) | (((v / 10) % 10) << 4) | (v % 10));
}

static uint16_t Unbcd(uint16_t b) {
    return (uint16_t)(((b >> 8) & 0xF) * 100 + ((b >> 4) & 0xF) * 10 + (b & 0xF));
}

static uint16_t Reward(unsigned symbol) { // Offset_0x026E80
    return rewards[symbol];
}

static uint16_t Times4(uint16_t d0) { // Offset_0x026E8E
    if ((int16_t)d0 < 0)
        return (uint16_t)(d0 << 2);
    return Bcd((uint16_t)(Unbcd(d0) * 4));
}

static uint16_t Times2(uint16_t d0) { // Offset_0x026EB2
    if ((int16_t)d0 < 0)
        return (uint16_t)(d0 + d0);
    return Bcd((uint16_t)(Unbcd(d0) * 2));
}

static void Evaluate(void) {
    const uint8_t d2 = (m[5] & 0xF0) >> 4, d3 = m[5] & 0xF; // (the second and third reels)
    const uint8_t first = m[4];
    int match = 0;
    if (d2 == first)
        match += 4;
    if (d3 == first)
        match += 8;
    uint16_t result;

    if (match == 12) { // all three the same
        SetW(4, Reward(d2));
        return;
    }
    if (match == 8) { // the first and the third
        if (d3 == 3) {
            SetW(4, Times4(Reward(d2)));
            return;
        }
        if (d2 == 3) {
            SetW(4, Times2(Reward(d3)));
            return;
        }
    } else if (match == 4) { // the first and the second
        if (d2 == 3) {
            SetW(4, Times4(Reward(d3)));
            return;
        }
        if (d3 == 3) {
            SetW(4, Times2(Reward(d2)));
            return;
        }
    }
    // Offset_0x026E2A: the second and the third
    if (d2 == d3) {
        if (first == 3) {
            SetW(4, Times2(Reward(d2)));
            return;
        }
        if (d2 == 3) {
            SetW(4, Times4(Reward(first & 0xF)));
            return;
        }
    }
    // Offset_0x026E5E: the fives
    result = 0;
    if (first == 5)
        result += 2;
    if (d2 == 5)
        result += 2;
    if (d3 == 5)
        result += 2;
    SetW(4, result);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The machine (Process_Slot_Machine)
// ---------------------------------------------------------------------------------------------------------------------------------------
// the slowing and the stop of a reel (the same code for the three, with their own words): true when the reel is to stop at a symbol
static bool Slowing(int pos_off, int speed_off, uint8_t pos_low_limit) {
    if ((int16_t)W(speed_off) <= 0x10) {
        const uint16_t d0 = (uint16_t)((W(pos_off) + 0x10) & 0xF0);
        return d0 == 0;
    }
    SetW(speed_off, (uint16_t)(W(speed_off) - 2));
    if ((int16_t)W(speed_off) > 0xE && m[pos_off + 1] <= pos_low_limit)
        SetW(speed_off, (uint16_t)(W(speed_off) - 1));
    return false;
}

static uint16_t StopPosition(int pos_off, bool shifted) { // (what the strip is at when the reel stops: the symbol it is on, and where in it)
    const uint16_t d0 = (uint16_t)(W(pos_off) + 0x80);
    uint16_t d1;
    if (shifted) // (the third reel's: the pre-beta shifts the sum down first, so that nothing is left of it)
        d1 = (uint16_t)(((d0 >> 8) & 0x700) - 0x10);
    else
        d1 = (uint16_t)((d0 & 0x700) - 0x10);
    SetW(pos_off, d1);
    return d0;
}

void SlotMachine_Process(void) {
    switch (m[0]) {
    case 0x00:
        memset(m, 0, 6);
        for (int off = 6; off < 0x12; off += 2)
            SetW(off, 0x0080);
        m[1] = 1;
        m[0] += 4;
        break;

    case 0x04:
        Draw();
        if (m[1] != 0)
            break;
        m[0] = 0x38;
        SetW(8, 0);
        SetW(0xC, 0);
        SetW(0x10, 0);
        break;

    case 0x08: { // it is set going: the speeds, and where the spin is to end
        SetW(8, (uint16_t)((frame_lo() & 7) - 4 + 0x20));
        SetW(0xC, (uint16_t)(((uint8_t)((frame_lo() << 4) | (frame_lo() >> 4)) & 7) - 4 + 0x20));
        SetW(0x10, (uint16_t)(((frame_count >> 8) & 7) - 4 + 0x20)); // (the pre-beta stores a register that nothing set here; this is what it meant)
        m[1] = 2;
        m[3] = 0;
        int d0 = frame_lo();
        const uint8_t *choice = spin_choices[0];
        for (int i = 0; i < 7; i++) {
            choice = spin_choices[i];
            if (d0 < choice[0])
                break;
            d0 -= choice[0];
        }
        m[4] = choice[1];
        m[5] = choice[2];
        m[0] += 4;
        break;
    }

    case 0x0C:
        Draw();
        if (m[1] != 0)
            break;
        SetW(8, (uint16_t)(W(8) + 0x20));
        SetW(0xC, (uint16_t)(W(0xC) + 0x20));
        SetW(0x10, (uint16_t)(W(0x10) + 0x20));
        m[1] = (uint8_t)((frame_lo() & 0xF) + 0x20);
        m[2] = 0;
        m[0] += 4;
        break;

    case 0x10: { // the first reel is left to spin until it can be stopped at the symbol it was set for
        Draw();
        bool go;
        if (m[4] == 0x0F) {
            go = m[1] == 0;
        } else {
            m[2]++;
            if (m[2] <= 0x60)
                break;
            const unsigned d0 = ((W(6) >> 8) - 3) & 7;
            go = reel_symbols[0][d0] == m[4];
        }
        if (go) {
            m[0] += 4;
            SetW(8, 0x40);
        }
        break;
    }

    case 0x14: // it slows, and stops on a symbol
        Draw();
        if (m[3] != 0)
            break;
        if (!Slowing(6, 8, 0x60))
            break;
        {
            const uint16_t d0 = StopPosition(6, false);
            m[4] = reel_symbols[0][(d0 >> 8) & 7];
            SetW(8, 0xFFF8);
            m[1] = 2;
            SetW(0xC, 0x40);
            if (m[5] & 0x80) {
                m[2] = (uint8_t)((frame_lo() & 0xF) + 0x12);
                m[0] += 4;
            } else {
                m[0] += 8;
            }
        }
        break;

    case 0x18:
        Draw();
        if (m[1] == 0)
            SetW(8, 0);
        if (--m[2] == 0)
            m[0] += 8;
        break;

    case 0x1C: {
        Draw();
        if (m[1] == 0)
            SetW(8, 0);
        if (m[3] != 4 || W(8) != 0)
            break;
        const uint8_t d1 = (m[5] & 0xF0) >> 4;
        const unsigned d0 = ((W(0xA) >> 8) - 3) & 7;
        if (reel_symbols[1][d0] == d1)
            m[0] += 4;
        break;
    }

    case 0x20:
        Draw();
        if (m[3] != 4)
            break;
        if (!Slowing(0xA, 0xC, 0x70))
            break;
        {
            const uint16_t d0 = StopPosition(0xA, false);
            const uint8_t symbol = reel_symbols[1][(d0 >> 8) & 7];
            m[5] = (uint8_t)((m[5] & 0x0F) | (symbol << 4));
            SetW(0xC, 0xFFF8);
            m[1] = 2;
            SetW(0x10, 0x40);
            if ((m[5] & 0xF) == 0xF) {
                m[2] = (uint8_t)((frame_lo() & 0xF) + 0x12);
                m[0] += 4;
            } else {
                m[0] += 8;
            }
        }
        break;

    case 0x24:
        Draw();
        if (m[1] == 0)
            SetW(0xC, 0);
        if (--m[2] == 0)
            m[0] += 8;
        break;

    case 0x28: {
        Draw();
        if (m[1] == 0)
            SetW(0xC, 0);
        if (m[3] != 8 || W(0xC) != 0)
            break;
        const uint8_t d1 = m[5] & 0xF;
        const unsigned d0 = ((W(0xE) >> 8) - 3) & 7;
        if (reel_symbols[2][d0] == d1)
            m[0] += 4;
        break;
    }

    case 0x2C:
        Draw();
        if (m[3] != 8)
            break;
        if (!Slowing(0xE, 0x10, 0x70))
            break;
        {
            const uint16_t d0 = (uint16_t)((W(0xE) + 0x80) >> 8); // (shifted before it is used: see StopPosition)
            SetW(0xE, (uint16_t)((d0 & 0x700) - 0x10));
            m[5] = (uint8_t)((m[5] & 0xF0) | reel_symbols[2][d0 & 7]);
            SetW(0x10, 0xFFF8);
            m[1] = 6;
            m[0] += 4;
        }
        break;

    case 0x30:
        Draw();
        if (--m[1] == 0) {
            SetW(0x10, 0);
            m[0] += 4;
        }
        break;

    case 0x34:
        Draw();
        if (m[1] != 0)
            break;
        Evaluate();
        m[0] = 0x38;
        break;

    default: // $38: resting
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D6: the starter
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    uint16_t state[2]; // 0x30, 0x34: for Sonic and for Tails: 0 waiting, 2 held, 4 let go (the next word of each is its timer)
    uint16_t timer[2]; // 0x32, 0x36
} Own_SlotStarter;

static void SlotStarter_Character(Sst *self, Own_SlotStarter *own, Sst *chr, int who) {
    const int32_t side = Solid_Character(self, chr, who, 0x23, 0x10, 0x11, self->x_pos.f.u, NULL);
    switch (own->state[who]) {
    case 0:
        if (side != -1)
            return;
        chr->x_pos.f.u = self->x_pos.f.u;
        chr->y_pos.f.u = self->y_pos.f.u;
        chr->x_vel = 0;
        chr->y_vel = 0;
        chr->ground_vel = 0;
        OBJ_CONTROL(chr) = 0x81;
        chr->status |= SST_PSTATUS_IN_BALL;
        chr->y_radius = 0xE;
        chr->x_radius = 7;
        chr->anim = SonAnimId_Roll;
        self->anim = 1;
        own->state[who] = 2;
        own->timer[who] = 0x78;
        SlotMachine_Start();
        break;

    case 2:
        if ((int16_t)--own->timer[who] >= 0) {
            if ((own->timer[who] & 7) == 0) {
                AddPoints(0xA);
                Sst *points = Sst_SpawnCode(Obj_Points, self->x_pos.f.u, self->y_pos.f.u);
                if (points != NULL)
                    points->mapping_frame = 0;
            }
            break;
        }
        self->anim = 0;
        OBJ_CONTROL(chr) = 0;
        self->status &= (uint8_t)~(1 << (3 + who));
        chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        chr->status |= SST_PSTATUS_IN_AIR;
        chr->y_vel = 0x400;
        own->state[who] = 4;
        own->timer[who] = 0x3C;
        break;

    default: // 4: a rest before it takes anyone again
        if ((int16_t)--own->timer[who] < 0)
            own->state[who] = 0;
        break;
    }
}

void Obj_CNZSlotStarter(Sst *self) {
    Own_SlotStarter *own = (Own_SlotStarter *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZSlotStarter;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZSlotStarter);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x18;
        self->priority = 1 * 0x80;
    }

    SlotStarter_Character(self, own, player, SolidChar_Sonic);
    if (TAILS_OBJ->code != NULL)
        SlotStarter_Character(self, own, TAILS_OBJ, SolidChar_Tails);
    Sst_Animate(self, Animation_CNZSlotStarter);
    SstOnScreenTest(self);
}
