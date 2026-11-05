#include "Sound.h"

// Sonic 1's sound ID space and what plays for each id. The engine's driver (ParadoxEngine's Sound.c) knows none of it: it reads this bank.

// ---------------------------------------------------------------------
// Sound ID dispatch table -- indexed by the renumbered enum SoundID
// (Sound.h; each real resource file's own name, e.g. Mus81/SndD0, still
// matches its ORIGINAL real-hardware ID for reference even though that's no
// longer its actual enum value here). A queued 0 means "nothing pending".
// ---------------------------------------------------------------------

#include "Resource/Music/Mus81_GHZ.h"
#include "Resource/Music/Mus82_LZ.h"
#include "Resource/Music/Mus83_MZ.h"
#include "Resource/Music/Mus84_SLZ.h"
#include "Resource/Music/Mus85_SYZ.h"
#include "Resource/Music/Mus86_SBZ.h"
#include "Resource/Music/Mus87_Invincibility.h"
#include "Resource/Music/Mus88_Extra_Life.h"
#include "Resource/Music/Mus89_Special_Stage.h"
#include "Resource/Music/Mus8A_Title_Screen.h"
#include "Resource/Music/Mus8B_Ending.h"
#include "Resource/Music/Mus8C_Boss.h"
#include "Resource/Music/Mus8D_FZ.h"
#include "Resource/Music/Mus8E_Sonic_Got_Through.h"
#include "Resource/Music/Mus8F_Game_Over.h"
#include "Resource/Music/Mus90_Continue_Screen.h"
#include "Resource/Music/Mus91_Credits.h"
#include "Resource/Music/Mus92_Drowning.h"
#include "Resource/Music/Mus93_Get_Emerald.h"
#ifdef SCP_SPLASH
#include "Resource/Music/Mus94_SSRG.h"
#endif

#include "Resource/Music/SndA0_Jump.h"
#include "Resource/Music/SndA1_Lamppost.h"
#include "Resource/Music/SndA2.h"
#include "Resource/Music/SndA3_Death.h"
#include "Resource/Music/SndA4_Skid.h"
#include "Resource/Music/SndA5.h"
#include "Resource/Music/SndA6_Hit_Spikes.h"
#include "Resource/Music/SndA7_Push_Block.h"
#include "Resource/Music/SndA8_SS_Goal.h"
#include "Resource/Music/SndA9_SS_Item.h"
#include "Resource/Music/SndAA_Splash.h"
#include "Resource/Music/SndAB.h"
#include "Resource/Music/SndAC_Hit_Boss.h"
#include "Resource/Music/SndAD_Get_Bubble.h"
#include "Resource/Music/SndAE_Fireball.h"
#include "Resource/Music/SndAF_Shield.h"
#include "Resource/Music/SndB0_Saw.h"
#include "Resource/Music/SndB1_Electric.h"
#include "Resource/Music/SndB2_Drown_Death.h"
#include "Resource/Music/SndB3_Flamethrower.h"
#include "Resource/Music/SndB4_Bumper.h"
#include "Resource/Music/SndB5_Ring.h"
#include "Resource/Music/SndB6_Spikes_Move.h"
#include "Resource/Music/SndB7_Rumbling.h"
#include "Resource/Music/SndB8.h"
#include "Resource/Music/SndB9_Collapse.h"
#include "Resource/Music/SndBA_SS_Glass.h"
#include "Resource/Music/SndBB_Door.h"
#include "Resource/Music/SndBC_Teleport.h"
#include "Resource/Music/SndBD_ChainStomp.h"
#include "Resource/Music/SndBE_Roll.h"
#include "Resource/Music/SndBF_Get_Continue.h"
#include "Resource/Music/SndC0_Basaran_Flap.h"
#include "Resource/Music/SndC1_Break_Item.h"
#include "Resource/Music/SndC2_Drown_Warning.h"
#include "Resource/Music/SndC3_Giant_Ring.h"
#include "Resource/Music/SndC4_Bomb.h"
#include "Resource/Music/SndC5_Cash_Register.h"
#include "Resource/Music/SndC6_Ring_Loss.h"
#include "Resource/Music/SndC7_Chain_Rising.h"
#include "Resource/Music/SndC8_Burning.h"
#include "Resource/Music/SndC9_Hidden_Bonus.h"
#include "Resource/Music/SndCA_Enter_SS.h"
#include "Resource/Music/SndCB_Wall_Smash.h"
#include "Resource/Music/SndCC_Spring.h"
#include "Resource/Music/SndCD_Switch.h"
#include "Resource/Music/SndCE_Ring_Left_Speaker.h"
#include "Resource/Music/SndCF_Signpost.h"
#include "Resource/Music/SndE0_Spindash_Rev.h"
#include "Resource/Music/SndD0_Waterfall.h"

