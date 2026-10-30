#include "Object.h"
#include "SstLoad.h"

#include "Object/AirBubbles.h"
#include "Object/Bumper.h"
#include "Object/Points.h"
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

void Obj_Sonic(Object *obj);
void Obj_SpecialSonic(Object *obj);
void Obj_Signpost(Object *obj);
void Obj_TitleSonic(Object *obj);
void Obj_PSB(Object *obj);
void Obj_GHZTunnel(Object *obj);
void Obj_GHZBridge(Object *obj);
void Obj_HUD(Object *obj);
void Obj_Ring(Object *obj);
void Obj_Monitor(Object *obj);
void Obj_Checkpoint(Object *obj);
void Obj_MonitorItem(Object *obj);
void Obj_TitleCard(Object *obj);
void Obj_GotThroughCard(Object *obj);
void Obj_Animals(Object *obj);
void Obj_RingLoss(Object *obj);
void Obj_ShieldInvincibility(Object *obj);
void Obj_GameOverCard(Object *obj);
void Obj_SwingingPlatform(Object *obj);
void Obj_BigSpikeBall(Object *obj);
void Obj_GHZEdge(Object *obj);
void Obj_Credits(Object *obj);
void Obj_EndSonic(Object *obj);
void Obj_EndChaos(Object *obj);
void Obj_EndSTH(Object *obj);
void Obj_EndEggman(Object *obj);
void Obj_TryChaos(Object *obj);
void Obj_EndEggmobile(Object *obj);
void Obj_ContScrItem(Object *obj);
void Obj_ContSonic(Object *obj);
void Obj_GiantRing(Object *obj);
void Obj_RingFlash(Object *obj);
void Obj_HiddenBonus(Object *obj);
void Obj_BasicPlatform(Object *obj);
void Obj_Scenery(Object *obj);
void Obj_CollapseLedge(Object *obj);
void Obj_CollapseFloor(Object *obj);
void Obj_Button(Object *obj);
void Obj_MovingBlock(Object *obj);
void Obj_Caterkiller(Object *obj);
void Obj_GlassBlock(Object *obj);
void Obj_ChainStomp(Object *obj);
void Obj_GeyserMaker(Object *obj);
void Obj_LavaGeyser(Object *obj);
void Obj_PushBlock(Object *obj);
void Obj_Roller(Object *obj);
void Obj_FloatingBlock(Object *obj);
void Obj_SidewaysStomper(Object *obj);
void Obj_LavaWall(Object *obj);
void Obj_Pole(Object *obj);
void Obj_LabyrinthBlock(Object *obj);
void Obj_LabyrinthConvey(Object *obj);
void Obj_Elevator(Object *obj);
void Obj_Seesaw(Object *obj);
void Obj_ScrapStomp(Object *obj);
void Obj_SpinPlatform(Object *obj);
void Obj_RunningDisc(Object *obj);
void Obj_RotatingJunction(Object *obj);
void Obj_Saw(Object *obj);
void Obj_SpinConveyor(Object *obj);
void Obj_Teleporter(Object *obj);
void Obj_BossGreenHill(Object *obj);
void Obj_BossBall(Object *obj);
void Obj_BossMarble(Object *obj);
void Obj_BossSpringYard(Object *obj);
void Obj_BossLabyrinth(Object *obj);
void Obj_BossStarLight(Object *obj);
void Obj_BossSpikeball(Object *obj);
void Obj_ScrapEggman(Object *obj);
void Obj_FalseFloor(Object *obj);
void Obj_BossFinal(Object *obj);
void Obj_SpecialResult(Object *obj);
void Obj_SpecialResultEmerald(Object *obj);
void Obj_EggmanCylinder(Object *obj);
void Obj_BossPlasma(Object *obj);
void Obj_BossBlock(Object *obj);
void Obj_PrisonCapsule(Object *obj);
void Obj_VanishSonic(Object *obj);

