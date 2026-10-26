// Sand Shower's level misc code, which runs with the dynamic level events: the blocks 168 to 181 are quicksand wherever the level has them (they have no collision of their own), so a character whose feet are
// in one of them is in it: no floor under him, sinking slowly, slowed in the way he goes, and jumping (any of A, B or C) takes him up with the sand's resistance. When he has sunk $30 pixels below the top
// of the column of sand he is in, it has him. The same for Sonic and for Tails; nothing here is an object
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

static bool IsSand(int16_t x, int16_t y) {
    const uint8_t *tile = FindNearestTile(NULL, x, y);
    const uint16_t block = (uint16_t)(((tile[0] << 8) | tile[1]) & META_TILE);
    return block >= SAND_FIRST_BLOCK && block <= SAND_LAST_BLOCK;
}

// The top of the column of sand that the point (x, y), which is in it, is in
static int16_t SandTop(int16_t x, int16_t y) {
    int16_t row = (int16_t)(y & ~0xF);
    for (int i = 0; i < 12 && IsSand(x, (int16_t)(row - 0x10)); i++)
        row = (int16_t)(row - 0x10);
    return row;
}

static void SandCharacter(Object *chr, uint8_t pressed, bool sonic) {
    if (chr->routine != 2 || OBJ_CONTROL(chr) != 0 || debug_use)
        return;
    const int16_t feet = (int16_t)(chr->pos.l.y.f.u + chr->y_rad);
    if (!IsSand(chr->pos.l.x.f.u, feet))
        return;
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

    if (pressed & (JPAD_A | JPAD_B | JPAD_C)) { // a jump, the way up
        chr->ysp = -SAND_JUMP_SPEED;
        PlaySound(sfx_Jump);
        if (!chr->status.p.f.in_ball) {
            chr->y_rad = sonic ? SONIC_BALL_HEIGHT : 0xE;
            chr->x_rad = sonic ? SONIC_BALL_WIDTH : 7;
            chr->anim = SonAnimId_Roll;
            chr->status.p.f.in_ball = true;
            chr->pos.l.y.f.u = (int16_t)(chr->pos.l.y.f.u + 5);
        }
    }
}

void SandShower_LevelMisc(void) {
    SandCharacter(player, jpad1_press2, true);
    if (TAILS_OBJ->type != 0)
        SandCharacter(TAILS_OBJ, jpad2_press, false);
}