// Raw-JSON-text companion headers (see sound_table_json below) -- same
// content, source form instead of compiled bytes, for the JSON engine.
#include "Resource/Music/Mus81_GHZ_json.h"
#include "Resource/Music/Mus82_LZ_json.h"
#include "Resource/Music/Mus83_MZ_json.h"
#include "Resource/Music/Mus84_SLZ_json.h"
#include "Resource/Music/Mus85_SYZ_json.h"
#include "Resource/Music/Mus86_SBZ_json.h"
#include "Resource/Music/Mus87_Invincibility_json.h"
#include "Resource/Music/Mus88_Extra_Life_json.h"
#include "Resource/Music/Mus89_Special_Stage_json.h"
#include "Resource/Music/Mus8A_Title_Screen_json.h"
#include "Resource/Music/Mus8B_Ending_json.h"
#include "Resource/Music/Mus8C_Boss_json.h"
#include "Resource/Music/Mus8D_FZ_json.h"
#include "Resource/Music/Mus8E_Sonic_Got_Through_json.h"
#include "Resource/Music/Mus8F_Game_Over_json.h"
#include "Resource/Music/Mus90_Continue_Screen_json.h"
#include "Resource/Music/Mus91_Credits_json.h"
#include "Resource/Music/Mus92_Drowning_json.h"
#include "Resource/Music/Mus93_Get_Emerald_json.h"
#ifdef SCP_SPLASH
#include "Resource/Music/Mus94_SSRG_json.h"
#endif

#include "Resource/Music/SndA0_Jump_json.h"
#include "Resource/Music/SndA1_Lamppost_json.h"
#include "Resource/Music/SndA2_json.h"
#include "Resource/Music/SndA3_Death_json.h"
#include "Resource/Music/SndA4_Skid_json.h"
#include "Resource/Music/SndA5_json.h"
#include "Resource/Music/SndA6_Hit_Spikes_json.h"
#include "Resource/Music/SndA7_Push_Block_json.h"
#include "Resource/Music/SndA8_SS_Goal_json.h"
#include "Resource/Music/SndA9_SS_Item_json.h"
#include "Resource/Music/SndAA_Splash_json.h"
#include "Resource/Music/SndAB_json.h"
#include "Resource/Music/SndAC_Hit_Boss_json.h"
#include "Resource/Music/SndAD_Get_Bubble_json.h"
#include "Resource/Music/SndAE_Fireball_json.h"
#include "Resource/Music/SndAF_Shield_json.h"
#include "Resource/Music/SndB0_Saw_json.h"
#include "Resource/Music/SndB1_Electric_json.h"
#include "Resource/Music/SndB2_Drown_Death_json.h"
#include "Resource/Music/SndB3_Flamethrower_json.h"
#include "Resource/Music/SndB4_Bumper_json.h"
#include "Resource/Music/SndB5_Ring_json.h"
#include "Resource/Music/SndB6_Spikes_Move_json.h"
#include "Resource/Music/SndB7_Rumbling_json.h"
#include "Resource/Music/SndB8_json.h"
#include "Resource/Music/SndB9_Collapse_json.h"
#include "Resource/Music/SndBA_SS_Glass_json.h"
#include "Resource/Music/SndBB_Door_json.h"
#include "Resource/Music/SndBC_Teleport_json.h"
#include "Resource/Music/SndBD_ChainStomp_json.h"
#include "Resource/Music/SndBE_Roll_json.h"
#include "Resource/Music/SndBF_Get_Continue_json.h"
#include "Resource/Music/SndC0_Basaran_Flap_json.h"
#include "Resource/Music/SndC1_Break_Item_json.h"
#include "Resource/Music/SndC2_Drown_Warning_json.h"
#include "Resource/Music/SndC3_Giant_Ring_json.h"
#include "Resource/Music/SndC4_Bomb_json.h"
#include "Resource/Music/SndC5_Cash_Register_json.h"
#include "Resource/Music/SndC6_Ring_Loss_json.h"
#include "Resource/Music/SndC7_Chain_Rising_json.h"
#include "Resource/Music/SndC8_Burning_json.h"
#include "Resource/Music/SndC9_Hidden_Bonus_json.h"
#include "Resource/Music/SndCA_Enter_SS_json.h"
#include "Resource/Music/SndCB_Wall_Smash_json.h"
#include "Resource/Music/SndCC_Spring_json.h"
#include "Resource/Music/SndCD_Switch_json.h"
#include "Resource/Music/SndCE_Ring_Left_Speaker_json.h"
#include "Resource/Music/SndCF_Signpost_json.h"
#include "Resource/Music/SndE0_Spindash_Rev_json.h"
#include "Resource/Music/SndD0_Waterfall_json.h"

