//Clownacy's implementation

#include "PLC.h"

#include "Nemesis.h"

#include <string.h>

//Level art
const uint8_t art_ghz1[] = {
	#include <Resource/Art/GHZ1.h>
	,0,
};
const uint8_t art_ghz2[] = {
	#include <Resource/Art/GHZ2.h>
	,0,
};
const uint8_t art_lz[] = {
	#include <Resource/Art/LZ.h>
	,0,
};
const uint8_t art_mz[] = {
	#include <Resource/Art/MZ.h>
	,0,
};
const uint8_t art_slz[] = {
	#include <Resource/Art/SLZ.h>
	,0,
};
const uint8_t art_syz[] = {
	#include <Resource/Art/SYZ.h>
	,0,
};
const uint8_t art_sbz[] = {
	#include <Resource/Art/SBZ.h>
	,0,
};

//PLC lists
typedef struct
{
	size_t plcs;
	const PLC *plc;
} PLCList;

static const PLCList PLC_Main = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Lamp, $F400		; lamppost
		//plcm	Nem_Hud, $D940		; HUD
		//plcm	Nem_Lives, $FA80	; lives	counter
		//plcm	Nem_Ring, $F640 	; rings
		//plcm	Nem_Points, $F2E0	; points from enemy
	}
};

static const PLCList PLC_Main2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Monitors, $D000	; monitors
		//plcm	Nem_Shield, $A820	; shield
		//plcm	Nem_Stars, $AB80	; invincibility	stars
	}
};

static const PLCList PLC_Explode = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Explode, $B400	; explosion
	}
};

static const PLCList PLC_GameOver = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_GameOver, $ABC0	; game/time over
	}
};

static const PLCList PLC_GHZ = {
	2,
	(const PLC[]){
		{art_ghz1, 0x0000},
		{art_ghz2, 0x39A0},
		//plcm	Nem_Stalk, $6B00	; flower stalk
		//plcm	Nem_PplRock, $7A00	; purple rock
		//plcm	Nem_Crabmeat, $8000	; crabmeat enemy
		//plcm	Nem_Buzz, $8880		; buzz bomber enemy
		//plcm	Nem_Chopper, $8F60	; chopper enemy
		//plcm	Nem_Newtron, $9360	; newtron enemy
		//plcm	Nem_Motobug, $9E00	; motobug enemy
		//plcm	Nem_Spikes, $A360	; spikes
		//plcm	Nem_HSpring, $A460	; horizontal spring
		//plcm	Nem_VSpring, $A660	; vertical spring
	}
};

static const PLCList PLC_GHZ2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Swing, $7000	; swinging platform
		//plcm	Nem_Bridge, $71C0	; bridge
		//plcm	Nem_SpikePole, $7300	; spiked pole
		//plcm	Nem_Ball, $7540		; giant	ball
		//plcm	Nem_GhzWall1, $A1E0	; breakable wall
		//plcm	Nem_GhzWall2, $6980	; normal wall
	}
};

static const PLCList PLC_LZ = {
	1,
	(const PLC[]){
		{art_lz, 0x0000},
		//plcm	Nem_LzBlock1, $3C00	; block
		//plcm	Nem_LzBlock2, $3E00	; blocks
		//plcm	Nem_Splash, $4B20	; waterfalls and splash
		//plcm	Nem_Water, $6000	; water	surface
		//plcm	Nem_LzSpikeBall, $6200	; spiked ball
		//plcm	Nem_FlapDoor, $6500	; flapping door
		//plcm	Nem_Bubbles, $6900	; bubbles and numbers
		//plcm	Nem_LzBlock3, $7780	; block
		//plcm	Nem_LzDoor1, $7880	; vertical door
		//plcm	Nem_Harpoon, $7980	; harpoon
		//plcm	Nem_Burrobot, $94C0	; burrobot enemy
	}
};

