#ifndef _SPLASH_H
#define _SPLASH_H

#include "Sst.h"

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

void Obj_Splash(Sst *self);
void Splash_SpawnSkidDust(Sst *parent);

// The companion of the level (the fixed one that shows the splash and the dust): made when the level starts, told what to show
void Splash_Make(void);
void Splash_SetAnim(uint8_t anim);
uint8_t Splash_GetAnim(void);
Sst *Splash_Companion(void); // (NULL when there is none)

#endif
