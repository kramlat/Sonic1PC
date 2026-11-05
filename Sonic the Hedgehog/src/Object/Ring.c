#include "Ring.h"
#include "Resource/Mappings/RingREV01.h"
#include "Resource/Animation/Ring.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Sound.h"

// Ring object
// Not static -- Object/DebugList.c reads this too, to build the debug
// placement preview's row/column formation stack, so the two can never
// silently drift out of sync (see Ring.h's extern declaration).
const int8_t ring_pos[16][2] = {
    { 0x10, 0x00 },
    { 0x18, 0x00 },
    { 0x20, 0x00 },
    { 0x00, 0x10 },
    { 0x00, 0x18 },
    { 0x00, 0x20 },
    { 0x10, 0x10 },
    { 0x18, 0x18 },
    { 0x20, 0x20 },
    { -0x10, 0x10 },
    { -0x18, 0x18 },
    { -0x20, 0x20 },
    { 0x10, 0x08 },
    { 0x18, 0x10 },
    { -0x10, 0x08 },
    { -0x18, 0x10 },
};

static bool Obj_Ring_ShiftChk(uint8_t* state) {
    if (*state & 1) {
        *state >>= 1;
        return true;
    }
    *state >>= 1;
    return false;
}

static void Obj_Ring_SetupRing(Sst *self, uint8_t index, int16_t x, int16_t y, Sst *ring) {
    Own_Ring *own = (Own_Ring *)ring->own.u8;
    // Set type and routine
    ring->code = Obj_Ring;
    ring->routine += 2;

    // Set position
    ring->x_pos.f.u = x;
    own->base_x = self->x_pos.f.u;
    ring->y_pos.f.u = y;

// Set object drawing information
    ring->mappings = Mappings_RingREV01;
    ring->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
    ring->render_flags = SST_RENDER_LEVEL;
    ring->priority = 2 * 0x80;

    // Set other state stuff
    ring->collision_flags = 0x47;
    ring->width_pixels = 8;
    ring->respawn = self->respawn;
    own->index = index;
}

// The level's rings are not objects: the ring handler (Rings.h) keeps them, as Sonic 3 & Knuckles does. This gives it their look and the level's layout.
void Ring_StartLayout(const uint8_t *layout) {
    Rings_SetLook(Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring));
    Rings_Init(layout);
}

// Test_Ring_Collisions: the player picks up the rings of the layout they touch
void Ring_TouchLayout(const Sst *player) {
    for (int n = Rings_Collide(player->x_pos.f.u, player->y_pos.f.u, player->y_radius); n > 0; n--)
        Ring_Collect();
}

static void ExtraLife(void) {
    lives++;
    life_count++;
}

// Adds a ring and handles the sound and 100/200-ring extra lives (also used by the special stage).
void Ring_Collect(void) {
    // Increment ring count
    rings++;
    ring_count |= 1;

    // The ring sound, or the extra-life jingle instead when this ring earns a life (100 and 200 rings).
    uint8_t sound = sfx_Ring;
    if (rings >= 100 && !(life_num & 1)) {
        life_num |= 1;
        ExtraLife();
        sound = bgm_ExtraLife;
    } else if (rings >= 200 && !(life_num & 2)) {
        life_num |= 2;
        ExtraLife();
        sound = bgm_ExtraLife;
    }
    QueueSound2(sound); // like the original (the driver alternates the ring's speaker itself)
}

void Obj_Ring(Sst *self) {
    Own_Ring *own = (Own_Ring *)self->own.u8;

    switch (self->routine) {
    case 0: {
        // Get spawning information
        uint8_t* statep = &(*SstLoad_State(self));
        uint8_t state = *statep;

        uint8_t num = self->subtype & 7; // subtype
        if (num == 7)
            num = 6;

        int16_t inc_x = ring_pos[self->subtype >> 4][0];
        int16_t inc_y = ring_pos[self->subtype >> 4][1];

        int16_t x = self->x_pos.f.u;
        int16_t y = self->y_pos.f.u;

        // Spawn rings
        uint16_t index = 0;

        if (!Obj_Ring_ShiftChk(&state)) {
            *statep &= 0x7F;
            Obj_Ring_SetupRing(self, index, x, y, self);
        }

        for (
            x += inc_x, y += inc_y, index++;
            num-- > 0;
            x += inc_x, y += inc_y, index++) {
            if (!Obj_Ring_ShiftChk(&state)) {
                *statep &= 0x7F;
                Sst *ring = Sst_Allocate();
                if (ring != NULL)
                    Obj_Ring_SetupRing(self, index, x, y, ring);
            }
        }

        if (*statep & 1) {
            Sst_Delete(self);
            return;
        }
    }
        // Fallthrough
    case 2: // Animate
        // Draw and unload once off-screen
        self->mapping_frame = sprite_anim[1].frame;
        Sst_Draw(self);
        SstDeleteIfNotInRange2(self, own->base_x);
        break;
    case 4: // Collected
        // Change object state
        self->routine += 2;
        self->collision_flags = 0x00;
        self->priority = 1 * 0x80;

        // Collect ring and mark as collected
        Ring_Collect();
        (*SstLoad_State(self)) |= (1 << own->index);
        // Fallthrough
    case 6: // Sparkling
        // Animate and draw
        Sst_Animate(self, Animation_Ring);
        Sst_Draw(self);
        break;
    case 8: // Delete
        Sst_Delete(self);
        break;
    }
}

