#pragma once

#include "EngineObject.h"
#include "Sst.h"

//Collision angle buffer
extern uint8_t angle_buffer0, angle_buffer1;

//Level collision interface
void FloorLog_Unk(void);
const uint8_t *FindNearestTile(Sst *obj, int16_t x, int16_t y);
int16_t FindFloor(Sst *obj, int16_t x, int16_t y, uint16_t solid, uint16_t flip, int16_t inc, uint8_t *angle);
int16_t FindWall(Sst *obj, int16_t x, int16_t y, uint16_t solid, uint16_t flip, int16_t inc, uint8_t *angle);

//Object collision functions
int16_t GetDistance2_Down(Sst *obj, int16_t x, int16_t y, uint8_t *hit_angle);
int16_t GetDistance2_Left(Sst *obj, int16_t x, int16_t y, uint8_t *hit_angle);
int16_t GetDistance2_Up(Sst *obj, int16_t x, int16_t y, uint8_t *hit_angle);
int16_t GetDistance2_Right(Sst *obj, int16_t x, int16_t y, uint8_t *hit_angle);
int16_t GetDistanceBelowAngle2(Sst *obj, uint8_t angle, uint8_t *hit_angle);

void GetDistance_Down(Sst *obj, int16_t *dist0, int16_t *dist1, uint8_t *hit_angle);
void GetDistance_Left(Sst *obj, int16_t *dist0, int16_t *dist1, uint8_t *hit_angle);
void GetDistance_Up(Sst *obj, int16_t *dist0, int16_t *dist1, uint8_t *hit_angle);
void GetDistance_Right(Sst *obj, int16_t *dist0, int16_t *dist1, uint8_t *hit_angle);
void GetDistanceBelowAngle(Sst *obj, uint8_t angle, int16_t *dist0, int16_t *dist1, uint8_t *hit_angle);

int16_t SstFloorDist(Sst *obj, int16_t x);   // (the object's own collision path: the characters')
int16_t ObjFloorDist(Object *obj, int16_t x); // an old object's
Sst *Collision_Proxy(void);
