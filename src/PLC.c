// Clownacy's implementation

#include "PLC.h"

#include "Constants.h"
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
        { Art_Lamppost,      0xD800 }, // moved from ART_VRAM(ArtTile_Lamppost) to make room for the Spin Dash dust (see Splash.h)
        { Art_HUD,           ART_VRAM(ArtTile_HUD) },
        { Art_HUDLife,       ART_VRAM(ArtTile_Lives_Counter) },
        { Art_Ring,          ART_VRAM(ArtTile_Ring) },
        { Art_Points,        ART_VRAM(ArtTile_Points) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - standard block 2
// ---------------------------------------------------------------------------
static const PLCList PLC_Main2 = {
    3,
    (const PLC[]) {
        { Art_Monitor,       ART_VRAM(ArtTile_Monitor) },
        { Art_Shield,        ART_VRAM(ArtTile_Shield) },
        { Art_Invincibility, ART_VRAM(ArtTile_Invincibility) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - explosion
// ---------------------------------------------------------------------------
static const PLCList PLC_Explode = {
    1,
    (const PLC[]) {
        { Art_Explosion,     ART_VRAM(ArtTile_Explosion) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - game/time	over
// ---------------------------------------------------------------------------
static const PLCList PLC_GameOver = {
    1,
    (const PLC[]) {
        { Art_GameOver,      ART_VRAM(ArtTile_Game_Over) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Green Hill
// ---------------------------------------------------------------------------
static const PLCList PLC_GHZ = {
    10,
    (const PLC[]) {
        { Art_GHZStalk,      ART_VRAM(ArtTile_GHZ_Flower_Stalk) },
        { Art_GHZRock,       ART_VRAM(ArtTile_GHZ_Purple_Rock) },
        { Art_Crabmeat,      ART_VRAM(ArtTile_Crabmeat) },
        { Art_BuzzBomber,    ART_VRAM(ArtTile_Buzz_Bomber) },
        { Art_Chopper,       ART_VRAM(ArtTile_Chopper) },
        { Art_Newtron,       ART_VRAM(ArtTile_Newtron) },
        { Art_Motobug,       ART_VRAM(ArtTile_Moto_Bug) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringH,       ART_VRAM(ArtTile_Spring_Horizontal) },
        { Art_SpringV,       ART_VRAM(ArtTile_Spring_Vertical) },
    }
};

static const PLCList PLC_GHZ2 = {
    6,
    (const PLC[]) {
        { Art_GHZSwing,      ART_VRAM(ArtTile_GHZ_MZ_Swing) },
        { Art_GHZBridge,     ART_VRAM(ArtTile_GHZ_Bridge) },
        { Art_GHZLog,        ART_VRAM(ArtTile_GHZ_Spike_Pole) },
        { Art_GHZBall,       ART_VRAM(ArtTile_GHZ_Giant_Ball) },
        { Art_GHZWall1,      ART_VRAM(ArtTile_GHZ_SLZ_Smashable_Wall) },
        { Art_GHZWall2,      ART_VRAM(ArtTile_GHZ_Edge_Wall) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Labyrinth
// ---------------------------------------------------------------------------
static const PLCList PLC_LZ = {
    10,
    (const PLC[]) {
        { Art_LZBlock1,      ART_VRAM(ArtTile_LZ_Block_1) },
        { Art_LZBlock2,      ART_VRAM(ArtTile_LZ_Block_2) },
        { Art_Water,         ART_VRAM(ArtTile_LZ_Water_Surface) },
        { Art_LZSpikeBall,   ART_VRAM(ArtTile_LZ_Spikeball_Chain) },
        { Art_FlapDoor,      ART_VRAM(ArtTile_LZ_Flapping_Door) },
        { Art_Bubbles,       ART_VRAM(ArtTile_LZ_Bubbles) },
        { Art_LZBlock3,      ART_VRAM(ArtTile_LZ_Moving_Block) },
        { Art_LZDoor1,       ART_VRAM(ArtTile_LZ_Door) },
        { Art_Harpoon,       ART_VRAM(ArtTile_LZ_Harpoon) },
        { Art_Burrobot,      ART_VRAM(ArtTile_Burrobot) },
    }
};

static const PLCList PLC_LZ2 = {
    13,
    (const PLC[]) {
        { Art_LZPole,        ART_VRAM(ArtTile_LZ_Pole) },
        { Art_LZDoor2,       ART_VRAM(ArtTile_LZ_Blocks) },
        { Art_LZWheel,       ART_VRAM(ArtTile_LZ_Conveyor_Belt) },
        { Art_Gargoyle,      ART_VRAM(ArtTile_LZ_Gargoyle) },
        { Art_LZSonic,       ART_VRAM(ArtTile_LZ_Sonic_Drowning) },
        { Art_LZPlatfm,      ART_VRAM(ArtTile_LZ_Rising_Platform) },
        { Art_Orbinaut,      ART_VRAM(ArtTile_LZ_Orbinaut) },
        { Art_Jaws,          ART_VRAM(ArtTile_Jaws) },
        { Art_LZSwitch,      ART_VRAM(ArtTile_GHZ_SLZ_Smashable_Wall) },
        { Art_Cork,          ART_VRAM(ArtTile_LZ_Cork) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringH,       ART_VRAM(ArtTile_Spring_Horizontal) },
        { Art_SpringV,       ART_VRAM(ArtTile_Spring_Vertical) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Marble
// ---------------------------------------------------------------------------
static const PLCList PLC_MZ = {
    9,
    (const PLC[]) {
        { Art_MZMetal,       ART_VRAM(ArtTile_MZ_Spike_Stomper) },
        { Art_MZFire,        ART_VRAM(ArtTile_MZ_Fireball) },
        { Art_GHZSwing,      ART_VRAM(ArtTile_GHZ_MZ_Swing) },
        { Art_MZGlass,       ART_VRAM(ArtTile_MZ_Glass_Pillar) },
        { Art_Lava,          ART_VRAM(ArtTile_MZ_Lava) },
        { Art_BuzzBomber,    ART_VRAM(ArtTile_Buzz_Bomber) },
        { Art_Yadrin,        ART_VRAM(ArtTile_Yadrin) },
        { Art_Basaran,       ART_VRAM(ArtTile_Basaran) },
        { Art_Caterkiller,   ART_VRAM(ArtTile_MZ_SYZ_Caterkiller) },
    }
};

static const PLCList PLC_MZ2 = {
    5,
    (const PLC[]) {
        { Art_MZSwitch,      ART_VRAM(ArtTile_Button_Main) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringH,       ART_VRAM(ArtTile_Spring_Horizontal) },
        { Art_SpringV,       ART_VRAM(ArtTile_Spring_Vertical) },
        { Art_MZBlock,       ART_VRAM(ArtTile_MZ_Block) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Star Light
// ---------------------------------------------------------------------------
static const PLCList PLC_SLZ = {
    8,
    (const PLC[]) {
        { Art_Bomb,          ART_VRAM(ArtTile_SLZ_Orbinaut) }, // shares the Orbinaut slot -- not simultaneously loaded
        { Art_Orbinaut,      ART_VRAM(ArtTile_SLZ_Orbinaut) },
        { Art_MZFire,        ART_VRAM(ArtTile_SLZ_Fireball) },
        { Art_SLZBlock,      ART_VRAM(ArtTile_SLZ_Collapsing_Floor) },
        { Art_SLZWall,       ART_VRAM(ArtTile_Button_Main) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringH,       ART_VRAM(ArtTile_Spring_Horizontal) },
        { Art_SpringV,       ART_VRAM(ArtTile_Spring_Vertical) },
    }
};

static const PLCList PLC_SLZ2 = {
    6,
    (const PLC[]) {
        { Art_Seesaw,        ART_VRAM(ArtTile_SLZ_Seesaw) },
        { Art_Fan,           ART_VRAM(ArtTile_SLZ_Fan) },
        { Art_Pylon,         ART_VRAM(ArtTile_SLZ_Pylon) },
        { Art_SLZSwing,      ART_VRAM(ArtTile_SLZ_Swing) },
        { Art_SLZCannon,     ART_VRAM(ArtTile_SLZ_Fireball_Launcher) },
        { Art_SLZSpike,      ART_VRAM(ArtTile_SLZ_Spikeball) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Spring Yard
// ---------------------------------------------------------------------------
static const PLCList PLC_SYZ = {
    4,
    (const PLC[]) {
        { Art_Crabmeat,      ART_VRAM(ArtTile_Crabmeat) },
        { Art_BuzzBomber,    ART_VRAM(ArtTile_Buzz_Bomber) },
        { Art_Yadrin,        ART_VRAM(ArtTile_Yadrin) },
        { Art_Roller,        ART_VRAM(ArtTile_Roller) },
    }
};

static const PLCList PLC_SYZ2 = {
    8,
    (const PLC[]) {
        { Art_Bumper,        ART_VRAM(ArtTile_SYZ_Bumper) },
        { Art_SYZSpike1,     ART_VRAM(ArtTile_SYZ_Big_Spikeball) },
        { Art_SYZSpike2,     ART_VRAM(ArtTile_SYZ_Spikeball_Chain) },
        { Art_Caterkiller,   ART_VRAM(ArtTile_MZ_SYZ_Caterkiller) },
        { Art_LZSwitch,      ART_VRAM(ArtTile_GHZ_SLZ_Smashable_Wall) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringH,       ART_VRAM(ArtTile_Spring_Horizontal) },
        { Art_SpringV,       ART_VRAM(ArtTile_Spring_Vertical) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Scrap Brain
// ---------------------------------------------------------------------------
static const PLCList PLC_SBZ = {
    11,
    (const PLC[]) {
        { Art_Stomper,       ART_VRAM(ArtTile_SBZ_Moving_Block_Short) },
        { Art_SBZDoor1,      ART_VRAM(ArtTile_SBZ_Door) },
        { Art_Girder,        ART_VRAM(ArtTile_SBZ_Girder) },
        { Art_Ballhog,       ART_VRAM(ArtTile_Ball_Hog) },
        { Art_SBZWheel1,     ART_VRAM(ArtTile_SBZ_Disc) },
        { Art_SBZWheel2,     ART_VRAM(ArtTile_SBZ_Junction) },
        { Art_SYZSpike1,     ART_VRAM(ArtTile_SBZ_Swing) },
        { Art_Cutter,        ART_VRAM(ArtTile_SBZ_Saw) },
        { Art_FlamePipe,     ART_VRAM(ArtTile_SBZ_Flamethrower) },
        { Art_SBZFloor,      ART_VRAM(ArtTile_SBZ_Collapsing_Floor) },
        { Art_SBZBlock,      ART_VRAM(ArtTile_SBZ_Vanishing_Block) },
    }
};

static const PLCList PLC_SBZ2 = {
    13,
    (const PLC[]) {
        { Art_Caterkiller,   ART_VRAM(ArtTile_SBZ_Caterkiller) },
        { Art_Bomb,          ART_VRAM(ArtTile_Bomb) },
        { Art_Orbinaut,      ART_VRAM(ArtTile_SBZ_Orbinaut) },
        { Art_SlideFloor,    ART_VRAM(ArtTile_SBZ_Moving_Block_Long) },
        { Art_SBZDoor2,      ART_VRAM(ArtTile_SBZ_Horizontal_Door) },
        { Art_Electric,      ART_VRAM(ArtTile_SBZ_Electric_Orb) },
        { Art_TrapDoor,      ART_VRAM(ArtTile_SBZ_Trap_Door) },
        { Art_SBZFloor,      0x7F20 }, // SBZ2's own variant offset for the same art -- no separate real constant
        { Art_SPinPform,     ART_VRAM(ArtTile_SBZ_Spinning_Platform) },
        { Art_LZSwitch,      ART_VRAM(ArtTile_GHZ_SLZ_Smashable_Wall) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringH,       ART_VRAM(ArtTile_Spring_Horizontal) },
        { Art_SpringV,       ART_VRAM(ArtTile_Spring_Vertical) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - title card
// ---------------------------------------------------------------------------
static const PLCList PLC_TitleCard = {
   1,
    (const PLC[]) {
        { Art_TitleCard,     ART_VRAM(ArtTile_Title_Card) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - act 3 boss
// ---------------------------------------------------------------------------
static const PLCList PLC_Boss = {
    6,
    (const PLC[]) {
        { Art_Eggman,        ART_VRAM(ArtTile_Eggman) },
        { Art_Weapons,       ART_VRAM(ArtTile_Eggman_Weapons) },
        { Art_Prison,        ART_VRAM(ArtTile_Prison_Capsule) },
        { Art_Bomb,          ART_VRAM(ArtTile_Eggman_Spikeball) },
        { Art_SLZSpike,      ART_VRAM(ArtTile_Eggman_Spikeball) },
        { Art_Exhaust,       ART_VRAM(ArtTile_Eggman_Exhaust) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - act 1/2 signpost
// ---------------------------------------------------------------------------
static const PLCList PLC_Signpost = {
    3,
    (const PLC[]) {
        { Art_Signpost,      ART_VRAM(ArtTile_Signpost) },
        { Art_HiddenBonus,   ART_VRAM(ArtTile_Hidden_Points) },
        { Art_BigFlash,      ART_VRAM(ArtTile_Giant_Ring_Flash) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - beta special stage warp effect
// ---------------------------------------------------------------------------
static const PLCList PLC_Warp = {
    1,
    (const PLC[]) {
        { Art_Warp,          ART_VRAM(ArtTile_Giant_Ring_Flash) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - special stage
// ---------------------------------------------------------------------------
static const PLCList PLC_SpecialStage = {
    17,
    (const PLC[]) {
        { Art_SSClouds,      ART_VRAM(ArtTile_SS_Background_Clouds) },
        { Art_SSBack,        ART_VRAM(ArtTile_SS_Background_Fish) },
        { Art_SSWall,        ART_VRAM(ArtTile_SS_Wall) },
        { Art_Bumper,        ART_VRAM(ArtTile_SS_Bumper) },
        { Art_SSGoal,        ART_VRAM(ArtTile_SS_Goal) },
        { Art_SSSpeed,       ART_VRAM(ArtTile_SS_Up_Down) },
        { Art_SSRotate,      ART_VRAM(ArtTile_SS_R_Block) },
        { Art_SSLife,        ART_VRAM(ArtTile_SS_Extra_Life) },
        { Art_SSTwinkle,     ART_VRAM(ArtTile_SS_Emerald_Sparkle) },
        { Art_SSChecker,     ART_VRAM(ArtTile_SS_Red_White_Block) },
        { Art_SSGhost,       ART_VRAM(ArtTile_SS_Ghost_Block) },
        { Art_SSWarp,        ART_VRAM(ArtTile_SS_W_Block) },
        { Art_SSGlass,       ART_VRAM(ArtTile_SS_Glass) },
        { Art_SSEmerald,     ART_VRAM(ArtTile_SS_Emerald) },
        { Art_SSZone1,       ART_VRAM(ArtTile_SS_Zone_1) },
        { Art_SSZone2,       ART_VRAM(ArtTile_SS_Zone_2) },
        { Art_SSZone3,       ART_VRAM(ArtTile_SS_Zone_3) },
        // These last 3 are unused
        { Art_SSZone4,       ART_VRAM(ArtTile_SS_Zone_4) },
        { Art_SSZone5,       ART_VRAM(ArtTile_SS_Zone_5) },
        { Art_SSZone6,       ART_VRAM(ArtTile_SS_Zone_6) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - GHZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_GHZAnimals = {
    2,
    (const PLC[]) {
        { Art_Rabbit,        ART_VRAM(ArtTile_Animal_1) },
        { Art_Flicky,        ART_VRAM(ArtTile_Animal_2) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - LZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_LZAnimals = {
    2,
    (const PLC[]) {
        { Art_Penguin,       ART_VRAM(ArtTile_Animal_1) },
        { Art_Seal,          ART_VRAM(ArtTile_Animal_2) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - MZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_MZAnimals = {
    2,
    (const PLC[]) {
        { Art_Squirrel,      ART_VRAM(ArtTile_Animal_1) },
        { Art_Seal,          ART_VRAM(ArtTile_Animal_2) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - SLZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_SLZAnimals = {
    2,
    (const PLC[]) {
        { Art_Pig,           ART_VRAM(ArtTile_Animal_1) },
        { Art_Flicky,        ART_VRAM(ArtTile_Animal_2) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - SYZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_SYZAnimals = {
    2,
    (const PLC[]) {
        { Art_Pig,           ART_VRAM(ArtTile_Animal_1) },
        { Art_Chicken,       ART_VRAM(ArtTile_Animal_2) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - SBZ animals
// ---------------------------------------------------------------------------
static const PLCList PLC_SBZAnimals = {
    2,
    (const PLC[]) {
        { Art_Rabbit,        ART_VRAM(ArtTile_Animal_1) },
        { Art_Chicken,       ART_VRAM(ArtTile_Animal_2) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - special stage results screen
// ---------------------------------------------------------------------------
static const PLCList PLC_SSResult = {
    2,
    (const PLC[]) {
        { Art_ResultEm,      ART_VRAM(ArtTile_SS_Results_Emeralds) },
        { Art_MiniSonic,     ART_VRAM(ArtTile_Mini_Sonic) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - ending sequence
// ---------------------------------------------------------------------------
static const PLCList PLC_Ending = {
    13,
    (const PLC[]) {
        { Art_GHZStalk,      ART_VRAM(ArtTile_GHZ_Flower_Stalk) },
        { Art_EndFlower,     ART_VRAM(ArtTile_Ending_Flowers) },
        { Art_EndEm,         ART_VRAM(ArtTile_Ending_Emeralds) },
        { Art_EndSonic,      ART_VRAM(ArtTile_Ending_Sonic) },
        { Art_EndEggman,     ART_VRAM(ArtTile_Ending_Eggman) },
        { Art_Rabbit,        ART_VRAM(ArtTile_Ending_Rabbit) },
        { Art_Chicken,       ART_VRAM(ArtTile_Ending_Chicken) },
        { Art_Penguin,       ART_VRAM(ArtTile_Ending_Penguin) },
        { Art_Seal,          ART_VRAM(ArtTile_Ending_Seal) },
        { Art_Pig,           ART_VRAM(ArtTile_Ending_Pig) },
        { Art_Flicky,        ART_VRAM(ArtTile_Ending_Flicky) },
        { Art_Squirrel,      ART_VRAM(ArtTile_Ending_Squirrel) },
        { Art_EndStH,        ART_VRAM(ArtTile_Ending_STH) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - "TRY AGAIN" and "END" screens
// ---------------------------------------------------------------------------
static const PLCList PLC_TryAgain = {
    3,
    (const PLC[]) {
        { Art_EndEm,         ART_VRAM(ArtTile_Try_Again_Emeralds) },
        { Art_TryAgain,      ART_VRAM(ArtTile_Try_Again_Eggman) },
        { Art_CreditText,    ART_VRAM(ArtTile_Credits_Font) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Eggman on SBZ 2
// ---------------------------------------------------------------------------
static const PLCList PLC_EggmanSBZ2 = {
    3,
    (const PLC[]) {
        { Art_SBZBlock,      ART_VRAM(ArtTile_Eggman_Trap_Floor) },
        { Art_SBZ2Eggman,    ART_VRAM(ArtTile_Eggman) },
        { Art_LZSwitch,      0x9400 }, // Eggman-SBZ2 boss's own reuse of the switch/wall art -- no separate real constant
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - final boss
// ---------------------------------------------------------------------------
static const PLCList PLC_FZBoss = {
    5,
    (const PLC[]) {
        { Art_FZEggman,      ART_VRAM(ArtTile_FZ_Eggman_Fleeing) },
        { Art_FZBoss,        ART_VRAM(ArtTile_FZ_Boss) },
        { Art_Eggman,        ART_VRAM(ArtTile_Eggman) },
        { Art_SBZ2Eggman,    ART_VRAM(ArtTile_FZ_Eggman_No_Vehicle) },
        { Art_Exhaust,       ART_VRAM(ArtTile_Eggman_Exhaust) },
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
