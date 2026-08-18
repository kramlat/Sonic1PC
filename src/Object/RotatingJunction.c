#include "RotatingJunction.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/RotatingJunction.h"

// Object 66 - rotating disc junction that grabs Sonic (SBZ)
//
// Spawns 2 extra "filler" objects (also this same type, routine 4 = just
// display) to cover up the gap between the small solid disc and its large
// circular sprite.

static const int8_t Jun_XYOffset[16][2] = {
    { -0x20,    0 }, { -0x1E,  0xE }, { -0x18, 0x18 }, {  -0xE, 0x1E },
    {     0, 0x20 }, {   0xE, 0x1E }, {  0x18, 0x18 }, {  0x1E,  0xE },
    {  0x20,    0 }, {  0x1E, -0xE }, {  0x18,-0x18 }, {   0xE,-0x1E },
    {     0,-0x20 }, {  -0xE,-0x1E }, { -0x18,-0x18 }, { -0x1E, -0xE },
};

static void Jun_Rotate(Object *obj, Scratch_Junction *scratch) {
    if (f_switch[scratch->switch_id] & 1) {
        if (!scratch->switchdown) {
            scratch->direction = (int8_t)-scratch->direction;
            scratch->switchdown = 1;
        }
    } else {
        scratch->switchdown = 0;
    }

    if (--obj->frame_time.b >= 0)
        return;
    obj->frame_time.b = 8 - 1;
    obj->frame = (uint8_t)((obj->frame + scratch->direction) & 0xF);
}

static void Jun_ChgPos(Object *obj) {
    const int8_t *off = Jun_XYOffset[obj->frame];
    player->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + off[0]);
    player->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + off[1]);
}

static void Jun_Action(Object *obj, Scratch_Junction *scratch) {
    Jun_Rotate(obj, scratch);

    if (!obj->render.f.on_screen) {
        RememberState(obj);
        return;
    }

    int16_t x_rad = (int16_t)(74 / 2 + 11 /* sonic_solid_width */);
    SolidObject(obj, (uint16_t)x_rad, x_rad, (uint16_t)(x_rad + 1), obj->pos.l.x.f.u, NULL, NULL);

    if (!obj->status.o.f.player_push) {
        RememberState(obj);
        return;
    }

    uint8_t check_frame = (player->pos.l.x.f.u < obj->pos.l.x.f.u) ? 0xE : 7;
    if (obj->frame != check_frame) {
        RememberState(obj);
        return;
    }

    scratch->grabframe = check_frame;
    obj->routine = 6; // advance to Jun_Inside
    lock_multi = 1; // lock Sonic's controls
    player->anim = SonAnimId_Roll;
    player->inertia = 0x800; // force fast ground speed for fast rolling animation
    player->xsp = 0;
    player->ysp = 0;
    obj->status.o.f.player_push = false;
    player->status.p.f.pushing = false;
    player->status.p.f.in_air = true;

    int16_t old_x = player->pos.l.x.f.u;
    int16_t old_y = player->pos.l.y.f.u;
    Jun_ChgPos(obj);
    // Average old and new position for a smooth in-between frame
    player->pos.l.x.f.u = (int16_t)((int16_t)(player->pos.l.x.f.u + old_x) >> 1);
    player->pos.l.y.f.u = (int16_t)((int16_t)(player->pos.l.y.f.u + old_y) >> 1);

    RememberState(obj);
}

static void Jun_Inside(Object *obj, Scratch_Junction *scratch) {
    uint8_t frame = obj->frame;

    if ((frame == 4 || frame == 7) && frame != scratch->grabframe) {
        player->xsp = 0;
        player->ysp = 0x800; // shoot Sonic down
        if (frame != 4) { // exited to the right, not the bottom
            player->xsp = 0x800;
            player->ysp = 0x800;
        }
        lock_multi = 0; // unlock Sonic's controls
        obj->routine = 2; // back to Jun_Action
    }

    Jun_Rotate(obj, scratch);
    Jun_ChgPos(obj);
    RememberState(obj);
}

static void Jun_Main(Object *obj, Scratch_Junction *scratch) {
    obj->routine = 2; // advance to Jun_Action

    Object *targets[3];
    int count = 0;
    targets[count++] = obj; // parent stays at routine 2

    for (int i = 0; i < 2; i++) {
        Object *filler = FindFreeObj();
        if (filler == NULL)
            continue;
        filler->type = ObjId_Junction;
        filler->routine = 4; // Jun_Display -- do nothing but display
        filler->pos.l.x.f.u = obj->pos.l.x.f.u;
        filler->pos.l.y.f.u = obj->pos.l.y.f.u;
        filler->priority = 3; // above parent
        filler->frame = 0x10; // large circular sprite
        targets[count++] = filler;
    }

    for (int i = 0; i < count; i++) {
        targets[i]->mappings = Mappings_RotatingJunction;
        targets[i]->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Junction); // | Tile_Pal3
        targets[i]->render.f.align_fg = true;
        targets[i]->width_pixels = 112 / 2;
    }

    obj->width_pixels = 96 / 2; // parent's own narrower solidity/display width
    obj->priority = 4;
    scratch->direction = 1; // default rotation: clockwise
    scratch->switch_id = obj->scratch.u8[0];

    Jun_Action(obj, scratch);
}

void Obj_RotatingJunction(Object *obj) {
    Scratch_Junction *scratch = (Scratch_Junction *)&obj->scratch;

    switch (obj->routine) {
    case 0: Jun_Main(obj, scratch); break;
    case 2: Jun_Action(obj, scratch); break;
    case 4: RememberState(obj); break;
    case 6: Jun_Inside(obj, scratch); break;
    }
}
