#include "Object.h"

#include "Object/AirBubbles.h"
#include "Object/DHZObjects.h"
#include "Object/DrownCount.h"
#include "Object/InvisibleBarrier.h"
#include "Object/PathSwapper.h"
#include "Object/Splash.h"
#include "Object/WaterSurface.h"

void Obj_Sonic(Object *obj);
void Obj_SpecialSonic(Object *obj);
void Obj_Signpost(Object *obj);
void Obj_TitleCharacters(Object *obj); // Sonic 2's: Sonic and Tails from the title screen
void Obj_Tails(Object *obj);
void Obj_TailsTails(Object *obj);
void Obj_PSB(Object *obj);
void Obj_GHZTunnel(Object *obj);
void Obj_GHZBridge(Object *obj);
void Obj_Bridge(Object *obj);
void Obj_Crabmeat(Object *obj);
void Obj_HUD(Object *obj);
void Obj_BuzzBomber(Object *obj);
void Obj_BuzzMissile(Object *obj);
void Obj_BuzzExplode(Object *obj);
void Obj_Ring(Object *obj);
void Obj_Monitor(Object *obj);
void Obj_Checkpoint(Object *obj);
void Obj_Explosion(Object *obj);
void Obj_ExplosionBomb(Object *obj);
void Obj_Chopper(Object *obj);
void Obj_MonitorItem(Object *obj);
void Obj_TitleCard(Object *obj);
void Obj_GotThroughCard(Object *obj);
void Obj_Animals(Object *obj);
void Obj_Points(Object *obj);
void Obj_Spikes(Object *obj);
void Obj_RingLoss(Object *obj);
void Obj_ShieldInvincibility(Object *obj);
void Obj_GameOverCard(Object *obj);
void Obj_GHZRock(Object *obj);
void Obj_SwingingPlatform(Object *obj);
void Obj_BigSpikeBall(Object *obj);
void Obj_Motobug(Object *obj);
void Obj_Spring(Object *obj);
void Obj_Newtron(Object *obj);
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
void Obj_Waterfall(Object *obj);
void Obj_GiantRing(Object *obj);
void Obj_RingFlash(Object *obj);
void Obj_HiddenBonus(Object *obj);
void Obj_BasicPlatform(Object *obj);
void Obj_SmashWall(Object *obj);
void Obj_Scenery(Object *obj);
void Obj_HPZDecor(Object *obj);
void Obj_EHZWaterfall(Object *obj);
void Obj_Buzzer(Object *obj);
void Obj_Masher(Object *obj);
void Obj_Snail(Object *obj);
void Obj_Spiral(Object *obj);
void Obj_EHZBoss(Object *obj);
void Obj_EHZBossPart(Object *obj);
void Obj_CPZPlatform(Object *obj);
void Obj_CPZPlatform2(Object *obj);
void Obj_HPZEmerald(Object *obj);
void Obj_HPZWaterfall(Object *obj);
void Obj_CollapsingLedge(Object *obj);
void Obj_CollapsingPlatform(Object *obj);
void Obj_BBat(Object *obj);
void Obj_NAWaterSurface(Object *obj);
void Obj_NAWaterSplash(Object *obj);
void Obj_Redz(Object *obj);
void Obj_CollapseLedge(Object *obj);
void Obj_CollapseFloor(Object *obj);
void Obj_VanishPlatform(Object *obj);
void Obj_Helix(Object *obj);
void Obj_MarbleBrick(Object *obj);
void Obj_Button(Object *obj);
void Obj_SmashBlock(Object *obj);
void Obj_MovingBlock(Object *obj);
void Obj_LargeGrass(Object *obj);
void Obj_GrassFire(Object *obj);
void Obj_LavaTag(Object *obj);
void Obj_Caterkiller(Object *obj);
void Obj_LavaMaker(Object *obj);
void Obj_LavaBall(Object *obj);
void Obj_GlassBlock(Object *obj);
void Obj_Basaran(Object *obj);
void Obj_ChainStomp(Object *obj);
void Obj_GeyserMaker(Object *obj);
void Obj_LavaGeyser(Object *obj);
void Obj_PushBlock(Object *obj);
void Obj_Yadrin(Object *obj);
void Obj_SpinningLight(Object *obj);
void Obj_Bumper(Object *obj);
void Obj_Roller(Object *obj);
void Obj_SpikeBall(Object *obj);
void Obj_FloatingBlock(Object *obj);
void Obj_SidewaysStomper(Object *obj);
void Obj_LavaWall(Object *obj);
void Obj_LZWaterfall(Object *obj);
void Obj_FlapDoor(Object *obj);
void Obj_Harpoon(Object *obj);
void Obj_Pole(Object *obj);
void Obj_Gargoyle(Object *obj);
void Obj_LabyrinthBlock(Object *obj);
void Obj_LabyrinthConvey(Object *obj);
void Obj_Jaws(Object *obj);
void Obj_Burrobot(Object *obj);
void Obj_Orbinaut(Object *obj);
void Obj_Bomb(Object *obj);
void Obj_Elevator(Object *obj);
void Obj_CirclingPlatform(Object *obj);
void Obj_Staircase(Object *obj);
void Obj_Pylon(Object *obj);
void Obj_Fan(Object *obj);
void Obj_Seesaw(Object *obj);
void Obj_HTZSeesaw(Object *obj);
void Obj_HTZLift(Object *obj);
void Obj_HTZBreakFloor(Object *obj);
void Obj_HTZQuakeBlock(Object *obj);
void Obj_HTZLavaBox(Object *obj);
void Obj_HTZFireball(Object *obj);
void Obj_DiagSpring(Object *obj);
void Obj_CPZTube(Object *obj);
void Obj_CPZElevator(Object *obj);
void Obj_CPZBooster(Object *obj);
void Obj_CPZPipeTipper(Object *obj);
void Obj_CPZBlock(Object *obj);
void Obj_ArrowShooter(Object *obj);
void Obj_NGHZLeaves(Object *obj);
void Obj_SwingDispatch(Object *obj);
void Obj_CPZWorm(Object *obj);
void Obj_FloatingPlatform(Object *obj);
void Obj_CPZBarrier(Object *obj);
void Obj_TubeCover(Object *obj);
void Obj_CPZInvisibleBlock(Object *obj);
void Obj_CPZRotor(Object *obj);
void Obj_CPZSlider(Object *obj);
void Obj_CPZTubeSpring(Object *obj);
void Obj_Electrocuter(Object *obj);
void Obj_SmallDoor(Object *obj);
void Obj_SBZConveyor(Object *obj);
void Obj_Flamethrower(Object *obj);
void Obj_GirderBlock(Object *obj);
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
void Obj_BossFire(Object *obj);
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
void Obj_MagicSwitch(Object *obj);
void Obj_BallHog(Object *obj);
void Obj_Cannonball(Object *obj);