// Sonic 1's objects by object id. The engine (EngineObject.h) runs whichever one an object slot's type names; an id that is not listed, or a NULL entry,
// runs as the null object.
const ObjectFunc game_objects[] = {
	/* 00 ObjId_Null                */ NULL,
	/* 01 ObjId_Sonic               */ Obj_Sonic,
	/* 02 ObjId_02                  */ Obj_Null,
	/* 03 ObjId_PathSwapper         */ Obj_PathSwapper,
	/* 04 ObjId_04                  */ Obj_Null,
	/* 05 ObjId_05                  */ Obj_Null,
	/* 06 ObjId_06                  */ Obj_Null,
	/* 07 ObjId_07                  */ Obj_Null,
	/* 08 ObjId_Splash              */ Obj_Splash,
	/* 09 ObjId_SpecialSonic        */ Obj_SpecialSonic,
	/* 0A ObjId_DrownCount          */ Obj_DrownCount,
	/* 0B ObjId_Pole                */ Obj_Pole,
	/* 0C ObjId_FlapDoor            */ NULL, // (moved to the status table: sst_object_codes)
	/* 0D ObjId_Signpost            */ Obj_Signpost,
	/* 0E ObjId_TitleSonic          */ Obj_TitleSonic,
	/* 0F ObjId_PSB                 */ Obj_PSB,
	/* 10 ObjId_GHZTunnel           */ Obj_GHZTunnel,
	/* 11 ObjId_GHZBridge           */ Obj_GHZBridge,
	/* 12 ObjId_SpinningLight       */ NULL, // (moved to the status table: sst_object_codes)
	/* 13 ObjId_LavaMaker           */ NULL, // (moved to the status table: sst_object_codes)
	/* 14 ObjId_LavaBall            */ NULL, // (moved to the status table: sst_object_codes)
	/* 15 ObjId_SwingingPlatform    */ Obj_SwingingPlatform,
	/* 16 ObjId_Harpoon             */ NULL, // (moved to the status table: sst_object_codes)
	/* 17 ObjId_Helix               */ NULL, // (moved to the status table: sst_object_codes)
	/* 18 ObjId_BasicPlatform       */ Obj_BasicPlatform,
	/* 19 ObjId_19                  */ Obj_Null,
	/* 1A ObjId_CollapseLedge       */ Obj_CollapseLedge,
	/* 1B ObjId_WaterSurface        */ Obj_WaterSurface,
	/* 1C ObjId_Scenery             */ Obj_Scenery,
	/* 1D ObjId_MagicSwitch         */ NULL, // (moved to the status table: sst_object_codes)
	/* 1E ObjId_BallHog             */ NULL, // (moved to the status table: sst_object_codes)
	/* 1F ObjId_Crabmeat            */ NULL, // (moved to the status table: sst_object_codes)
	/* 20 ObjId_Cannonball          */ NULL, // (moved to the status table: sst_object_codes)
	/* 21 ObjId_HUD                 */ Obj_HUD,
	/* 22 ObjId_BuzzBomber          */ NULL, // (moved to the status table: sst_object_codes)
	/* 23 ObjId_BuzzMissile         */ NULL, // (moved to the status table: sst_object_codes)
	/* 24 ObjId_BuzzExplode         */ NULL, // (moved to the status table: sst_object_codes)
	/* 25 ObjId_Ring                */ Obj_Ring,
	/* 26 ObjId_Monitor             */ Obj_Monitor,
	/* 27 ObjId_Explosion           */ Obj_ExplosionObject, // (an old object that turns into one: the explosion is on the status table)
	/* 28 ObjId_Animal              */ Obj_Animals,
	/* 29 ObjId_Points              */ NULL, // (moved to the status table: sst_object_codes)
	/* 2A ObjId_SmallDoor           */ NULL, // (moved to the status table: sst_object_codes)
	/* 2B ObjId_Chopper             */ NULL, // (moved to the status table: sst_object_codes)
	/* 2C ObjId_Jaws                */ NULL, // (moved to the status table: sst_object_codes)
	/* 2D ObjId_Burrobot            */ NULL, // (moved to the status table: sst_object_codes)
	/* 2E ObjId_MonitorItem         */ Obj_MonitorItem,
	/* 2F ObjId_LargeGrass          */ NULL, // (moved to the status table: sst_object_codes)
	/* 30 ObjId_GlassBlock          */ Obj_GlassBlock,
	/* 31 ObjId_ChainStomp          */ Obj_ChainStomp,
	/* 32 ObjId_Button              */ Obj_Button,
	/* 33 ObjId_PushBlock           */ Obj_PushBlock,
	/* 34 ObjId_TitleCard           */ Obj_TitleCard,
	/* 35 ObjId_GrassFire           */ NULL, // (moved to the status table: sst_object_codes)
	/* 36 ObjId_Spikes              */ NULL, // (moved to the status table: sst_object_codes)
	/* 37 ObjId_RingLoss            */ Obj_RingLoss,
	/* 38 ObjId_ShieldInvincibility */ Obj_ShieldInvincibility,
	/* 39 ObjId_GameOverCard        */ Obj_GameOverCard,
	/* 3A ObjId_GotThroughCard      */ Obj_GotThroughCard,
	/* 3B ObjId_GHZRock             */ NULL, // (moved to the status table: sst_object_codes)
	/* 3C ObjId_SmashWall           */ NULL, // (moved to the status table: sst_object_codes)
	/* 3D ObjId_BossGreenHill       */ Obj_BossGreenHill,
	/* 3E ObjId_PrisonCapsule       */ Obj_PrisonCapsule,
	/* 3F ObjId_ExplosionBomb       */ Obj_ExplosionBombObject,
	/* 40 ObjId_Motobug             */ NULL, // (moved to the status table: sst_object_codes)
	/* 41 ObjId_Spring              */ NULL, // (moved to the status table: sst_object_codes)
	/* 42 ObjId_Newtron             */ NULL, // (moved to the status table: sst_object_codes)
	/* 43 ObjId_Roller              */ Obj_Roller,
	/* 44 ObjId_GHZEdge             */ Obj_GHZEdge,
	/* 45 ObjId_SidewaysStomper     */ Obj_SidewaysStomper,
	/* 46 ObjId_MarbleBrick         */ NULL, // (moved to the status table: sst_object_codes)
	/* 47 ObjId_Bumper              */ NULL, // (moved to the status table: sst_object_codes)
	/* 48 ObjId_BossBall            */ Obj_BossBall,
	/* 49 ObjId_Waterfall           */ NULL, // (moved to the status table: sst_object_codes)
	/* 4A ObjId_VanishSonic         */ Obj_VanishSonic,
	/* 4B ObjId_GiantRing           */ Obj_GiantRing,
	/* 4C ObjId_GeyserMaker         */ Obj_GeyserMaker,
	/* 4D ObjId_LavaGeyser          */ Obj_LavaGeyser,
	/* 4E ObjId_LavaWall            */ Obj_LavaWall,
	/* 4F ObjId_4F                  */ Obj_Null,
	/* 50 ObjId_Yadrin              */ NULL, // (moved to the status table: sst_object_codes)
	/* 51 ObjId_SmashBlock          */ NULL, // (moved to the status table: sst_object_codes)
	/* 52 ObjId_MovingBlock         */ Obj_MovingBlock,
	/* 53 ObjId_CollapseFloor       */ Obj_CollapseFloor,
	/* 54 ObjId_LavaTag             */ NULL, // (moved to the status table: sst_object_codes)
	/* 55 ObjId_Basaran             */ NULL, // (moved to the status table: sst_object_codes)
	/* 56 ObjId_FloatingBlock       */ Obj_FloatingBlock,
	/* 57 ObjId_SpikeBall           */ NULL, // (moved to the status table: sst_object_codes)
	/* 58 ObjId_BigSpikeBall        */ Obj_BigSpikeBall,
	/* 59 ObjId_Elevator            */ Obj_Elevator,
	/* 5A ObjId_CirclingPlatform    */ NULL, // (moved to the status table: sst_object_codes)
	/* 5B ObjId_Staircase           */ NULL, // (moved to the status table: sst_object_codes)
	/* 5C ObjId_Pylon               */ NULL, // (moved to the status table: sst_object_codes)
	/* 5D ObjId_Fan                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 5E ObjId_Seesaw              */ Obj_Seesaw,
	/* 5F ObjId_Bomb                */ NULL, // (moved to the status table: sst_object_codes)
	/* 60 ObjId_Orbinaut            */ NULL, // (moved to the status table: sst_object_codes)
	/* 61 ObjId_LabyrinthBlock      */ Obj_LabyrinthBlock,
	/* 62 ObjId_Gargoyle            */ NULL, // (moved to the status table: sst_object_codes)
	/* 63 ObjId_LabyrinthConvey     */ Obj_LabyrinthConvey,
	/* 64 ObjId_Bubble              */ Obj_Bubble,
	/* 65 ObjId_LZWaterfall         */ NULL, // (moved to the status table: sst_object_codes)
	/* 66 ObjId_Junction            */ Obj_RotatingJunction,
	/* 67 ObjId_RunningDisc         */ Obj_RunningDisc,
	/* 68 ObjId_SBZConveyor         */ NULL, // (moved to the status table: sst_object_codes)
	/* 69 ObjId_SpinPlatform        */ Obj_SpinPlatform,
	/* 6A ObjId_Saw                 */ Obj_Saw,
	/* 6B ObjId_ScrapStomp          */ Obj_ScrapStomp,
	/* 6C ObjId_VanishPlatform      */ NULL, // (moved to the status table: sst_object_codes)
	/* 6D ObjId_Flamethrower        */ NULL, // (moved to the status table: sst_object_codes)
	/* 6E ObjId_Electrocuter        */ NULL, // (moved to the status table: sst_object_codes)
	/* 6F ObjId_SpinConvey          */ Obj_SpinConveyor,
	/* 70 ObjId_GirderBlock         */ NULL, // (moved to the status table: sst_object_codes)
	/* 71 ObjId_InvisibleBarrier    */ Obj_InvisibleBarrier,
	/* 72 ObjId_Teleporter          */ Obj_Teleporter,
	/* 73 ObjId_BossMarble          */ Obj_BossMarble,
	/* 74 ObjId_BossFire            */ NULL, // (moved to the status table: sst_object_codes)
	/* 75 ObjId_BossSpringYard      */ Obj_BossSpringYard,
	/* 76 ObjId_BossBlock           */ Obj_BossBlock,
	/* 77 ObjId_BossLabyrinth      */ Obj_BossLabyrinth,
	/* 78 ObjId_Caterkiller         */ Obj_Caterkiller,
	/* 79 ObjId_Checkpoint          */ Obj_Checkpoint,
	/* 7A ObjId_BossStarLight       */ Obj_BossStarLight,
	/* 7B ObjId_BossSpikeball       */ Obj_BossSpikeball,
	/* 7C ObjId_RingFlash           */ Obj_RingFlash,
	/* 7D ObjId_HiddenBonus         */ Obj_HiddenBonus,
	/* 7E ObjId_SSResult            */ Obj_SpecialResult,
	/* 7F ObjId_SSRChaos            */ Obj_SpecialResultEmerald,
	/* 80 ObjId_80                  */ Obj_ContScrItem,
	/* 81 ObjId_81                  */ Obj_ContSonic,
	/* 82 ObjId_ScrapEggman         */ Obj_ScrapEggman,
	/* 83 ObjId_FalseFloor          */ Obj_FalseFloor,
	/* 84 ObjId_EggmanCylinder      */ Obj_EggmanCylinder,
	/* 85 ObjId_BossFinal           */ Obj_BossFinal,
	/* 86 ObjId_BossPlasma          */ Obj_BossPlasma,
	/* 87 ObjId_87                  */ Obj_EndSonic,
	/* 88 ObjId_88                  */ Obj_EndChaos,
	/* 89 ObjId_89                  */ Obj_EndSTH,
	/* 8A ObjId_Credits             */ Obj_Credits,
	/* 8B ObjId_8B                  */ Obj_EndEggman,
	/* 8C ObjId_8C                  */ Obj_TryChaos,
	/* 8D ObjId_8D                  */ Obj_EndEggmobile,
};
const int game_object_count = (int)(sizeof(game_objects) / sizeof(game_objects[0]));

// The objects that have moved over to the status table of Sonic 3 & Knuckles (engine/Sst.h), by id: the loader of that table (engine/SstLoad.c) loads their layout entries, and ObjectsManager.c leaves them alone.
// An object moves over by its row here, its row above going NULL; the entries of its id in res/ObjectLayout lose their bit 15 (tools/objlayout_clear_remember.pl).
const SstCode sst_object_codes[0x100] = {
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
