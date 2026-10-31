#include "GHZEdge.h"
#include "Resource/Mappings/GHZEdge.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Sonic.h"

#include "Macros.h"

// GHZ edge object
static int32_t Obj44_SolidWall2(Sst *self, uint16_t x_rad, uint16_t y_rad, int16_t *x_off, int16_t *y_off) {
    // Check if we're touching horizontally
    *x_off = player->x_pos.f.u - self->x_pos.f.u + x_rad;
    uint16_t x_dia = x_rad << 1;

    if (*x_off < 0 || *x_off > x_dia)
        return 0;

    // Check if we're touching vertically
    y_rad += player->y_radius;

    *y_off = player->y_pos.f.u - self->y_pos.f.u + 4 + y_rad;
    uint16_t y_dia = y_rad << 1;

    if (*y_off < 0 || *y_off >= y_dia)
        return 0;

    // Check if we can touch
    if ((lock_multi & 0x80) || player->routine >= 6 || debug_use)
        return 0;

    // Get X clip
    uint16_t x_clip = *x_off;
    if (x_rad < *x_off) {
        *x_off -= x_dia;
        x_clip = -*x_off;
    }

    // Get Y clip
    uint16_t y_clip = *y_off;
    if (y_rad < *y_off) {
        *y_off -= y_dia;
        y_clip = -*y_off;
    }

    if (x_clip <= y_clip)
        return 1;
    else
        return -1;
}

static void Obj44_SolidWall(Sst *self, uint16_t x_rad, uint16_t y_rad)
{
    int16_t x_off, y_off;

    signed int res = Obj44_SolidWall2(self, x_rad, y_rad, &x_off, &y_off);
    if (res > 0) {
        // Hit horizontal
        if (x_off > 0) {
            if (player->x_vel >= 0) {
                player->x_pos.f.u -= x_off;
                player->ground_vel = 0;
                player->x_vel = 0;
            }
        } else if (x_off < 0) {
            if (player->x_vel < 0) {
                player->x_pos.f.u -= x_off;
                player->ground_vel = 0;
                player->x_vel = 0;
            }
        }

        // Update push flags
        if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
            player->status |= SST_PSTATUS_PUSHING;
            self->status |= SST_STATUS_PLAYER_PUSH;
        } else {
            self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
            player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        }
        return;
    } else if (res < 0) {
        // Hit vertical
        if (player->y_vel >= 0 || y_off >= 0)
            return;
        player->y_pos.f.u -= y_off;
        player->y_vel = 0;
    }

    // Clear pushing state
    if (((self->status & SST_STATUS_PLAYER_PUSH) != 0)) {
        // (no forced running animation here: the original's "walk-jump bug", see SolidObject)
        self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    }
}

void Obj_GHZEdge(Sst *self)
{
    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_GHZEdge;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Edge_Wall);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 8;
        self->priority = 6 * 0x80;
        self->mapping_frame = self->subtype; // subtype

        // Check if object should be solid
        if (!(self->mapping_frame & 0x10)) {
            // Solid
            Obj44_SolidWall(self, 0x13, 0x28);
        } else {
            // Non-solid
            self->mapping_frame &= ~0x10;
            self->routine += 2;
        }
        break;
    case 2: // Solid and draw
        Obj44_SolidWall(self, 0x13, 0x28);
        break;
    case 4: // Draw
        break;
    default:
        return;
    }

    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