// Music IDs (bgm_GHZ..bgm_SSRG) use the smpsHeaderVoice/Chan/Tempo/DAC/FM/PSG
// layout (see LoadMusic); SFX IDs (sfx_Jump..sfx_Waterfall) use the shorter
// Voice/TempoSFX/ChanSFX/SFXChannel layout (see LoadSFX).
// Indexed by the enum SoundID (Sound.h) rather than a hand-typed hex literal
// per entry -- means this table (and sound_table_driver_ver below) never
// need to change again if the ID space itself is ever renumbered; only
// Sound.h's enum would.
static const uint8_t *const sound_table[0x100] = {
    [bgm_GHZ] = Mus81_GHZ,
    [bgm_LZ] = Mus82_LZ,
    [bgm_MZ] = Mus83_MZ,
    [bgm_SLZ] = Mus84_SLZ,
    [bgm_SYZ] = Mus85_SYZ,
    [bgm_SBZ] = Mus86_SBZ,
    [bgm_Invincible] = Mus87_Invincibility,
    [bgm_ExtraLife] = Mus88_Extra_Life,
    [bgm_SS] = Mus89_Special_Stage,
    [bgm_Title] = Mus8A_Title_Screen,
    [bgm_Ending] = Mus8B_Ending,
    [bgm_Boss] = Mus8C_Boss,
    [bgm_FZ] = Mus8D_FZ,
    [bgm_GotThrough] = Mus8E_Sonic_Got_Through,
    [bgm_GameOver] = Mus8F_Game_Over,
    [bgm_Continue] = Mus90_Continue_Screen,
    [bgm_Credits] = Mus91_Credits,
    [bgm_Drowning] = Mus92_Drowning,
    [bgm_Emerald] = Mus93_Get_Emerald,
#ifdef SCP_SPLASH
    [bgm_SSRG] = Mus94_SSRG,
#endif

    [sfx_Jump] = SndA0_Jump,
    [sfx_Lamppost] = SndA1_Lamppost,
    [sfx_Unk_A2] = SndA2,
    [sfx_Death] = SndA3_Death,
    [sfx_Skid] = SndA4_Skid,
    [sfx_Unk_A5] = SndA5,
    [sfx_HitSpikes] = SndA6_Hit_Spikes,
    [sfx_Push] = SndA7_Push_Block,
    [sfx_SSGoal] = SndA8_SS_Goal,
    [sfx_SSItem] = SndA9_SS_Item,
    [sfx_Splash] = SndAA_Splash,
    [sfx_Unk_AB] = SndAB,
    [sfx_HitBoss] = SndAC_Hit_Boss,
    [sfx_Bubble] = SndAD_Get_Bubble,
    [sfx_Fireball] = SndAE_Fireball,
    [sfx_Shield] = SndAF_Shield,
    [sfx_Saw] = SndB0_Saw,
    [sfx_Electric] = SndB1_Electric,
    [sfx_Drown] = SndB2_Drown_Death,
    [sfx_Flamethrower] = SndB3_Flamethrower,
    [sfx_Bumper] = SndB4_Bumper,
    [sfx_Ring] = SndB5_Ring,
    [sfx_SpikesMove] = SndB6_Spikes_Move,
    [sfx_Rumbling] = SndB7_Rumbling,
    [sfx_Unk_B8] = SndB8,
    [sfx_Collapse] = SndB9_Collapse,
    [sfx_SSGlass] = SndBA_SS_Glass,
    [sfx_Door] = SndBB_Door,
    [sfx_Teleport] = SndBC_Teleport,
    [sfx_ChainStomp] = SndBD_ChainStomp,
    [sfx_Roll] = SndBE_Roll,
    [sfx_Continue] = SndBF_Get_Continue,
    [sfx_Basaran] = SndC0_Basaran_Flap,
    [sfx_BreakItem] = SndC1_Break_Item,
    [sfx_Warning] = SndC2_Drown_Warning,
    [sfx_GiantRing] = SndC3_Giant_Ring,
    [sfx_Bomb] = SndC4_Bomb,
    [sfx_Cash] = SndC5_Cash_Register,
    [sfx_RingLoss] = SndC6_Ring_Loss,
    [sfx_ChainRise] = SndC7_Chain_Rising,
    [sfx_Burning] = SndC8_Burning,
    [sfx_Bonus] = SndC9_Hidden_Bonus,
    [sfx_EnterSS] = SndCA_Enter_SS,
    [sfx_WallSmash] = SndCB_Wall_Smash,
    [sfx_Spring] = SndCC_Spring,
    [sfx_Switch] = SndCD_Switch,
    [sfx_RingLeft] = SndCE_Ring_Left_Speaker,
    [sfx_Signpost] = SndCF_Signpost,
    [sfx_SpindashRev] = SndE0_Spindash_Rev,
    [sfx_Waterfall] = SndD0_Waterfall,

    // Continuous SFX ($SOUND_ID_CONTINUOUS_SFX_FIRST-LAST, empty range) --
    // deliberately no entries here yet (see Sound.h's enum comment); every
    // ID in that range naturally reads back NULL from this sparse array,
    // and DispatchQueue already treats a NULL song pointer as a clean skip.
};

