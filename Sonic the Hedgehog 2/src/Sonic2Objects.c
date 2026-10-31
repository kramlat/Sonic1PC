#include "SstLoad.h"
#include "Object/Bridge.h"
#include "Object/CNZBoss.h"
#include "Object/CNZObjects.h"
#include "Object/CPZBadniks.h"
#include "Object/CPZObjects.h"
#include "Object/CPZTubes.h"
#include "Object/Coconuts.h"
#include "Object/Countdown.h"
#include "Object/Crocobot.h"
#include "Object/DHZBadniks.h"
#include "Object/DHZBoss.h"
#include "Object/DHZObjects.h"
#include "Object/DHZVines.h"
#include "Object/DiagSpring.h"
#include "Object/DustSplash.h"
#include "Object/EHZBadniks.h"
#include "Object/EHZBoss.h"
#include "Object/EHZWaterfall.h"
#include "Object/HPZBadniks.h"
#include "Object/HPZObjects.h"
#include "Object/HTZBadniks.h"
#include "Object/HTZBoss.h"
#include "Object/HTZObjects.h"
#include "Object/MTZBadniks.h"
#include "Object/MTZBoss.h"
#include "Object/MTZObjects.h"
#include "Object/NGHZBadniks.h"
#include "Object/NGHZBoss.h"
#include "Object/OOZObjects.h"
#include "Object/OxygenBubbles.h"
#include "Object/Platform.h"
#include "Object/Scenery.h"
#include "Object/Spikes.h"
#include "Object/Spiral.h"
#include "Object/Spring.h"
#include "Object/WaterObjects.h"
void Obj_Asteron(Sst *self);
void Obj_AutoSpin(Sst *self);
void Obj_BBat(Sst *self);
void Obj_BasicPlatform(Sst *self);
void Obj_BossExplosion(Sst *self);
void Obj_Bridge(Sst *self);
void Obj_Buzzer(Sst *self);
void Obj_CNZBoss(Sst *self);
void Obj_CNZBumper(Sst *self);
void Obj_CPZBarrier(Sst *self);
void Obj_CPZBlock(Sst *self);
void Obj_CPZBooster(Sst *self);
void Obj_CPZElevator(Sst *self);
void Obj_CPZInvisibleBlock(Sst *self);
void Obj_CPZPipeTipper(Sst *self);
void Obj_CPZRotor(Sst *self);
void Obj_CPZSlider(Sst *self);
void Obj_CPZTube(Sst *self);
void Obj_CPZTubeSpring(Sst *self);
void Obj_CPZWorm(Sst *self);
void Obj_ChopChop(Sst *self);
void Obj_Coconuts(Sst *self);
void Obj_CollapsingLedge(Sst *self);
void Obj_CollapsingPlatform(Sst *self);
void Obj_Countdown(Sst *self);
void Obj_Crawlton(Sst *self);
void Obj_Crocobot(Sst *self);
void Obj_DHZBoss(Sst *self);
void Obj_DHZGate(Sst *self);
void Obj_DHZStomper(Sst *self);
void Obj_DHZVerticalBridge(Sst *self);
void Obj_DHZVines(Sst *self);
void Obj_DHZVinesSwitch(Sst *self);
void Obj_DiagSpring(Sst *self);
void Obj_DustSplash(Sst *self);
void Obj_EHZBoss(Sst *self);
void Obj_EHZBossPart(Sst *self);
void Obj_EHZWaterfall(Sst *self);
void Obj_EnemyWeapon(Sst *self);
void Obj_Flasher(Sst *self);
void Obj_FloatingPlatform(Sst *self);
void Obj_Grabber(Sst *self);
void Obj_Grounder(Sst *self);
void Obj_GrounderRock(Sst *self);
void Obj_GrounderWall(Sst *self);
void Obj_HPZDecor(Sst *self);
void Obj_HPZEmerald(Sst *self);
void Obj_HPZWaterfall(Sst *self);
void Obj_HTZBoss(Sst *self);
void Obj_HTZBreakFloor(Sst *self);
void Obj_HTZFireball(Sst *self);
void Obj_HTZLavaBox(Sst *self);
void Obj_HTZLift(Sst *self);
void Obj_HTZQuakeBlock(Sst *self);
void Obj_HTZSeesaw(Sst *self);
void Obj_MTZBlockArrow(Sst *self);
void Obj_MTZBoss(Sst *self);
void Obj_MTZBossBall(Sst *self);
void Obj_MTZConveyor(Sst *self);
void Obj_MTZElevator(Sst *self);
void Obj_MTZGear(Sst *self);
void Obj_MTZHarpoon(Sst *self);
void Obj_MTZMachine(Sst *self);
void Obj_MTZMovingPlatform(Sst *self);
void Obj_MTZPiston(Sst *self);
void Obj_MTZPlatform(Sst *self);
void Obj_MTZScrewNut(Sst *self);
void Obj_MTZSpringWall(Sst *self);
void Obj_MTZSteamVent(Sst *self);
void Obj_MTZTeleport(Sst *self);
void Obj_Masher(Sst *self);
void Obj_NAWaterSurface(Sst *self);
void Obj_NGHZBoss(Sst *self);
void Obj_OOZCannon(Sst *self);
void Obj_OOZLauncher(Sst *self);
void Obj_OOZPushSpring(Sst *self);
void Obj_OOZSpikeball(Sst *self);
void Obj_OOZSpringBall(Sst *self);
void Obj_OilSurface(Sst *self);
void Obj_OxygenBubbles(Sst *self);
void Obj_PlatformSpikes(Sst *self);
void Obj_Redz(Sst *self);
void Obj_RexonBody(Sst *self);
void Obj_RexonHead(Sst *self);
void Obj_Rhinobot(Sst *self);
void Obj_RotatingBoxes(Sst *self);
void Obj_Scenery(Sst *self);
void Obj_Shellcracker(Sst *self);
void Obj_ShellcrackerClaw(Sst *self);
void Obj_Slicer(Sst *self);
void Obj_SlicerPincers(Sst *self);
void Obj_Sol(Sst *self);
void Obj_SpikeballChain(Sst *self);
void Obj_Spiker(Sst *self);
void Obj_SpikerDrill(Sst *self);
void Obj_Spikes(Sst *self);
void Obj_Spiny(Sst *self);
void Obj_SpinyVertical(Sst *self);
void Obj_Spiral(Sst *self);
void Obj_Spring(Sst *self);
void Obj_Switch(Sst *self);
void Obj_TitleCharacters(Sst *self);
void Obj_TubeCover(Sst *self);
void Obj_Whisp(Sst *self);
#include "Object/NGHZObjects.h"
#include "Object/TitleCard.h"
#include "Object/Animals.h"
#include "Object/Monitor.h"
#include "Object/Signpost.h"
#include "Object/Points.h"
#include "Object/HUD.h"
#include "Object/PrisonCapsule.h"
#include "Object/Ring.h"
#include "Object/GameOverCard.h"
#include "Object/Credits.h"
#include "Object/HiddenBonus.h"
#include "Object/Checkpoint.h"
#include "Object/ShieldInvincibility.h"
#include "Object/SuperSonicStars.h"
#include "Object/SpecialSonic.h"
#include "Object/SmashWall.h"
#include "Object/GHZRock.h"
#include "Object/Helix.h"
#include "Object/Explosion.h"
#include "Object.h"

