#ifndef _SOUNDIDS_H
#define _SOUNDIDS_H

// Sonic 2's sound ID space. Its music ids are the final game's $81-$9D (here 1-$1D, 0 being silence as everywhere in this port, so an id here is the driver's minus $80; the alpha's sound test order, which mostly matches the final game's) and its effects $A0-$E1 are the Simon Wai prototype's
// (laid out one after the other from sfx_Jump: the prototype has one effect for each id from $A0 on, with Helicopter at both $DE and $DF). Sonic 1's sound names are kept for the code that is shared with it
// (the zone music of Sonic 1's levels is not used: Sonic 2's Level_Music has its own table), each pointing at what plays for it in the prototype.
enum SoundID {
	mus_VsResults = 1, // $81 (2P Results)
	mus_EHZ, // $82 (Emerald Hill)
	mus_MCZ2P, // $83 (Mystic Cave 2P)
	mus_OOZ, // $84 (Oil Ocean)
	mus_MTZ, // $85 (Metropolis)
	mus_HTZ, // $86 (Hill Top)
	mus_ARZ, // $87 (Neo Green Hill)
	mus_CNZ2P, // $88 (Casino Night 2P)
	mus_CNZ, // $89 (Casino Night)
	mus_DEZ, // $8A (Death Egg)
	mus_MCZ, // $8B (Mystic Cave (Dust Hill))
	mus_EHZ2P, // $8C (Emerald Hill 2P)
	mus_SCZ, // $8D (Sky Chase)
	mus_CPZ, // $8E (Chemical Plant)
	mus_WFZ, // $8F (Wing Fortress)
	mus_HPZ, // $90 (Hidden Palace)
	mus_Menu, // $91 (Options / Level Select)
	mus_SpecStg, // $92 (Special Stage)
	mus_Boss, // $93 (Boss)
	mus_FinalBoss, // $94 (Death Egg Final Boss)
	mus_Ending, // $95 (Ending)
	mus_Super, // $96 (Super Sonic)
	mus_Invinc, // $97 (Invincibility)
	mus_ExtraLife, // $98 (Extra Life)
	mus_Title, // $99 (Title Screen)
	mus_ActClear, // $9A (Act Clear)
	mus_GameOver, // $9B (Game Over)
	mus_Continue, // $9C (Continue)
	mus_Emerald, // $9D (Got an Emerald)

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

	// Sonic 1's names, for the code shared with it: what plays for each
	bgm_GHZ = mus_VsResults, // (the first music)
	bgm_LZ = mus_EHZ,
	bgm_MZ = mus_MTZ,
	bgm_SLZ = mus_CNZ,
	bgm_SYZ = mus_MCZ,
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
	bgm_Drowning = mus_Menu,
	bgm_Emerald = mus_Emerald,
	bgm_SSRG = mus_Emerald,
	sfx_Waterfall = sfx_CNZBossZap, // (Sonic 1's waterfall is not in Sonic 2)
};

#endif //_SOUNDIDS_H
