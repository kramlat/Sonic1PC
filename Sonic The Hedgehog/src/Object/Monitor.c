#include "Monitor.h"
#include "Resource/Animation/Monitor.h"
#include "Object/Explosion.h"
#include "Resource/Mappings/Monitor.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "ShieldInvincibility.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Sound.h"

// Monitor solid routine
static int32_t Mon_SolidSides(Sst *self, uint16_t x_rad, uint16_t y_rad, int16_t *x_off, int16_t *y_off) {
    // Check if player is in horizontal range
    *x_off = player->x_pos.f.u - self->x_pos.f.u + x_rad;
    uint16_t x_dia = x_rad << 1;
    if (*x_off < 0 || *x_off > x_dia)
        return 0;

    // Check if player is in vertical range
    y_rad += player->y_radius;
    *y_off = player->y_pos.f.u - self->y_pos.f.u + y_rad;
    uint16_t y_dia = y_rad << 1;

    if (*y_off < 0 || *y_off >= y_dia)
        return 0;

    // Check if player can collide with object
    if ((lock_multi & 0x80) || player->routine >= 6 || debug_use)
        return 0;

    // Shift x off when on other side of monitor
    if (x_rad < *x_off)
        *x_off -= x_dia;

    // Check if we're touching the top or not
    if (*y_off >= 0x10)
        return 1;

    x_rad = self->width_pixels + 4;
    x_dia = x_rad << 1;

    int16_t x_off2 = player->x_pos.f.u - self->x_pos.f.u + x_rad;
    if (x_off2 >= 0 && x_off2 < x_dia)
        return -1;

    return 1;
}

// Monitor object
void Obj_Monitor(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        // (Monitors with invalid subtypes, above 8, are barriers in the object layout now: SBZ2 has a handful of these broken monitors hidden in walls in the real
        // ROM data, all matching properly sized invisible solid barriers; the converted layout names the barrier, see res/ObjectLayout/SBZ2.)
        // Increment routine
        self->routine += 2;

        // Initialize collision
        self->y_radius = 14;
        self->x_radius = 14;

        // Set object drawing information
        self->mappings = Mappings_Monitor;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Monitor);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 15;

        // Check if monitor was destroyed
        (*SstLoad_State(self)) &= 0x7F;
        if ((*SstLoad_State(self)) & 1) {
            // Destroyed
            self->routine = 8;
            self->mapping_frame = 11;
            break; // Not drawn
        }

        // Set collision type and animation
        self->collision_flags = 0x46;
        self->anim = self->subtype; // subtype
        // Fallthrough
    case 2: // Solid
        switch (self->routine_secondary) {
        case 0: // Solid
        {
            // Check if we're touching the monitor
            int16_t x_off, y_off;
            int32_t solid = Mon_SolidSides(self, 26, 15, &x_off, &y_off);

            // Bug fix: spin dashing right next to a monitor wouldn't break
            // it, since Mon_SolidSides' own push/stand handling only backed
            // off for the Roll animation, not the (still curled-up, about
            // to release) Spin Dash animation.
            if (solid && (player->y_vel < 0 || (player->anim != SonAnimId_Roll && player->anim != SonAnimId_SpinDash))) {
                if (solid < 0) {
                    // Stand on monitor
                    player->y_pos.f.u -= y_off;
                    SstPlatform_SetStand(self);
                    self->routine_secondary = 2;
                } else {
                    // Check if player shall be clipped by monitor sides
                    if ((x_off > 0 && player->x_vel >= 0) || (x_off < 0 && player->x_vel < 0)) {
                        player->x_pos.f.u -= x_off;
                        player->ground_vel = 0;
                        player->x_vel = 0;
                    }

                    // Start or stop pushing
                    if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
                        player->status |= SST_PSTATUS_PUSHING;
                        self->status |= SST_STATUS_PLAYER_PUSH;
                    } else {
                        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                        self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
                    }
                }
            } else {
                // Stop pushing
                if (((self->status & SST_STATUS_PLAYER_PUSH) != 0)) {
                    // (no forced running animation here: the original's "walk-jump bug", see SolidObject)
                    player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                    self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
                }
            }
            break;
        }
        case 2: // Player standing
        {
            // Handle player standing on the monitor
            uint16_t x_rad = self->width_pixels + 11;
            SstExitPlatform(self, x_rad, x_rad, NULL);
            if (!((player->status & SST_PSTATUS_OBJECT_STAND) != 0)) // Checks player?
                self->routine_secondary = 0;
            else
                SstMvSonicOnPtfm(self, self->y_pos.f.u - 16, self->x_pos.f.u);
            break;
        }
        default: // Falling
        {
            // Fall to the ground
            Sst_MoveSprite(self);

            int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
            if (floor_dist < 0) {
                // Clip out of floor and stop falling
                self->y_pos.f.u += floor_dist;
                self->y_vel = 0;
                self->routine_secondary = 0;
            }
            break;
        }
        }
        // Fallthrough
    case 6: // Animate
        Sst_Animate(self, Animation_Monitor);
        // Fallthrough
    case 8: // Broken
        Sst_Draw(self);

        // Delete once off-screen
        SstDeleteIfNotInRange2(self, self->x_pos.f.u);
        break;
    case 4: // Breaking open
        // Increment routine
        self->routine += 2;
        self->collision_flags = 0x00;

        // Create item object
        Sst *item = Sst_Allocate();
        if (item != NULL) {
            item->code = Obj_MonitorItem;
            item->x_pos.f.u = self->x_pos.f.u;
            item->y_pos.f.u = self->y_pos.f.u;
            item->anim = self->anim;
        }

        // Create explosion object
        Sst *explosion = Sst_Allocate();
        if (explosion != NULL) {
            explosion->code = Obj_Explosion;
            explosion->routine += 2;
            explosion->x_pos.f.u = self->x_pos.f.u;
            explosion->y_pos.f.u = self->y_pos.f.u;
        }

        // Mark as broken and draw
        (*SstLoad_State(self)) |= 1;

        self->anim = 9;
        Sst_Draw(self);
        break;
    }
}

