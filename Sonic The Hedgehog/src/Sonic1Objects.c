#include "Object.h"
#include "SstLoad.h"

#include "Object/AirBubbles.h"
#include "Object/Bumper.h"
#include "Object/Points.h"
#include "Object/SwingingPlatform.h"
#include "Object/Seesaw.h"
#include "Object/BossFinal.h"
#include "Object/BossStarLight.h"
#include "Object/BossLabyrinth.h"
#include "Object/BossBlock.h"
#include "Object/BossSpringYard.h"
#include "Object/BossMarble.h"
#include "Object/BossBall.h"
#include "Object/BossGreenHill.h"
#include "Object/HUD.h"
#include "Object/TitleCard.h"
#include "Object/Monitor.h"
#include "Object/PrisonCapsule.h"
#include "Object/Animal.h"
#include "Object/Ring.h"
#include "Object/Ending.h"
#include "Object/SpecialResult.h"
#include "Object/ContinueScreen.h"
#include "Object/Signpost.h"
#include "Object/ScrapEggman.h"
#include "Object/Button.h"
#include "Object/GiantRing.h"
#include "Object/GameOverCard.h"
#include "Object/Credits.h"
#include "Object/PSB.h"
#include "Object/TitleSonic.h"
#include "Object/VanishSonic.h"
#include "Object/GHZTunnel.h"
#include "Object/HiddenBonus.h"
#include "Object/Checkpoint.h"
#include "Object/ShieldInvincibility.h"
#include "Object/EndingSonic.h"
#include "Object/ContinueSonic.h"
#include "Object/SpecialSonic.h"
#include "Object/Sonic.h"
#include "Object/Caterkiller.h"
#include "Object/Pole.h"
#include "Object/PushBlock.h"
#include "Object/BigSpikeBall.h"
#include "Object/SpinPlatform.h"
#include "Object/SpinConveyor.h"
#include "Object/LZConveyor.h"
#include "Object/RunningDisc.h"
#include "Object/LavaWall.h"
#include "Object/GeyserMaker.h"
#include "Object/LavaGeyser.h"
#include "Object/LZBlocks.h"
#include "Object/ChainStomp.h"
#include "Object/ScrapStomp.h"
#include "Object/Saw.h"
#include "Object/RotatingJunction.h"
#include "Object/Roller.h"
#include "Object/SidewaysStomper.h"
#include "Object/GHZEdge.h"
#include "Object/Scenery.h"
#include "Object/GHZBridge.h"
#include "Object/CollapseLedge.h"
#include "Object/CollapseFloor.h"
#include "Object/GlassBlock.h"
#include "Object/Teleporter.h"
#include "Object/Elevator.h"
#include "Object/FloatingBlock.h"
#include "Object/MovingBlock.h"
#include "Object/BasicPlatform.h"
#include "Object/Fan.h"
#include "Object/MarbleBrick.h"
#include "Object/VanishingPlatform.h"
#include "Object/SmashBlock.h"
#include "Object/SmashWall.h"
#include "Object/SBZConveyor.h"
#include "Object/GirderBlock.h"
#include "Object/FlapDoor.h"
#include "Object/MagicSwitch.h"
#include "Object/LavaTag.h"
#include "Object/Harpoon.h"
#include "Object/SmallDoor.h"
#include "Object/GHZRock.h"
#include "Object/Bomb.h"
#include "Object/Basaran.h"
#include "Object/Burrobot.h"
#include "Object/Newtron.h"
#include "Object/BuzzBomber.h"
#include "Object/Orbinaut.h"
#include "Object/BossFire.h"
#include "Object/LavaMaker.h"
#include "Object/LavaBall.h"
#include "Object/GrassFire.h"
#include "Object/LargeGrass.h"
#include "Object/Staircase.h"
#include "Object/SpikeBall.h"
#include "Object/Helix.h"
#include "Object/Crabmeat.h"
#include "Object/CirclingPlatform.h"
#include "Object/Cannonball.h"
#include "Object/BallHog.h"
#include "Object/Gargoyle.h"
#include "Object/Flamethrower.h"
#include "Object/Electrocuter.h"
#include "Object/Yadrin.h"
#include "Object/Jaws.h"
#include "Object/Chopper.h"
#include "Object/MotoBug.h"
#include "Object/Explosion.h"
#include "Object/Pylon.h"
#include "Object/SpinningLight.h"
#include "Object/LZWaterfall.h"
#include "Object/Spikes.h"
#include "Object/Spring.h"
#include "Object/Waterfall.h"
#include "Object/DrownCount.h"
#include "Object/InvisibleBarrier.h"
#include "Object/PathSwapper.h"
#include "Object/Splash.h"
#include "Object/WaterSurface.h"



