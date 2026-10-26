// Sand Shower's level misc code, which runs with the dynamic level events: some of its blocks are sand wherever the level has them (they have no collision of their own), in three kinds:
//  - quicksand (blocks 168 to 181): a character whose feet are in one is in it: no floor under him, sinking slowly, slowed in the way he goes, and jumping (any of A, B or C) takes him up with the sand's
//    resistance. When he has sunk $30 pixels below the top of the column of sand he is in, it has him.
//  - sand falls (sand_falls below): the same sand, flowing: it carries a character down, or up with the block's Y flip, and does not kill.
//  - sand that holds (sand_holds below): horizontal falls: a character in one sticks to it, carried along (to the right, to the left with the block's X flip: a Y flip does not turn a flow that goes sideways), and a jump
//    takes him out of it at the strength of a jump in the air.
// The flips are the chunk's (the block as the chunk places it). The same for Sonic and for Tails; nothing here is an object
#include "SandShower.h"
#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#define SAND_FIRST_BLOCK 168
#define SAND_LAST_BLOCK 181
#define SAND_DEPTH 0x30
#define SAND_SINK_SPEED 0x40 // (a quarter of a pixel a frame: $30 pixels take about 3 seconds)
#define SAND_JUMP_SPEED 0x410 // (5/8 of a jump's $680)

#define FLOW_SPEED 0x180 // (a pixel and a half a frame, the way a sand fall carries a character)
#define HOLD_SPEED 0x200 // (two pixels a frame, the way sand that holds carries him)
#define JUMP_SPEED 0x680

static uint16_t SandWord(int16_t x, int16_t y) {
    const uint8_t *tile = FindNearestTile(NULL, x, y);
    return (uint16_t)((tile[0] << 8) | tile[1]);
}

static bool InRanges(int block, const uint8_t *ranges, int count) { // pairs, from and to
    for (int i = 0; i < count; i++)
        if (block >= ranges[i * 2] && block <= ranges[i * 2 + 1])
            return true;
    return false;
}

// The sand falls (the user's lists): $51-$54, $79, $7A, $7C-$7F, $96, $97, $A1, $A2, $A4
static bool IsFall(int block) {
    static const uint8_t ranges[][2] = { { 0x51, 0x54 }, { 0x79, 0x7A }, { 0x7C, 0x7F }, { 0x96, 0x97 }, { 0xA1, 0xA2 }, { 0xA4, 0xA4 } };
    return InRanges(block, &ranges[0][0], (int)(sizeof(ranges) / sizeof(ranges[0])));
}

// The sand that holds, horizontal falls: $98-$9D, $9F, $A0, $A3
static bool IsHold(int block) {
    static const uint8_t ranges[][2] = { { 0x98, 0x9D }, { 0x9F, 0xA0 }, { 0xA3, 0xA3 } };
    return InRanges(block, &ranges[0][0], (int)(sizeof(ranges) / sizeof(ranges[0])));
}

static bool IsSand(int16_t x, int16_t y) {
    const int block = SandWord(x, y) & META_TILE;
    return block >= SAND_FIRST_BLOCK && block <= SAND_LAST_BLOCK;
}

// The top of the column of sand that the point (x, y), which is in it, is in
static int16_t SandTop(int16_t x, int16_t y) {
    int16_t row = (int16_t)(y & ~0xF);
    for (int i = 0; i < 12 && IsSand(x, (int16_t)(row - 0x10)); i++)
        row = (int16_t)(row - 0x10);
    return row;
}

// A jump from inside sand, started by hand (the character is in the air, where his own jump does not run): `speed` up, rolled up
static void SandJump(Object *chr, bool sonic, int16_t speed) {
    chr->ysp = (int16_t)-speed;
    PlaySound(sfx_Jump);
    if (!chr->status.p.f.in_ball) {
        chr->y_rad = sonic ? SONIC_BALL_HEIGHT : 0xE;
        chr->x_rad = sonic ? SONIC_BALL_WIDTH : 7;
        chr->anim = SonAnimId_Roll;
        chr->status.p.f.in_ball = true;
        chr->pos.l.y.f.u = (int16_t)(chr->pos.l.y.f.u + 5);
    }
}

static void SandCharacter(Object *chr, uint8_t pressed, bool sonic) {
    if (chr->routine != 2 || OBJ_CONTROL(chr) != 0 || debug_use)
        return;
    const bool jump = (pressed & (JPAD_A | JPAD_B | JPAD_C)) != 0;
    const int16_t feet = (int16_t)(chr->pos.l.y.f.u + chr->y_rad);
    if (IsSand(chr->pos.l.x.f.u, feet)) {
        if (feet - SandTop(chr->pos.l.x.f.u, feet) >= SAND_DEPTH) {
            if (sonic)
                KillSonic(chr, chr);
            else
                KillTails(chr);
            return;
        }
        if (!chr->status.p.f.in_air)
            return; // (what is under him is the level's own floor: the bottom of the sand, or a wall's foot)

        if (chr->ysp > SAND_SINK_SPEED)
            chr->ysp = SAND_SINK_SPEED;
        else if (chr->ysp < 0)
            chr->ysp = (int16_t)(chr->ysp - (chr->ysp >> 5)); // (the sand slows him as he rises)
        chr->xsp = (int16_t)(chr->xsp - (chr->xsp >> 4));
        if (jump)
            SandJump(chr, sonic, SAND_JUMP_SPEED);
        return;
    }

    if (!chr->status.p.f.in_air)
        return;
    const uint16_t word = SandWord(chr->pos.l.x.f.u, chr->pos.l.y.f.u);
    const int block = word & META_TILE;
    const bool x_flip = (word & META_X_FLIP) != 0, y_flip = (word & META_Y_FLIP) != 0;
    if (IsFall(block)) { // a fall: carried down (up if the block is flipped), with the sand's drag, and not fatal
        const int16_t target = y_flip ? -FLOW_SPEED : FLOW_SPEED;
        chr->ysp = (int16_t)(chr->ysp + ((target - chr->ysp) >> 3));
        chr->xsp = (int16_t)(chr->xsp - (chr->xsp >> 4));
        if (jump)
            SandJump(chr, sonic, SAND_JUMP_SPEED);
    } else if (IsHold(block)) { // sand that holds: he sticks to it and is carried sideways, until he jumps out
        if (jump) {
            SandJump(chr, sonic, JUMP_SPEED);
            return;
        }
        if (chr->ysp < 0)
            return; // (on his way out of it)
        chr->ysp = 0;
        chr->xsp = x_flip ? -HOLD_SPEED : HOLD_SPEED;
    }
}

void SandShower_LevelMisc(void) {
    SandCharacter(player, jpad1_press2, true);
    if (TAILS_OBJ->type != 0)
        SandCharacter(TAILS_OBJ, jpad2_press, false);
}