static const PLCList PLC_LZ2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_LzPole, $7BC0	; pole that breaks
		//plcm	Nem_LzDoor2, $7CC0	; large	horizontal door
		//plcm	Nem_LzWheel, $7EC0	; wheel
		//plcm	Nem_Gargoyle, $5D20	; gargoyle head
		//if Revision=0
		//plcm	Nem_LzSonic, $8800	; Sonic	holding	his breath
		//else
		//endc
		//plcm	Nem_LzPlatfm, $89E0	; rising platform
		//plcm	Nem_Orbinaut, $8CE0	; orbinaut enemy
		//plcm	Nem_Jaws, $90C0		; jaws enemy
		//plcm	Nem_LzSwitch, $A1E0	; switch
		//plcm	Nem_Cork, $A000		; cork block
		//plcm	Nem_Spikes, $A360	; spikes
		//plcm	Nem_HSpring, $A460	; horizontal spring
		//plcm	Nem_VSpring, $A660	; vertical spring
	}
};

static const PLCList PLC_MZ = {
	1,
	(const PLC[]){
		{art_mz, 0x0000},
		//plcm	Nem_MzMetal, $6000	; metal	blocks
		//plcm	Nem_MzFire, $68A0	; fireballs
		//plcm	Nem_Swing, $7000	; swinging platform
		//plcm	Nem_MzGlass, $71C0	; green	glassy block
		//plcm	Nem_Lava, $7500		; lava
		//plcm	Nem_Buzz, $8880		; buzz bomber enemy
		//plcm	Nem_Yadrin, $8F60	; yadrin enemy
		//plcm	Nem_Basaran, $9700	; basaran enemy
		//plcm	Nem_Cater, $9FE0	; caterkiller enemy
	}
};

static const PLCList PLC_MZ2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_MzSwitch, $A260	; switch
		//plcm	Nem_Spikes, $A360	; spikes
		//plcm	Nem_HSpring, $A460	; horizontal spring
		//plcm	Nem_VSpring, $A660	; vertical spring
		//plcm	Nem_MzBlock, $5700	; green	stone block
	}
};

static const PLCList PLC_SLZ = {
	1,
	(const PLC[]){
		{art_slz, 0x0000},
		//plcm	Nem_Bomb, $8000		; bomb enemy
		//plcm	Nem_Orbinaut, $8520	; orbinaut enemy
		//plcm	Nem_MzFire, $9000	; fireballs
		//plcm	Nem_SlzBlock, $9C00	; block
		//plcm	Nem_SlzWall, $A260	; breakable wall
		//plcm	Nem_Spikes, $A360	; spikes
		//plcm	Nem_HSpring, $A460	; horizontal spring
		//plcm	Nem_VSpring, $A660	; vertical spring
	}
};

static const PLCList PLC_SLZ2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Seesaw, $6E80	; seesaw
		//plcm	Nem_Fan, $7400		; fan
		//plcm	Nem_Pylon, $7980	; foreground pylon
		//plcm	Nem_SlzSwing, $7B80	; swinging platform
		//plcm	Nem_SlzCannon, $9B00	; fireball launcher
		//plcm	Nem_SlzSpike, $9E00	; spikeball
	}
};

static const PLCList PLC_SYZ = {
	1,
	(const PLC[]){
		{art_syz, 0x0000},
		//plcm	Nem_Crabmeat, $8000	; crabmeat enemy
		//plcm	Nem_Buzz, $8880		; buzz bomber enemy
		//plcm	Nem_Yadrin, $8F60	; yadrin enemy
		//plcm	Nem_Roller, $9700	; roller enemy
	}
};

static const PLCList PLC_SYZ2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Bumper, $7000	; bumper
		//plcm	Nem_SyzSpike1, $72C0	; large	spikeball
		//plcm	Nem_SyzSpike2, $7740	; small	spikeball
		//plcm	Nem_Cater, $9FE0	; caterkiller enemy
		//plcm	Nem_LzSwitch, $A1E0	; switch
		//plcm	Nem_Spikes, $A360	; spikes
		//plcm	Nem_HSpring, $A460	; horizontal spring
		//plcm	Nem_VSpring, $A660	; vertical spring
	}
};

