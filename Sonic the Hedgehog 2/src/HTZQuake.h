#ifndef _HTZQUAKE_H
#define _HTZQUAKE_H

#include "Object.h"

// Hill Top's earthquake, the Simon Wai prototype's: its dynamic level events (DynResize_HTz), the background scroll that goes with them (the shaking branch of Bg_Scroll_HTz) and the solid blocks of
// the moving ground (object 30)
extern uint8_t htz_quake;     // Screen_Shaking_Flag_HTZ: the camera is in a quake's stretch of the level, and the background moves by the events' own diffs
extern uint8_t htz_shaking;   // Screen_Shaking_Flag: the ground is moving, and the screen shakes
extern int16_t htz_bg_y_offset; // Camera_BG_Y_offset: how far the moving ground is from its place (object 30 rides on it)

void HTZQuake_Reset(void);
void HTZQuake_Events(void);
void HTZQuake_Deform(void);
void HTZQuake_SpritesNormal(void); // the sprites follow the camera itself again (the shaking branch is not running) // the shaking branch of the background scroll (loc_6236)

void Obj_HTZQuakeBlock(Object *obj);

#endif //_HTZQUAKE_H