// Sonic 1's objects by object id. The engine (EngineObject.h) runs whichever one an object slot's type names; an id that is not listed, or a NULL entry,
// runs as the null object.
const ObjectFunc game_objects[] = {
	/* 00 ObjId_Null                */ NULL,
	/* 01 ObjId_Sonic               */ Obj_Sonic,
	/* 02 ObjId_02                 */ Obj_Tails,
	/* 03 ObjId_PathSwapper         */ Obj_PathSwapper,
	/* 04 ObjId_04                 */ Obj_NAWaterSurface,
	/* 05 ObjId_05                 */ Obj_TailsTails,
	/* 06 ObjId_06                 */ Obj_Spiral,
	/* 07 ObjId_07                 */ Obj_Null, // removed in Nick Arcade
	/* 08 ObjId_Splash              */ Obj_NAWaterSplash, // (Nick Arcade's water splash: waits in the slot Sonic 1 puts the spin dash dust in until it is asked for)
	/* 09 ObjId_SpecialSonic        */ Obj_SpecialSonic,
	/* 0A ObjId_DrownCount          */ Obj_DrownCount,
	/* 0B ObjId_Pole               */ Obj_CPZPipeTipper, // (the prototype's section of pipe that tips you off)
	/* 0C ObjId_FlapDoor           */ Obj_FloatingPlatform, // (the prototype's small floating platform: none in its levels)
	/* 0D ObjId_Signpost            */ Obj_Signpost,
	/* 0E ObjId_TitleSonic          */ Obj_TitleCharacters,
	/* 0F ObjId_PSB                */ Obj_Null, // Nick Arcade's Unknown0F: not ported yet
	/* 10 ObjId_GHZTunnel          */ Obj_Null, // Nick Arcade's SonAniTest: not ported yet
	/* 11 ObjId_GHZBridge           */ Obj_Bridge,
	/* 12 ObjId_SpinningLight      */ Obj_HPZEmerald,
	/* 13 ObjId_LavaMaker          */ Obj_HPZWaterfall,
	/* 14 ObjId_LavaBall           */ Obj_HTZSeesaw,
	/* 15 ObjId_SwingingPlatform    */ Obj_SwingDispatch, // (Neo Green Hill's swinging platform is the prototype's own: see NGHZObjects.c; the other zones keep the one they had)
	/* 16 ObjId_Harpoon            */ Obj_HTZLift,
	/* 17 ObjId_Helix               */ Obj_Helix,
	/* 18 ObjId_BasicPlatform       */ Obj_BasicPlatform,
	/* 19 ObjId_19                 */ Obj_CPZElevator, // (the prototype's elevator of Chemical Plant)
	/* 1A ObjId_CollapseLedge       */ Obj_CollapsingLedge,
	/* 1B ObjId_WaterSurface       */ Obj_CPZBooster, // (the prototype's speed booster of Chemical Plant)
	/* 1C ObjId_Scenery             */ Obj_Scenery,
	/* 1D ObjId_MagicSwitch        */ Obj_CPZWorm, // (the prototype's droplet chain of Chemical Plant)
	/* 1E ObjId_BallHog            */ Obj_CPZTube, // (the prototype's tube network of Chemical Plant)
	/* 1F ObjId_Crabmeat            */ Obj_CollapsingPlatform, // (the prototype's collapsing platform of Dust Hill and Oil Ocean)
	/* 20 ObjId_Cannonball         */ Obj_HTZFireball, // (the prototype's fireball of Hill Top: the lava bubble)
	/* 21 ObjId_HUD                 */ Obj_HUD,
	/* 22 ObjId_BuzzBomber          */ Obj_ArrowShooter, // (the prototype's arrow shooter of Neo Green Hill)
	/* 23 ObjId_BuzzMissile         */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 24 ObjId_BuzzExplode         */ Obj_BuzzExplode,
	/* 25 ObjId_Ring                */ Obj_Ring,
	/* 26 ObjId_Monitor             */ Obj_Monitor,
	/* 27 ObjId_Explosion           */ Obj_Explosion,
	/* 28 ObjId_Animal              */ Obj_Animals,
	/* 29 ObjId_Points              */ Obj_Points,
	/* 2A ObjId_SmallDoor           */ Obj_DHZStomper, // (the prototype's stomper of Dust Hill)
	/* 2B ObjId_Chopper             */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 2C ObjId_Jaws                */ Obj_NGHZLeaves, // (the prototype's leaves of Neo Green Hill)
	/* 2D ObjId_Burrobot           */ Obj_CPZBarrier, // (the prototype's one way barrier of Chemical Plant)
	/* 2E ObjId_MonitorItem         */ Obj_MonitorItem,
	/* 2F ObjId_LargeGrass         */ Obj_HTZBreakFloor, // (the prototype's breakable floor of Hill Top)
	/* 30 ObjId_GlassBlock         */ Obj_HTZQuakeBlock, // (the prototype's solid ground of Hill Top's earthquake)
	/* 31 ObjId_ChainStomp         */ Obj_HTZLavaBox, // (the prototype's lava boxes of Hill Top)
	/* 32 ObjId_Button             */ Obj_TubeCover, // (the prototype's tube cover of Chemical Plant)
	/* 33 ObjId_PushBlock          */ Obj_Null, // removed in Nick Arcade
	/* 34 ObjId_TitleCard           */ Obj_TitleCard,
	/* 35 ObjId_GrassFire          */ Obj_Null, // removed in Nick Arcade
	/* 36 ObjId_Spikes              */ Obj_Spikes,
	/* 37 ObjId_RingLoss            */ Obj_RingLoss,
	/* 38 ObjId_ShieldInvincibility */ Obj_ShieldInvincibility,
	/* 39 ObjId_GameOverCard        */ Obj_GameOverCard,
	/* 3A ObjId_GotThroughCard      */ Obj_GotThroughCard,
	/* 3B ObjId_GHZRock             */ Obj_GHZRock,
	/* 3C ObjId_SmashWall           */ Obj_SmashWall,
	/* 3D ObjId_BossGreenHill       */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 3E ObjId_PrisonCapsule       */ Obj_PrisonCapsule,
	/* 3F ObjId_ExplosionBomb       */ Obj_ExplosionBomb,
	/* 40 ObjId_Motobug             */ Obj_DiagSpring, // (the prototype's diagonal springs)
	/* 41 ObjId_Spring              */ Obj_Spring,
	/* 42 ObjId_Newtron             */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 43 ObjId_Roller             */ Obj_Null, // removed in Nick Arcade
	/* 44 ObjId_GHZEdge             */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 45 ObjId_SidewaysStomper    */ Obj_Null, // removed in Nick Arcade
	/* 46 ObjId_MarbleBrick        */ Obj_Null, // removed in Nick Arcade
	/* 47 ObjId_Bumper             */ Obj_Switch, // (the prototype's switch)
	/* 48 ObjId_BossBall            */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 49 ObjId_Waterfall          */ Obj_EHZWaterfall,
	/* 4A ObjId_VanishSonic        */ Obj_Null, // Nick Arcade's Octus: not ported yet
	/* 4B ObjId_GiantRing          */ Obj_Buzzer,
	/* 4C ObjId_GeyserMaker        */ Obj_BBat,
	/* 4D ObjId_LavaGeyser         */ Obj_Null, // Nick Arcade's Stego: not ported yet
	/* 4E ObjId_LavaWall           */ Obj_Null, // Nick Arcade's Gator: not ported yet
	/* 4F ObjId_4F                 */ Obj_Redz,
	/* 50 ObjId_Yadrin             */ Obj_Null, // Nick Arcade's Seahorse: not ported yet
	/* 51 ObjId_SmashBlock         */ Obj_Null, // Nick Arcade's Skyhorse: not ported yet
	/* 52 ObjId_MovingBlock        */ Obj_Null, // Nick Arcade's BFish: not ported yet
	/* 53 ObjId_CollapseFloor      */ Obj_Masher,
	/* 54 ObjId_LavaTag            */ Obj_Snail,
	/* 55 ObjId_Basaran            */ Obj_EHZBoss,
	/* 56 ObjId_FloatingBlock      */ Obj_Null, // Nick Arcade's EHZBoss2: not ported yet
	/* 57 ObjId_SpikeBall          */ Obj_Null, // Nick Arcade's EHZBoss3: not ported yet
	/* 58 ObjId_BigSpikeBall       */ Obj_EHZBossPart,
	/* 59 ObjId_Elevator           */ Obj_Null, // removed in Nick Arcade
	/* 5A ObjId_CirclingPlatform   */ Obj_Null, // removed in Nick Arcade
	/* 5B ObjId_Staircase          */ Obj_Null, // removed in Nick Arcade
	/* 5C ObjId_Pylon              */ Obj_Null, // removed in Nick Arcade
	/* 5D ObjId_Fan                */ Obj_Null, // removed in Nick Arcade
	/* 5E ObjId_Seesaw             */ Obj_Null, // removed in Nick Arcade
	/* 5F ObjId_Bomb               */ Obj_Null, // removed in Nick Arcade
	/* 60 ObjId_Orbinaut           */ Obj_Null, // removed in Nick Arcade
	/* 61 ObjId_LabyrinthBlock     */ Obj_Null, // removed in Nick Arcade
	/* 62 ObjId_Gargoyle           */ Obj_Null, // removed in Nick Arcade
	/* 63 ObjId_LabyrinthConvey    */ Obj_Null, // removed in Nick Arcade
	/* 64 ObjId_Bubble             */ Obj_Null, // removed in Nick Arcade
	/* 65 ObjId_LZWaterfall        */ Obj_Null, // removed in Nick Arcade
	/* 66 ObjId_Junction           */ Obj_Null, // removed in Nick Arcade
	/* 67 ObjId_RunningDisc        */ Obj_Null, // removed in Nick Arcade
	/* 68 ObjId_SBZConveyor        */ Obj_Null, // removed in Nick Arcade
	/* 69 ObjId_SpinPlatform       */ Obj_Null, // removed in Nick Arcade
	/* 6A ObjId_Saw                */ Obj_RotatingBoxes, // (the prototype's moving boxes of Dust Hill; Metropolis's come with that zone)
	/* 6B ObjId_ScrapStomp         */ Obj_CPZBlock, // (the prototype's moving block of Chemical Plant: the staircases)
	/* 6C ObjId_VanishPlatform     */ Obj_Null, // removed in Nick Arcade
	/* 6D ObjId_Flamethrower       */ Obj_Null, // removed in Nick Arcade
	/* 6E ObjId_Electrocuter       */ Obj_Null, // removed in Nick Arcade
	/* 6F ObjId_SpinConvey         */ Obj_Null, // removed in Nick Arcade
	/* 70 ObjId_GirderBlock        */ Obj_Null, // removed in Nick Arcade
	/* 71 ObjId_InvisibleBarrier   */ Obj_HPZDecor,
	/* 72 ObjId_Teleporter         */ Obj_Null, // removed in Nick Arcade
	/* 73 ObjId_BossMarble         */ Obj_Null, // removed in Nick Arcade
	/* 74 ObjId_BossFire           */ Obj_CPZInvisibleBlock, // (the prototype's invisible block)
	/* 75 ObjId_BossSpringYard     */ Obj_SpikeballChain, // (the prototype's spiked ball on a chain of Dust Hill)
	/* 76 ObjId_BossBlock          */ Obj_PlatformSpikes, // (the prototype's platform with spikes at its sides, Dust Hill)
	/* 77 ObjId_BossLabyrinth      */ Obj_DHZGate, // (the prototype's drawbridge of Dust Hill)
	/* 78 ObjId_Caterkiller        */ Obj_CPZRotor, // (the prototype's rotating platforms of Chemical Plant)
	/* 79 ObjId_Checkpoint          */ Obj_Checkpoint,
	/* 7A ObjId_BossStarLight      */ Obj_CPZSlider, // (the prototype's sliding platforms of Chemical Plant)
	/* 7B ObjId_BossSpikeball      */ Obj_CPZTubeSpring, // (the prototype's spring tubes of Chemical Plant)
	/* 7C ObjId_RingFlash          */ Obj_Null, // removed in Nick Arcade
	/* 7D ObjId_HiddenBonus         */ Obj_HiddenBonus,
	/* 7E ObjId_SSResult           */ Obj_Null, // removed in Nick Arcade
	/* 7F ObjId_SSRChaos           */ Obj_Null, // removed in Nick Arcade
	/* 80 ObjId_80                 */ Obj_Null, // removed in Nick Arcade
	/* 81 ObjId_81                 */ Obj_Null, // removed in Nick Arcade
	/* 82 ObjId_ScrapEggman        */ Obj_Null, // removed in Nick Arcade
	/* 83 ObjId_FalseFloor         */ Obj_Null, // removed in Nick Arcade
	/* 84 ObjId_EggmanCylinder     */ Obj_Null, // removed in Nick Arcade
	/* 85 ObjId_BossFinal          */ Obj_Null, // removed in Nick Arcade
	/* 86 ObjId_BossPlasma         */ Obj_Null, // removed in Nick Arcade
	/* 87 ObjId_87                 */ Obj_Null, // removed in Nick Arcade
	/* 88 ObjId_88                 */ Obj_Null, // removed in Nick Arcade
	/* 89 ObjId_89                 */ Obj_Null, // removed in Nick Arcade
	/* 8A ObjId_Credits             */ Obj_Credits,
	/* 8B ObjId_8B                 */ Obj_Null, // removed in Nick Arcade
	/* 8C ObjId_8C                 */ Obj_Null, // removed in Nick Arcade
	/* 8D ObjId_8D                 */ Obj_Null, // removed in Nick Arcade
};
const int game_object_count = (int)(sizeof(game_objects) / sizeof(game_objects[0]));
