#include "PLC.h"

#include "Constants.h"

// Sonic 2's pattern load cues: Sonic 1's (this started as a copy of Sonic1PLC.c), with the zone slots' lists replaced by their Sonic 2 zones' as those come (slot 3 = Emerald Hill).
// (The zones' tilesets are not here: Level.c decompresses the level header's at level start, as in Sonic 1.)
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
#include "Resource/Art/EHZBridge.h"
#include "Resource/Art/EHZWaterfall.h"
#include "Resource/Art/CPZDust.h"
#include "Resource/Art/CPZMetalStructure.h"
#include "Resource/Art/CPZStripes.h"
#include "Resource/Art/CPZBooster.h"
#include "Resource/Art/CPZElevator.h"
#include "Resource/Art/CPZAnimBits.h"
#include "Resource/Art/CPZTubeSpring.h"
#include "Resource/Art/CPZStairBlock.h"
#include "Resource/Art/CPZMetalBlock.h"
#include "Resource/Art/CPZDroplet.h"
#include "Resource/Art/NGHZWaterSurface.h"
#include "Resource/Art/AirBubbles.h"
#include "Resource/Art/NGHZLeaves.h"
#include "Resource/Art/NGHZArrowShooter.h"
#include "Resource/Art/NGHZWaterSplash.h"
#include "Resource/Art/LeverSpring.h"
#include "Resource/Art/HPZBridge.h"
#include "Resource/Art/Redz.h"
#include "Resource/Art/BBat.h"
#include "Resource/Art/HPZWaterfall.h"
#include "Resource/Art/HPZPlatform.h"
#include "Resource/Art/HPZOrb.h"
#include "Resource/Art/HTZLift.h"
#include "Resource/Art/HTZSeesaw.h"
#include "Resource/Art/HPZCork.h"
#include "Resource/Art/HPZEmerald.h"
#include "Resource/Art/WaterSurface.h"
#include "Resource/Art/CPZPlatform.h"
#include "Resource/Art/BossShip.h"
#include "Resource/Art/BossEHZ.h"
#include "Resource/Art/BossEHZBlades.h"
#include "Resource/Art/Buzzer.h"
#include "Resource/Art/Snail.h"
#include "Resource/Art/Masher.h"
#include "Resource/Art/EHZFireball.h"
#include "Resource/Art/SpringUp.h"
#include "Resource/Art/SpringSide.h"
#include "Resource/Art/SpringDiag.h"
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