static const PLCList PLC_SBZ = {
	1,
	(const PLC[]){
		{art_sbz, 0x0000},
		//plcm	Nem_Stomper, $5800	; moving platform and stomper
		//plcm	Nem_SbzDoor1, $5D00	; door
		//plcm	Nem_Girder, $5E00	; girder
		//plcm	Nem_BallHog, $6040	; ball hog enemy
		//plcm	Nem_SbzWheel1, $6880	; spot on large	wheel
		//plcm	Nem_SbzWheel2, $6900	; wheel	that grabs Sonic
		//plcm	Nem_SyzSpike1, $7220	; large	spikeball
		//plcm	Nem_Cutter, $76A0	; pizza	cutter
		//plcm	Nem_FlamePipe, $7B20	; flaming pipe
		//plcm	Nem_SbzFloor, $7EA0	; collapsing floor
		//plcm	Nem_SbzBlock, $9860	; vanishing block
	}
};

static const PLCList PLC_SBZ2 = {
	0,
	(const PLC[]){
		{NULL, 0}, //ISO C forbids empty initializer braces
		//plcm	Nem_Cater, $5600	; caterkiller enemy
		//plcm	Nem_Bomb, $8000		; bomb enemy
		//plcm	Nem_Orbinaut, $8520	; orbinaut enemy
		//plcm	Nem_SlideFloor, $8C00	; floor	that slides away
		//plcm	Nem_SbzDoor2, $8DE0	; horizontal door
		//plcm	Nem_Electric, $8FC0	; electric orb
		//plcm	Nem_TrapDoor, $9240	; trapdoor
		//plcm	Nem_SbzFloor, $7F20	; collapsing floor
		//plcm	Nem_SpinPform, $9BE0	; small	spinning platform
		//plcm	Nem_LzSwitch, $A1E0	; switch
		//plcm	Nem_Spikes, $A360	; spikes
		//plcm	Nem_HSpring, $A460	; horizontal spring
		//plcm	Nem_VSpring, $A660	; vertical spring
	}
};

static const PLCList *plcs[PlcId_Num] = {
	/* PlcId_Main        */ &PLC_Main,
	/* PlcId_Main2       */ &PLC_Main2,
	/* PlcId_Explode     */ &PLC_Explode,
	/* PlcId_GameOver    */ &PLC_GameOver,
	/* PlcId_GHZ         */ &PLC_GHZ,
	/* PlcId_GHZ2        */ &PLC_GHZ2,
	/* PlcId_LZ          */ &PLC_LZ,
	/* PlcId_LZ2         */ &PLC_LZ2,
	/* PlcId_MZ          */ &PLC_MZ,
	/* PlcId_MZ2         */ &PLC_MZ2,
	/* PlcId_SLZ         */ &PLC_SLZ,
	/* PlcId_SLZ2        */ &PLC_SLZ2,
	/* PlcId_SYZ         */ &PLC_SYZ,
	/* PlcId_SYZ2        */ &PLC_SYZ2,
	/* PlcId_SBZ         */ &PLC_SBZ,
	/* PlcId_SBZ2        */ &PLC_SBZ2,
	/* PlcId_TitleCard   */ NULL,
	/* PlcId_Boss        */ NULL,
	/* PlcId_Signpost    */ NULL,
	/* PlcId_Warp        */ NULL,
	/* PlcId_SpecialStage*/ NULL,
	/* PlcId_GHZAnimals  */ NULL,
	/* PlcId_LZAnimals   */ NULL,
	/* PlcId_MZAnimals   */ NULL,
	/* PlcId_SLZAnimals  */ NULL,
	/* PlcId_SYZAnimals  */ NULL,
	/* PlcId_SBZAnimals  */ NULL,
	/* PlcId_SSResult    */ NULL,
	/* PlcId_Ending      */ NULL,
	/* PlcId_TryAgain    */ NULL,
	/* PlcId_EggmanSBZ2  */ NULL,
	/* PlcId_FZBoss      */ NULL,
};

