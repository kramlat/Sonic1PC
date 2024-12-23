#include "Object.h"

//'PRESS START BUTTON' Assets
#include "Resource/Animation/PSB.h"
#include "Resource/Mappings/PSB.h"

//'PRESS START BUTTON' object
void Obj_PSB(Object* obj)
{
    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine and set position
        obj->routine += 2;
        obj->pos.s.x = 0xD0 + (PLANE_WIDEADD * 4);
        obj->pos.s.y = 0x130;

        // Set object drawing information
        obj->mappings = Mappings_PSB;
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x200);

        // Handle different frames
        if (obj->frame >= 2) {
            obj->routine += 2;
            if (obj->frame == 3) {
                // Trademark
                obj->tile = TILE_MAP(0, 1, 0, 0, 0x510);
                obj->pos.s.x = 0x170 + (PLANE_WIDEADD * 4);
                obj->pos.s.y = 0xF8;
            } else if (obj->frame == 2) {
                // Sonic mask
                obj->pos.s.x -= SCREEN_WIDEADD2; // Widescreen hack so you don't see the masking sprites
            }
            break;
        }
        // Fallthrough
    case 2: // Press Start Button
        AnimateSprite(obj, Animation_PSB);
        break;
    case 4: // TM or Sonic mask
        break;
    }

    DisplaySprite(obj);
}
