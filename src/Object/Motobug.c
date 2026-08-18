#include "MotoBug.h"

#include "LevelCollision.h"

// Motobug object
void Obj_Motobug(Object* obj) {
    Scratch_Motobug* scratch = (Scratch_Motobug*)&obj->scratch;

    int16_t floor_dist;

    switch (obj->routine) {
    case 0: // Initialization
        // Set object drawing information
        obj->mappings = Mappings_Motobug;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Moto_Bug);
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 4;
        obj->width_pixels = 20;

        // Check if we're smoke
        if (!obj->anim) {
            // Motobug
            // Initialize collision
            obj->y_rad = 14;
            obj->x_rad = 8;
            obj->col_type = 0x0C;

            // Fall before enabling
            ObjectFall(obj);
            if ((floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u)) >= 0)
                break;

            // Clip and increment routine
            obj->pos.l.y.f.u += floor_dist;
            obj->ysp = 0;
            obj->routine += 2;
            obj->status.o.f.x_flip ^= 1;
        } else {
            // Smoke
            // Increment routine and draw
            obj->routine += 4;
            AnimateSprite(obj, Animation_Motobug);
            DisplaySprite(obj);
            break;
        }
        // Fallthrough
    case 2: // Moving around
        switch (obj->routine_sec) {
        case 0:
            // Wait for timer to expire
            if (--scratch->time < 0) {
                // Turn around and begin moving
                obj->routine_sec += 2;
                obj->xsp = -0x100;
                obj->anim = 1;
                if ((obj->status.o.f.x_flip ^= 1))
                    obj->xsp = -obj->xsp;
            }
            break;
        case 2:
            // Move and check floor
            SpeedToPos(obj);
            floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);

            if (floor_dist >= -8 && floor_dist < 12) {
                // Clip
                obj->pos.l.y.f.u += floor_dist;

                // Emit smoke every 16 frames
                if (--scratch->smoke_delay < 0) {
                    scratch->smoke_delay = 15;

                    // Find smoke object slot
                    Object* smoke = FindFreeObj();
                    if (smoke == NULL)
                        break;

                    // Setup smoke object
                    smoke->type = ObjId_Motobug;
                    smoke->pos.l.x.f.u = obj->pos.l.x.f.u;
                    smoke->pos.l.y.f.u = obj->pos.l.y.f.u;
                    smoke->status.o.b = obj->status.o.b;
                    smoke->anim = 2;
                }
            } else {
                // Stop moving
                obj->routine_sec -= 2;
                scratch->time = 59;
                obj->xsp = 0;
                obj->anim = 0;
            }
            break;
        }

        // Animate, draw, and unload when off-screen
        AnimateSprite(obj, Animation_Motobug);
        RememberState(obj);
        break;
    case 4: // Smoke
        AnimateSprite(obj, Animation_Motobug);
        DisplaySprite(obj);
        break;
    case 6: // Delete (smoke)
        ObjectDelete(obj);
        break;
    }
}