// Parallel to sound_table, same ID space -- each entry's own JSON SOURCE
// TEXT (not compiled bytes) for the JSON tree-walking engine
// (Sound_PlayFromJSON below). Not wired into DispatchQueue/the real
// music-and-SFX queue yet (that's still exclusively the byte-VM, sound_table
// above) -- this is a second, independent entry point, currently only used
// by the level-select sound test, which is a deliberately low-risk first
// real (non-debug-tool) place to exercise the JSON engine in-game.
static const char *const sound_table_json[0x100] = {
    [bgm_GHZ] = Mus81_GHZ_json,
    [bgm_LZ] = Mus82_LZ_json,
    [bgm_MZ] = Mus83_MZ_json,
    [bgm_SLZ] = Mus84_SLZ_json,
    [bgm_SYZ] = Mus85_SYZ_json,
    [bgm_SBZ] = Mus86_SBZ_json,
    [bgm_Invincible] = Mus87_Invincibility_json,
    [bgm_ExtraLife] = Mus88_Extra_Life_json,
    [bgm_SS] = Mus89_Special_Stage_json,
    [bgm_Title] = Mus8A_Title_Screen_json,
    [bgm_Ending] = Mus8B_Ending_json,
    [bgm_Boss] = Mus8C_Boss_json,
    [bgm_FZ] = Mus8D_FZ_json,
    [bgm_GotThrough] = Mus8E_Sonic_Got_Through_json,
    [bgm_GameOver] = Mus8F_Game_Over_json,
    [bgm_Continue] = Mus90_Continue_Screen_json,
    [bgm_Credits] = Mus91_Credits_json,
    [bgm_Drowning] = Mus92_Drowning_json,
    [bgm_Emerald] = Mus93_Get_Emerald_json,
#ifdef SCP_SPLASH
    [bgm_SSRG] = Mus94_SSRG_json,
#endif

    [sfx_Jump] = SndA0_Jump_json,
    [sfx_Lamppost] = SndA1_Lamppost_json,
    [sfx_Unk_A2] = SndA2_json,
    [sfx_Death] = SndA3_Death_json,
    [sfx_Skid] = SndA4_Skid_json,
    [sfx_Unk_A5] = SndA5_json,
    [sfx_HitSpikes] = SndA6_Hit_Spikes_json,
    [sfx_Push] = SndA7_Push_Block_json,
    [sfx_SSGoal] = SndA8_SS_Goal_json,
    [sfx_SSItem] = SndA9_SS_Item_json,
    [sfx_Splash] = SndAA_Splash_json,
    [sfx_Unk_AB] = SndAB_json,
    [sfx_HitBoss] = SndAC_Hit_Boss_json,
    [sfx_Bubble] = SndAD_Get_Bubble_json,
    [sfx_Fireball] = SndAE_Fireball_json,
    [sfx_Shield] = SndAF_Shield_json,
    [sfx_Saw] = SndB0_Saw_json,
    [sfx_Electric] = SndB1_Electric_json,
    [sfx_Drown] = SndB2_Drown_Death_json,
    [sfx_Flamethrower] = SndB3_Flamethrower_json,
    [sfx_Bumper] = SndB4_Bumper_json,
    [sfx_Ring] = SndB5_Ring_json,
    [sfx_SpikesMove] = SndB6_Spikes_Move_json,
    [sfx_Rumbling] = SndB7_Rumbling_json,
    [sfx_Unk_B8] = SndB8_json,
    [sfx_Collapse] = SndB9_Collapse_json,
    [sfx_SSGlass] = SndBA_SS_Glass_json,
    [sfx_Door] = SndBB_Door_json,
    [sfx_Teleport] = SndBC_Teleport_json,
    [sfx_ChainStomp] = SndBD_ChainStomp_json,
    [sfx_Roll] = SndBE_Roll_json,
    [sfx_Continue] = SndBF_Get_Continue_json,
    [sfx_Basaran] = SndC0_Basaran_Flap_json,
    [sfx_BreakItem] = SndC1_Break_Item_json,
    [sfx_Warning] = SndC2_Drown_Warning_json,
    [sfx_GiantRing] = SndC3_Giant_Ring_json,
    [sfx_Bomb] = SndC4_Bomb_json,
    [sfx_Cash] = SndC5_Cash_Register_json,
    [sfx_RingLoss] = SndC6_Ring_Loss_json,
    [sfx_ChainRise] = SndC7_Chain_Rising_json,
    [sfx_Burning] = SndC8_Burning_json,
    [sfx_Bonus] = SndC9_Hidden_Bonus_json,
    [sfx_EnterSS] = SndCA_Enter_SS_json,
    [sfx_WallSmash] = SndCB_Wall_Smash_json,
    [sfx_Spring] = SndCC_Spring_json,
    [sfx_Switch] = SndCD_Switch_json,
    [sfx_RingLeft] = SndCE_Ring_Left_Speaker_json,
    [sfx_Signpost] = SndCF_Signpost_json,
    [sfx_SpindashRev] = SndE0_Spindash_Rev_json,
    [sfx_Waterfall] = SndD0_Waterfall_json,
};

