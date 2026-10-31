#include "RotatingJunction.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
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

static void Jun_Rotate(Sst *self, Own_Junction *own) {
    if (f_switch[own->switch_id] & 1) {
        if (!own->switchdown) {
            own->direction = (int8_t)-own->direction;
            own->switchdown = 1;
        }
    } else {
        own->switchdown = 0;
    }

    if (--self->anim_frame_timer >= 0)
        return;
    self->anim_frame_timer = 8 - 1;
    self->mapping_frame = (uint8_t)((self->mapping_frame + own->direction) & 0xF);
}

static void Jun_ChgPos(Sst *self) {
    const int8_t *off = Jun_XYOffset[self->mapping_frame];
    player->x_pos.f.u = (int16_t)(self->x_pos.f.u + off[0]);
    player->y_pos.f.u = (int16_t)(self->y_pos.f.u + off[1]);
}

static void Jun_Action(Sst *self, Own_Junction *own) {
    Jun_Rotate(self, own);

    if (SstOutOfRange(self->x_pos.f.u)) {
        SstOnScreenTest(self);
        return;
    }

    int16_t x_rad = (int16_t)(74 / 2 + 11 /* sonic_solid_width */);
    SstSolidObject(self, (uint16_t)x_rad, x_rad, (uint16_t)(x_rad + 1), self->x_pos.f.u, NULL, NULL);

    if (!((self->status & SST_STATUS_PLAYER_PUSH) != 0)) {
        SstOnScreenTest(self);
        return;
    }

    uint8_t check_frame = (player->x_pos.f.u < self->x_pos.f.u) ? 0xE : 7;
    if (self->mapping_frame != check_frame) {
        SstOnScreenTest(self);
        return;
    }

    own->grabframe = check_frame;
    self->routine = 6; // advance to Jun_Inside
    lock_multi = 1; // lock Sonic's controls
    player->anim = SonAnimId_Roll;
    player->ground_vel = 0x800; // force fast ground speed for fast rolling animation
    player->x_vel = 0;
    player->y_vel = 0;
    self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
    player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    player->status |= SST_PSTATUS_IN_AIR;

    int16_t old_x = player->x_pos.f.u;
    int16_t old_y = player->y_pos.f.u;
    Jun_ChgPos(self);
    // Average old and new position for a smooth in-between frame
    player->x_pos.f.u = (int16_t)((int16_t)(player->x_pos.f.u + old_x) >> 1);
    player->y_pos.f.u = (int16_t)((int16_t)(player->y_pos.f.u + old_y) >> 1);

    SstOnScreenTest(self);
}

static void Jun_Inside(Sst *self, Own_Junction *own) {
    uint8_t frame = self->mapping_frame;

    if ((frame == 4 || frame == 7) && frame != own->grabframe) {
        player->x_vel = 0;
        player->y_vel = 0x800; // shoot Sonic down
        if (frame != 4) { // exited to the right, not the bottom
            player->x_vel = 0x800;
            player->y_vel = 0x800;
        }
        lock_multi = 0; // unlock Sonic's controls
        self->routine = 2; // back to Jun_Action
    }

    Jun_Rotate(self, own);
    Jun_ChgPos(self);
    SstOnScreenTest(self);
}

static void Jun_Main(Sst *self, Own_Junction *own) {
    self->routine = 2; // advance to Jun_Action

    Sst *targets[3];
    int count = 0;
    targets[count++] = self; // parent stays at routine 2

    for (int i = 0; i < 2; i++) {
        Sst *filler = Sst_Allocate();
        if (filler == NULL)
            continue;
        filler->code = Obj_RotatingJunction;
        filler->routine = 4; // Jun_Display -- do nothing but display
        filler->x_pos.f.u = self->x_pos.f.u;
        filler->y_pos.f.u = self->y_pos.f.u;
        filler->priority = 3 * 0x80; // above parent
        filler->mapping_frame = 0x10; // large circular sprite
        targets[count++] = filler;
    }

    for (int i = 0; i < count; i++) {
        targets[i]->mappings = Mappings_RotatingJunction;
        targets[i]->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Junction); // | Tile_Pal3
        targets[i]->render_flags |= SST_RENDER_LEVEL;
        targets[i]->width_pixels = 112 / 2;
    }

    self->width_pixels = 96 / 2; // parent's own narrower solidity/display width
    self->priority = 4 * 0x80;
    own->direction = 1; // default rotation: clockwise
    own->switch_id = self->subtype;

    Jun_Action(self, own);
}

void Obj_RotatingJunction(Sst *self) {
    Own_Junction *own = (Own_Junction *)self->own.u8;

    switch (self->routine) {
    case 0: Jun_Main(self, own); break;
    case 2: Jun_Action(self, own); break;
    case 4: SstOnScreenTest(self); break;
    case 6: Jun_Inside(self, own); break;
    }
}
