#ifndef _SOUNDIDS_H
#define _SOUNDIDS_H

// Sonic 2's sound ID space: the Simon Wai prototype's. Its music ids are $81-$9F (here 1-$1F, 0 being silence as everywhere in this port, so an id here is the prototype's minus $80) and its effects $A0-$E1
// (laid out one after the other from sfx_Jump: the prototype has one effect for each id from $A0 on, with Helicopter at both $DE and $DF). Sonic 1's sound names are kept for the code that is shared with it
// (the zone music of Sonic 1's levels is not used: Sonic 2's Level_Music has its own table), each pointing at what plays for it in the prototype.
enum SoundID {
	mus_OOZ = 1, // the prototype's names, from its sound/music files, are not the final game's: this OOZ becomes the Casino Night 2P theme (Oil Ocean takes the SSZ track), its GHZ the Emerald Hill one...
	mus_GHZ,
	mus_MTZ,
	mus_CNZ,
	mus_DHZ,
	mus_HPZ, // (becomes the Mystic Cave 2P theme in the final game; Hidden Palace plays it in the prototype)
	mus_NGHZ,
	mus_DEZ, // (the 100 rings jingle as well)
	mus_SpecStg,
	mus_LevelSel, // ($8A: where the code that handles drowning puts its music)
	mus_LevelSelDup,
	mus_FinalBoss,
	mus_CPZ,
	mus_Boss,
	mus_RWZ, // (becomes Sky Chase Zone's theme in the final game)
	mus_SSZ, // (ids $10 and $11: Sand Shower Zone's desert theme in the prototype, zone slot 3 plays it; becomes Oil Ocean's theme in the final game)
	mus_SSZDup,
	mus_Unused1, // (id $12: becomes Wing Fortress Zone's theme in the final game)
	mus_BOZ, // (zone slot 6, "BLZ" in the prototype's list: becomes the Emerald Hill 2P theme in the final game)
	mus_Unused2, // (id $14: becomes the two-player results theme in the final game)
	mus_Invinc, // (id $15, "Invincible": becomes the Super Sonic theme in the final game)
	mus_HTZ,
	mus_HTZDup,
	mus_ExtraLife,
	mus_Title,
	mus_ActClear,
	mus_GameOver,
	mus_Continue,
	mus_Emerald,
	mus_EmeraldDup,
	mus_EmeraldDup2,

	sfx_Jump, // $A0
	sfx_Lamppost,
	sfx_Unk_A2,
	sfx_Death,
	sfx_Skid,
	sfx_Unk_A5,
	sfx_HitSpikes,
	sfx_Push,
	sfx_SSGoal,
	sfx_SSItem,
	sfx_Splash,
	sfx_Unk_AB,
	sfx_HitBoss,
	sfx_Bubble,
	sfx_Fireball,
	sfx_Shield,
	sfx_Saw,
	sfx_Electric,
	sfx_Drown,
	sfx_Flamethrower,
	sfx_Bumper,
	sfx_Ring,
	sfx_SpikesMove,
	sfx_Rumbling,
	sfx_Unk_B8,
	sfx_Collapse,
	sfx_SSGlass,
	sfx_Door,
	sfx_Teleport,
	sfx_ChainStomp,
	sfx_Roll,
	sfx_Continue,
	sfx_Basaran,
	sfx_BreakItem,
	sfx_Warning,
	sfx_GiantRing,
	sfx_Bomb,
	sfx_Cash,
	sfx_RingLoss,
	sfx_ChainRise,
	sfx_Burning,
	sfx_Bonus,
	sfx_EnterSS,
	sfx_WallSmash,
	sfx_Spring,
	sfx_Switch,
	sfx_RingLeft,
	sfx_Signpost, // $CF
	sfx_CNZBossZap, // $D0: the prototype's own
	sfx_Unk_D1,
	sfx_Unk_D2,
	sfx_Signpost2P,
	sfx_OOZLidPop,
	sfx_SlidingSpike,
	sfx_CNZElevator,
	sfx_PlatformKnock,
	sfx_BonusBumper,
	sfx_LargeBumper,
	sfx_Gloop,
	sfx_PreArrowFiring,
	sfx_Fire,
	sfx_ArrowStick, // $DD
	sfx_Helicopter, // $DE
	sfx_HelicopterDup, // $DF
	sfx_SuperTransform, // $E0
	sfx_SpindashRev, // $E1

	bgm_Fade = 0xF0,
	sfx_Sega,
	bgm_Speedup,
	bgm_Slowdown,
	bgm_Stop,

	// The last sound the sound test can reach
	SOUND_ID_LAST = sfx_SpindashRev,

	// Sonic 1's names, for the code shared with it: what plays for each in the prototype
	bgm_GHZ = mus_OOZ, // (the first music)
	bgm_LZ = mus_GHZ,
	bgm_MZ = mus_MTZ,
	bgm_SLZ = mus_CNZ,
	bgm_SYZ = mus_DHZ,
	bgm_SBZ = mus_HPZ,
	bgm_Invincible = mus_Invinc,
	bgm_ExtraLife = mus_ExtraLife,
	bgm_SS = mus_SpecStg,
	bgm_Title = mus_Title,
	bgm_Ending = 0,
	bgm_Boss = mus_Boss,
	bgm_FZ = mus_FinalBoss,
	bgm_GotThrough = mus_ActClear,
	bgm_GameOver = mus_GameOver,
	bgm_Continue = mus_Continue,
	bgm_Credits = mus_Title,
	bgm_Drowning = mus_LevelSel,
	bgm_Emerald = mus_Emerald,
	bgm_SSRG = mus_EmeraldDup2,
	sfx_Waterfall = sfx_CNZBossZap, // (Sonic 1's waterfall is not in Sonic 2)
};

#endif //_SOUNDIDS_H
