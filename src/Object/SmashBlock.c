#include "SmashBlock.h"

#include "Level.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 51 - smashable green block (MZ)

static const int16_t smab_speeds[4][2] = {
    { -0x200, -0x200 },
    { -0x100, -0x100 },
    { 0x200, -0x200 },
    { 0x100, -0x100 },
};

// Points per smashed block (combo chain), /10 -- 16th and later blocks
// are hardcoded to 10000 regardless of this table.
static const uint16_t smab_scores[4] = { 10, 20, 50, 100 };

void Obj_SmashBlock(Object *obj) {
    Scratch_SmashBlock *scratch = (Scratch_SmashBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_SmashableGreenBlock;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x2B8); // ArtTile_MZ_Block | Tile_Pal3
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 4;
        break;
    case 2: { // Solid
        scratch->combo = item_bonus;
        scratch->sonic_anim = player->anim;

        SolidObject(obj, 32 / 2 + 11 /* sonic_solid_width */, 32 / 2, 34 / 2, obj->pos.l.x.f.u, NULL, NULL);
        if (!obj->status.o.f.player_stand) {
            RememberState(obj);
            break;
        }
        if (scratch->sonic_anim != SonAnimId_Roll) {
            RememberState(obj);
            break;
        }

        item_bonus = scratch->combo;

        player->status.p.f.in_ball = true;
        player->y_rad = 28 / 2; // sonic_roll_height
        player->x_rad = 14 / 2; // sonic_roll_width
        player->anim = SonAnimId_Roll;
        player->ysp = -0x300;
        player->status.p.f.in_air = true;
        player->status.p.f.object_stand = false;
        player->routine = 2; // force Sonic to Sonic_Control
        obj->status.o.f.player_stand = false;

        // Two mapping frames exist -- 2-piece (normal) and 4-piece
        // (fragmentation effect) -- swap to the 4-piece one right before
        // smashing.
        obj->frame = 1;

        SmashObject(obj, 4, &smab_speeds[0][0]);

        Object *points = FindFreeObj();
        if (points != NULL) {
            points->type = ObjId_Points;
            points->pos.l.x.f.u = obj->pos.l.x.f.u;
            points->pos.l.y.f.u = obj->pos.l.y.f.u;

            uint16_t combo = item_bonus;
            item_bonus += 2; // word-based combo table below, so +2 per hit
            if (combo >= 3 * 2)
                combo = 3 * 2;

            uint16_t score_x10 = smab_scores[combo / 2];
            uint8_t frame = (uint8_t)(combo / 2);
            if (item_bonus >= 16 * 2) {
                score_x10 = 1000;
                frame = 5;
            }
            AddPoints(score_x10 * 10);
            points->frame = frame;
        }
        __attribute__((fallthrough)); // parent object is now the first fragment (routine 4)
    }
    case 4: // Fragment
        SpeedToPos(obj);
        obj->ysp += 0x38; // gravity

        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}
