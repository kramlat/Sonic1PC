// The shield and the invincibility stars for Sonic 2 (Nick Arcade's object 38): Sonic 1's with Nick Arcade's changes. The shield is wider and Emerald Hill's art is at its own place; the stars are unfinished:
// each one is Sonic's own mapping frame (not a star's) drawn where Nick Arcade's star trail buffer says, and that buffer is never written (the routine meant to fill it is never called), so the stars sit at the
// level's top left corner and, but for the invincibility itself, nothing shows.
#include "Object/ShieldInvincibility.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"

#define ArtTile_ShieldEHZ 0x560 // (Emerald Hill's PLC loads the shield here, as the tiles at $4BE are taken there)

void Obj_ShieldInvincibility(Object* obj) {
    switch (obj->routine) {
    case 0: // Initialiation
        // Increment routine
        obj->routine += 2;

        // Set object drawing information
        obj->mappings = Mappings_ShieldInvincibility;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 1;
        obj->width_pixels = 0x18;

        // Check if invincibility or shield
        if (!obj->anim) {
            // Shield
            obj->tile = TILE_MAP(0, 0, 0, 0, LEVEL_ZONE(level_id) == ZoneId_EHZ ? ArtTile_ShieldEHZ : ArtTile_Shield);
        } else {
            // Invincibility: Sonic's own mappings, with the stars' tiles
            obj->routine += 2;
            obj->mappings = Mappings_Sonic;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Invincibility);
            obj->priority = 2;
        }
        break;
    case 2: // Shield
        // Check if shield should exist
        if (invincibility)
            break;
        if (!shield) {
            ObjectDelete(obj);
            break;
        }

        // Update shield position
        obj->pos.l.x.f.u = player->pos.l.x.f.u;
        obj->pos.l.y.f.u = player->pos.l.y.f.u;
        obj->status.b = player->status.b;

        // Animate and draw shield
        AnimateSprite(obj, Animation_ShieldInvincibility);
        DisplaySprite(obj);
        break;
    case 4: // Invincibility
        // Check if invincibility should exist
        if (!invincibility) {
            ObjectDelete(obj);
            break;
        }

        // The star trail buffer (never filled in): the corner of the level
        obj->pos.l.x.f.u = 0;
        obj->pos.l.y.f.u = 0;
        obj->status.b = player->status.b;
        obj->frame = player->frame;
        obj->render.b = player->render.b;
        DisplaySprite(obj);
        break;
    }
}
