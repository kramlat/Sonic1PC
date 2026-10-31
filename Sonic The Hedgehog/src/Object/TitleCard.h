#ifndef _TITLECARD_H
#define _TITLECARD_H

#include "Sst.h"

void Obj_TitleCard(Sst *self);
void Obj_GotThroughCard(Sst *self);

// The level start waits for the card to be in place, then tells it to leave
bool TitleCard_Arrived(void);
void TitleCard_Leave(void);

typedef struct {
    int16_t main_x; // 0x30
    int16_t final_x; // 0x32
    uint16_t timer;  // frames to wait (frame_time.w)
} Own_TitleCard;

#endif //_TITLECARD_H
