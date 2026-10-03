#ifndef _SPLASH_H
#define _SPLASH_H

#include "Object.h"

// VRAM tile base for the dynamically-loaded splash/dust art window
// ($F400, 18 tiles -- see PLC.c's PLC_Main for the neighbouring layout).
#define ArtTile_SplashDust 0x7A0
#define SPLASHDUST_GFX_SIZE 0x240 // 18 tiles

// Splash/dust display modes (obj->anim)
#define SplashAnim_Null   0
#define SplashAnim_Splash 1
#define SplashAnim_Dash   2
#define SplashAnim_Skid   3

extern uint8_t splashdust_gfx_buffer[SPLASHDUST_GFX_SIZE];
extern uint8_t splashdust_frame_chg;

void Obj_Splash(Object *obj);
void Splash_SpawnSkidDust(Object *parent);

#endif