// The objects that have moved over to the status table of Sonic 3 & Knuckles (engine/Sst.h), by id: the loader of that table (engine/SstLoad.c) loads their layout entries.
// An object moves over by its row here, its row above going NULL; the entries of its id in res/ObjectLayout lose their bit 15 (tools/objlayout_clear_remember.pl).
const SstCode sst_object_codes[0x100] = {
	[ObjId_SwingingPlatform] = Obj_SwingingPlatform,
	[ObjId_Seesaw] = Obj_Seesaw,
	[ObjId_BossPlasma] = Obj_BossPlasma,
	[ObjId_EggmanCylinder] = Obj_EggmanCylinder,
	[ObjId_BossFinal] = Obj_BossFinal,
	[ObjId_BossSpikeball] = Obj_BossSpikeball,
	[ObjId_BossStarLight] = Obj_BossStarLight,
	[ObjId_BossLabyrinth] = Obj_BossLabyrinth,
	[ObjId_BossBlock] = Obj_BossBlock,
	[ObjId_BossSpringYard] = Obj_BossSpringYard,
	[ObjId_BossMarble] = Obj_BossMarble,
	[ObjId_BossBall] = Obj_BossBall,
	[ObjId_BossGreenHill] = Obj_BossGreenHill,
	[ObjId_Splash] = Obj_Splash,
	[ObjId_Bubble] = Obj_Bubble,
	[ObjId_DrownCount] = Obj_DrownCount,
	[ObjId_GotThroughCard] = Obj_GotThroughCard,
	[ObjId_TitleCard] = Obj_TitleCard,
	[ObjId_MonitorItem] = Obj_MonitorItem,
	[ObjId_Monitor] = Obj_Monitor,
	[ObjId_PrisonCapsule] = Obj_PrisonCapsule,
	[ObjId_Animal] = Obj_Animals,
	[ObjId_RingLoss] = Obj_RingLoss,
	[ObjId_Ring] = Obj_Ring,
	[ObjId_8D] = Obj_EndEggmobile,
	[ObjId_8C] = Obj_TryChaos,
	[ObjId_8B] = Obj_EndEggman,
	[ObjId_89] = Obj_EndSTH,
	[ObjId_88] = Obj_EndChaos,
	[ObjId_SSRChaos] = Obj_SpecialResultEmerald,
	[ObjId_SSResult] = Obj_SpecialResult,
	[ObjId_80] = Obj_ContScrItem,
	[ObjId_Signpost] = Obj_Signpost,
	[ObjId_InvisibleBarrier] = Obj_InvisibleBarrier,
	[ObjId_FalseFloor] = Obj_FalseFloor,
	[ObjId_ScrapEggman] = Obj_ScrapEggman,
	[ObjId_Button] = Obj_Button,
	[ObjId_RingFlash] = Obj_RingFlash,
	[ObjId_GiantRing] = Obj_GiantRing,
	[ObjId_GameOverCard] = Obj_GameOverCard,
	[ObjId_Credits] = Obj_Credits,
	[ObjId_PSB] = Obj_PSB,
	[ObjId_TitleSonic] = Obj_TitleSonic,
	[ObjId_VanishSonic] = Obj_VanishSonic,
	[ObjId_GHZTunnel] = Obj_GHZTunnel,
	[ObjId_PathSwapper] = Obj_PathSwapper,
	[ObjId_HiddenBonus] = Obj_HiddenBonus,
	[ObjId_Checkpoint] = Obj_Checkpoint,
	[ObjId_WaterSurface] = Obj_WaterSurface,
	[ObjId_ShieldInvincibility] = Obj_ShieldInvincibility,
	[ObjId_87] = Obj_EndSonic,
	[ObjId_81] = Obj_ContSonic,
	[ObjId_SpecialSonic] = Obj_SpecialSonic,
	[ObjId_Sonic] = Obj_Sonic,
	[ObjId_Caterkiller] = Obj_Caterkiller,
	[ObjId_Pole] = Obj_Pole,
	[ObjId_PushBlock] = Obj_PushBlock,
	[ObjId_BigSpikeBall] = Obj_BigSpikeBall,
	[ObjId_SpinPlatform] = Obj_SpinPlatform,
	[ObjId_SpinConvey] = Obj_SpinConveyor,
	[ObjId_LabyrinthConvey] = Obj_LabyrinthConvey,
	[ObjId_RunningDisc] = Obj_RunningDisc,
	[ObjId_LavaWall] = Obj_LavaWall,
	[ObjId_GeyserMaker] = Obj_GeyserMaker,
	[ObjId_LavaGeyser] = Obj_LavaGeyser,
	[ObjId_LabyrinthBlock] = Obj_LabyrinthBlock,
	[ObjId_ChainStomp] = Obj_ChainStomp,
	[ObjId_ScrapStomp] = Obj_ScrapStomp,
	[ObjId_Saw] = Obj_Saw,
	[ObjId_Junction] = Obj_RotatingJunction,
	[ObjId_Roller] = Obj_Roller,
	[ObjId_SidewaysStomper] = Obj_SidewaysStomper,
	[ObjId_GHZEdge] = Obj_GHZEdge,
	[ObjId_Scenery] = Obj_Scenery,
	[ObjId_GHZBridge] = Obj_GHZBridge,
	[ObjId_CollapseLedge] = Obj_CollapseLedge,
	[ObjId_CollapseFloor] = Obj_CollapseFloor,
	[ObjId_GlassBlock] = Obj_GlassBlock,
	[ObjId_Teleporter] = Obj_Teleporter,
	[ObjId_Elevator] = Obj_Elevator,
	[ObjId_FloatingBlock] = Obj_FloatingBlock,
	[ObjId_MovingBlock] = Obj_MovingBlock,
	[ObjId_BasicPlatform] = Obj_BasicPlatform,
	[ObjId_Fan] = Obj_Fan,
	[ObjId_MarbleBrick] = Obj_MarbleBrick,
	[ObjId_VanishPlatform] = Obj_VanishPlatform,
	[ObjId_SmashBlock] = Obj_SmashBlock,
	[ObjId_SmashWall] = Obj_SmashWall,
	[ObjId_SBZConveyor] = Obj_SBZConveyor,
	[ObjId_GirderBlock] = Obj_GirderBlock,
	[ObjId_FlapDoor] = Obj_FlapDoor,
	[ObjId_MagicSwitch] = Obj_MagicSwitch,
	[ObjId_LavaTag] = Obj_LavaTag,
	[ObjId_Harpoon] = Obj_Harpoon,
	[ObjId_SmallDoor] = Obj_SmallDoor,
	[ObjId_GHZRock] = Obj_GHZRock,
	[ObjId_Bomb] = Obj_Bomb,
	[ObjId_Basaran] = Obj_Basaran,
	[ObjId_Burrobot] = Obj_Burrobot,
	[ObjId_Newtron] = Obj_Newtron,
	[ObjId_BuzzExplode] = Obj_BuzzExplode,
	[ObjId_BuzzMissile] = Obj_BuzzMissile,
	[ObjId_BuzzBomber] = Obj_BuzzBomber,
	[ObjId_Orbinaut] = Obj_Orbinaut,
	[ObjId_BossFire] = Obj_BossFire,
	[ObjId_LavaMaker] = Obj_LavaMaker,
	[ObjId_LavaBall] = Obj_LavaBall,
	[ObjId_GrassFire] = Obj_GrassFire,
	[ObjId_LargeGrass] = Obj_LargeGrass,
	[ObjId_Staircase] = Obj_Staircase,
	[ObjId_SpikeBall] = Obj_SpikeBall,
	[ObjId_Helix] = Obj_Helix,
	[ObjId_Crabmeat] = Obj_Crabmeat,
	[ObjId_CirclingPlatform] = Obj_CirclingPlatform,
	[ObjId_Cannonball] = Obj_Cannonball,
	[ObjId_BallHog] = Obj_BallHog,
	[ObjId_Waterfall] = Obj_Waterfall,
	[ObjId_Pylon] = Obj_Pylon,
	[ObjId_Points] = Obj_Points,
	[ObjId_Gargoyle] = Obj_Gargoyle,
	[ObjId_Flamethrower] = Obj_Flamethrower,
	[ObjId_Electrocuter] = Obj_Electrocuter,
	[ObjId_Yadrin] = Obj_Yadrin,
	[ObjId_Jaws] = Obj_Jaws,
	[ObjId_Chopper] = Obj_Chopper,
	[ObjId_Motobug] = Obj_Motobug,
	[ObjId_Explosion] = Obj_Explosion,
	[ObjId_ExplosionBomb] = Obj_ExplosionBomb,
	[ObjId_Bumper] = Obj_Bumper,
	[ObjId_Spikes] = Obj_Spikes,
	[ObjId_Spring] = Obj_Spring,
	[ObjId_SpinningLight] = Obj_SpinningLight,
	[ObjId_LZWaterfall] = Obj_LZWaterfall,
};