#include "Object/AirBubbles.h"
#include "Object/DHZObjects.h"
#include "Object/MTZObjects.h"
#include "Object/OOZObjects.h"
#include "Object/InvisibleBarrier.h"
#include "Object/PathSwapper.h"
#include "Object/Splash.h"
#include "Object/WaterSurface.h"

void Obj_Sonic(Sst *obj);
void Obj_Tails(Sst *obj);
void Obj_TailsTails(Sst *obj);
void Obj_PSB(Object *obj);
void Obj_GHZTunnel(Object *obj);
void Obj_GHZBridge(Object *obj);
void Obj_Crabmeat(Object *obj);
void Obj_BuzzBomber(Object *obj);
void Obj_BuzzMissile(Object *obj);
void Obj_BuzzExplode(Object *obj);
void Obj_Chopper(Object *obj);
void Obj_SwingingPlatform(Object *obj);
void Obj_BigSpikeBall(Object *obj);
void Obj_Motobug(Object *obj);
void Obj_Newtron(Object *obj);
void Obj_GHZEdge(Object *obj);
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
void Obj_CollapseLedge(Object *obj);
void Obj_CollapseFloor(Object *obj);
void Obj_VanishPlatform(Object *obj);
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
void Obj_VanishSonic(Object *obj);
void Obj_MagicSwitch(Object *obj);
void Obj_BallHog(Object *obj);
void Obj_Cannonball(Object *obj);

