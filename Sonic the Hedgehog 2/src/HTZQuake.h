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

// The second view of a split screen shakes with the ground too (the prototype's two player scroll never does): what its foreground is moved down by, and the shake of its background and sprites
extern int16_t htz_p2_shake_y;
void HTZQuake_SetView(int view);  // the view the deformation runs for (0 the first, 1 the second: LevelScroll.c sets it around the second's)
int HTZQuake_Owner(void);         // the view whose camera started the quake that is on (the other only shakes)
void HTZQuake_EventsP2(void);     // the events for the second camera (called with it swapped in)
void HTZQuake_ShakeView(void);    // after the zone's usual deformation for that view: it shakes with the ground

void Obj_HTZQuakeBlock(Object *obj);

#endif //_HTZQUAKE_H
