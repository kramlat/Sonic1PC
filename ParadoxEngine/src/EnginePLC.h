#pragma once

#include <stdint.h>
#include <stddef.h>

//PLC structure
typedef struct {
	const uint8_t *art;
	size_t off;
} PLC;

//PLC buffer
extern PLC plc_buffer[32]; // (16 in the original: GameInfo.plc_capacity says how many a game uses)

//PLC IDs: an index into the game's plcs table (the game defines it; the engine only runs the lists it is asked to)
typedef unsigned PlcId;

//A pattern load cue list: the art to load, and where in VRAM each goes
typedef struct {
	size_t plcs;
	const PLC *plc;
} PLCList;
extern const PLCList *plcs[];

//PLC interface
void AddPLC(PlcId plc);
void NewPLC(PlcId plc);
void ClearPLC(void);
void RunPLC(void);
void ProcessDPLC(void);
void ProcessDPLC2(void);
void QuickPLC(PlcId plc);
