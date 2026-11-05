#pragma once

#include "Object.h"
#include "Sst.h"
#include "LevelCollision.h"

// The solid and platform helpers of Object.c for the objects on the status table of Sonic 3 & Knuckles (Sst.h): the same code, for an `Sst` standing in the object's place. The character is still the old
// `player` until it moves over too; what it stands on is remembered by Standing.h (so an old and a new object can hand it to each other).
void SstMvSonicOnPtfm(Sst *obj, int16_t y, int16_t prev_x);
void SstPlatformObject(Sst *obj, uint16_t x_rad);
void SstPlatformObject_CustomHeight(Sst *obj, uint16_t x_rad, int16_t height);
void SstPlatform3(Sst *obj, int16_t top);
void SstPlatform_SetStand(Sst *obj);

// BossMove and BossDefeated (Object.h) for the status table
void SstBossMove(const Sst *obj, dword_s *boss_x, dword_s *boss_y);
void SstBossDefeated(const Sst *obj);
bool SstExitPlatform(Sst *obj, uint16_t x_rad, uint16_t x_rad2, int16_t *x_off_p);
void SstSlopeObject(Sst *obj, uint16_t x_rad, const uint8_t *heightmap);
void SstSlopeObject_AssumeStoodOn(Sst *obj, uint16_t x_rad, const uint8_t *heightmap, int16_t prev_x);
int32_t SstSolid_ChkEnter(Sst *obj, uint16_t x_rad, uint16_t y_rad, int16_t *x_off, int16_t *y_off);
int32_t SstSolidObject(Sst *obj, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x, int16_t *x_off, int16_t *y_off);
int32_t SstSolidObject_Heightmap(Sst *obj, uint16_t x_rad, uint16_t y_rad, const uint8_t *heightmap);
// SmashObject / FragmentatePlatform for a status table object: the object becomes the first piece of its frame and `count`-1 more objects (of the same code) the others, each drawing one lone piece (static mappings).
// The fragments' own delay (FragmentatePlatform) is in own.u8[SST_COLLAPSE_DELAY]: the objects that collapse keep their `collapsible_timedelay` there.
#define SST_COLLAPSE_DELAY 0x10
void SstSmashObject(Sst *obj, int count, const int16_t *frag_speeds);
void SstFragmentatePlatform(Sst *obj, int count, const uint8_t *delays);
int16_t SstHitWallRight(Sst *obj, int16_t x_off);
int16_t SstHitWallLeft(Sst *obj, int16_t x_off);
int16_t SstHitCeiling(Sst *obj);
bool SstChkObjectVisible(const Sst *obj);
bool SstChkPartiallyVisible(const Sst *obj);

// Status bits of an object that a character stands on or pushes (the character's stand and push flags)
#define SST_STATUS_PLAYER_STAND 0x08
#define SST_STATUS_PLAYER_PUSH  0x20
