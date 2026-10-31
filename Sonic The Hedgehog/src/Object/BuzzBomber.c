#include "BuzzBomber.h"
#include "Constants.h"
#include "Object/Explosion.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Resource/Mappings/BuzzBomber.h"
#include "Resource/Mappings/BuzzMissile.h"

#include "Resource/Animation/BuzzBomber.h"
#include "Resource/Animation/BuzzMissile.h"

// Buzz Bomber's missile blowing up. (The original dissolves it with an animation; this port never had that, and the object it turned into did nothing and never went: it goes now.)
void Obj_BuzzExplode(Sst *self) {
    Sst_Delete(self);
}

static void BuzzMissile_Construct(Sst *self) {
    self->mappings = Mappings_BuzzMissile;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Buzz_Bomber);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;
    self->width_pixels = 8;
    self->status &= 3; // (the flips stay)
}

static bool BuzzMissile_CheckNewtron(Sst *self) {
    if (self->subtype) {
        self->routine = 8;
        self->collision_flags = 0x87;
        self->anim = 1;
        Sst_Animate(self, Animation_BuzzMissile);
        Sst_Draw(self);
        return true;
    }
    return false;
}

static void BuzzMissile_Charge(Sst *self, Own_BuzzMissile *own) {
    // Delete object if parent Buzz Bomber has exploded
    if (sst[own->parent_index].code == Obj_Explosion)
        Sst_Delete(self);

    // Animate and draw
    if (self->code != NULL) {
        Sst_Animate(self, Animation_BuzzMissile);
        Sst_Draw(self);
    }
}

static void BuzzMissile_Fire(Sst *self) {
    // Check if we've 'hit Sonic' (disabled)
    if (!(self->status & 0x80)) {
        // Use fired collision and animation
        self->collision_flags = 0x87;
        self->anim = 1;

        // Move and animate
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_BuzzMissile);
        Sst_Draw(self);

        // Delete if fallen below stage
        if ((limit_btm2 + SCREEN_HEIGHT) < self->y_pos.f.u)
            Sst_Delete(self);

    } else {
        // Explode
        self->code = Obj_BuzzExplode;
        self->routine = 0;
        self->code(self);
    }
}

static void BuzzMissile_NewtFire(Sst *self) {
    // Move and animate, deleted once off-screen
    Sst_MoveSprite2(self);
    Sst_Animate(self, Animation_BuzzMissile);
    SstOnScreenTest(self);
}

void Obj_BuzzMissile(Sst *self) {
    Own_BuzzMissile *own = (Own_BuzzMissile *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        // Wait for timer to expire
        if (--own->time_delay >= 0)
            break;

        // Increment routine
        self->routine += 2;

        // Set object drawing information
        BuzzMissile_Construct(self);

        // Check if we were created by a Newtron
        if (BuzzMissile_CheckNewtron(self))
            break;

        // Fallthrough
    case 2: // Charging
        BuzzMissile_Charge(self, own);
        break;
    case 4: // Fired by Buzz Bomber
        BuzzMissile_Fire(self);
        break;
    case 6: // Delete
        Sst_Delete(self);
        break;
    case 8: // Fired by Newtron
        BuzzMissile_NewtFire(self);
        break;
    }
}

static void Obj_BuzzBomber_Construct(Sst *self) {
    // Set object drawing information
    self->mappings = Mappings_BuzzBomber;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Buzz_Bomber);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;
    self->collision_flags = 0x08;
    self->width_pixels = 24;
}

static void Obj_BuzzBomber_Fly(Sst *self, Own_BuzzBomber *own) {
    // Not near Sonic
    self->routine_secondary += 2;
    own->time_delay = 127; // It's a word, Yuji!
    self->x_vel = 0x400;
    self->anim = 1;
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        self->x_vel = -self->x_vel;
}

static void Obj_BuzzBomber_Fire(Sst *self, Sst *missile, Own_BuzzBomber *own) {
    Own_BuzzMissile *mscratch = (Own_BuzzMissile *)missile->own.u8;

    missile->code = Obj_BuzzMissile;
    missile->x_pos.f.u = self->x_pos.f.u;
    missile->y_pos.f.u = self->y_pos.f.u + 28;
    missile->y_vel = 0x200;
    missile->x_vel = 0x200;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        missile->x_pos.f.u -= 24;
        missile->x_vel = -missile->x_vel;
    } else
        missile->x_pos.f.u += 24;

    missile->status = self->status;
    mscratch->time_delay = 14;
    mscratch->parent_index = (uint8_t)(self - sst);

    // Set our state
    own->buzz_status = 1;
    own->time_delay = 59;
    self->anim = 2;
}

static bool Obj_BuzzBomber_CheckCloseToSonic(Sst *self) {
    // Get X difference and check if we're close enough to Sonic
    uint16_t x_off = (self->x_pos.f.u < player->x_pos.f.u) ? (player->x_pos.f.u - self->x_pos.f.u) : (self->x_pos.f.u - player->x_pos.f.u);

    if (x_off >= 0x60 || !(self->render_flags & SST_RENDER_ON_SCREEN))
        return false;
    return true;
}

static void Obj_BuzzBomber_SetStatusAttack(Own_BuzzBomber *own) {
    // Set that we're near Sonic
    own->buzz_status = 2;
    own->time_delay = 29;
}

static void Obj_BuzzBomber_TurnAround(Sst *self, Own_BuzzBomber *own) {
    // Change direction
    own->buzz_status = 0;
    self->status ^= SST_STATUS_X_FLIP;
    own->time_delay = 59;
}

static void Obj_BuzzBomber_Stop(Sst *self) {
    // Stop
    self->routine_secondary -= 2;
    self->x_vel = 0;
    self->anim = 0;
}

void Obj_BuzzBomber(Sst *self)
{
    Own_BuzzBomber *own = (Own_BuzzBomber *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        Obj_BuzzBomber_Construct(self);

        // Fallthrough
    case 2: // Moving
        switch (self->routine_secondary) {
        case 0: // Moving
            // Wait for timer to expire
            if (--own->time_delay >= 0)
                break;

            // Start firing missile if near Sonic
            if (!(own->buzz_status & 2)) {
                Obj_BuzzBomber_Fly(self, own);
            } else {
                // Near Sonic
                // Create missile object
                Sst *missile = Sst_Allocate();
                if (missile == NULL)
                    break;
                Obj_BuzzBomber_Fire(self,missile,own);
            }
            break;
        case 2: // Check near Sonic
            // Wait for timer to expire
            if (--own->time_delay >= 0) {
                // Move and check if we're near Sonic
                Sst_MoveSprite2(self);

                if (own->buzz_status)
                    break;

                if(Obj_BuzzBomber_CheckCloseToSonic(self)) {
                    Obj_BuzzBomber_SetStatusAttack(own);
                } else
                    break;
            } else {
                Obj_BuzzBomber_TurnAround(self,own);
            }

            Obj_BuzzBomber_Stop(self);
            break;
        }

        // Animate, draw, and unload when off-screen
        Sst_Animate(self, Animation_BuzzBomber);
        SstOnScreenTest(self);
        break;
    case 4: // Delete
        Sst_Delete(self);
        break;
    }
}
