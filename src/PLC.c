// Clownacy's implementation

#include "PLC.h"

#include "Nemesis.h"

#include "Backend/VDP.h"

#include <string.h>

// PLC constants
#define PLC_SPEED_1 9 // How many tiles are loaded per frame during a 'loading' state
#define PLC_SPEED_2 3 // How many tiles are loaded per frame while the game's running

// Level art
#include "Resource/Art/GHZ1.h"
#include "Resource/Art/GHZ2.h"
#include "Resource/Art/LZ.h"
#include "Resource/Art/MZ.h"
#include "Resource/Art/SBZ.h"
#include "Resource/Art/SLZ.h"
#include "Resource/Art/SYZ.h"

// Object art
#include "Resource/Art/BigFlash.h"
#include "Resource/Art/Bumper.h"
#include "Resource/Art/BuzzBomber.h"
#include "Resource/Art/Chopper.h"
#include "Resource/Art/Crabmeat.h"
#include "Resource/Art/Explosion.h"
#include "Resource/Art/GHZBall.h"
#include "Resource/Art/GHZBridge.h"
#include "Resource/Art/GHZLog.h"
#include "Resource/Art/GHZRock.h"
#include "Resource/Art/GHZStalk.h"
#include "Resource/Art/GHZSwing.h"
#include "Resource/Art/GHZWall1.h"
#include "Resource/Art/GHZWall2.h"
#include "Resource/Art/LZBlock1.h"
#include "Resource/Art/LZBlock2.h"
#include "Resource/Art/LZBlock3.h"
#include "Resource/Art/Splash.h"
#include "Resource/Art/Water.h"
#include "Resource/Art/LZSpikeBall.h"
#include "Resource/Art/FlapDoor.h"
#include "Resource/Art/Bubbles.h"
#include "Resource/Art/LZDoor1.h"
#include "Resource/Art/LZDoor2.h"
#include "Resource/Art/Harpoon.h"
#include "Resource/Art/Burrobot.h"
#include "Resource/Art/LZPole.h"
#include "Resource/Art/LZWheel.h"
#include "Resource/Art/Gargoyle.h"
#include "Resource/Art/LZSonic.h"
#include "Resource/Art/LZPlatfm.h"
#include "Resource/Art/Orbinaut.h"
#include "Resource/Art/Jaws.h"
#include "Resource/Art/LZSwitch.h"
#include "Resource/Art/Cork.h"
#include "Resource/Art/GameOver.h"
#include "Resource/Art/HUD.h"
#include "Resource/Art/HUDLife.h"
#include "Resource/Art/HiddenBonus.h"
#include "Resource/Art/Invincibility.h"
#include "Resource/Art/Lamppost.h"
#include "Resource/Art/Monitor.h"
#include "Resource/Art/Motobug.h"
#include "Resource/Art/Newtron.h"
#include "Resource/Art/Points.h"
#include "Resource/Art/Ring.h"
#include "Resource/Art/SSBack.h"
#include "Resource/Art/SSChecker.h"
#include "Resource/Art/SSClouds.h"
#include "Resource/Art/SSEmerald.h"
#include "Resource/Art/SSGhost.h"
#include "Resource/Art/SSGlass.h"
#include "Resource/Art/SSGoal.h"
#include "Resource/Art/SSLife.h"
#include "Resource/Art/SSRotate.h"
#include "Resource/Art/SSSpeed.h"
#include "Resource/Art/SSTwinkle.h"
#include "Resource/Art/SSWall.h"
#include "Resource/Art/SSWarp.h"
#include "Resource/Art/SSZone1.h"
#include "Resource/Art/SSZone2.h"
#include "Resource/Art/SSZone3.h"
#include "Resource/Art/SSZone4.h"
#include "Resource/Art/SSZone5.h"
#include "Resource/Art/SSZone6.h"
#include "Resource/Art/Shield.h"
#include "Resource/Art/Signpost.h"
#include "Resource/Art/Spikes.h"
#include "Resource/Art/SpringH.h"
#include "Resource/Art/SpringV.h"
#include "Resource/Art/Rabbit.h"
#include "Resource/Art/Flicky.h"
#include "Resource/Art/Penguin.h"
#include "Resource/Art/Seal.h"
#include "Resource/Art/Squirrel.h"
#include "Resource/Art/Pig.h"
#include "Resource/Art/Chicken.h"