//PLC state
PLC plc_buffer[16];

static NemesisState plc_buffer_regs;
static uint16_t plc_buffer_reg18;
static uint16_t plc_buffer_reg1A;

//PLC interface
void AddPLC(PlcId plc)
{
	//Get PLC list to load
	const PLCList *list = plcs[plc];
	if (list == NULL)
		return;
	
	//Find empty PLC slot
	PLC *plc_free = plc_buffer;
	while (plc_free->art != NULL)
		plc_free++;
	
	//Push PLCs to buffer
	for (size_t i = 0; i < list->plcs; i++)
		plc_free[i] = list->plc[i];
}

void NewPLC(PlcId plc)
{
	//Get PLC list to load
	const PLCList *list = plcs[plc];
	if (list == NULL)
		return;
	
	//Clear previous PLCs
	ClearPLC();
	
	//Push PLCs to buffer
	for (size_t i = 0; i < list->plcs; i++)
		plc_buffer[i] = list->plc[i];
}

void ClearPLC()
{
	//Clear PLC buffer
	memset(plc_buffer, 0, sizeof(plc_buffer));
}

void RunPLC()
{
	if (plc_buffer[0].art != NULL && plc_buffer_reg18 == 0)
	{
		plc_buffer_regs.source = plc_buffer[0].art;
		plc_buffer_regs.vram_mode = true;
		plc_buffer_regs.dictionary = nemesis_buffer;
		
		uint16_t header = (plc_buffer_regs.source[0] << 8) | plc_buffer_regs.source[1];
		
		plc_buffer_regs.source += 2;
		plc_buffer_regs.xor_mode = header & 0x8000;
		plc_buffer_reg18 = header & 0x7FFF;
		
		NemDecPrepare(&plc_buffer_regs);
		
		plc_buffer_regs.d5 = (plc_buffer_regs.source[0] << 8) | plc_buffer_regs.source[1];
		plc_buffer_regs.source += 2;
		
		plc_buffer_regs.d0 = 0;
		plc_buffer_regs.d1 = 0;
		plc_buffer_regs.d2 = 0;
		plc_buffer_regs.d6 = 0x10;
	}
}

static void ProcessDPLC_Main(size_t off)
{
	NemDecSeek(off);
	
	do
	{
		plc_buffer_regs.remaining = 8;
		
		//Inlined NemDec_WriteIter
		plc_buffer_regs.d3 = 8;
		plc_buffer_regs.d4 = 0;
		
		NemDecRun(&plc_buffer_regs);
		
		if (--plc_buffer_reg18 == 0)
		{
			//Pop one request off the buffer so that the next one can be filled
			for (size_t i = 0; i < sizeof(plc_buffer) / sizeof(*plc_buffer) - 1; i++)
				plc_buffer[i] = plc_buffer[i + 1];
			return;
		}
	} while (--plc_buffer_reg1A != 0);
}

void ProcessDPLC()
{
	if (plc_buffer_reg18 != 0)
	{
		plc_buffer_reg1A = 9; //Process 9 tiles
		
		size_t off = plc_buffer[0].off;
		plc_buffer[0].off += 9 * 0x20;
		
		ProcessDPLC_Main(off);
	}
}

void ProcessDPLC2()
{
	if (plc_buffer_reg18 != 0)
	{
		plc_buffer_reg1A = 3; //Process 3 tiles
		
		size_t off = plc_buffer[0].off;
		plc_buffer[0].off += 3 * 0x20;
		
		ProcessDPLC_Main(off);
	}
}

void QuickPLC(PlcId plc)
{
	//Get PLC list to load and decompress immediately
	const PLCList *list = plcs[plc];
	if (list == NULL)
		return;
	for (size_t i = 0; i < list->plcs; i++)
		NemDec(list->plc[i].off, list->plc[i].art);
}
