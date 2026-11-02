#ifndef _CNZSLOTS_H
#define _CNZSLOTS_H

#include "Sst.h"

// Casino Night's slot machine of the Sonic 2 pre-beta: its starter (object $D6: a block a character lands on, which holds him in a ball for $78 frames giving 100 points every 8, then lets him fall
// at $400) and the reels, which the level's events run every frame (Process_Slot_Machine in the pre-beta's DynResize_CNz): three strips of symbols that spin at speeds from the frame counter, stop one
// after the other on the symbols a table of the frame counter chose, and put the strips' tiles in VRAM (the tiles at $550, $560 and $570, where the pre-beta's blocks have no use for them yet)
void Obj_CNZSlotStarter(Sst *self);
void SlotMachine_Reset(void);   // (the level's start: the machine is not started)
void SlotMachine_Process(void); // (every frame)
uint8_t SlotMachine_State(void);
void SlotMachine_Start(void); // (if it is resting)
// The result of the last spin: the reward word that Offset_0x026D94 makes (the pre-beta uses it for nothing)
uint16_t SlotMachine_Result(void);

#endif //_CNZSLOTS_H