// Sonic 1's objects by object id. The engine (EngineObject.h) runs whichever one an object slot's type names; an id that is not listed, or a NULL entry,
// runs as the null object.
const ObjectFunc game_objects[] = {
	/* 00 ObjId_Null                */ NULL,
	/* 01 ObjId_Sonic               */ NULL, // (the characters are on the status table: sst_object_codes)
	/* 02 ObjId_02                 */ NULL,
	/* 03 ObjId_PathSwapper         */ NULL, // (moved to the status table: sst_object_codes)
	/* 04 ObjId_04                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 05 ObjId_05                 */ NULL,
	/* 06 ObjId_06                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 07 ObjId_07                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 08 ObjId_Splash              */ NULL, // (moved to the status table: sst_object_codes)
	/* 09 ObjId_SpecialSonic        */ NULL, // (moved to the status table: sst_object_codes)
	/* 0A ObjId_DrownCount          */ NULL, // (moved to the status table: sst_object_codes)
	/* 0B ObjId_Pole               */ NULL, // (moved to the status table: sst_object_codes)
	/* 0C ObjId_FlapDoor           */ NULL, // (moved to the status table: sst_object_codes)
	/* 0D ObjId_Signpost            */ NULL, // (moved to the status table: sst_object_codes)
	/* 0E ObjId_TitleSonic          */ NULL, // (moved to the status table: sst_object_codes)
	/* 0F ObjId_PSB                */ Obj_Null, // Nick Arcade's Unknown0F: not ported yet
	/* 10 ObjId_GHZTunnel          */ Obj_Null, // Nick Arcade's SonAniTest: not ported yet
	/* 11 ObjId_GHZBridge           */ NULL, // (moved to the status table: sst_object_codes)
	/* 12 ObjId_SpinningLight      */ NULL, // (moved to the status table: sst_object_codes)
	/* 13 ObjId_LavaMaker          */ NULL, // (moved to the status table: sst_object_codes)
	/* 14 ObjId_LavaBall           */ NULL, // (moved to the status table: sst_object_codes)
	/* 15 ObjId_SwingingPlatform    */ NULL, // (moved to the status table: sst_object_codes)
	/* 16 ObjId_Harpoon            */ NULL, // (moved to the status table: sst_object_codes)
	/* 17 ObjId_Helix               */ NULL, // (moved to the status table: sst_object_codes)
	/* 18 ObjId_BasicPlatform       */ NULL, // (moved to the status table: sst_object_codes)
	/* 19 ObjId_19                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 1A ObjId_CollapseLedge       */ NULL, // (moved to the status table: sst_object_codes)
	/* 1B ObjId_WaterSurface       */ NULL, // (moved to the status table: sst_object_codes)
	/* 1C ObjId_Scenery             */ NULL, // (moved to the status table: sst_object_codes)
	/* 1D ObjId_MagicSwitch        */ NULL, // (moved to the status table: sst_object_codes)
	/* 1E ObjId_BallHog            */ NULL, // (moved to the status table: sst_object_codes)
	/* 1F ObjId_Crabmeat            */ NULL, // (moved to the status table: sst_object_codes)
	/* 20 ObjId_Cannonball         */ NULL, // (moved to the status table: sst_object_codes)
	/* 21 ObjId_HUD                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 22 ObjId_BuzzBomber          */ NULL, // (moved to the status table: sst_object_codes)
	/* 23 ObjId_BuzzMissile         */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 24 ObjId_BuzzExplode         */ NULL, // (moved to the status table: sst_object_codes)
	/* 25 ObjId_Ring                */ NULL, // (moved to the status table: sst_object_codes)
	/* 26 ObjId_Monitor             */ NULL, // (moved to the status table: sst_object_codes)
	/* 27 ObjId_Explosion           */ NULL, // (moved to the status table: sst_object_codes)
	/* 28 ObjId_Animal              */ NULL, // (moved to the status table: sst_object_codes)
	/* 29 ObjId_Points              */ NULL, // (moved to the status table: sst_object_codes)
	/* 2A ObjId_SmallDoor           */ NULL, // (moved to the status table: sst_object_codes)
	/* 2B ObjId_Chopper             */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 2C ObjId_Jaws                */ NULL, // (moved to the status table: sst_object_codes)
	/* 2D ObjId_Burrobot           */ NULL, // (moved to the status table: sst_object_codes)
	/* 2E ObjId_MonitorItem         */ NULL, // (moved to the status table: sst_object_codes)
	/* 2F ObjId_LargeGrass         */ NULL, // (moved to the status table: sst_object_codes)
	/* 30 ObjId_GlassBlock         */ NULL, // (moved to the status table: sst_object_codes)
	/* 31 ObjId_ChainStomp         */ NULL, // (moved to the status table: sst_object_codes)
	/* 32 ObjId_Button             */ NULL, // (moved to the status table: sst_object_codes)
	/* 33 ObjId_PushBlock          */ NULL, // (moved to the status table: sst_object_codes)
	/* 34 ObjId_TitleCard           */ NULL, // (moved to the status table: sst_object_codes)
	/* 35 ObjId_GrassFire          */ Obj_Null, // removed in Nick Arcade
	/* 36 ObjId_Spikes              */ NULL, // (moved to the status table: sst_object_codes)
	/* 37 ObjId_RingLoss            */ NULL, // (moved to the status table: sst_object_codes)
	/* 38 ObjId_ShieldInvincibility */ NULL, // (moved to the status table: sst_object_codes)
	/* 39 ObjId_GameOverCard        */ NULL, // (moved to the status table: sst_object_codes)
	/* 3A ObjId_GotThroughCard      */ NULL, // (moved to the status table: sst_object_codes)
	/* 3B ObjId_GHZRock             */ NULL, // (moved to the status table: sst_object_codes)
	/* 3C ObjId_SmashWall           */ NULL, // (moved to the status table: sst_object_codes)
	/* 3D ObjId_BossGreenHill       */ Obj_Null, // (the prototype's object here is not ported yet)
	/* 3E ObjId_PrisonCapsule       */ NULL, // (moved to the status table: sst_object_codes)
	/* 3F ObjId_ExplosionBomb       */ NULL, // (moved to the status table: sst_object_codes)
	/* 40 ObjId_Motobug             */ NULL, // (moved to the status table: sst_object_codes)
	/* 41 ObjId_Spring              */ NULL, // (moved to the status table: sst_object_codes)
	/* 42 ObjId_Newtron             */ NULL, // (moved to the status table: sst_object_codes)
	/* 43 ObjId_Roller             */ NULL, // (moved to the status table: sst_object_codes)
	/* 44 ObjId_GHZEdge             */ NULL, // (moved to the status table: sst_object_codes)
	/* 45 ObjId_SidewaysStomper    */ NULL, // (moved to the status table: sst_object_codes)
	/* 46 ObjId_MarbleBrick        */ NULL, // (moved to the status table: sst_object_codes)
	/* 47 ObjId_Bumper             */ NULL, // (moved to the status table: sst_object_codes)
	/* 48 ObjId_BossBall            */ NULL, // (moved to the status table: sst_object_codes)
	/* 49 ObjId_Waterfall          */ NULL, // (moved to the status table: sst_object_codes)
	/* 4A ObjId_VanishSonic        */ Obj_Null, // Nick Arcade's Octus: not ported yet
	/* 4B ObjId_GiantRing          */ NULL, // (moved to the status table: sst_object_codes)
	/* 4C ObjId_GeyserMaker        */ NULL, // (moved to the status table: sst_object_codes)
	/* 4D ObjId_LavaGeyser         */ NULL, // (moved to the status table: sst_object_codes)
	/* 4E ObjId_LavaWall           */ NULL, // (moved to the status table: sst_object_codes)
	/* 4F ObjId_4F                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 50 ObjId_Yadrin             */ Obj_Null, // Nick Arcade's Seahorse: not ported yet
	/* 51 ObjId_SmashBlock         */ NULL, // (moved to the status table: sst_object_codes)
	/* 52 ObjId_MovingBlock        */ NULL, // (moved to the status table: sst_object_codes)
	/* 53 ObjId_53                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 54 ObjId_54                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 55 ObjId_55                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 56 ObjId_56                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 57 ObjId_SpikeBall          */ NULL, // (moved to the status table: sst_object_codes)
	/* 58 ObjId_58                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 59 ObjId_59                 */ Obj_Null, // (the alpha's Motobug, which is Nick Arcade's Snail: unlinked, as the alpha's Emerald Hill has none; its code is still in EHZBadniks.c)
	/* 5A ObjId_CirclingPlatform   */ Obj_Null, // removed in Nick Arcade
	/* 5B ObjId_5B                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 5C ObjId_Pylon              */ NULL, // (moved to the status table: sst_object_codes)
	/* 5D ObjId_Fan                */ Obj_Null, // removed in Nick Arcade
	/* 5E ObjId_Seesaw             */ Obj_Null, // removed in Nick Arcade
	/* 5F ObjId_Bomb               */ Obj_Null, // removed in Nick Arcade
	/* 60 ObjId_Orbinaut           */ Obj_Null, // removed in Nick Arcade
	/* 61 ObjId_LabyrinthBlock     */ Obj_Null, // removed in Nick Arcade
	/* 62 ObjId_Gargoyle           */ Obj_Null, // removed in Nick Arcade
	/* 63 ObjId_LabyrinthConvey    */ Obj_Null, // removed in Nick Arcade
	/* 64 ObjId_Bubble             */ NULL, // (moved to the status table: sst_object_codes)
	/* 65 ObjId_LZWaterfall        */ NULL, // (moved to the status table: sst_object_codes)
	/* 66 ObjId_Junction           */ NULL, // (moved to the status table: sst_object_codes)
	/* 67 ObjId_RunningDisc        */ NULL, // (moved to the status table: sst_object_codes)
	/* 68 ObjId_SBZConveyor        */ NULL, // (moved to the status table: sst_object_codes)
	/* 69 ObjId_SpinPlatform       */ NULL, // (moved to the status table: sst_object_codes)
	/* 6A ObjId_Saw                */ NULL, // (moved to the status table: sst_object_codes)
	/* 6B ObjId_ScrapStomp         */ NULL, // (moved to the status table: sst_object_codes)
	/* 6C ObjId_VanishPlatform     */ NULL, // (moved to the status table: sst_object_codes)
	/* 6D ObjId_Flamethrower       */ NULL, // (moved to the status table: sst_object_codes)
	/* 6E ObjId_Electrocuter       */ NULL, // (moved to the status table: sst_object_codes)
	/* 6F ObjId_SpinConvey         */ NULL, // (moved to the status table: sst_object_codes)
	/* 70 ObjId_GirderBlock        */ NULL, // (moved to the status table: sst_object_codes)
	/* 71 ObjId_InvisibleBarrier   */ NULL, // (moved to the status table: sst_object_codes)
	/* 72 ObjId_Teleporter         */ NULL, // (moved to the status table: sst_object_codes)
	/* 73 ObjId_BossMarble         */ Obj_Null, // removed in Nick Arcade
	/* 74 ObjId_BossFire           */ NULL, // (moved to the status table: sst_object_codes)
	/* 75 ObjId_BossSpringYard     */ NULL, // (moved to the status table: sst_object_codes)
	/* 76 ObjId_BossBlock          */ NULL, // (moved to the status table: sst_object_codes)
	/* 77 ObjId_BossLabyrinth      */ NULL, // (moved to the status table: sst_object_codes)
	/* 78 ObjId_Caterkiller        */ NULL, // (moved to the status table: sst_object_codes)
	/* 79 ObjId_Checkpoint          */ NULL, // (moved to the status table: sst_object_codes)
	/* 7A ObjId_BossStarLight      */ NULL, // (moved to the status table: sst_object_codes)
	/* 7B ObjId_BossSpikeball      */ NULL, // (moved to the status table: sst_object_codes)
	/* 7C ObjId_RingFlash          */ Obj_Null, // removed in Nick Arcade
	/* 7D ObjId_HiddenBonus         */ NULL, // (moved to the status table: sst_object_codes)
	/* 7E ObjId_SSResult           */ NULL, // (moved to the status table: sst_object_codes; the alpha's Super Sonic stars: nothing makes them yet)
	/* 7F ObjId_SSRChaos           */ NULL, // (moved to the status table: sst_object_codes)
	/* 80 ObjId_80                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 81 ObjId_81                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 82 ObjId_ScrapEggman        */ Obj_Null, // removed in Nick Arcade
	/* 83 ObjId_FalseFloor         */ NULL, // (moved to the status table: sst_object_codes)
	/* 84 ObjId_EggmanCylinder     */ NULL, // (moved to the status table: sst_object_codes)
	/* 85 ObjId_BossFinal          */ Obj_Null, // removed in Nick Arcade
	/* 86 ObjId_BossPlasma         */ Obj_Null, // removed in Nick Arcade
	/* 87 ObjId_87                 */ Obj_Null, // removed in Nick Arcade
	/* 88 ObjId_88                 */ Obj_Null, // removed in Nick Arcade
	/* 89 ObjId_89                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 8A ObjId_Credits             */ NULL, // (moved to the status table: sst_object_codes)
	/* 8B ObjId_8B                 */ Obj_Null, // removed in Nick Arcade
	/* 8C ObjId_8C                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 8D ObjId_8D                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 8E ObjId_8E                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 8F ObjId_8F                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 90 ObjId_90                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 91 ObjId_91                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 92 ObjId_92                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 93 ObjId_93                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 94 ObjId_94                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 95 ObjId_95                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 96 ObjId_96                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 97 ObjId_97                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 98 ObjId_98                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 99 ObjId_99                 */ Obj_Null, // (the alpha's Nebula: not ported yet)
	/* 9A ObjId_9A                 */ Obj_Null, // (the alpha's Turtloid: not ported yet)
	/* 9B ObjId_9B                 */ Obj_Null, // (the alpha's Turtloid rider: not ported yet)
	/* 9C ObjId_9C                 */ Obj_Null, // (the alpha's enemy boost: not ported yet)
	/* 9D ObjId_9D                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 9E ObjId_9E                 */ NULL, // (moved to the status table: sst_object_codes)
	/* 9F ObjId_9F                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A0 ObjId_A0                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A1 ObjId_A1                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A2 ObjId_A2                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A3 ObjId_A3                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A4 ObjId_A4                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A5 ObjId_A5                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A6 ObjId_A6                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A7 ObjId_A7                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A8 ObjId_A8                 */ NULL, // (moved to the status table: sst_object_codes)
	/* A9 ObjId_A9                 */ NULL, // (moved to the status table: sst_object_codes)
	/* AA ObjId_AA                 */ NULL, // (moved to the status table: sst_object_codes)
	/* AB ObjId_AB                 */ NULL, // (moved to the status table: sst_object_codes)
	/* AC ObjId_AC                 */ Obj_Null, // (the alpha's Balkiry: not ported yet)
	/* AD ObjId_AD                 */ Obj_Null, // (the alpha's Clucker platform: not ported yet)
	/* AE ObjId_AE                 */ Obj_Null, // (the alpha's Clucker: not ported yet)
};
const int game_object_count = (int)(sizeof(game_objects) / sizeof(game_objects[0]));