// PLC lists
typedef struct {
    size_t plcs;
    const PLC* plc;
} PLCList;

// ---------------------------------------------------------------------------
// Pattern load cues - standard block 1
// ---------------------------------------------------------------------------
static const PLCList PLC_Main = {
    5,
    (const PLC[]) {
        { Art_Lamppost, 0xF400 },
        { Art_HUD, 0xD940 },
        { Art_HUDLife, 0xFA80 },
        { Art_Ring, 0xF640 },
        { Art_Points, 0xF2E0 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - standard block 2
// ---------------------------------------------------------------------------
static const PLCList PLC_Main2 = {
    3,
    (const PLC[]) {
        { Art_Monitor, 0xD000 },
        { Art_Shield, 0xA820 },
        { Art_Invincibility, 0xAB80 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - explosion
// ---------------------------------------------------------------------------
static const PLCList PLC_Explode = {
    1,
    (const PLC[]) {
        { Art_Explosion, 0xB400 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - game/time	over
// ---------------------------------------------------------------------------
static const PLCList PLC_GameOver = {
    1,
    (const PLC[]) {
        { Art_GameOver, 0xABC0 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Green Hill
// ---------------------------------------------------------------------------
static const PLCList PLC_GHZ = {
    12,
    (const PLC[]) {
        { Art_GHZ1, 0x0000 },
        { Art_GHZ2, 0x39A0 },
        { Art_GHZStalk, 0x6B00 },
        { Art_GHZRock, 0x7A00 },
        { Art_Crabmeat, 0x8000 },
        { Art_BuzzBomber, 0x8880 },
        { Art_Chopper, 0x8F60 },
        { Art_Newtron, 0x9360 },
        { Art_Motobug, 0x9E00 },
        { Art_Spikes, 0xA360 },
        { Art_SpringH, 0xA460 },
        { Art_SpringV, 0xA660 },
    }
};

static const PLCList PLC_GHZ2 = {
    6,
    (const PLC[]) {
        { Art_GHZSwing, 0x7000 },
        { Art_GHZBridge, 0x71C0 },
        { Art_GHZLog, 0x7300 },
        { Art_GHZBall, 0x7540 },
        { Art_GHZWall1, 0xA1E0 },
        { Art_GHZWall2, 0x6980 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Labyrinth
// ---------------------------------------------------------------------------
static const PLCList PLC_LZ = {
    12,
    (const PLC[]) {
        { Art_LZ, 0x0000 },
        { Art_LZBlock1, 0x3C00 },
        { Art_LZBlock2, 0x3E00 },
        { Art_Splash, 0x4B20 },
        { Art_Water, 0x6000 },
        { Art_LZSpikeBall, 0x6200 },
        { Art_FlapDoor, 0x6500 },
        { Art_Bubbles, 0x6900 },
        { Art_LZBlock3, 0x7780 },
        { Art_LZDoor1, 0x7880 },
        { Art_Harpoon, 0x7980 },
        { Art_Burrobot, 0x94C0 },
    }
};

static const PLCList PLC_LZ2 = {
    13,
    (const PLC[]) {
        { Art_LZPole, 0x7BC0 },
        { Art_LZDoor2, 0x7CC0 },
        { Art_LZWheel, 0x7EC0 },
        { Art_Gargoyle, 0x5D20 },
        { Art_LZSonic, 0x8800 },
        { Art_LZPlatfm, 0x89E0 },
        { Art_Orbinaut, 0x8CE0 },
        { Art_Jaws, 0x90C0 },
        { Art_LZSwitch, 0xA1E0 },
        { Art_Cork, 0xA000 },
        { Art_Spikes, 0xA360 },
        { Art_SpringH, 0xA460 },
        { Art_SpringV, 0xA660 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Marble
// ---------------------------------------------------------------------------
static const PLCList PLC_MZ = {
    3,
    (const PLC[]) {
        { Art_MZ, 0x0000 },
        // plcm	Nem_MzMetal, $6000	; metal	blocks
        // plcm	Nem_MzFire, $68A0	; fireballs
        { Art_GHZSwing, 0x7000 },
        // plcm	Nem_MzGlass, $71C0	; green	glassy block
        // plcm	Nem_Lava, $7500		; lava
        { Art_BuzzBomber, 0x8880 },
        // plcm	Nem_Yadrin, $8F60	; yadrin enemy
        // plcm	Nem_Basaran, $9700	; basaran enemy
        // plcm	Nem_Cater, $9FE0	; caterkiller enemy
    }
};

static const PLCList PLC_MZ2 = {
    3,
    (const PLC[]) {
        // plcm	Nem_MzSwitch, $A260	; switch
        { Art_Spikes, 0xA360 },
        { Art_SpringH, 0xA460 },
        { Art_SpringV, 0xA660 },
        // plcm	Nem_MzBlock, $5700	; green	stone block
    }
};

static const PLCList PLC_SLZ = {
    4,
    (const PLC[]) {
        { Art_SLZ, 0x0000 },
        // plcm	Nem_Bomb, $8000		; bomb enemy
        // plcm	Nem_Orbinaut, $8520	; orbinaut enemy
        // plcm	Nem_MzFire, $9000	; fireballs
        // plcm	Nem_SlzBlock, $9C00	; block
        // plcm	Nem_SlzWall, $A260	; breakable wall
        { Art_Spikes, 0xA360 },
        { Art_SpringH, 0xA460 },
        { Art_SpringV, 0xA660 },
    }
};

static const PLCList PLC_SLZ2 = {
    0,
    (const PLC[]) {
        { NULL, 0 }, // ISO C forbids empty initializer braces
        // plcm	Nem_Seesaw, $6E80	; seesaw
        // plcm	Nem_Fan, $7400		; fan
        // plcm	Nem_Pylon, $7980	; foreground pylon
        // plcm	Nem_SlzSwing, $7B80	; swinging platform
        // plcm	Nem_SlzCannon, $9B00	; fireball launcher
        // plcm	Nem_SlzSpike, $9E00	; spikeball
    }
};

static const PLCList PLC_SYZ = {
    3,
    (const PLC[]) {
        { Art_SYZ, 0x0000 },
        { Art_Crabmeat, 0x8000 },
        { Art_BuzzBomber, 0x8880 },
        // plcm	Nem_Yadrin, $8F60	; yadrin enemy
        // plcm	Nem_Roller, $9700	; roller enemy
    }
};

static const PLCList PLC_SYZ2 = {
    4,
    (const PLC[]) {
        { Art_Bumper, 0x7000 },
        // plcm	Nem_SyzSpike1, $72C0	; large	spikeball
        // plcm	Nem_SyzSpike2, $7740	; small	spikeball
        // plcm	Nem_Cater, $9FE0	; caterkiller enemy
        // plcm	Nem_LzSwitch, $A1E0	; switch
        { Art_Spikes, 0xA360 },
        { Art_SpringH, 0xA460 },
        { Art_SpringV, 0xA660 },
    }
};

static const PLCList PLC_SBZ = {
    1,
    (const PLC[]) {
        { Art_SBZ, 0x0000 },
        // plcm	Nem_Stomper, $5800	; moving platform and stomper
        // plcm	Nem_SbzDoor1, $5D00	; door
        // plcm	Nem_Girder, $5E00	; girder
        // plcm	Nem_BallHog, $6040	; ball hog enemy
        // plcm	Nem_SbzWheel1, $6880	; spot on large	wheel
        // plcm	Nem_SbzWheel2, $6900	; wheel	that grabs Sonic
        // plcm	Nem_SyzSpike1, $7220	; large	spikeball
        // plcm	Nem_Cutter, $76A0	; pizza	cutter
        // plcm	Nem_FlamePipe, $7B20	; flaming pipe
        // plcm	Nem_SbzFloor, $7EA0	; collapsing floor
        // plcm	Nem_SbzBlock, $9860	; vanishing block
    }
};

static const PLCList PLC_SBZ2 = {
    3,
    (const PLC[]) {
        // plcm	Nem_Cater, $5600	; caterkiller enemy
        // plcm	Nem_Bomb, $8000		; bomb enemy
        // plcm	Nem_Orbinaut, $8520	; orbinaut enemy
        // plcm	Nem_SlideFloor, $8C00	; floor	that slides away
        // plcm	Nem_SbzDoor2, $8DE0	; horizontal door
        // plcm	Nem_Electric, $8FC0	; electric orb
        // plcm	Nem_TrapDoor, $9240	; trapdoor
        // plcm	Nem_SbzFloor, $7F20	; collapsing floor
        // plcm	Nem_SpinPform, $9BE0	; small	spinning platform
        // plcm	Nem_LzSwitch, $A1E0	; switch
        { Art_Spikes, 0xA360 },
        { Art_SpringH, 0xA460 },
        { Art_SpringV, 0xA660 },
    }
};

static const PLCList PLC_TitleCard = {
    0,
    (const PLC[]) {
        // plcm	Nem_TitleCard, ArtTile_Title_Card
    }
};

static const PLCList PLC_Boss = {
    0,
    (const PLC[]) {
        // plcm	Nem_Eggman,   ArtTile_Eggman           ; Eggman main patterns
        // plcm	Nem_Weapons,  ArtTile_Eggman_Weapons   ; Eggman's weapons
        // plcm	Nem_Prison,   ArtTile_Prison_Capsule   ; prison capsule
        // plcm	Nem_Bomb,     ArtTile_Eggman_Spikeball ; bomb enemy (gets overwritten)
        // plcm	Nem_SlzSpike, ArtTile_Eggman_Spikeball ; spikeball (SLZ boss)
        // plcm	Nem_Exhaust,  ArtTile_Eggman_Exhaust   ; exhaust flame
    }
};

static const PLCList PLC_Signpost = {
    3,
    (const PLC[]) {
        { Art_Signpost, 0xD000 },
        { Art_HiddenBonus, 0x96C0 },
        { Art_BigFlash, 0x8C40 },
    }
};

static const PLCList PLC_Warp = {
    0,
    (const PLC[]) {
        // plcm	Nem_Warp, ArtTile_Warp
    }
};

static const PLCList PLC_SpecialStage = {
    17,
    (const PLC[]) {
        { Art_SSClouds, 0x0000 },
        { Art_SSBack, 0x0A20 },
        { Art_SSWall, 0x2840 },
        { Art_Bumper, 0x4760 },
        { Art_SSGoal, 0x4A20 },
        { Art_SSSpeed, 0x4C60 },
        { Art_SSRotate, 0x5E00 },
        { Art_SSLife, 0x6E00 },
        { Art_SSTwinkle, 0x7E00 },
        { Art_SSChecker, 0x8E00 },
        { Art_SSGhost, 0x9E00 },
        { Art_SSWarp, 0xAE00 },
        { Art_SSGlass, 0xBE00 },
        { Art_SSEmerald, 0xEE00 },
        { Art_SSZone1, 0xF2E0 },
        { Art_SSZone2, 0xF400 },
        { Art_SSZone3, 0xF520 },
        // These last 3 are unused
        { Art_SSZone4, 0xF2E0 },
        { Art_SSZone5, 0xF400 },
        { Art_SSZone6, 0xF520 },
    }
};

static const PLCList PLC_GHZAnimals = {
    2,
    (const PLC[]) {
        { Art_Rabbit, 0xB000 },
        { Art_Flicky, 0xB240 },
    }
};

static const PLCList PLC_LZAnimals = {
    2,
    (const PLC[]) {
        { Art_Penguin, 0xB000 },
        { Art_Seal, 0xB240 },
    }
};

static const PLCList PLC_MZAnimals = {
    2,
    (const PLC[]) {
        { Art_Squirrel, 0xB000 },
        { Art_Seal, 0xB240 },
    }
};

static const PLCList PLC_SLZAnimals = {
    2,
    (const PLC[]) {
        { Art_Pig, 0xB000 },
        { Art_Flicky, 0xB240 },
    }
};

static const PLCList PLC_SYZAnimals = {
    2,
    (const PLC[]) {
        { Art_Pig, 0xB000 },
        { Art_Chicken, 0xB240 },
    }
};

static const PLCList PLC_SBZAnimals = {
    2,
    (const PLC[]) {
        { Art_Rabbit, 0xB000 },
        { Art_Chicken, 0xB240 },
    }
};

static const PLCList PLC_SSResult = {
    0,
    (const PLC[]) {
        // plcm	Nem_ResultEm,  ArtTile_SS_Results_Emeralds ; emeralds
        // plcm	Nem_MiniSonic, ArtTile_Mini_Sonic          ; mini Sonic
    }
};

static const PLCList PLC_Ending = {
    0,
    (const PLC[]) {
        // plcm	Nem_GHZ_1st,   ArtTile_Level            ; GHZ main patterns
        // plcm	Nem_GHZ_2nd,   ArtTile_Level+$1CD       ; GHZ secondary patterns
        // plcm	Nem_Stalk,     ArtTile_GHZ_Flower_Stalk ; flower stalk
        // plcm	Nem_EndFlower, ArtTile_Ending_Flowers   ; flowers
        // plcm	Nem_EndEm,     ArtTile_Ending_Emeralds  ; emeralds
        // plcm	Nem_EndSonic,  ArtTile_Ending_Sonic     ; Sonic
        // plcm	Nem_EndEggman, ArtTile_Ending_Eggman    ; Eggman's death (unused)
        // plcm	Nem_Rabbit,    ArtTile_Ending_Rabbit    ; rabbit
        // plcm	Nem_Chicken,   ArtTile_Ending_Chicken   ; chicken
        // plcm	Nem_Penguin,   ArtTile_Ending_Penguin   ; penguin
        // plcm	Nem_Seal,      ArtTile_Ending_Seal      ; seal
        // plcm	Nem_Pig,       ArtTile_Ending_Pig       ; pig
        // plcm	Nem_Flicky,    ArtTile_Ending_Flicky    ; flicky
        // plcm	Nem_Squirrel,  ArtTile_Ending_Squirrel  ; squirrel
        // plcm	Nem_EndStH,    ArtTile_Ending_STH       ; "SONIC THE HEDGEHOG"
    }
};

static const PLCList PLC_TryAgain = {
    0,
    (const PLC[]) {
        // plcm	Nem_EndEm,      ArtTile_Try_Again_Emeralds ; emeralds
        // plcm	Nem_TryAgain,   ArtTile_Try_Again_Eggman   ; Eggman
        // plcm	Nem_CreditText, ArtTile_Credits_Font       ; credits alphabet
    }
};

static const PLCList PLC_EggmanSBZ2 = {
    0,
    (const PLC[]) {
        // plcm	Nem_SbzBlock,   ArtTile_Eggman_Trap_Floor ; block
        // plcm	Nem_Sbz2Eggman, ArtTile_Eggman            ; Eggman
        // plcm	Nem_LzSwitch,   ArtTile_Eggman_Button-4   ; switch
    }
};

static const PLCList PLC_FZBoss = {
    0,
    (const PLC[]) {
        // plcm	Nem_FzEggman,   ArtTile_FZ_Eggman_Fleeing    ; Eggman after boss
        // plcm	Nem_FzBoss,     ArtTile_FZ_Boss              ; FZ boss
        // plcm	Nem_Eggman,     ArtTile_Eggman               ; Eggman main patterns
        // plcm	Nem_Sbz2Eggman, ArtTile_FZ_Eggman_No_Vehicle ; Eggman without ship
        // plcm	Nem_Exhaust,    ArtTile_Eggman_Exhaust       ; exhaust flame
    }
};


// PLC list
static const PLCList* plcs[PlcId_Num] = {
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
    /* PlcId_TitleCard   */ &PLC_TitleCard,
    /* PlcId_Boss        */ &PLC_Boss,
    /* PlcId_Signpost    */ &PLC_Signpost,
    /* PlcId_Warp        */ &PLC_Warp,
    /* PlcId_SpecialStage*/ &PLC_SpecialStage,
    /* PlcId_GHZAnimals  */ &PLC_GHZAnimals,
    /* PlcId_LZAnimals   */ &PLC_LZAnimals,
    /* PlcId_MZAnimals   */ &PLC_MZAnimals,
    /* PlcId_SLZAnimals  */ &PLC_SLZAnimals,
    /* PlcId_SYZAnimals  */ &PLC_SYZAnimals,
    /* PlcId_SBZAnimals  */ &PLC_SBZAnimals,
    /* PlcId_SSResult    */ &PLC_SSResult,
    /* PlcId_Ending      */ &PLC_Ending,
    /* PlcId_TryAgain    */ &PLC_TryAgain,
    /* PlcId_EggmanSBZ2  */ &PLC_EggmanSBZ2,
    /* PlcId_FZBoss      */ &PLC_FZBoss,
};

// PLC state
PLC plc_buffer[16];

static NemesisState plc_buffer_regs;
static uint16_t plc_buffer_reg18;
static uint16_t plc_buffer_reg1A;

// PLC interface
void AddPLC(PlcId plc) {
    // Get PLC list to load
    const PLCList* list = plcs[plc];
    if (list == NULL)
        return;

    // Find empty PLC slot
    PLC* plc_free = plc_buffer;
    while (plc_free->art != NULL)
        plc_free++;

    // Push PLCs to buffer
    for (size_t i = 0; i < list->plcs; i++)
        plc_free[i] = list->plc[i];
}

void NewPLC(PlcId plc) {
    // Get PLC list to load
    const PLCList* list = plcs[plc];
    if (list == NULL)
        return;

    // Clear previous PLCs
    ClearPLC();

    // Push PLCs to buffer
    for (size_t i = 0; i < list->plcs; i++)
        plc_buffer[i] = list->plc[i];
}

void ClearPLC(void) {
    // Clear PLC buffer
    plc_buffer_reg18 = 0;
    memset(plc_buffer, 0, sizeof(plc_buffer));
}

void RunPLC(void) {
    if (plc_buffer[0].art != NULL && plc_buffer_reg18 == 0) {
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

static void ProcessDPLC_Main(size_t off) {
    VDP_SeekVRAM(off);

    do {
        plc_buffer_regs.remaining = 8;

        // Inlined NemDec_WriteIter
        plc_buffer_regs.d3 = 8;
        plc_buffer_regs.d4 = 0;

        NemDecRun(&plc_buffer_regs);

        if (--plc_buffer_reg18 == 0) {
            // Pop one request off the buffer so that the next one can be filled
            for (size_t i = 0; i < sizeof(plc_buffer) / sizeof(*plc_buffer) - 1; i++)
                plc_buffer[i] = plc_buffer[i + 1];
            return;
        }
    } while (--plc_buffer_reg1A != 0);
}

void ProcessDPLC(void) {
    if (plc_buffer_reg18 != 0) {
        plc_buffer_reg1A = PLC_SPEED_1; // Process PLC_SPEED_1 tiles

        size_t off = plc_buffer[0].off;
        plc_buffer[0].off += PLC_SPEED_1 * 0x20;

        ProcessDPLC_Main(off);
    }
}

void ProcessDPLC2(void) {
    if (plc_buffer_reg18 != 0) {
        plc_buffer_reg1A = PLC_SPEED_2; // Process PLC_SPEED_2 tiles

        size_t off = plc_buffer[0].off;
        plc_buffer[0].off += PLC_SPEED_2 * 0x20;

        ProcessDPLC_Main(off);
    }
}

void QuickPLC(PlcId plc) {
    // Get PLC list to load and decompress immediately
    const PLCList* list = plcs[plc];
    if (list == NULL)
        return;
    for (size_t i = 0; i < list->plcs; i++) {
        VDP_SeekVRAM(list->plc[i].off);
        NemDec(list->plc[i].art);
    }
}