// ---------------------------------------------------------------------------
// Pattern load cues - standard block 1
// ---------------------------------------------------------------------------
static const PLCList PLC_Main = {
    5,
    (const PLC[]) {
        { Art_Lamppost,      ART_VRAM(ArtTile_Lamppost) }, // moved from ART_VRAM(ArtTile_Lamppost) to make room for the Spin Dash dust (see Splash.h)
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
static const PLCList PLC_GHZ = { // Neo Green Hill (the prototype's Neo_Green_Hill_Sprites_1; its addresses in VRAM are bytes, here in tiles): the water's surface, the leaves, the arrow shooter and its arrows, the splash
    4,
    (const PLC[]) {
        { Art_NGHZWaterSurface, ART_VRAM(0x8000 >> 5) },
        { Art_NGHZLeaves,       ART_VRAM(0x8200 >> 5) },
        { Art_NGHZArrowShooter, ART_VRAM(0x82E0 >> 5) },
        { Art_NGHZWaterSplash,  ART_VRAM(0x8500 >> 5) },
    }
};

static const PLCList PLC_GHZ2 = { // Neo_Green_Hill_Sprites_2: the air bubbles and numbers (which also hold the water splash), the spikes, the lever spring and the straight springs
    5,
    (const PLC[]) {
        { Art_AirBubbles,    ART_VRAM(0xA000 >> 5) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_LeverSpring,   ART_VRAM(0x8800 >> 5) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Labyrinth
// ---------------------------------------------------------------------------
static const PLCList PLC_LZ = {
    11,
    (const PLC[]) {
        { Art_LZBlock1,      ART_VRAM(ArtTile_LZ_Block_1) },
        { Art_LZBlock2,      ART_VRAM(ArtTile_LZ_Block_2) },
        { Art_Splash,        ART_VRAM(ArtTile_LZ_Splash) }, // waterfalls and splash -- object art, not part of the Kosinski level art
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
    14,
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
        { Art_SpringDiag,    ART_VRAM(ArtTile_SpringDiag) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Marble
// ---------------------------------------------------------------------------
static const PLCList PLC_MZ = { // Chemical Plant (the prototype's Chemical_Plant_Sprites_1: its addresses in VRAM are bytes, here in tiles)
    9,
    (const PLC[]) {
        { Art_CPZMetalStructure, ART_VRAM(0x6E60 >> 5) },
        { Art_CPZStripes,        ART_VRAM(0x7280 >> 5) },
        { Art_CPZBooster,        ART_VRAM(0x7380 >> 5) },
        { Art_CPZElevator,       ART_VRAM(0x7400 >> 5) },
        { Art_CPZAnimBits,       ART_VRAM(0x7600 >> 5) },
        { Art_CPZTubeSpring,     ART_VRAM(0x7C00 >> 5) },
        { Art_WaterSurface,      ART_VRAM(0x8000 >> 5) },
        { Art_CPZStairBlock,     ART_VRAM(0x8300 >> 5) },
        { Art_CPZMetalBlock,     ART_VRAM(0x8600 >> 5) },
    }
};

static const PLCList PLC_MZ2 = { // Chemical_Plant_Sprites_2: the air bubbles and numbers (which also hold the water splash), the spikes, the droplets (the worms), the lever spring and the straight springs; no diagonal springs'
    6,                           // art: the diagonal art of Sonic 1's places sits on the lever spring's tiles
    (const PLC[]) {
        { Art_AirBubbles,    ART_VRAM(0xA000 >> 5) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_CPZDroplet,    ART_VRAM(0x8780 >> 5) },
        { Art_LeverSpring,   ART_VRAM(0x8800 >> 5) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Star Light
// ---------------------------------------------------------------------------
static const PLCList PLC_SLZ = { // Emerald Hill
    11,
    (const PLC[]) {
        { Art_Shield,        ART_VRAM(0x560) }, // (the barrier's place in Emerald Hill: see ShieldInvincibility.c)
        { Art_EHZBridge,     ART_VRAM(0x3C6) },
        { Art_EHZWaterfall,  ART_VRAM(0x3AE) },
        { Art_Buzzer,        ART_VRAM(0x3E6) },
        { Art_Snail,         ART_VRAM(0x402) },
        { Art_Masher,        ART_VRAM(0x41C) },
        { Art_EHZFireball,   ART_VRAM(0x39E) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringDiag,    ART_VRAM(ArtTile_SpringDiag) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};

static const PLCList PLC_SLZ2 = { // (Emerald Hill's objects' art comes with the objects)
    0,
    NULL,
};
// ---------------------------------------------------------------------------
// Pattern load cues - Spring Yard
// ---------------------------------------------------------------------------
static const PLCList PLC_SYZ = { // Hidden Palace
    13,
    (const PLC[]) {
        { Art_Redz,          ART_VRAM(0x500) },
        { Art_BBat,          ART_VRAM(0x530) },
        { Art_HPZBridge,     ART_VRAM(0x300) },
        { Art_HPZWaterfall,  ART_VRAM(0x315) },
        { Art_HPZPlatform,   ART_VRAM(0x34A) },
        { Art_HPZOrb,        ART_VRAM(0x35A) },
        { Art_HPZCork,       ART_VRAM(0x37C) },
        { Art_HPZEmerald,    ART_VRAM(0x392) },
        { Art_WaterSurface,  ART_VRAM(0x400) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringDiag,    ART_VRAM(ArtTile_SpringDiag) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};

static const PLCList PLC_SYZ2 = {
    9,
    (const PLC[]) {
        { Art_Bumper,        ART_VRAM(ArtTile_SYZ_Bumper) },
        { Art_SYZSpike1,     ART_VRAM(ArtTile_SYZ_Big_Spikeball) },
        { Art_SYZSpike2,     ART_VRAM(ArtTile_SYZ_Spikeball_Chain) },
        { Art_Caterkiller,   ART_VRAM(ArtTile_MZ_SYZ_Caterkiller) },
        { Art_LZSwitch,      ART_VRAM(ArtTile_GHZ_SLZ_Smashable_Wall) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringDiag,    ART_VRAM(ArtTile_SpringDiag) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - Scrap Brain
// ---------------------------------------------------------------------------
static const PLCList PLC_SBZ = { // Hill Top: Emerald Hill's art, and its own from tile $1FC
    6,
    (const PLC[]) {
        { Art_HTZSeesaw,     ART_VRAM(0x3CE) },
        { Art_HTZLift,       ART_VRAM(0x3E6) },
        { Art_Spikes,        ART_VRAM(ArtTile_Spikes) },
        { Art_SpringDiag,    ART_VRAM(ArtTile_SpringDiag) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
    }
};

static const PLCList PLC_SBZ2 = {
    14,
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
        { Art_SpringDiag,    ART_VRAM(ArtTile_SpringDiag) },
        { Art_SpringUp,      ART_VRAM(ArtTile_SpringUp) },
        { Art_SpringSide,    ART_VRAM(ArtTile_SpringSide) },
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
static const PLCList PLC_Boss = { // Emerald Hill's boss: Eggman's ship, the drill car and its blades
    3,
    (const PLC[]) {
        { Art_BossShip,      ART_VRAM(0x460) },
        { Art_BossEHZ,       ART_VRAM(0x4C0) },
        { Art_BossEHZBlades, ART_VRAM(0x540) },
    }
};
// ---------------------------------------------------------------------------
// Pattern load cues - act 1/2 signpost
// ---------------------------------------------------------------------------
static const PLCList PLC_Signpost = { // (the prototype's End_Level_Sprites: the signpost's art only. The hidden bonus's and the giant ring flash's entries that follow it in its list are not loaded
    1,                                //  ("Not all sprites are loaded in to VRam"): the flash's place, $8C40, is inside the signpost's 82 tiles from $8680)
    (const PLC[]) {
        { Art_Signpost,      ART_VRAM(ArtTile_Signpost) },
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
const PLCList* plcs[PlcId_Num] = {
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

