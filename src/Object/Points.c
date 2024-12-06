#include "Points.h"

void Obj_Points_Construct(Object* obj) {
	obj->mappings = Mappings_Points;
	obj->tile = TILE_MAP(0, 1, 0, 0, 0x797);
	obj->render.b = 4;
	obj->priority = 1;
	obj->width_pixels = 8;
	obj->ysp = -0x300; // move object upwards
}

void Obj_Points_Slow(Object* obj) {
	SpeedToPos(obj);
	obj->ysp += 0x18; // reduce object speed
}

void Obj_Points(Object* obj) {
    switch (obj->routine) {
        case 0: // Poi_Main
            Obj_Points_Construct(obj);
            // Fall through
        case 2: // Poi_Slower
            if (obj->ysp >= 0) {
                ObjectDelete(obj);
                return;
            }
            Obj_Points_Slow(obj); // reduce object speed
            break;
    }
    DisplaySprite(obj);
}