// The objects that have moved over to the status table of Sonic 3 & Knuckles (engine/Sst.h), by id; Sonic 1's shared ones. The loader of that table loads their layout entries.
const SstCode sst_object_codes[0x100] = {
	[0x04] = Obj_NAWaterSurface,
	[0x06] = Obj_Spiral,
	[0x07] = Obj_OilSurface,
	[0x08] = Obj_DustSplash,
	[0x0A] = Obj_Countdown,
	[0x0B] = Obj_CPZPipeTipper,
	[0x0C] = Obj_FloatingPlatform,
	[0x0E] = Obj_TitleCharacters,
	[0x11] = Obj_Bridge,
	[0x12] = Obj_HPZEmerald,
	[0x13] = Obj_HPZWaterfall,
	[0x14] = Obj_HTZSeesaw,
	[0x16] = Obj_HTZLift,
	[0x18] = Obj_BasicPlatform,
	[0x19] = Obj_CPZElevator,
	[0x1A] = Obj_CollapsingLedge,
	[0x1B] = Obj_CPZBooster,
	[0x1C] = Obj_Scenery,
	[0x1D] = Obj_CPZWorm,
	[0x1E] = Obj_CPZTube,
	[0x1F] = Obj_CollapsingPlatform,
	[0x20] = Obj_HTZFireball,
	[0x24] = Obj_OxygenBubbles,
	[0x2A] = Obj_DHZStomper,
	[0x2D] = Obj_CPZBarrier,
	[0x2F] = Obj_HTZBreakFloor,
	[0x30] = Obj_HTZQuakeBlock,
	[0x31] = Obj_HTZLavaBox,
	[0x32] = Obj_TubeCover,
	[0x33] = Obj_OOZLauncher,
	[0x36] = Obj_Spikes,
	[0x40] = Obj_DiagSpring,
	[0x41] = Obj_Spring,
	[0x42] = Obj_MTZSteamVent,
	[0x43] = Obj_OOZSpikeball,
	[0x44] = Obj_CNZBumper,
	[0x45] = Obj_OOZPushSpring,
	[0x46] = Obj_OOZSpringBall,
	[0x47] = Obj_Switch,
	[0x48] = Obj_OOZCannon,
	[0x49] = Obj_EHZWaterfall,
	[0x4B] = Obj_Buzzer,
	[0x4C] = Obj_BBat,
	[0x4D] = Obj_Rhinobot,
	[0x4E] = Obj_Crocobot,
	[0x4F] = Obj_Redz,
	[0x51] = Obj_CNZBoss,
	[0x52] = Obj_HTZBoss,
	[0x53] = Obj_MTZBossBall,
	[0x54] = Obj_MTZBoss,
	[0x55] = Obj_MTZBoss,
	[0x56] = Obj_EHZBoss,
	[0x57] = Obj_DHZBoss,
	[0x58] = Obj_BossExplosion,
	[0x5B] = Obj_EHZBossPart,
	[0x5C] = Obj_Masher,
	[0x64] = Obj_MTZPiston,
	[0x65] = Obj_MTZPlatform,
	[0x66] = Obj_MTZSpringWall,
	[0x67] = Obj_MTZTeleport,
	[0x68] = Obj_MTZBlockArrow,
	[0x69] = Obj_MTZScrewNut,
	[0x6A] = Obj_RotatingBoxes,
	[0x6B] = Obj_CPZBlock,
	[0x6C] = Obj_MTZMovingPlatform,
	[0x6D] = Obj_MTZHarpoon,
	[0x6E] = Obj_MTZMachine,
	[0x6F] = Obj_MTZElevator,
	[0x70] = Obj_MTZGear,
	[0x71] = Obj_HPZDecor,
	[0x72] = Obj_MTZConveyor,
	[0x74] = Obj_CPZInvisibleBlock,
	[0x75] = Obj_SpikeballChain,
	[0x76] = Obj_PlatformSpikes,
	[0x77] = Obj_DHZGate,
	[0x78] = Obj_CPZRotor,
	[0x7A] = Obj_CPZSlider,
	[0x7B] = Obj_CPZTubeSpring,
	[0x7F] = Obj_DHZVinesSwitch,
	[0x80] = Obj_DHZVines,
	[0x81] = Obj_DHZVerticalBridge,
	[0x84] = Obj_AutoSpin,
	[0x89] = Obj_NGHZBoss,
	[0x8C] = Obj_Whisp,
	[0x8D] = Obj_Grounder,
	[0x8E] = Obj_Grounder,
	[0x8F] = Obj_GrounderWall,
	[0x90] = Obj_GrounderRock,
	[0x91] = Obj_ChopChop,
	[0x92] = Obj_Spiker,
	[0x93] = Obj_SpikerDrill,
	[0x94] = Obj_RexonBody,
	[0x95] = Obj_Sol,
	[0x96] = Obj_RexonBody,
	[0x97] = Obj_RexonHead,
	[0x98] = Obj_EnemyWeapon,
	[0x9D] = Obj_Coconuts,
	[0x9E] = Obj_Crawlton,
	[0x9F] = Obj_Shellcracker,
	[0xA0] = Obj_ShellcrackerClaw,
	[0xA1] = Obj_Slicer,
	[0xA2] = Obj_SlicerPincers,
	[0xA3] = Obj_Flasher,
	[0xA4] = Obj_Asteron,
	[0xA5] = Obj_Spiny,
	[0xA6] = Obj_SpinyVertical,
	[0xA7] = Obj_Grabber,
	[0xA8] = Obj_Grabber,
	[0xA9] = Obj_Grabber,
	[0xAA] = Obj_Grabber,
	[0xAB] = Obj_Grabber,
	[ObjId_FalseFloor] = Obj_NGHZRotator,
	[ObjId_SwingingPlatform] = Obj_SwingDispatch,
	[ObjId_Jaws] = Obj_NGHZLeaves,
	[ObjId_BuzzBomber] = Obj_ArrowShooter,
	[ObjId_HUD] = Obj_HUD,
	[ObjId_GotThroughCard] = Obj_GotThroughCard,
	[ObjId_TitleCard] = Obj_TitleCard,
	[ObjId_Animal] = Obj_FlickyAnimals,
	[ObjId_MonitorItem] = Obj_MonitorItem,
	[ObjId_Monitor] = Obj_Monitor,
	[ObjId_PrisonCapsule] = Obj_PrisonCapsule,
	[ObjId_RingLoss] = Obj_RingLoss,
	[ObjId_Ring] = Obj_Ring,
	[ObjId_Signpost] = Obj_Signpost,
	[ObjId_GameOverCard] = Obj_GameOverCard,
	[ObjId_Credits] = Obj_Credits,
	[ObjId_HiddenBonus] = Obj_HiddenBonus,
	[ObjId_PathSwapper] = Obj_PathSwapper,
	[ObjId_Checkpoint] = Obj_Checkpoint,
	[ObjId_Sonic] = Obj_Sonic,
	[ObjId_ShieldInvincibility] = Obj_ShieldInvincibility,
	[ObjId_SSResult] = Obj_SuperSonicStars,
	[ObjId_02] = Obj_Tails,
	[ObjId_05] = Obj_TailsTails,
	[ObjId_SpecialSonic] = Obj_SpecialSonic,
	[ObjId_Points] = Obj_Points,
	[ObjId_SmashWall] = Obj_SmashWall,
	[ObjId_GHZRock] = Obj_GHZRock,
	[ObjId_Helix] = Obj_Helix,
	[ObjId_Explosion] = Obj_Explosion,
	[ObjId_ExplosionBomb] = Obj_ExplosionBomb, // (Sonic 1's, shared)
};
