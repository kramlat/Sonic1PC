#include "SmashWall.h"

#include "Level.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 3C - smashable wall (GHZ, SLZ)

// Fragment X/Y speeds, indexed [piece][0]=xsp [piece][1]=ysp
static const int16_t frag_speed_right[8][2] = {
    { 0x400, -0x500 }, { 0x600, -0x100 }, { 0x600, 0x100 }, { 0x400, 0x500 },
    { 0x600, -0x600 }, { 0x800, -0x200 }, { 0x800, 0x200 }, { 0x600, 0x600 },
};
static const int16_t frag_speed_left[8][2] = {
    { -0x600, -0x600 }, { -0x800, -0x200 }, { -0x800, 0x200 }, { -0x600, 0x600 },
    { -0x400, -0x500 }, { -0x600, -0x100 }, { -0x600, 0x100 }, { -0x400, 0x500 },
};

void Obj_SmashWall(Object *obj) {
    Scratch_SmashWall *scratch = (Scratch_SmashWall *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_SmashableWalls;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_SLZ_Smashable_Wall); // | Tile_Pal3
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 4;
        obj->frame = obj->scratch.u8[0]; // subtype -- 0=left, 1=middle, 2=right
        // Fallthrough
    case 2: { // Solid
        scratch->speed = player->xsp;

        SolidObject(obj, 32 / 2 + 11 /* sonic_solid_width */, 64 / 2, 64 / 2, obj->pos.l.x.f.u, NULL, NULL);
        if (!obj->status.o.f.player_push) {
            RememberState(obj);
            break;
        }

        if (player->anim != SonAnimId_Roll) {
            RememberState(obj);
            break;
        }

        int16_t impact_speed = scratch->speed < 0 ? (int16_t)-scratch->speed : scratch->speed;
        if (impact_speed < 0x480) {
            RememberState(obj);
            break;
        }

        player->xsp = scratch->speed; // restore Sonic's speed from before SolidObject changed it
        const int16_t(*frag_speeds)[2];
        if (obj->pos.l.x.f.u < player->pos.l.x.f.u) {
            player->pos.l.x.f.u += 4; // push Sonic right a bit for pseudo-seamless movement
            frag_speeds = frag_speed_right;
        } else {
            player->pos.l.x.f.u -= 4 * 2; // push Sonic left a bit (and undo the +4 above)
            frag_speeds = frag_speed_left;
        }

        player->inertia = player->xsp;
        obj->status.o.f.player_push = false;
        player->status.p.f.pushing = false;

        SmashObject(obj, 8, &frag_speeds[0][0]);
        __attribute__((fallthrough)); // parent object is now the first fragment -- routine 4
    }
    case 4: // Fragment
        SpeedToPos(obj);
        obj->ysp += 0x38 * 2; // double gravity

        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}