// Parallel to sound_table -- which coordination-flag table (see
// SoundChipSet::driver_version) each ID's compiled data was built against.
// Defaults every entry to 0 (driver-version-1, Sonic 1/2-compatible)
// implicitly via C's own zero-initialization, so every song wired into
// sound_table above needs no change at all to stay on the exact behavior it
// has today -- only an ID actually compiled with "driverVersion": 3 in its
// source .jsonc (via json_to_header.py/compiler.c's generated
// `<ArrayName>_DRIVERVER` constant) needs an explicit entry here.
static const uint8_t sound_table_driver_ver[0x100] = {
    // No entries yet -- no music/SFX ID currently in sound_table above is
    // compiled with driverVersion 3. Add `[bgm_GHZ] = Mus81_GHZ_DRIVERVER,`
    // (referencing the compiler-generated constant, not a hand-typed 3)
    // alongside sound_table's own line for any future song that opts in.
    [0] = 0,
};



// Matches SoundPriorities: higher wins. Indexed by id - SOUND_ID_SFX_FIRST,
// covering regular SFX through the (currently empty) continuous-SFX range
// to special SFX -- purely a numeric priority-arbitration table, one byte
// per sound ID, mechanically transcribed. Content unchanged by the ID
// renumbering (same 49 values, same relative order -- only the base ID
// each position corresponds to shifted).
static const uint8_t sound_priorities[sfx_Waterfall - sfx_Jump + 1] = {
    0x80, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x68, 0x70, 0x70, 0x70, 0x60, 0x70, // sfx_Jump+
    0x70, 0x60, 0x70, 0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x7F, // (cont.)
    0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, // (cont.)
    0x70,                                                                                           // sfx_SpindashRev
    0x80,                                                                                           // sfx_Waterfall
};


