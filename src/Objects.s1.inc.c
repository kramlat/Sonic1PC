#ifndef _Included_SOnic1_Object_List
#define _Included_SOnic1_Object_List

void Obj_Sonic(Object *obj);
void Obj_SpecialSonic(Object *obj);
void Obj_Signpost(Object *obj);
void Obj_TitleSonic(Object *obj);
void Obj_PSB(Object *obj);
void Obj_GHZTunnel(Object *obj);
void Obj_GHZBridge(Object *obj);
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
void Obj_Waterfall(Object *obj);
void Obj_GiantRing(Object *obj);
void Obj_RingFlash(Object *obj);
void Obj_HiddenBonus(Object *obj);
void Obj_BasicPlatform(Object *obj);
void Obj_SmashWall(Object *obj);
void Obj_Scenery(Object *obj);
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
void Obj_BossBlock(Object *obj);
void Obj_PrisonCapsule(Object *obj);
void Obj_VanishSonic(Object *obj);
void Obj_MagicSwitch(Object *obj);
void Obj_BallHog(Object *obj);
void Obj_Cannonball(Object *obj);

static void (*object_func[])(Object*) = {
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
	/* 0C ObjId_FlapDoor            */ Obj_FlapDoor,
	/* 0D ObjId_Signpost            */ Obj_Signpost,
	/* 0E ObjId_TitleSonic          */ Obj_TitleSonic,
	/* 0F ObjId_PSB                 */ Obj_PSB,
	/* 10 ObjId_GHZTunnel           */ Obj_GHZTunnel,
	/* 11 ObjId_GHZBridge           */ Obj_GHZBridge,
	/* 12 ObjId_SpinningLight       */ Obj_SpinningLight,
	/* 13 ObjId_LavaMaker           */ Obj_LavaMaker,
	/* 14 ObjId_LavaBall            */ Obj_LavaBall,
	/* 15 ObjId_SwingingPlatform    */ Obj_SwingingPlatform,
	/* 16 ObjId_Harpoon             */ Obj_Harpoon,
	/* 17 ObjId_Helix               */ Obj_Helix,
	/* 18 ObjId_BasicPlatform       */ Obj_BasicPlatform,
	/* 19 ObjId_19                  */ Obj_Null,
	/* 1A ObjId_CollapseLedge       */ Obj_CollapseLedge,
	/* 1B ObjId_WaterSurface        */ Obj_WaterSurface,
	/* 1C ObjId_Scenery             */ Obj_Scenery,
	/* 1D ObjId_MagicSwitch         */ Obj_MagicSwitch,
	/* 1E ObjId_BallHog             */ Obj_BallHog,
	/* 1F ObjId_Crabmeat            */ Obj_Crabmeat,
	/* 20 ObjId_Cannonball          */ Obj_Cannonball,
	/* 21 ObjId_HUD                 */ Obj_HUD,
	/* 22 ObjId_BuzzBomber          */ Obj_BuzzBomber,
	/* 23 ObjId_BuzzMissile         */ Obj_BuzzMissile,
	/* 24 ObjId_BuzzExplode         */ Obj_BuzzExplode,
	/* 25 ObjId_Ring                */ Obj_Ring,
	/* 26 ObjId_Monitor             */ Obj_Monitor,
	/* 27 ObjId_Explosion           */ Obj_Explosion,
	/* 28 ObjId_Animal              */ Obj_Animals,
	/* 29 ObjId_Points              */ Obj_Points,
	/* 2A ObjId_SmallDoor           */ Obj_SmallDoor,
	/* 2B ObjId_Chopper             */ Obj_Chopper,
	/* 2C ObjId_Jaws                */ Obj_Jaws,
	/* 2D ObjId_Burrobot            */ Obj_Burrobot,
	/* 2E ObjId_MonitorItem         */ Obj_MonitorItem,
	/* 2F ObjId_LargeGrass          */ Obj_LargeGrass,
	/* 30 ObjId_GlassBlock          */ Obj_GlassBlock,
	/* 31 ObjId_ChainStomp          */ Obj_ChainStomp,
	/* 32 ObjId_Button              */ Obj_Button,
	/* 33 ObjId_PushBlock           */ Obj_PushBlock,
	/* 34 ObjId_TitleCard           */ Obj_TitleCard,
	/* 35 ObjId_GrassFire           */ Obj_GrassFire,
	/* 36 ObjId_Spikes              */ Obj_Spikes,
	/* 37 ObjId_RingLoss            */ Obj_RingLoss,
	/* 38 ObjId_ShieldInvincibility */ Obj_ShieldInvincibility,
	/* 39 ObjId_GameOverCard        */ Obj_GameOverCard,
	/* 3A ObjId_GotThroughCard      */ Obj_GotThroughCard,
	/* 3B ObjId_GHZRock             */ Obj_GHZRock,
	/* 3C ObjId_SmashWall           */ Obj_SmashWall,
	/* 3D ObjId_BossGreenHill       */ Obj_BossGreenHill,
	/* 3E ObjId_PrisonCapsule       */ Obj_PrisonCapsule,
	/* 3F ObjId_ExplosionBomb       */ Obj_ExplosionBomb,
	/* 40 ObjId_Motobug             */ Obj_Motobug,
	/* 41 ObjId_Spring              */ Obj_Spring,
	/* 42 ObjId_Newtron             */ Obj_Newtron,
	/* 43 ObjId_Roller              */ Obj_Roller,
	/* 44 ObjId_GHZEdge             */ Obj_GHZEdge,
	/* 45 ObjId_SidewaysStomper     */ Obj_SidewaysStomper,
	/* 46 ObjId_MarbleBrick         */ Obj_MarbleBrick,
	/* 47 ObjId_Bumper              */ Obj_Bumper,
	/* 48 ObjId_BossBall            */ Obj_BossBall,
	/* 49 ObjId_Waterfall           */ Obj_Waterfall,
	/* 4A ObjId_VanishSonic         */ Obj_VanishSonic,
	/* 4B ObjId_GiantRing           */ Obj_GiantRing,
	/* 4C ObjId_GeyserMaker         */ Obj_GeyserMaker,
	/* 4D ObjId_LavaGeyser          */ Obj_LavaGeyser,
	/* 4E ObjId_LavaWall            */ Obj_LavaWall,
	/* 4F ObjId_4F                  */ Obj_Null,
	/* 50 ObjId_Yadrin              */ Obj_Yadrin,
	/* 51 ObjId_SmashBlock          */ Obj_SmashBlock,
	/* 52 ObjId_MovingBlock         */ Obj_MovingBlock,
	/* 53 ObjId_CollapseFloor       */ Obj_CollapseFloor,
	/* 54 ObjId_LavaTag             */ Obj_LavaTag,
	/* 55 ObjId_Basaran             */ Obj_Basaran,
	/* 56 ObjId_FloatingBlock       */ Obj_FloatingBlock,
	/* 57 ObjId_SpikeBall           */ Obj_SpikeBall,
	/* 58 ObjId_BigSpikeBall        */ Obj_BigSpikeBall,
	/* 59 ObjId_Elevator            */ Obj_Elevator,
	/* 5A ObjId_CirclingPlatform    */ Obj_CirclingPlatform,
	/* 5B ObjId_Staircase           */ Obj_Staircase,
	/* 5C ObjId_Pylon               */ Obj_Pylon,
	/* 5D ObjId_Fan                 */ Obj_Fan,
	/* 5E ObjId_Seesaw              */ Obj_Seesaw,
	/* 5F ObjId_Bomb                */ Obj_Bomb,
	/* 60 ObjId_Orbinaut            */ Obj_Orbinaut,
	/* 61 ObjId_LabyrinthBlock      */ Obj_LabyrinthBlock,
	/* 62 ObjId_Gargoyle            */ Obj_Gargoyle,
	/* 63 ObjId_LabyrinthConvey     */ Obj_LabyrinthConvey,
	/* 64 ObjId_Bubble              */ Obj_Bubble,
	/* 65 ObjId_LZWaterfall         */ Obj_LZWaterfall,
	/* 66 ObjId_Junction            */ Obj_RotatingJunction,
	/* 67 ObjId_RunningDisc         */ Obj_RunningDisc,
	/* 68 ObjId_SBZConveyor         */ Obj_SBZConveyor,
	/* 69 ObjId_SpinPlatform        */ Obj_SpinPlatform,
	/* 6A ObjId_Saw                 */ Obj_Saw,
	/* 6B ObjId_ScrapStomp          */ Obj_ScrapStomp,
	/* 6C ObjId_VanishPlatform      */ Obj_VanishPlatform,
	/* 6D ObjId_Flamethrower        */ Obj_Flamethrower,
	/* 6E ObjId_Electrocuter        */ Obj_Electrocuter,
	/* 6F ObjId_SpinConvey          */ Obj_SpinConveyor,
	/* 70 ObjId_GirderBlock         */ Obj_GirderBlock,
	/* 71 ObjId_InvisibleBarrier    */ Obj_InvisibleBarrier,
	/* 72 ObjId_Teleporter          */ Obj_Teleporter,
	/* 73 ObjId_BossMarble          */ Obj_BossMarble,
	/* 74 ObjId_BossFire            */ Obj_BossFire,
	/* 75 ObjId_BossSpringYard      */ Obj_BossSpringYard,
	/* 76 ObjId_BossBlock           */ Obj_BossBlock,
	/* 77 ObjId_BossLabyrinth      */ Obj_BossLabyrinth,
	/* 78 ObjId_Caterkiller         */ Obj_Caterkiller,
	/* 79 ObjId_Checkpoint          */ Obj_Checkpoint,
	/* 7A ObjId_7A                  */ Obj_Null,
	/* 7B ObjId_7B                  */ Obj_Null,
	/* 7C ObjId_RingFlash           */ Obj_RingFlash,
	/* 7D ObjId_HiddenBonus         */ Obj_HiddenBonus,
	/* 7E ObjId_7E                  */ Obj_Null,
	/* 7F ObjId_7F                  */ Obj_Null,
	/* 80 ObjId_80                  */ Obj_Null,
	/* 81 ObjId_81                  */ Obj_Null,
	/* 82 ObjId_82                  */ Obj_Null,
	/* 83 ObjId_83                  */ Obj_Null,
	/* 84 ObjId_84                  */ Obj_Null,
	/* 85 ObjId_85                  */ Obj_Null,
	/* 86 ObjId_86                  */ Obj_Null,
	/* 87 ObjId_87                  */ Obj_Null,
	/* 88 ObjId_88                  */ Obj_Null,
	/* 89 ObjId_89                  */ Obj_Null,
	/* 8A ObjId_Credits             */ Obj_Credits,
	/* 8B ObjId_8B                  */ Obj_Null,
	/* 8C ObjId_8C                  */ Obj_Null,
};

#endif // _Included_SOnic1_Object_List
