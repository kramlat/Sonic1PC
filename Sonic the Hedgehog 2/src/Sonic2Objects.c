#include "SstLoad.h"
#include "Object/Bridge.h"
#include "Object/CNZBoss.h"
#include "Object/CNZObjects.h"
#include "Object/CNZFlippers.h"
#include "Object/CNZSlots.h"
#include "Object/CNZPlunger.h"
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
#include "Object/CPZBoss.h"
#include "Object/SFZObjects.h"
#include "Object/SkyBadniks.h"
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
void Obj_GrabberClaw(Sst *self);
void Obj_GrabberHead(Sst *self);
void Obj_GrabberStrand(Sst *self);
void Obj_GrabberUnused(Sst *self);
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
#include "Object/NGHZPillars.h"
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


// The ids without a routine here are skipped by the loader (their layout entries stay in the level, no object is made):
//   not ported yet: 0F (Nick Arcade's Unknown0F), 10 (SonAniTest), 23 and 2B and 3D (the prototype's own objects), 4A (Octus), 50 (Seahorse), 59 (the alpha's Motobug, unlinked: its code is in EHZBadniks.c),
//     99 (Nebula), 9A (Turtloid), 9B (Turtloid rider), 9C (enemy boost), AC (Balkiry), AD (Clucker platform), AE (Clucker)
//   removed in Nick Arcade: 35, 5A, 5D to 63, 73, 7C, 82, 85 to 88, 8B

// The objects that have moved over to the status table of Sonic 3 & Knuckles (engine/Sst.h), by id; Sonic 1's shared ones. The loader of that table loads their layout entries.
const SstCode sst_object_codes[0x100] = {
	[0x04] = Obj_NAWaterSurface,
	[0x06] = Obj_Spiral,
	[0x07] = Obj_OilSurface,
	[0x08] = Obj_DustSplash,
	[0x0A] = Obj_Countdown,
	[0x0B] = Obj_CPZPipeTipper,
	[0x0C] = Obj_FloatingPlatform,
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
	[0x5D] = Obj_CPZBoss,
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
	[0x23] = Obj_NGHZPillar,
	[0x2B] = Obj_NGHZRisingPillar,
	[0xD8] = Obj_CNZGreenBumper,
	[0xD7] = Obj_CNZBlueBumper,
	[0x86] = Obj_CNZFlipper,
	[0x85] = Obj_CNZPlunger,
	[0xD6] = Obj_CNZSlotStarter,
	[0x82] = Obj_NGHZFallingPillar,
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
	[0x99] = Obj_Nebula,
	[0x9A] = Obj_Turtloid,
	[0x9B] = Obj_TurtloidTurret,
	[0x9C] = Obj_EnemyBoost,
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
	[0xA8] = Obj_GrabberClaw,
	[0xA9] = Obj_GrabberHead,
	[0xAA] = Obj_GrabberStrand,
	[0xAB] = Obj_GrabberUnused,
	[0x8B] = Obj_SFZPaletteSwitch,
	[0xAC] = Obj_Balkiry,
	[0xAD] = Obj_SFZCluckerPlatform,
	[0xAE] = Obj_Clucker,
	[0xB2] = Obj_SFZTornado,
	[0xBA] = Obj_SFZConveyorWheels,
	[0xBD] = Obj_SFZElevator,
	[0xC0] = Obj_SFZSpeedBooster,
	[0xB3] = Obj_SFZClouds,
	[0xB4] = Obj_SFZVerticalHelix,
	[0xBB] = Obj_SFZChainHook,
	[0xBC] = Obj_SFZRocketBoost,
	[0xBE] = Obj_SFZHatchway,
	[0xBF] = Obj_SFZEnginePart,
	[0xB5] = Obj_SFZHorizontalHelix,
	[0xB6] = Obj_SFZPlatformHelix,
	[0xB7] = Obj_SFZLargeLaser,
	[0xB8] = Obj_SFZCannon,
	[0xB9] = Obj_SFZLaser,
	[0xC1] = Obj_SFZBreakableWall,
	[0xC2] = Obj_SFZBossSwitch,
	[0xC3] = Obj_SFZRotatingPlatforms,
	[0xC4] = Obj_SFZTornadoSmoke,
	[0xD9] = Obj_SFZHang,
	[ObjId_FalseFloor] = Obj_NGHZRotator,
	[ObjId_SwingingPlatform] = Obj_SwingDispatch,
	[ObjId_Jaws] = Obj_NGHZLeaves,
	[ObjId_BuzzBomber] = Obj_ArrowShooter,
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
