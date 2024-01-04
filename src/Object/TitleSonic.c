#include "Object.h"

// Title Sonic assets
#include "Resource/Animation/TitleSonic.h"
#include "Resource/Mappings/TitleSonic.h"

// Title Sonic object
void Obj_TitleSonic(Object* obj)
{
    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine and initialize position
        obj->routine += 2;

        obj->pos.s.x = 0xF0 + (PLANE_WIDEADD * 4);
        obj->pos.s.y = 0xDE;

        // Set object drawing information
        obj->mappings = Mappings_TitleSonic;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x300);
        obj->priority = 1;

        // Initialize state
        obj->frame_time.b = 29;
        AnimateSprite(obj, Animation_TitleSonic);
        // Fallthrough
    case 2: // Waiting to appear
        // Wait for timer to clear
        if ((--obj->frame_time.b) >= 0)
            return;

        // Increment routine
        obj->routine += 2;
        DisplaySprite(obj);
        break;
    case 4: // Moving upwards
        // Move upwards and increment routine when end point reached
        if ((obj->pos.s.y -= 8) == 0x96)
            obj->routine += 2;
        DisplaySprite(obj);
        break;
    case 6: // Animating
        AnimateSprite(obj, Animation_TitleSonic);
        DisplaySprite(obj);
        break;
    }
}