// Real per-song replacement main_tempo values used while speed shoes are active (the original's SpeedUpIndex), indexed by (music_id - bgm_GHZ).
// Only the first 8 songs (GHZ..ExtraLife) have real entries in the original driver; for any song past that the driver leaves the tempo alone.
static const uint8_t speedup_index[8] = {
    0x07, // GHZ
    0x72, // LZ
    0x73, // MZ
    0x26, // SLZ
    0x15, // SYZ
    0x08, // SBZ
    0xFF, // Invincibility
    0x05, // Extra Life
};

// The SMPS Inspector's list of sounds (Tools menu): music first, then effects.
#define INSPECT_ENTRY(id, is_music, name) {(uint8_t)(id), (is_music), (name)}
static const SoundInspectEntry inspect_entries[] = {
    INSPECT_ENTRY(bgm_GHZ, 1, "Green Hill Zone"),
    INSPECT_ENTRY(bgm_LZ, 1, "Labyrinth Zone"),
    INSPECT_ENTRY(bgm_MZ, 1, "Marble Zone"),
    INSPECT_ENTRY(bgm_SLZ, 1, "Star Light Zone"),
    INSPECT_ENTRY(bgm_SYZ, 1, "Spring Yard Zone"),
    INSPECT_ENTRY(bgm_SBZ, 1, "Scrap Brain Zone"),
    INSPECT_ENTRY(bgm_Invincible, 1, "Invincibility"),
    INSPECT_ENTRY(bgm_ExtraLife, 1, "Extra Life"),
    INSPECT_ENTRY(bgm_SS, 1, "Special Stage"),
    INSPECT_ENTRY(bgm_Title, 1, "Title Screen"),
    INSPECT_ENTRY(bgm_Ending, 1, "Ending"),
    INSPECT_ENTRY(bgm_Boss, 1, "Boss"),
    INSPECT_ENTRY(bgm_FZ, 1, "Final Zone"),
    INSPECT_ENTRY(bgm_GotThrough, 1, "Sonic Got Through"),
    INSPECT_ENTRY(bgm_GameOver, 1, "Game Over"),
    INSPECT_ENTRY(bgm_Continue, 1, "Continue Screen"),
    INSPECT_ENTRY(bgm_Credits, 1, "Credits"),
    INSPECT_ENTRY(bgm_Drowning, 1, "Drowning"),
    INSPECT_ENTRY(bgm_Emerald, 1, "Get Emerald"),
    INSPECT_ENTRY(sfx_Jump, 0, "Jump"),
    INSPECT_ENTRY(sfx_Lamppost, 0, "Lamppost"),
    INSPECT_ENTRY(sfx_Unk_A2, 0, "Unused A2"),
    INSPECT_ENTRY(sfx_Death, 0, "Death"),
    INSPECT_ENTRY(sfx_Skid, 0, "Skid"),
    INSPECT_ENTRY(sfx_Unk_A5, 0, "Unused A5"),
    INSPECT_ENTRY(sfx_HitSpikes, 0, "Hit Spikes"),
    INSPECT_ENTRY(sfx_Push, 0, "Push Block"),
    INSPECT_ENTRY(sfx_SSGoal, 0, "SS Goal"),
    INSPECT_ENTRY(sfx_SSItem, 0, "SS Item"),
    INSPECT_ENTRY(sfx_Splash, 0, "Splash"),
    INSPECT_ENTRY(sfx_Unk_AB, 0, "Unused AB"),
    INSPECT_ENTRY(sfx_HitBoss, 0, "Hit Boss"),
    INSPECT_ENTRY(sfx_Bubble, 0, "Get Bubble"),
    INSPECT_ENTRY(sfx_Fireball, 0, "Fireball"),
    INSPECT_ENTRY(sfx_Shield, 0, "Shield"),
    INSPECT_ENTRY(sfx_Saw, 0, "Saw"),
    INSPECT_ENTRY(sfx_Electric, 0, "Electric"),
    INSPECT_ENTRY(sfx_Drown, 0, "Drown Death"),
    INSPECT_ENTRY(sfx_Flamethrower, 0, "Flamethrower"),
    INSPECT_ENTRY(sfx_Bumper, 0, "Bumper"),
    INSPECT_ENTRY(sfx_Ring, 0, "Ring"),
    INSPECT_ENTRY(sfx_SpikesMove, 0, "Spikes Move"),
    INSPECT_ENTRY(sfx_Rumbling, 0, "Rumbling"),
    INSPECT_ENTRY(sfx_Unk_B8, 0, "Unused B8"),
    INSPECT_ENTRY(sfx_Collapse, 0, "Collapse"),
    INSPECT_ENTRY(sfx_SSGlass, 0, "SS Glass"),
    INSPECT_ENTRY(sfx_Door, 0, "Door"),
    INSPECT_ENTRY(sfx_Teleport, 0, "Teleport / Spin Dash Release"),
    INSPECT_ENTRY(sfx_ChainStomp, 0, "Chain Stomp"),
    INSPECT_ENTRY(sfx_Roll, 0, "Roll"),
    INSPECT_ENTRY(sfx_Continue, 0, "Get Continue"),
    INSPECT_ENTRY(sfx_Basaran, 0, "Basaran Flap"),
    INSPECT_ENTRY(sfx_BreakItem, 0, "Break Item"),
    INSPECT_ENTRY(sfx_Warning, 0, "Drown Warning"),
    INSPECT_ENTRY(sfx_GiantRing, 0, "Giant Ring"),
    INSPECT_ENTRY(sfx_Bomb, 0, "Bomb"),
    INSPECT_ENTRY(sfx_Cash, 0, "Cash Register"),
    INSPECT_ENTRY(sfx_RingLoss, 0, "Ring Loss"),
    INSPECT_ENTRY(sfx_ChainRise, 0, "Chain Rising"),
    INSPECT_ENTRY(sfx_Burning, 0, "Burning"),
    INSPECT_ENTRY(sfx_Bonus, 0, "Hidden Bonus"),
    INSPECT_ENTRY(sfx_EnterSS, 0, "Enter Special Stage"),
    INSPECT_ENTRY(sfx_WallSmash, 0, "Wall Smash"),
    INSPECT_ENTRY(sfx_Spring, 0, "Spring"),
    INSPECT_ENTRY(sfx_Switch, 0, "Switch"),
    INSPECT_ENTRY(sfx_RingLeft, 0, "Ring (left speaker)"),
    INSPECT_ENTRY(sfx_Signpost, 0, "Signpost"),
    INSPECT_ENTRY(sfx_SpindashRev, 0, "Spin Dash Rev"),
    INSPECT_ENTRY(sfx_Waterfall, 0, "Waterfall"),
};


const SoundBank game_sound_bank = {
    .songs = sound_table,
    .songs_json = sound_table_json,
    .driver_version = sound_table_driver_ver,
    .music_first = bgm_GHZ,
    .music_last = bgm_SSRG,
    .sfx_first = sfx_Jump,
    .special_last = sfx_Waterfall,
    .command_first = bgm_Fade,
    .command_last = bgm_Stop,
    .sfx_priorities = sound_priorities,
    .command_fade = bgm_Fade,
    .command_sega = sfx_Sega,
    .command_speedup = bgm_Speedup,
    .command_slowdown = bgm_Slowdown,
    .command_stop = bgm_Stop,
    .one_up_music = bgm_ExtraLife,
    .spindash_rev_sfx = sfx_SpindashRev,
    .ring_sfx = sfx_Ring,
    .ring_left_sfx = sfx_RingLeft,
    .push_sfx = sfx_Push,
    .speedup_tempo = speedup_index,
    .speedup_tempo_count = sizeof(speedup_index) / sizeof(speedup_index[0]),
    .inspect_entries = inspect_entries,
    .inspect_entry_count = (int)(sizeof(inspect_entries) / sizeof(inspect_entries[0])),
};
