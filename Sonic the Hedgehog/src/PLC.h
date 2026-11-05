#pragma once

#include "EnginePLC.h"

//Sonic 1 PLC ids (the order of plcs in Sonic1PLC.c)
enum {
	PlcId_Main,
	PlcId_Main2,
	PlcId_Explode,
	PlcId_GameOver,
	PlcId_GHZ,
	PlcId_GHZ2,
	PlcId_LZ,
	PlcId_LZ2,
	PlcId_MZ,
	PlcId_MZ2,
	PlcId_SLZ,
	PlcId_SLZ2,
	PlcId_SYZ,
	PlcId_SYZ2,
	PlcId_SBZ,
	PlcId_SBZ2,
	PlcId_TitleCard,
	PlcId_Boss,
	PlcId_Signpost,
	PlcId_Warp,
	PlcId_SpecialStage,
	PlcId_GHZAnimals,
	PlcId_LZAnimals,
	PlcId_MZAnimals,
	PlcId_SLZAnimals,
	PlcId_SYZAnimals,
	PlcId_SBZAnimals,
	PlcId_SSResult,
	PlcId_Ending,
	PlcId_TryAgain,
	PlcId_EggmanSBZ2,
	PlcId_FZBoss,
#ifdef PLC_IDS_EXTRA // a game built on this one with pattern load cues of its own (Sonic 2's zones that are not in Sonic 1's slots): it names them in PlcIdsExtra.h
#include "PlcIdsExtra.h"
#endif
	PlcId_Num,
};

//Level art
extern const uint8_t Art_GHZ1[];
extern const uint8_t Art_GHZ2[];
extern const uint8_t Art_LZ[];
extern const uint8_t Art_MZ[];
extern const uint8_t Art_SLZ[];
extern const uint8_t Art_SYZ[];
extern const uint8_t Art_SBZ[];

