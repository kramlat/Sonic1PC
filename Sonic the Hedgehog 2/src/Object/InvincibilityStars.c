// The invincibility stars of the Sonic 2 pre-beta (September 14, 1992: its object $35, Data/Objects/Obj_0x35.asm). One object that is Sonic's place and draws eight stars as sub-sprites: six that
// go round him (their places, by his angle, the way he faces and whether the frame is even, from tables that step on 6 entries of 3 bytes a frame) and two that trail behind him (a table of their own, stepping
// on 6 bytes). The data is the importer's (tools/import_s2prebeta_stars.pl). The object goes when Sonic is not invincible.
#include "Object/InvincibilityStars.h"
#include "Constants.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Level.h"
#include "Object/Sonic.h"
#include "Macros.h"

#include "Object/InvincibilityStarsData.h"
#include "Resource/Mappings/InvincibilityStars.h"

typedef struct {
    uint8_t orbit; // the step in the six stars' tables (0 to $72, by 6)
    uint8_t trail; // the step in the trailing pair's (0 to $9C, by 6)
} Own_InvincibilityStars;

// The five bytes of a star in the orbit tables are three: its frame, its x and its y
static void Stars_Put(Sst *self, int index, const int8_t *entry, int16_t x, int16_t y, bool flip) {
    int16_t dx = entry[1];
    if (flip)
        dx = (int16_t)-dx;
    self->children[index].x = (int16_t)(x + dx);
    self->children[index].y = (int16_t)(y + entry[2]);
    self->children[index].frame = (uint8_t)entry[0];
}

void Obj_InvincibilityStars(Sst *self) {
    Own_InvincibilityStars *own = (Own_InvincibilityStars *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_InvincibilityStars;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Invincibility);
        self->render_flags = SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->width_pixels = 0x10;
        self->height_pixels = 0x10;
        self->priority = 1 * 0x80;
        self->child_count = 8;
    }

    if (!invincibility) {
        Sst_Delete(self);
        return;
    }
    self->x_pos.f.u = player->x_pos.f.u;
    self->y_pos.f.u = player->y_pos.f.u;

    // where the first six go round Sonic (Offset_0x0184FC): the angle he runs at (his way up or down in the air), a quarter of the circle of 8 for each pair of places
    uint8_t status = player->status;
    int16_t angle = player->angle;
    if (status & SST_PSTATUS_IN_AIR) {
        angle = 0xC0;
        if (player->y_vel >= 0)
            angle -= 0x80;
        status = 0;
    }
    if (status & SST_PSTATUS_X_FLIP)
        angle = (int16_t)-angle;
    uint8_t step = (uint8_t)(angle + 0x10);
    step = (uint8_t)((step >> 3) & 0x1C);
    bool flip = (status & SST_PSTATUS_X_FLIP) != 0;
    if ((((int16_t)step - 8) & 0x10) == 0)
        flip = !flip;

    const bool odd = (frame_count & 1) != 0;
    const int8_t *offsets = stars_angle + step + (odd ? 2 : 0);
    const int8_t *orbit = odd ? stars_orbit_odd : stars_orbit_even;

    int16_t x = player->x_pos.f.u;
    int16_t y = player->y_pos.f.u;
    int16_t dx = offsets[0];
    if (flip)
        dx = (int16_t)-dx;
    const int16_t orbit_x = (int16_t)(x + dx), orbit_y = (int16_t)(y + offsets[1]);

    own->orbit += 6;
    if (own->orbit >= 0x78)
        own->orbit = 0;
    const int8_t *entry = orbit + own->orbit * 3;
    for (int i = 0; i < 6; i++, entry += 3)
        Stars_Put(self, i, entry, orbit_x, orbit_y, flip);

    // the two behind him
    own->trail += 6;
    if (own->trail >= 0xA2)
        own->trail = 0;
    entry = stars_trail + own->trail;
    for (int i = 0; i < 2; i++, entry += 3)
        Stars_Put(self, 6 + i, entry, x, y, flip);

    Sst_Draw(self);
}
