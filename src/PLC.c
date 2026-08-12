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
#include "Resource/Art/TitleCard.h"
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
#include "Resource/Art/MZMetal.h"
#include "Resource/Art/MZFire.h"
#include "Resource/Art/MZGlass.h"
#include "Resource/Art/Lava.h"
#include "Resource/Art/Yadrin.h"
#include "Resource/Art/Basaran.h"
#include "Resource/Art/Caterkiller.h"
#include "Resource/Art/MZSwitch.h"
#include "Resource/Art/MZBlock.h"
#include "Resource/Art/Bomb.h"
#include "Resource/Art/SLZBlock.h"
#include "Resource/Art/SLZWall.h"
#include "Resource/Art/Seesaw.h"
#include "Resource/Art/Fan.h"
#include "Resource/Art/Pylon.h"
#include "Resource/Art/SLZSwing.h"
#include "Resource/Art/SLZCannon.h"
#include "Resource/Art/SLZSpike.h"
#include "Resource/Art/Roller.h"
#include "Resource/Art/SYZSpike1.h"
#include "Resource/Art/SYZSpike2.h"
#include "Resource/Art/Stomper.h"
#include "Resource/Art/SBZDoor1.h"
#include "Resource/Art/SBZDoor2.h"
#include "Resource/Art/Girder.h"
#include "Resource/Art/Ballhog.h"
#include "Resource/Art/SBZWheel1.h"
#include "Resource/Art/SBZWheel2.h"
#include "Resource/Art/Cutter.h"
#include "Resource/Art/FlamePipe.h"
#include "Resource/Art/SBZFloor.h"
#include "Resource/Art/SBZBlock.h"
#include "Resource/Art/SlideFloor.h"
#include "Resource/Art/Electric.h"
#include "Resource/Art/TrapDoor.h"
#include "Resource/Art/SPinPform.h"
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
#include "Resource/Art/Eggman.h"
#include "Resource/Art/Weapons.h"
#include "Resource/Art/Prison.h"
#include "Resource/Art/Exhaust.h"
#include "Resource/Art/Warp.h"
#include "Resource/Art/ResultEm.h"
#include "Resource/Art/MiniSonic.h"
#include "Resource/Art/EndFlower.h"
#include "Resource/Art/EndEm.h"
#include "Resource/Art/EndSonic.h"
#include "Resource/Art/EndEggman.h"
#include "Resource/Art/EndStH.h"
#include "Resource/Art/TryAgain.h"
#include "Resource/Art/CreditText.h"
#include "Resource/Art/FZEggman.h"
#include "Resource/Art/FZBoss.h"
#include "Resource/Art/SBZ2Eggman.h"

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
        { Art_Lamppost,      0xD800 }, // moved from 0xF400 to make room for the Spin Dash dust (see Splash.h)
        { Art_HUD,           0xD940 },
        { Art_HUDLife,       0xFA80 },
        { Art_Ring,          0xF640 },
        { Art_Points,        0xF2E0 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - standard block 2
// ---------------------------------------------------------------------------
static const PLCList PLC_Main2 = {
    3,
    (const PLC[]) {
        { Art_Monitor,       0xD000 },
        { Art_Shield,        0xA820 },
        { Art_Invincibility, 0xAB80 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - explosion
// ---------------------------------------------------------------------------
static const PLCList PLC_Explode = {
    1,
    (const PLC[]) {
        { Art_Explosion,     0xB400 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - game/time	over
// ---------------------------------------------------------------------------
static const PLCList PLC_GameOver = {
    1,
    (const PLC[]) {
        { Art_GameOver,      0xABC0 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Green Hill
// ---------------------------------------------------------------------------
static const PLCList PLC_GHZ = {
    10,
    (const PLC[]) {
        { Art_GHZStalk,      0x6B00 },
        { Art_GHZRock,       0x7A00 },
        { Art_Crabmeat,      0x8000 },
        { Art_BuzzBomber,    0x8880 },
        { Art_Chopper,       0x8F60 },
        { Art_Newtron,       0x9360 },
        { Art_Motobug,       0x9E00 },
        { Art_Spikes,        0xA360 },
        { Art_SpringH,       0xA460 },
        { Art_SpringV,       0xA660 },
    }
};

static const PLCList PLC_GHZ2 = {
    6,
    (const PLC[]) {
        { Art_GHZSwing,      0x7000 },
        { Art_GHZBridge,     0x71C0 },
        { Art_GHZLog,        0x7300 },
        { Art_GHZBall,       0x7540 },
        { Art_GHZWall1,      0xA1E0 },
        { Art_GHZWall2,      0x6980 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Labyrinth
// ---------------------------------------------------------------------------
static const PLCList PLC_LZ = {
    10,
    (const PLC[]) {
        { Art_LZBlock1,      0x3C00 },
        { Art_LZBlock2,      0x3E00 },
        { Art_Water,         0x6000 },
        { Art_LZSpikeBall,   0x6200 },
        { Art_FlapDoor,      0x6500 },
        { Art_Bubbles,       0x6900 },
        { Art_LZBlock3,      0x7780 },
        { Art_LZDoor1,       0x7880 },
        { Art_Harpoon,       0x7980 },
        { Art_Burrobot,      0x94C0 },
    }
};

static const PLCList PLC_LZ2 = {
    13,
    (const PLC[]) {
        { Art_LZPole,        0x7BC0 },
        { Art_LZDoor2,       0x7CC0 },
        { Art_LZWheel,       0x7EC0 },
        { Art_Gargoyle,      0x5D20 },
        { Art_LZSonic,       0x8800 },
        { Art_LZPlatfm,      0x89E0 },
        { Art_Orbinaut,      0x8CE0 },
        { Art_Jaws,          0x90C0 },
        { Art_LZSwitch,      0xA1E0 },
        { Art_Cork,          0xA000 },
        { Art_Spikes,        0xA360 },
        { Art_SpringH,       0xA460 },
        { Art_SpringV,       0xA660 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Marble
// ---------------------------------------------------------------------------
static const PLCList PLC_MZ = {
    9,
    (const PLC[]) {
        { Art_MZMetal,       0x6000 },
        { Art_MZFire,        0x68A0 },
        { Art_GHZSwing,      0x7000 },
        { Art_MZGlass,       0x71C0 },
        { Art_Lava,          0x7500 },
        { Art_BuzzBomber,    0x8880 },
        { Art_Yadrin,        0x8F60 },
        { Art_Basaran,       0x9700 },
        { Art_Caterkiller,   0x9FE0 },
    }
};

static const PLCList PLC_MZ2 = {
    5,
    (const PLC[]) {
        { Art_MZSwitch,      0xA260 },
        { Art_Spikes,        0xA360 },
        { Art_SpringH,       0xA460 },
        { Art_SpringV,       0xA660 },
        { Art_MZBlock,       0x5700 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Star Light
// ---------------------------------------------------------------------------
static const PLCList PLC_SLZ = {
    8,
    (const PLC[]) {
        { Art_Bomb,          0x8520 },
        { Art_Orbinaut,      0x8520 },
        { Art_MZFire,        0x9000 },
        { Art_SLZBlock,      0x9C00 },
        { Art_SLZWall,       0xA260 },
        { Art_Spikes,        0xA360 },
        { Art_SpringH,       0xA460 },
        { Art_SpringV,       0xA660 },
    }
};

static const PLCList PLC_SLZ2 = {
    6,
    (const PLC[]) {
        { Art_Seesaw,        0x6E80 },
        { Art_Fan,           0x7400 },
        { Art_Pylon,         0x7980 },
        { Art_SLZSwing,      0x7B80 },
        { Art_SLZCannon,     0x9B00 },
        { Art_SLZSpike,      0x9E00 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Spring Yard
// ---------------------------------------------------------------------------
static const PLCList PLC_SYZ = {
    4,
    (const PLC[]) {
        { Art_Crabmeat,      0x8000 },
        { Art_BuzzBomber,    0x8880 },
        { Art_Yadrin,        0x8F60 },
        { Art_Roller,        0x9700 },
    }
};

static const PLCList PLC_SYZ2 = {
    8,
    (const PLC[]) {
        { Art_Bumper,        0x7000 },
        { Art_SYZSpike1,     0x72C0 },
        { Art_SYZSpike2,     0x7740 },
        { Art_Caterkiller,   0x9FE0 },
        { Art_LZSwitch,      0xA1E0 },
        { Art_Spikes,        0xA360 },
        { Art_SpringH,       0xA460 },
        { Art_SpringV,       0xA660 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Scrap Brain
// ---------------------------------------------------------------------------
static const PLCList PLC_SBZ = {
    11,
    (const PLC[]) {
        { Art_Stomper,       0x5800 },
        { Art_SBZDoor1,      0x5D00 },
        { Art_Girder,        0x5E00 },
        { Art_Ballhog,       0x6040 },
        { Art_SBZWheel1,     0x6880 },
        { Art_SBZWheel2,     0x6900 },
        { Art_SYZSpike1,     0x7220 },
        { Art_Cutter,        0x76A0 },
        { Art_FlamePipe,     0x7B20 },
        { Art_SBZFloor,      0x7EA0 },
        { Art_SBZBlock,      0x9860 },
    }
};

static const PLCList PLC_SBZ2 = {
    13,
    (const PLC[]) {
        { Art_Caterkiller,   0x5600 },
        { Art_Bomb,          0x8000 },
        { Art_Orbinaut,      0x8520 },
        { Art_SlideFloor,    0x8C00 },
        { Art_SBZDoor2,      0x8DE0 },
        { Art_Electric,      0x8FC0 },
        { Art_TrapDoor,      0x9240 },
        { Art_SBZFloor,      0x7F20 },
        { Art_SPinPform,     0x9BE0 },
        { Art_LZSwitch,      0xA1E0 },
        { Art_Spikes,        0xA360 },
        { Art_SpringH,       0xA460 },
        { Art_SpringV,       0xA660 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - title card
// ---------------------------------------------------------------------------
static const PLCList PLC_TitleCard = {
   1,
    (const PLC[]) {
        { Art_TitleCard,     0xB000 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - act 3 boss
// ---------------------------------------------------------------------------
static const PLCList PLC_Boss = {
    6,
    (const PLC[]) {
        { Art_Eggman,        0x8000 },
        { Art_Weapons,       0x8D80 },
        { Art_Prison,        0x93A0 },
        { Art_Bomb,          0xA300 },
        { Art_SLZSpike,      0xA300 },
        { Art_Exhaust,       0xA540 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - act 1/2 signpost
// ---------------------------------------------------------------------------
static const PLCList PLC_Signpost = {
    3,
    (const PLC[]) {
        { Art_Signpost,      0xD000 },
        { Art_HiddenBonus,   0x96C0 },
        { Art_BigFlash,      0x8C40 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - beta special stage warp effect
// ---------------------------------------------------------------------------
static const PLCList PLC_Warp = {
    1,
    (const PLC[]) {
        { Art_Warp,          0x8C40 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - special stage
// ---------------------------------------------------------------------------
static const PLCList PLC_SpecialStage = {
    17,
    (const PLC[]) {
        { Art_SSClouds,      0x0000 },
        { Art_SSBack,        0x0A20 },
        { Art_SSWall,        0x2840 },
        { Art_Bumper,        0x4760 },
        { Art_SSGoal,        0x4A20 },
        { Art_SSSpeed,       0x4C60 },
        { Art_SSRotate,      0x5E00 },
        { Art_SSLife,        0x6E00 },
        { Art_SSTwinkle,     0x7E00 },
        { Art_SSChecker,     0x8E00 },
        { Art_SSGhost,       0x9E00 },
        { Art_SSWarp,        0xAE00 },
        { Art_SSGlass,       0xBE00 },
        { Art_SSEmerald,     0xEE00 },
        { Art_SSZone1,       0xF2E0 },
        { Art_SSZone2,       0xF400 },
        { Art_SSZone3,       0xF520 },
        // These last 3 are unused
        { Art_SSZone4,       0xF2E0 },
        { Art_SSZone5,       0xF400 },
        { Art_SSZone6,       0xF520 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - GHZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_GHZAnimals = {
    2,
    (const PLC[]) {
        { Art_Rabbit,        0xB000 },
        { Art_Flicky,        0xB240 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - LZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_LZAnimals = {
    2,
    (const PLC[]) {
        { Art_Penguin,       0xB000 },
        { Art_Seal,          0xB240 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - MZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_MZAnimals = {
    2,
    (const PLC[]) {
        { Art_Squirrel,      0xB000 },
        { Art_Seal,          0xB240 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - SLZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_SLZAnimals = {
    2,
    (const PLC[]) {
        { Art_Pig,           0xB000 },
        { Art_Flicky,        0xB240 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - SYZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_SYZAnimals = {
    2,
    (const PLC[]) {
        { Art_Pig,           0xB000 },
        { Art_Chicken,       0xB240 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - SBZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_SBZAnimals = {
    2,
    (const PLC[]) {
        { Art_Rabbit,        0xB000 },
        { Art_Chicken,       0xB240 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - special stage results screen
// ---------------------------------------------------------------------------
static const PLCList PLC_SSResult = {
    2,
    (const PLC[]) {
        { Art_ResultEm,      0xA820 },
        { Art_MiniSonic,     0xAA20 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - ending sequence
// ---------------------------------------------------------------------------
static const PLCList PLC_Ending = {
    13,
    (const PLC[]) {
        { Art_GHZStalk,      0x6B00 },
        { Art_EndFlower,     0x7400 },
        { Art_EndEm,         0x78A0 },
        { Art_EndSonic,      0x7C20 },
        { Art_EndEggman,     0xA480 },
        { Art_Rabbit,        0xAA60 },
        { Art_Chicken,       0xACA0 },
        { Art_Penguin,       0xAE60 },
        { Art_Seal,          0xB0A0 },
        { Art_Pig,           0xB260 },
        { Art_Flicky,        0xB4A0 },
        { Art_Squirrel,      0xB660 },
        { Art_EndStH,        0xB8A0 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - "TRY AGAIN" and "END" screens
// ---------------------------------------------------------------------------
static const PLCList PLC_TryAgain = {
    3,
    (const PLC[]) {
        { Art_EndEm,         0x78A0 },
        { Art_TryAgain,      0x7C20 },
        { Art_CreditText,    0xB400 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Eggman on SBZ 2
// ---------------------------------------------------------------------------
static const PLCList PLC_EggmanSBZ2 = {
    3,
    (const PLC[]) {
        { Art_SBZBlock,      0xA300 },
        { Art_SBZ2Eggman,    0x8000 },
        { Art_LZSwitch,      0x9400 },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - final boss
// ---------------------------------------------------------------------------
static const PLCList PLC_FZBoss = {
    5,
    (const PLC[]) {
        { Art_FZEggman,      0x7400 },
        { Art_FZBoss,        0x6000 },
        { Art_Eggman,        0x8000 },
        { Art_SBZ2Eggman,    0x8E00 },
        { Art_Exhaust,       0xA540 },
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
    PLC* const plc_end = plc_buffer + (sizeof(plc_buffer) / sizeof(*plc_buffer));
    PLC* plc_free = plc_buffer;
    while (plc_free < plc_end && plc_free->art != NULL)
        plc_free++;

    // Push PLCs to buffer
    for (size_t i = 0; i < list->plcs && plc_free + i < plc_end; i++)
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
            plc_buffer[sizeof(plc_buffer) / sizeof(*plc_buffer) - 1] = (PLC){0};
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