// Monitor item object
static void ExtraLife(void) {
    lives++;
    life_count++;
    PlayMusic(bgm_ExtraLife);
}

void Obj_MonitorItem(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_Monitor;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Monitor);
        self->render_flags = SST_RENDER_LEVEL;
        self->render_flags |= SST_RENDER_STATIC;
        self->priority = 3 * 0x80;
        self->width_pixels = 8;

        // Initialize object state
        self->y_vel = -0x300;
        self->mapping_frame = self->anim + 2;

        const uint8_t* monitor_piece;
        Mappings_FramePieces(Mappings_Monitor, self->mapping_frame, &monitor_piece); // the frame's first piece, as a raw single-piece mapping
        self->mappings = monitor_piece;
        // Fallthrough
    case 2: // Move
        if (self->y_vel < 0) {
            // Move
            Sst_MoveSprite2(self);
            self->y_vel += 0x18;
        } else {
            // Increment routine
            self->routine += 2;
            self->anim_frame_timer = 29;

            // Give item
            Scratch_Sonic* scratch = (Scratch_Sonic *)player->own.u8;

            switch (self->anim) // I'm not doing an else if chain LOL
            {
            case 1: // Eggman
                // Matches real hardware's FixBugs fix (reuses Spikes_Hurt's
                // own guard logic and Y-position undo, since that's what
                // the real disasm's own fix literally jumps into) --
                // without these checks, the monitor would still hurt Sonic
                // while invincible/flashing/already hurt.
                if (!invincibility && scratch->flash_time == 0 && player->routine < 4) {
                    player->y_pos.v -= (int32_t)player->y_vel << 8;
                    HurtSonicFrom(player, self->x_pos.f.u, false);
                }
                break;
            case 2: // Sonic
                ExtraLife();
                break;
            case 3: // Shoes
                shoes = true;
                scratch->shoes_time = 1200;
                sonspeed_max = 0xC00;
                sonspeed_acc = 0x18;
                sonspeed_dec = 0x80;
                SpeedUpMusic();
                break;
            case 4: // Shield
                shield = true;
                ShieldInvincibility_Make(0);
                PlaySound(sfx_Shield);
                break;
            case 5: // Invincibility
                invincibility = true;
                scratch->invincibility_time = 1200;
                ShieldInvincibility_Make(1);
                ShieldInvincibility_Make(2);
                ShieldInvincibility_Make(3);
                ShieldInvincibility_Make(4);
                if (!lock_screen) {
                    PlayMusic(bgm_Invincible);
                }
                break;
            case 6: // Rings
                // Give 10 rings
                rings += 10;
                ring_count |= 1;

                // Give extra life
                if (rings >= 100 && !(life_num & 1)) {
                    life_num |= 1;
                    ExtraLife();
                } else if (rings >= 200 && !(life_num & 2)) {
                    life_num |= 2;
                    ExtraLife();
                } else {
                    PlaySound(sfx_Ring);
                }
                break;
            case 7: // S
                break; // Nothing
            }
        }
        break;
    case 4: // Delete
        if (--self->anim_frame_timer < 0)
            Sst_Delete(self);
        break;
    }

    // Draw object
    Sst_Draw(self);
}