// Ring loss object
static void Obj_RingLoss_SetupRing(Sst *self, int16_t *xsp, int16_t *ysp, word_u *angle, Sst *ring) {
    // Set object type and routine
    ring->code = Obj_RingLoss;
    ring->routine += 2;

    // Set collision and position
    ring->y_radius = 8;
    ring->x_radius = 8;
    ring->x_pos.f.u = self->x_pos.f.u;
    ring->y_pos.f.u = self->y_pos.f.u;

    // Set object drawing information
    ring->mappings = Mappings_RingREV01;
    ring->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
    ring->render_flags = SST_RENDER_LEVEL;
    ring->priority = 3 * 0x80;

    // Set other object state stuff
    ring->collision_flags = 0x47;
    ring->width_pixels = 8;
    sprite_anim[3].time = -1;

    // Handle object angle and velocity
    if (!(angle->f.u & 0x80)) {
        int16_t sin, cos;
        CalcSine(angle->f.l, &sin, &cos);
        *xsp = sin << angle->f.u;
        *ysp = cos << angle->f.u;

        if ((angle->f.l += 0x10) < 0x10) {
            if ((angle->v -= 0x80) >= 0xFF80)
                angle->v = 0x288;
        }
    }

    ring->x_vel = *xsp;
    ring->y_vel = *ysp;

    *xsp = -*xsp;
    angle->v = -angle->v;
}

void Obj_RingLoss(Sst *self) {
    switch (self->routine) {
    case 0: {
        // Get how many rings to drop
        uint16_t drop = rings;
        if (drop >= 32)
            drop = 32; // Cap at 32
        drop--; // dbf

        // Drop rings
        int16_t xsp, ysp;
        word_u angle;
        angle.v = 0x0288;

        Obj_RingLoss_SetupRing(self, &xsp, &ysp, &angle, self);
        while (drop-- > 0) {
            Sst *ring = Sst_Allocate();
            if (ring == NULL)
                break;
            Obj_RingLoss_SetupRing(self, &xsp, &ysp, &angle, ring);
        }

        // Lose rings
        rings = 0;
        ring_count = 0x80;
        life_num = 0;
        PlaySound(sfx_RingLoss);
    }
        // Fallthrough
    case 2: // Moving
        // Use animation frame
        self->mapping_frame = sprite_anim[3].frame;

        // Fall
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;

        // Do floor collision (every 4 frames)
        if (self->y_vel >= 0 && (((int)(self - sst) + (vbla_count & 0xFF)) & 3) == 0) {
            int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
            if (floor_dist < 0) {
                // Bounce off floor
                self->y_pos.f.u += floor_dist;
                self->y_vel = -(self->y_vel - (self->y_vel >> 2));
            }
        }

        // Delete once below level or animation is done
        if (!sprite_anim[3].time) {
            Sst_Delete(self);
            break;
        }
        if ((limit_btm2 + SCREEN_HEIGHT) < self->y_pos.f.u) {
            Sst_Delete(self);
            break;
        }

        // Draw
        Sst_Draw(self);
        break;
    case 4: // Collected
        // Change object state
        self->routine += 2;
        self->collision_flags = 0x00;
        self->priority = 1 * 0x80;

        // Collect ring
        Ring_Collect();
        // Fallthrough
    case 6: // Sparkling
        // Animate and draw
        Sst_Animate(self, Animation_Ring);
        Sst_Draw(self);
        break;
    case 8: // Delete
        Sst_Delete(self);
        break;
    }
}
