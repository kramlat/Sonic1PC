#include "Sound.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// JSON-tree-walking playback engine (see SoundChannel/SoundChipSet's own
// json_* field comments, and the "SMPS runtime: walk JSON directly" plan) --
// staged as new functions within this same file (not a separate .c) so it
// can reuse the existing static FM/PSG register helpers below directly,
// rather than duplicating or exposing them across a translation-unit
// boundary purely for this.
#include "compiler.h" // libparadoxsmps -- ps_note_value() (json.h itself already pulled in via Sound.h)

// Numeric fields in the JSON schema are stored as hex/decimal strings
// ("0x6") or, less often, plain JSON numbers -- strtol's base-0
// autodetection handles both "0x.." and plain decimal (with an optional
// leading '-') itself, so this doesn't need to reimplement compiler.c's own
// unhex_int byte-for-byte, just its two accepted shapes. Declared up here
// (not down by the rest of the JSON engine) since FM_LoadVoiceJSON needs it
// earlier in the file.
static int SFXChannelIndex(uint8_t chanid); // forward decl -- LoadSFXJSON (below) needs this before its real definition later in the file

static int JsonUnhexInt(const PJValue *v, int fallback) {
    if (pj_type(v) == PJ_NUMBER)
        return (int)pj_get_number(v, (double)fallback);
    const char *s = pj_get_string(v, NULL);
    if (!s || !*s)
        return fallback;
    return (int)strtol(s, NULL, 0);
}

// Diagnostic trace (see Sound_SetTrace): logs every note/duration event
// TickChannel decodes, so real song data can be verified against its own
// .asm source directly instead of guessing from audio alone.
static int sound_trace_enabled = 0;
void Sound_SetTrace(int enabled) { sound_trace_enabled = enabled; }
// Running Sound_Frame() count, printed as every trace line's prefix -- lets
// two channels' event timing be diffed by exact frame number instead of by
// eye, e.g. to check whether two channels sharing a voice for a rhythmically
// -locked part actually line up or have drifted apart.
static uint32_t sound_trace_frame = 0;
#define SOUND_TRACE(...)                          \
    do {                                            \
        if (sound_trace_enabled) {                  \
            fprintf(stderr, "[%u] ", sound_trace_frame); \
            fprintf(stderr, __VA_ARGS__);            \
        }                                            \
    } while (0)

// SFX channel IDs and DAC sample IDs -- matches smps2asmc/smps.h's cPSG1..
// cFM6/dKick.. constants (not #included directly here to avoid pulling in
// smps.h's SMPS_Byte()-emitting macro functions, which only make sense in
// the offline translator, not this runtime).
#define SND_cPSG1  0x80
#define SND_cPSG2  0xA0
#define SND_cPSG3  0xC0
#define SND_cNoise 0xE0
#define SND_cFM3   0x02
#define SND_cFM4   0x04
#define SND_cFM5   0x05
// No SND_cFM6 -- $06 is a Sonic 3/S&K/S3D-only channel ID ("overrides DAC");
// _smps2asm_inc.asm's smpsHeaderSFXChannel macro makes it a hard compile-time
// fatal for Sonic 1/2 driver versions ("unsupported... change it to another
// channel"), and no SFX file in this project uses it. Sonic 1's SFX RAM
// layout only ever allocates FM3-FM5 (3 channels) for regular SFX and a
// single fixed FM4 slot for the one special SFX ($D0, Waterfall).
#define SND_dKick  0x81

SoundChipSet sound_music;
SoundChipSet sound_sfx;

// YM2612 input clock: NTSC master clock / 7 (the Genesis feeds the FM chip
// a divided-down version of the same clock the PSG and 68000 share).
#define SOUND_FM_CLOCK (SOUND_PSG_CLOCK * 15 / 7) // 3579545 * 15/7 = 7670454 Hz

void Sound_Init(void) {
    SN76489_Init(&sound_music.psg);
    SN76489_Init(&sound_sfx.psg);
    sound_music.fm = YM2612_Create();
    sound_sfx.fm = YM2612_Create();
    // Music runs the clean/mathematically-correct FM core (an "enhanced
    // soundtrack" -- the real chip's DAC ladder-effect distortion is a
    // property of the real hardware's imperfections, not the composers'
    // intent), while SFX keep the authentic per-hardware character voice
    // banks were originally tuned around. See YM2612_SetLadderEffect.
    YM2612_SetLadderEffect(sound_music.fm, 0);
    YM2612_SetLadderEffect(sound_sfx.fm, 1);
    // Default DAC pan (full L+R) -- LoadMusic() also sets this when a song
    // with its own DAC track loads, but PlaySegaSound_Trigger() (and any
    // other direct-to-DAC playback) runs independently of that, so without
    // this, sound_sfx.dac_pan would stay at its zero-initialized default
    // forever -- both L and R gates closed, silent regardless of whether
    // playback is actually triggered.
    sound_music.dac_pan = 0xC0;
    sound_sfx.dac_pan = 0xC0;
    // -1, not the zero-initialized default -- 0 is a real Hi-Timpani variant
    // index, and (for dac_pitch_override_sample) DAC_SAMPLE_KICK.
    sound_music.dac_timpani_variant = -1;
    sound_sfx.dac_timpani_variant = -1;
    sound_music.dac_pitch_override_sample = -1;
    sound_sfx.dac_pitch_override_sample = -1;
}

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

#define SOUND_ID_MUSIC_FIRST bgm_GHZ
#define SOUND_ID_MUSIC_LAST  bgm_SSRG

// ---------------------------------------------------------------------
// DAC/DPCM percussion samples
// ---------------------------------------------------------------------

#include "Resource/DAC/bongo.h"
#include "Resource/DAC/clap.h"
#include "Resource/DAC/kick.h"
#include "Resource/DAC/scratch.h"
#include "Resource/DAC/snare.h"
#include "Resource/DAC/timpani.h"
#include "Resource/DAC/tom.h"
#include "Resource/DAC/s3snare.h"
#include "Resource/DAC/s3tom.h"
#include "Resource/DAC/s3kick.h"
#include "Resource/DAC/muffled_snare.h"
#include "Resource/DAC/crash.h"
#include "Resource/DAC/ride.h"
#include "Resource/DAC/metal_hit_1.h"
#include "Resource/DAC/metal_hit_2.h"
#include "Resource/DAC/metal_hit_3.h"
#include "Resource/DAC/s3clap.h"
#include "Resource/DAC/etom.h"
#include "Resource/DAC/pitched_snare.h"
#include "Resource/DAC/s3timpani.h"
#include "Resource/DAC/quick_loose_snare.h"
#include "Resource/DAC/click.h"
#include "Resource/DAC/power_kick.h"
#include "Resource/DAC/quick_glass_crash.h"
#include "Resource/DAC/glass_crash_snare.h"
#include "Resource/DAC/glass_crash.h"
#include "Resource/DAC/glass_crash_kick.h"
#include "Resource/DAC/quiet_glass_crash.h"
#include "Resource/DAC/snare_kick.h"
#include "Resource/DAC/xbass_kick.h"
#include "Resource/DAC/dance_snare.h"
#include "Resource/DAC/loose_kick.h"
#include "Resource/DAC/mod_loose_kick.h"
#include "Resource/DAC/wood_block.h"
#include "Resource/DAC/wood_block_2.h"
#include "Resource/DAC/conga.h"
#include "Resource/DAC/hit_drum_1.h"
#include "Resource/DAC/hit_drum_2.h"
#include "Resource/DAC/hit_drum_3.h"
#include "Resource/DAC/metal_crash_hit.h"
#include "Resource/DAC/low_echo_clap.h"
#include "Resource/DAC/hiphop_kick.h"
#include "Resource/DAC/dance_kick.h"
#include "Resource/DAC/hiphop_kick_2.h"
#include "Resource/DAC/hiphop_kick_3.h"
#include "Resource/DAC/deep_hit.h"
#include "Resource/DAC/wood_block_3.h"
#include "Resource/DAC/unused.h"
#include "Resource/DAC/reverse.h"
#include "Resource/DAC/psytrance_kick.h"
#include "Resource/DAC/psytrance_snare.h"
#include "Resource/DAC/cowbell.h"
#include "Resource/DAC/rimshot.h"
#include "Resource/DAC/cuica.h"
#include "Resource/DAC/guiro.h"
#include "Resource/PCM/sega.h"

// Field order mirrors Sonic 3's own DAC_Setup macro/struct: rate, then the
// voice/sample data itself, then its size. We store Hz directly rather than
// a raw hardware timer loop count, since this project's DAC_Generate
// resamples in software and has no timer-loop to model -- same information
// (S3's dpcmLoopCounter is itself just a deterministic function of the
// desired Hz), no lossy round-trip.
typedef struct {
    uint32_t rate;      // Hz -- real per-sample rate from the driver's zPCM_Table, NOT shared
    const uint8_t *data;
    uint32_t length; // bytes (2 nibbles each)
} DACSample;

#define DAC_Setup(rate, dacptr) {(rate), (dacptr), sizeof(dacptr)}

static const DACSample dac_samples[DAC_SAMPLE_COUNT] = {
    [DAC_SAMPLE_KICK] = DAC_Setup(8250, DAC_kick),
    [DAC_SAMPLE_SNARE] = DAC_Setup(24000, DAC_snare),
    // This is the DEFAULT/untriggered rate for plain Timpani ($83) only --
    // was 7250 Hz, confirmed by ear via ParadoxComposer's DAC preview to be
    // roughly an octave too low, doubled to 14500 Hz. Unlike Kick/Snare
    // above, no real disassembly source has confirmed this specific value
    // (the $88-$8B pitch-shifted variants below DO now have a real source
    // -- see dac_notes -- but that table only covers the post-override
    // case; $83's own default before any $88-$8B has ever fired is still
    // this doubled-guess value, not independently verified).
    // Cross-referencing Sonic 2's own base*scale formula against this
    // project's real Timpani variant table implies a base closer to
    // ~7300-7500 Hz -- NOT changed, since Sonic 2's Kick/Snare/Timpani
    // sound the same as Sonic 1's but are confirmed to be different DPCM
    // recordings, so that formula doesn't transfer to this project's own
    // sample data.
    [DAC_SAMPLE_TIMPANI] = DAC_Setup(14500, DAC_timpani),
    // Sonic 2-exclusive base samples (per your direction), folded into this
    // project's own extended DAC scheme -- see the DAC_SAMPLE_* enum
    // comment in Sound.h and the $81-$92 dispatch comment below. Base
    // rates all directly confirmed (not guessed/derived), same standard as
    // Kick/Snare above.
    [DAC_SAMPLE_SCRATCH] = DAC_Setup(15000, DAC_scratch),
    [DAC_SAMPLE_CLAP] = DAC_Setup(17000, DAC_clap),
    [DAC_SAMPLE_TOM] = DAC_Setup(13500, DAC_tom),
    [DAC_SAMPLE_BONGO] = DAC_Setup(7375, DAC_bongo),
    [DAC_SAMPLE_S3SNARE] = DAC_Setup(19000, DAC_s3snare),
};

// One DAC_Setup entry per possible DAC note byte, SND_dKick($81) through the
// highest reachable note byte (note bytes run 0-$DF; $E0+ is coordination-
// flag territory, see the `if (b < 0xE0)` branch below) -- covers the full
// range a DAC track's data could ever address, not just the ones currently
// wired up, so a new pitched slot (even one re-using an existing sample's
// data at a different rate -- see DAC_Setup's own comment) is a single new
// line here, nothing else to touch. Gaps (no DAC_Setup entry for that
// index) are implicitly {0,0,NULL} -- DAC_TriggerNote below no-ops for
// those instead of misreading garbage, same spirit as a real driver's
// unused table slot just never getting referenced by any real song data.
#define DAC_NOTE_COUNT (0xDF - SND_dKick + 1)
static const DACSample dac_notes[DAC_NOTE_COUNT] = {
    [0x81 - SND_dKick] = DAC_Setup(8250, DAC_kick),     // DAC_SAMPLE_KICK
    [0x82 - SND_dKick] = DAC_Setup(24000, DAC_snare),   // DAC_SAMPLE_SNARE
    [0x83 - SND_dKick] = DAC_Setup(14500, DAC_timpani), // DAC_SAMPLE_TIMPANI (default/unpitched)
    [0x84 - SND_dKick] = DAC_Setup(15000, DAC_scratch), // DAC_SAMPLE_SCRATCH
    [0x85 - SND_dKick] = DAC_Setup(17000, DAC_clap),    // DAC_SAMPLE_CLAP
    [0x86 - SND_dKick] = DAC_Setup(13500, DAC_tom),     // DAC_SAMPLE_TOM (default/unpitched)
    [0x87 - SND_dKick] = DAC_Setup(7375, DAC_bongo),    // DAC_SAMPLE_BONGO (default/unpitched)
    // $88-$8B: Timpani Hi/Mid/Low/Floor -- transcribed directly from real
    // Sonic 1's own DAC_sample_rate/byte_71CC4 table (dpcmLoopCounter(Hz)
    // per entry, referenced from DACUpdateTrack), not derived/guessed.
    [0x88 - SND_dKick] = DAC_Setup(9750, DAC_timpani),
    [0x89 - SND_dKick] = DAC_Setup(8750, DAC_timpani),
    [0x8A - SND_dKick] = DAC_Setup(7150, DAC_timpani),
    [0x8B - SND_dKick] = DAC_Setup(7000, DAC_timpani),
    // $8C-$8E: Tom Mid/Low/Floor -- Sonic 2-exclusive, per your direction.
    // No standalone absolute-Hz disassembly table available for these;
    // derived from Sonic 2's own dac_sample_metadata scale factors
    // (1.70/1.30/1.10) against DAC_SAMPLE_TOM's confirmed base rate above.
    [0x8C - SND_dKick] = DAC_Setup(22950, DAC_tom),
    [0x8D - SND_dKick] = DAC_Setup(17550, DAC_tom),
    [0x8E - SND_dKick] = DAC_Setup(14850, DAC_tom),
    // $8F-$91: Bongo Hi/Mid/Low -- same derivation as Tom above, scale
    // factors 2.00/1.75/1.30 against DAC_SAMPLE_BONGO's base rate (7375 Hz).
    [0x8F - SND_dKick] = DAC_Setup(14750, DAC_bongo),
    [0x90 - SND_dKick] = DAC_Setup(12906, DAC_bongo),
    [0x91 - SND_dKick] = DAC_Setup(9588, DAC_bongo),
    [0x92 - SND_dKick] = DAC_Setup(19000, DAC_s3snare), // Sonic 3's snare -- temporarily has this slot, see DAC_SAMPLE_S3SNARE's own comment
    [0x93 - SND_dKick] = DAC_Setup(11500, DAC_s3tom),   // Sonic 3's Hi-Tom
    [0x94 - SND_dKick] = DAC_Setup(9000, DAC_s3tom),    // Sonic 3's Mid-Tom -- same sample as Hi-Tom, different rate
    [0x95 - SND_dKick] = DAC_Setup(7500, DAC_s3tom),    // Sonic 3's Low-Tom -- same sample, different rate
    [0x96 - SND_dKick] = DAC_Setup(6500, DAC_s3tom),    // Sonic 3's Floor-Tom -- same sample, different rate
    [0x97 - SND_dKick] = DAC_Setup(19000, DAC_s3kick),  // Sonic 3's Kick
    [0x98 - SND_dKick] = DAC_Setup(19000, DAC_muffled_snare), // Muffled Snare
    [0x99 - SND_dKick] = DAC_Setup(17000, DAC_crash),   // Crash Cymbal
    [0x9A - SND_dKick] = DAC_Setup(13500, DAC_ride),    // Ride Cymbal
    [0x9B - SND_dKick] = DAC_Setup(9000, DAC_metal_hit_1), // Low Metal Hit
    [0x9C - SND_dKick] = DAC_Setup(7375, DAC_metal_hit_1), // Floor Metal Hit -- same sample, different rate
    [0x9D - SND_dKick] = DAC_Setup(15000, DAC_metal_hit_2), // High Metal Hit
    [0x9E - SND_dKick] = DAC_Setup(13000, DAC_metal_hit_3), // Higher Metal Hit
    [0x9F - SND_dKick] = DAC_Setup(10000, DAC_metal_hit_3), // Mid Metal Hit -- same sample, different rate
    [0xA0 - SND_dKick] = DAC_Setup(15000, DAC_s3clap),  // Sonic 3's Clap
    [0xA1 - SND_dKick] = DAC_Setup(20500, DAC_etom),    // Electric High Tom
    [0xA2 - SND_dKick] = DAC_Setup(16000, DAC_etom),    // Electric Mid Tom -- same sample, different rate
    [0xA3 - SND_dKick] = DAC_Setup(13500, DAC_etom),    // Electric Low Tom -- same sample, different rate
    [0xA4 - SND_dKick] = DAC_Setup(11500, DAC_etom),    // Electric Floor Tom -- same sample, different rate
    [0xA5 - SND_dKick] = DAC_Setup(17000, DAC_pitched_snare), // Tight Snare
    [0xA6 - SND_dKick] = DAC_Setup(13500, DAC_pitched_snare), // Mid Pitched Snare -- same sample, different rate
    [0xA7 - SND_dKick] = DAC_Setup(12000, DAC_pitched_snare), // Loose Snare -- same sample, different rate
    [0xA8 - SND_dKick] = DAC_Setup(9750, DAC_pitched_snare), // Looser Snare -- same sample, different rate
    [0xA9 - SND_dKick] = DAC_Setup(13000, DAC_s3timpani), // Sonic 3's Hi Timpani
    [0xAA - SND_dKick] = DAC_Setup(8500, DAC_s3timpani), // Sonic 3's Low Timpani -- same sample, different rate
    [0xAB - SND_dKick] = DAC_Setup(9250, DAC_s3timpani), // Sonic 3's Mid Timpani -- same sample, different rate
    [0xAC - SND_dKick] = DAC_Setup(12500, DAC_quick_loose_snare), // Quick Loose Snare
    [0xAD - SND_dKick] = DAC_Setup(13500, DAC_click),   // Click -- candidate for a future ParadoxComposer metronome
    [0xAE - SND_dKick] = DAC_Setup(8000, DAC_power_kick), // Power Kick
    [0xAF - SND_dKick] = DAC_Setup(8000, DAC_quick_glass_crash), // Quick Glass Crash
    [0xB0 - SND_dKick] = DAC_Setup(12500, DAC_glass_crash_snare), // Glass Crash + Snare
    [0xB1 - SND_dKick] = DAC_Setup(12500, DAC_glass_crash), // Glass Crash
    [0xB2 - SND_dKick] = DAC_Setup(13500, DAC_glass_crash_kick), // Glass Crash + Kick
    [0xB3 - SND_dKick] = DAC_Setup(13500, DAC_quiet_glass_crash), // Quiet Glass Crash
    [0xB4 - SND_dKick] = DAC_Setup(8000, DAC_snare_kick), // Odd Snare-Kick -- replaces a redundant sample that was never added to res/DAC/
    [0xB5 - SND_dKick] = DAC_Setup(16000, DAC_xbass_kick), // Snuck through, so became Claves
    // $B6 = Sonic 3's own $A6 slot ("Come On!") skipped for copyright
    // reasons -- replaced with S3's $A7 (Dance Snare) instead.
    [0xB6 - SND_dKick] = DAC_Setup(8000, DAC_dance_snare), // Dance Snare
    [0xB7 - SND_dKick] = DAC_Setup(8000, DAC_loose_kick), // Loose Kick
    [0xB8 - SND_dKick] = DAC_Setup(8000, DAC_mod_loose_kick), // Snuck through, so patched with hand drum
    // Sonic 3's Woo!/Go!/Snare+Go! vocal samples skipped for copyright
    // reasons -- straight to S3's Power Tom ($AF) instead.
    [0xB9 - SND_dKick] = DAC_Setup(14000, DAC_wood_block), // Power Tom (Sonic 3 $AF)
    [0xBA - SND_dKick] = DAC_Setup(9750, DAC_wood_block), // Hi Wood Block -- same sample, different rate
    [0xBB - SND_dKick] = DAC_Setup(8000, DAC_wood_block_2), // Low Wood Block
    [0xBC - SND_dKick] = DAC_Setup(8500, DAC_conga), // "Hi Hit Drum" per the guide, sounds more like a conga
    [0xBD - SND_dKick] = DAC_Setup(12500, DAC_conga), // "Kick w/ extra bass" per the guide -- same sample as $BC, reads as a Hi Conga
    [0xBE - SND_dKick] = DAC_Setup(8500, DAC_hit_drum_1), // Gavel-like hit drum
    [0xBF - SND_dKick] = DAC_Setup(12500, DAC_hit_drum_1), // Gavel-like hit drum -- same sample, different rate
    [0xC0 - SND_dKick] = DAC_Setup(8500, DAC_hit_drum_2), // Muted-gunshot-like hit drum
    [0xC1 - SND_dKick] = DAC_Setup(12500, DAC_hit_drum_2), // Muted-gunshot-like hit drum -- same sample, different rate
    // 5 pitches of hit_drum_3 -- likely Hi/Mid/Low/Floor + one more hit-drum
    // variant, same sample throughout.
    [0xC2 - SND_dKick] = DAC_Setup(12500, DAC_hit_drum_3),
    [0xC3 - SND_dKick] = DAC_Setup(11000, DAC_hit_drum_3),
    [0xC4 - SND_dKick] = DAC_Setup(10000, DAC_hit_drum_3),
    [0xC5 - SND_dKick] = DAC_Setup(9750, DAC_hit_drum_3),
    [0xC6 - SND_dKick] = DAC_Setup(13000, DAC_hit_drum_3), // corrected from a stated 1300 Hz -- confirmed typo
    [0xC7 - SND_dKick] = DAC_Setup(12500, DAC_metal_crash_hit), // Metal Crash Hit
    [0xC8 - SND_dKick] = DAC_Setup(12500, DAC_low_echo_clap), // Echoed Clap Hit
    [0xC9 - SND_dKick] = DAC_Setup(8000, DAC_low_echo_clap), // "Lower Echoed Clap Hit" per the guide, sounds more like a drum
    // Real driver has this sample at 2 note slots, both 12500 Hz (a
    // redundant duplicate) -- second one given 11000 Hz here instead so
    // it's an actually-distinct pitch rather than a wasted duplicate slot.
    [0xCA - SND_dKick] = DAC_Setup(12500, DAC_hiphop_kick), // Hip-Hop Kick
    [0xCB - SND_dKick] = DAC_Setup(11000, DAC_hiphop_kick), // Hip-Hop Kick -- same sample, different rate
    [0xCC - SND_dKick] = DAC_Setup(8000, DAC_dance_kick), // "Dance Style Kick" per the guide, sounds reversed
    [0xCD - SND_dKick] = DAC_Setup(8000, DAC_hiphop_kick_2), // "Hip-Hop Kick 2" per the guide, sounds like a record scratch
    [0xCE - SND_dKick] = DAC_Setup(8000, DAC_hiphop_kick_3), // Hip-Hop Kick 3
    [0xCF - SND_dKick] = DAC_Setup(12500, DAC_deep_hit), // Deep Hit Drum
    [0xD0 - SND_dKick] = DAC_Setup(12500, DAC_wood_block_3), // Wood Block 3 -- moved down from $D1 to close the gap left by a skipped (MJ-esque vocal) sample
    [0xD1 - SND_dKick] = DAC_Setup(12500, DAC_unused), // "Unused" per the guide, sounds like a soundtrack hit
    [0xD2 - SND_dKick] = DAC_Setup(16000, DAC_reverse), // Reverse Cymbal -- native rate
    [0xD3 - SND_dKick] = DAC_Setup(16000, DAC_psytrance_kick), // Psytrance Kick (base)
    [0xD4 - SND_dKick] = DAC_Setup(20000, DAC_psytrance_kick), // Psytrance Kick -- Hi
    [0xD5 - SND_dKick] = DAC_Setup(13000, DAC_psytrance_kick), // Psytrance Kick -- Low
    [0xD6 - SND_dKick] = DAC_Setup(10500, DAC_psytrance_kick), // Psytrance Kick -- Floor
    [0xD7 - SND_dKick] = DAC_Setup(16000, DAC_psytrance_snare), // Psytrance Snare (base)
    [0xD8 - SND_dKick] = DAC_Setup(20000, DAC_psytrance_snare), // Psytrance Snare -- Hi
    [0xD9 - SND_dKick] = DAC_Setup(13000, DAC_psytrance_snare), // Psytrance Snare -- Low
    [0xDA - SND_dKick] = DAC_Setup(10500, DAC_psytrance_snare), // Psytrance Snare -- Floor
    [0xDB - SND_dKick] = DAC_Setup(16000, DAC_cowbell), // Cowbell
    [0xDC - SND_dKick] = DAC_Setup(16000, DAC_rimshot), // Rimshot / Side-Stick
    [0xDD - SND_dKick] = DAC_Setup(16000, DAC_cuica), // Cuica
    [0xDE - SND_dKick] = DAC_Setup(16000, DAC_guiro), // Guiro
    // $DF: highest possible note byte, reserved for the "SEGA!" boot voice
    // clip -- NOT a real DPCM sample (res/PCM/sega is a different codec
    // entirely, see PlaySegaSound_Trigger), so it's dispatched as a special
    // case rather than through this table (see the $DF check below); listed
    // here only as a placeholder reservation so this slot doesn't get
    // reused by mistake for something else.
};

// Real driver's DAC_sample_rate table (byte_71CC4, referenced from
// DACUpdateTrack) -- each entry is dpcmLoopCounter(Hz) for note bytes
// $88-$8B (Hi/Mid/Low/Floor). Real Sonic 1's own table continues with
// $8C/$8D (both dpcmLoopCounter saturating at $FF, i.e. "so slow you may
// want to skip them" per its own comment) and $8E/$8F (no entry at all) --
// not implemented here, since this project's own $8C+ range is used for Tom
// variants instead. Tom/Bongo variants ($8C-$8E/$8F-$91) are Sonic
// 2-exclusive, per your direction -- no standalone absolute-Hz disassembly
// table was available for those; derived from Sonic 2's own
// dac_sample_metadata macro (base_rate * scale, scale factors
// 1.70/1.30/1.10 for Tom and 2.00/1.75/1.30 for Bongo) applied against
// DAC_SAMPLE_TOM/BONGO's own confirmed base rates (13500 Hz, 7375 Hz), then
// rounded to the nearest whole Hz. All of the above now lives directly in
// dac_notes' own $88-$91 entries -- this comment is kept only as a source
// citation for where those specific numbers came from.

// JMan2050's DAC decode table (see zDACDecodeTbl in the real driver): each
// 4-bit nibble is a signed step added to an 8-bit accumulator, giving cheap
// exponential (not linear) DPCM compression -- small nibbles for fine
// detail near silence, large nibbles for fast transients like a kick's
// attack. Purely a mathematical progression (powers of two, then their
// negatives), not driver-specific data.
static const int8_t dac_delta_table[16] = {
    0, 1, 2, 4, 8, 0x10, 0x20, 0x40, (int8_t)0x80,
    -1, -2, -4, -8, -0x10, -0x20, -0x40,
};

// The "SEGA!" boot voice clip (res/PCM/sega) is plain unsigned 8-bit mono
// PCM -- no header, no delta-decode, just raw samples written straight to
// the DAC output register one at a time (see zPlay_SegaPCM/
// zPlaySEGAPCMLoop). 16000 is the Sonic 1 driver's own named pitch-loop
// rate constant (pcmLoopCounter(16000)) -- the Sonic 3 sample is format-
// compatible but runs faster (its own WAV header gives 33598 Hz), so this
// value has to match whichever sample actually ends up in res/PCM/sega.
#define SEGA_PCM_HZ 16000

static void PlaySegaSound_Trigger(SoundChipSet *cs) {
    cs->pcm_pos = 0;
    cs->pcm_playing = 1;
    cs->pcm_phase = 0;
}

static void PCM_Generate(SoundChipSet *cs, int32_t *out, uint32_t count, uint32_t sample_rate) {
    if (!cs->pcm_playing)
        return;
    int left = (cs->dac_pan & 0x80) != 0;
    int right = (cs->dac_pan & 0x40) != 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!cs->pcm_playing)
            break;
        int32_t s = ((int32_t)PCM_sega[cs->pcm_pos] - 0x80) << 6; // roughly matches PSG/DPCM amplitude scale
        if (left)
            out[2 * i + 0] += s;
        if (right)
            out[2 * i + 1] += s;

        cs->pcm_phase += SEGA_PCM_HZ;
        while (cs->pcm_phase >= sample_rate) {
            cs->pcm_phase -= sample_rate;
            cs->pcm_pos++;
            if (cs->pcm_pos >= sizeof(PCM_sega)) {
                cs->pcm_playing = 0;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------
// PSG volume envelopes (res/PSG/psg1-9.bin -- matches fTone_01..fTone_09
// in smps.h, the smpsHeaderPSG "voice" argument). Each is a sequence of
// signed per-tick deltas added cumulatively to a channel's base volume
// (PSGUpdateVolFX in the real driver); $80 is the terminator -- once hit,
// the envelope holds at its current value forever (VolEnvHold just
// decrements the index back so the same terminator keeps getting re-read).
// ---------------------------------------------------------------------

#include "Resource/PSG/psg1.h"
#include "Resource/PSG/psg2.h"
#include "Resource/PSG/psg3.h"
#include "Resource/PSG/psg4.h"
#include "Resource/PSG/psg5.h"
#include "Resource/PSG/psg6.h"
#include "Resource/PSG/psg7.h"
#include "Resource/PSG/psg8.h"
#include "Resource/PSG/psg9.h"

typedef struct {
    const uint8_t *data;
    uint32_t length;
} PSGEnvelope;

#define PSG_ENVELOPE_COUNT 9
static const PSGEnvelope psg_envelopes[PSG_ENVELOPE_COUNT] = {
    {PSG_psg1, sizeof(PSG_psg1)}, {PSG_psg2, sizeof(PSG_psg2)}, {PSG_psg3, sizeof(PSG_psg3)},
    {PSG_psg4, sizeof(PSG_psg4)}, {PSG_psg5, sizeof(PSG_psg5)}, {PSG_psg6, sizeof(PSG_psg6)},
    {PSG_psg7, sizeof(PSG_psg7)}, {PSG_psg8, sizeof(PSG_psg8)}, {PSG_psg9, sizeof(PSG_psg9)},
};

// Steps one channel's volume envelope by one tick and returns the resulting
// PSG attenuation (0-15), or -1 if nothing changed (envelope disabled, at
// rest, or held at its terminator -- caller should leave the register
// alone). ch->voice_index selects the envelope (0 = none, 1-9 = psg_envelopes
// index); ch->vol_env_index is the current position within it.
static int PSGStepEnvelope(SoundChannel *ch) {
    if (ch->voice_index == 0 || ch->voice_index > PSG_ENVELOPE_COUNT)
        return -1;
    const PSGEnvelope *env = &psg_envelopes[ch->voice_index - 1];
    if (ch->vol_env_index >= env->length)
        return -1; // Out of range data -- treat as silently held, don't walk off the array

    int8_t delta = (int8_t)env->data[ch->vol_env_index];
    if ((uint8_t)delta == 0x80)
        return -1; // Terminator -- hold (index does NOT advance)
    ch->vol_env_index++;

    int vol = (int)ch->volume + delta;
    if (vol < 0 || vol > 0x0F)
        vol = 0x0F; // Real driver clamps both overflow AND underflow to silence, not to loudest
    return vol;
}

// Driver-version-3 only: steps an FM channel's "volume flutter" envelope
// (cfFMVolEnv, $FF,$06) by one tick, borrowing the same psg_envelopes[]
// table PSGStepEnvelope uses above (Flamedriver's own FM vol-env reuses a
// PSG-style envelope table, per the researched semantics) rather than a
// second near-duplicate table. ch->fm_vol_env_index is 0 = disabled,
// 1-9 = table index; ch->fm_vol_env_pos is this envelope's own position,
// kept separate from vol_env_index (PSG-only, driven by a different flag).
// Returns 1 if ch->volume changed (caller should re-apply via
// FM_ApplyVolume), 0 otherwise.
static int FMStepVolEnv(SoundChannel *ch) {
    if (ch->fm_vol_env_index <= 0 || ch->fm_vol_env_index > PSG_ENVELOPE_COUNT)
        return 0;
    const PSGEnvelope *env = &psg_envelopes[ch->fm_vol_env_index - 1];
    if (ch->fm_vol_env_pos >= env->length)
        return 0;
    int8_t delta = (int8_t)env->data[ch->fm_vol_env_pos];
    if ((uint8_t)delta == 0x80)
        return 0; // Terminator -- hold
    ch->fm_vol_env_pos++;
    int vol = (int)ch->volume + delta;
    ch->volume = (uint8_t)(vol < 0 ? 0 : (vol > 0x7F ? 0x7F : vol));
    return 1;
}

static void DAC_Trigger(SoundChipSet *cs, int sample_id) {
    if (sample_id < 0 || sample_id >= DAC_SAMPLE_COUNT)
        return;
    cs->dac_data = dac_samples[sample_id].data;
    cs->dac_sample_id = sample_id;
    cs->dac_nibble_count = dac_samples[sample_id].length * 2;
    cs->dac_rate = (sample_id == cs->dac_pitch_override_sample) ? cs->dac_pitch_override_rate
                                                                  : dac_samples[sample_id].rate;
    cs->dac_nibble_pos = 0;
    cs->dac_accum = 0x80;
    cs->dac_playing = 1;
    cs->dac_phase = 0;
}

// Same as DAC_Trigger, but takes a dac_notes[] entry directly (by note byte,
// not by DAC_SAMPLE_* id) plus that note's own family (for the override-rate
// check below) -- used by the main $81-$DF note dispatch, which needs a
// dac_notes[] slot's own baked-in rate/data rather than a named sample.
static void DAC_TriggerNote(SoundChipSet *cs, const DACSample *note, int family) {
    cs->dac_data = note->data;
    cs->dac_sample_id = family;
    cs->dac_nibble_count = note->length * 2;
    cs->dac_rate = (family >= 0 && family == cs->dac_pitch_override_sample) ? cs->dac_pitch_override_rate
                                                                              : note->rate;
    cs->dac_nibble_pos = 0;
    cs->dac_accum = 0x80;
    cs->dac_playing = 1;
    cs->dac_phase = 0;
}

// Triggers a DAC hit by raw note byte ($81-$DF, or $DF's "SEGA!" special
// case), including the family/pitch-override bookkeeping -- factored out of
// the main DAC-track note dispatch below so driver-version-3's cfPlayDACSample
// ($EA -- fires a DAC sample from a NON-DAC track as a side effect, per
// Flamedriver) can trigger the exact same way without duplicating this
// logic. No-ops silently for a note byte with no dac_notes[] entry.
static void DAC_TriggerByNoteByte(SoundChipSet *cs, uint8_t b) {
    if (b == 0xDF) {
        PlaySegaSound_Trigger(cs);
        return;
    }
    int note = b - SND_dKick;
    if (note < 0 || note >= DAC_NOTE_COUNT || !dac_notes[note].data)
        return;
    // family is which DAC_SAMPLE_* this note's sample belongs to -- always
    // set (needed so a plain base byte's own DAC_TriggerNote call can tell
    // whether ITS family currently has an override active), even though
    // only $88-$8B/$8C-$8E/$8F-$91 (pitch-shifted variants) actually WRITE a
    // new override below. Plain base bytes ($81-$87, $92) neither set nor
    // clear one, so they keep using whatever override (if any) a previous
    // variant left active, matching real hardware exactly.
    int family = -1;
    int is_variant = 0;
    if (b >= SND_dKick && b <= SND_dKick + 6) {
        family = b - SND_dKick; // $81-$87 map 1:1 onto DAC_SAMPLE_KICK..BONGO
    } else if (b >= 0x88 && b <= 0x8B) {
        family = DAC_SAMPLE_TIMPANI;
        is_variant = 1;
    } else if (b >= 0x8C && b <= 0x8E) {
        family = DAC_SAMPLE_TOM;
        is_variant = 1;
    } else if (b >= 0x8F && b <= 0x91) {
        family = DAC_SAMPLE_BONGO;
        is_variant = 1;
    } else if (b == 0x92) {
        family = DAC_SAMPLE_S3SNARE;
    }
    if (is_variant) {
        cs->dac_pitch_override_sample = family;
        cs->dac_pitch_override_rate = dac_notes[note].rate;
    }
    cs->dac_timpani_variant = (b >= 0x88 && b <= 0x8B) ? (b - 0x88) : -1;
    DAC_TriggerNote(cs, &dac_notes[note], family);
}

// Steps DAC playback by `count` output samples at `sample_rate`, adding the
// decoded (and centered) waveform into `out`, at the currently-triggered
// sample's own real rate (see dac_samples[]/DAC_Trigger) -- not a shared
// fixed rate; kick/snare/timpani genuinely differ in the real driver's own
// zPCM_Table. Ignores the DAC header's pitch byte -- per-sample pitch
// shifting (used for the timpani variants) isn't implemented yet.
static void DAC_Generate(SoundChipSet *cs, int32_t *out, uint32_t count, uint32_t sample_rate) {
    if (!cs->dac_playing)
        return;
    int left = (cs->dac_pan & 0x80) != 0;
    int right = (cs->dac_pan & 0x40) != 0;
    for (uint32_t i = 0; i < count; i++) {
        if (!cs->dac_playing)
            break;
        int32_t s = ((int32_t)cs->dac_accum - 0x80) << 6; // roughly matches PSG's amplitude scale
        if (left)
            out[2 * i + 0] += s;
        if (right)
            out[2 * i + 1] += s;

        cs->dac_phase += cs->dac_rate;
        while (cs->dac_phase >= sample_rate) {
            cs->dac_phase -= sample_rate;
            if (cs->dac_nibble_pos >= cs->dac_nibble_count) {
                cs->dac_playing = 0;
                break;
            }
            uint8_t byte = cs->dac_data[cs->dac_nibble_pos / 2];
            uint8_t nibble = (cs->dac_nibble_pos & 1) ? (byte >> 4) : (byte & 0x0F);
            int step = dac_delta_table[nibble];
            int accum = (int)cs->dac_accum + step;
            if (accum < 0)
                accum = 0;
            if (accum > 0xFF)
                accum = 0xFF;
            cs->dac_accum = (uint8_t)accum;
            cs->dac_nibble_pos++;
        }
    }
}

// ---------------------------------------------------------------------
// PSG register helpers (SN76489 latch/data write protocol)
// ---------------------------------------------------------------------

static void PSG_SetAttenuation(SN76489 *chip, int channel, uint8_t atten4) {
    if (atten4 > 0x0F)
        atten4 = 0x0F;
    if (getenv("SONIC_PSG_TRACE")) fprintf(stdout, "[%u] PSGATTEN ch%d=%u\n", sound_trace_frame, channel, atten4);
    SN76489_Write(chip, (uint8_t)(0x80 | (channel << 5) | 0x10 | atten4));
}

static void PSG_SetTonePeriod(SN76489 *chip, int channel, uint16_t period10) {
    if (period10 > 0x3FF)
        period10 = 0x3FF;
    if (getenv("SONIC_PSG_TRACE")) fprintf(stdout, "[%u] PSGPERIOD ch%d=%u\n", sound_trace_frame, channel, period10);
    SN76489_Write(chip, (uint8_t)(0x80 | (channel << 5) | (period10 & 0x0F)));
    SN76489_Write(chip, (uint8_t)((period10 >> 4) & 0x3F));
}

// ---------------------------------------------------------------------
// YM2612 register helpers
// ---------------------------------------------------------------------

// channel_index here is SOUND_CHANNEL_FM_BASE..+5 (real FM channels 1-6,
// SOUND_CHANNEL_DAC excluded -- the DAC doesn't have YM2612 registers).
// Returns the write offset (0/1 = port 1 addr/data for channels 1-3,
// 2/3 = port 2 for channels 4-6) and the in-port channel number (0-2).
static void FMPortChannel(int channel_index, int *port_offset, int *chan_in_port) {
    int fm_index = channel_index - SOUND_CHANNEL_FM_BASE; // 0-5
    *port_offset = (fm_index < 3) ? 0 : 2;
    *chan_in_port = fm_index % 3;
}

static void FM_WriteReg(YM2612 *fm, int port_offset, uint8_t reg, uint8_t data) {
    if (getenv("SONIC_FM_TRACE"))
        fprintf(stdout, "[%u] FMREG port=%d reg=$%02X data=$%02X\n", sound_trace_frame, port_offset, reg, data);
    YM2612_Write(fm, (uint32_t)port_offset, reg);
    YM2612_Write(fm, (uint32_t)port_offset + 1, data);
}

// TL (Total Level) is attenuation, same convention as PSG -- 0 = loudest,
// higher = quieter. Real max is 127 (7-bit).
static uint8_t FM_ClampTL(int tl) { return (uint8_t)(tl < 0 ? 0 : (tl > 127 ? 127 : tl)); }

// Applies sch->volume as a live offset to every operator's cached base TL
// and writes it to hardware immediately -- used by $E6 (designed for FM,
// "effective immediately" per the coordination-flag table). $EC (designed
// for PSG) instead just updates sch->volume without calling this -- for FM
// channels that only takes effect "on next voice change", i.e. the next
// time FM_LoadVoice folds sch->volume in below.

// Real driver's FMSlotMask (s1.sounddriver.asm): per-algorithm bitmask of
// which operators are "carriers" -- the ones an algorithm actually routes
// to audible output. Only carriers get the track's volume attenuation
// added to their TL; modulator operators keep their authored TL untouched,
// since changing a modulator's level changes the FM modulation depth (the
// timbre itself), not just the loudness. Indexed by algorithm (0-7 real
// hardware, 8-15 fmcore-original -- see fm_voice.c's own comment on each);
// bit order matches the real table's own op1,op3,op2,op4 write order (SCHG
// FM voice format table / FMInstrumentTLTable -- see FM_LoadVoice's own
// comment), not natural op order -- FM_SLOT_BIT[] below remaps logical op
// index to that bit position. (This was previously {3,2,1,0}, derived
// against the op4,op3,op2,op1 order used before that fix -- left stale when
// the write order changed, silently marking op1 as algorithm 0-3's carrier
// instead of the real op4, and similarly wrong elsewhere.) Entries 8-15
// derived directly from FM_VOICE_ALGORITHM_CONNECT[8..15]'s own carrier
// sets (fmcore/fm_voice.c), same FM_SLOT_BIT remap applied for consistency.
static const uint8_t FM_SLOT_MASK[16] = {8, 8, 8, 8, 0xA, 0xE, 0xE, 0xF, 8, 0xC, 8, 8, 0xA, 0xF, 0xF, 0xF};
static const int FM_SLOT_BIT[4] = {0, 2, 1, 3}; // natural op1..op4 -> FMSlotMask bit position

static int FM_IsCarrier(uint8_t algorithm, int op) {
    return (FM_SLOT_MASK[algorithm & 0xF] >> FM_SLOT_BIT[op]) & 1;
}

static void FM_ApplyVolume(YM2612 *fm, SoundChannel *sch, int channel_index) {
    if (getenv("SONIC_VOL_TRACE")) fprintf(stdout, "[%u] APPLYVOL ch%d volume=%u\n", sound_trace_frame, channel_index, sch->volume);
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    for (int op = 0; op < 4; op++) {
        int slot = op * 4; // register offset per operator is natural: op1=+0x00, op2=+0x04, op3=+0x08, op4=+0x0C
        int is_carrier = FM_IsCarrier(sch->feedback_algo, op);
        int add = is_carrier ? sch->volume : 0;
        // Debug-only mute (ParadoxComposer's Channels panel): only carrier
        // operators reach the audible mix, so only those need forcing to
        // max attenuation -- modulator operators keep running normally
        // (they're inaudible on their own either way), which also means
        // un-muting doesn't need to restore any modulator state.
        uint8_t tl = (sch->debug_muted && is_carrier) ? 0x7F : FM_ClampTL(sch->fm_base_tl[op] + add);
        FM_WriteReg(fm, port, (uint8_t)(0x40 + slot + ch), tl);
    }
}

// Loads one 25-byte voice block (see smps.c's smpsVcTotalLevel comment for
// the exact layout this mirrors) into a channel's 4 operators + algorithm/
// feedback registers.
static void FM_LoadVoice(YM2612 *fm, SoundChannel *sch, int channel_index, const uint8_t *voice) {
    // Algorithm is 4 bits (0-7 real hardware, 8-15 fmcore-original -- see
    // fm_voice.c's own comment): low 3 bits as always, plus bit7 as the
    // high algorithm bit ("0"=real algorithm 0-7, "1"=custom fmcore
    // algorithm 8-15). Real hardware register $B0 leaves bits 6-7 unused/
    // ignored, so this is harmless if this exact byte were ever loaded on
    // real silicon -- it just sees algorithm 0-7's low bits and silently
    // ignores bit7. Feedback stays bits3-5, unchanged from real hardware's
    // own layout.
    sch->feedback_algo = (uint8_t)((((voice[0] >> 7) & 1) << 3) | (voice[0] & 7));

    // voice[] is stored op1,op3,op2,op4 per row (see smps.c's
    // smpsVcTotalLevel comment -- matches the SCHG FM voice format table
    // and the real driver's FMInstrumentOperatorTable register-write
    // sequence). For natural operator index op (0=op1..3=op4), its byte
    // position within each row is pos_map[op] below (self-inverse: op2 and
    // op3 swap positions, op1/op4 stay put).
    static const int pos_map[4] = {0, 2, 1, 3};
    uint8_t op_regs[4][6];
    for (int op = 0; op < 4; op++) {
        int pos = pos_map[op];
        op_regs[op][0] = voice[1 + pos];  // DT/MUL
        op_regs[op][1] = voice[5 + pos];  // RS/AR
        op_regs[op][2] = voice[9 + pos];  // AM/D1R
        op_regs[op][3] = voice[13 + pos]; // D2R
        op_regs[op][4] = voice[17 + pos]; // D1L/RR

        uint8_t base_tl = voice[21 + pos] & 0x7F;
        sch->fm_base_tl[op] = base_tl;
        int add = FM_IsCarrier(sch->feedback_algo, op) ? sch->volume : 0;
        op_regs[op][5] = FM_ClampTL(base_tl + add); // TL
    }
    if (getenv("SONIC_FM_TRACE"))
        fprintf(stdout, "[%u] FMVOICELOAD channel_index=%d alg_fb=$%02X\n", sound_trace_frame, channel_index, voice[0]);
    YM2612_LoadVoice(fm, channel_index - SOUND_CHANNEL_FM_BASE, voice[0], op_regs);
}

// JSON-engine equivalent of FM_LoadVoice above -- reads a voice object's
// own named fields directly (operators[] already in natural op1..op4
// order per the schema, see json_to_header.py's own op_field()) instead of
// a 25-byte packed/reordered blob, so unlike FM_LoadVoice there's no
// pos_map/write-order shuffle needed: slot op*4 reads straight from
// operators[op].
static void FM_LoadVoiceJSON(YM2612 *fm, SoundChannel *sch, int channel_index, const PJValue *voice) {
    int alg = JsonUnhexInt(pj_object_get(voice, "smpsVcAlgorithm"), 0);
    int fb = JsonUnhexInt(pj_object_get(voice, "smpsVcFeedback"), 0);
    int ub = JsonUnhexInt(pj_object_get(voice, "smpsVcUnusedBits"), 0);
    uint8_t byte0 = (uint8_t)((((alg >> 3) & 1) << 7) | ((ub & 1) << 6) | ((fb & 7) << 3) | (alg & 7));
    sch->feedback_algo = (uint8_t)((((byte0 >> 7) & 1) << 3) | (byte0 & 7));

    const PJValue *ops = pj_object_get(voice, "operators");
    uint8_t op_regs[4][6];
    for (int op = 0; op < 4; op++) {
        const PJValue *o = pj_array_get(ops, (size_t)op);
        int dt = JsonUnhexInt(pj_object_get(o, "smpsVcDetune"), 0);
        int mul = JsonUnhexInt(pj_object_get(o, "smpsVcCoarseFreq"), 0);
        op_regs[op][0] = (uint8_t)(((dt & 7) << 4) | (mul & 0xF)); // DT/MUL
        int rs = JsonUnhexInt(pj_object_get(o, "smpsVcRateScale"), 0);
        int ar = JsonUnhexInt(pj_object_get(o, "smpsVcAttackRate"), 0);
        op_regs[op][1] = (uint8_t)(((rs & 3) << 6) | (ar & 0x1F)); // RS/AR
        int am = JsonUnhexInt(pj_object_get(o, "smpsVcAmpMod"), 0);
        int d1r = JsonUnhexInt(pj_object_get(o, "smpsVcDecayRate1"), 0);
        op_regs[op][2] = (uint8_t)(((am & 1) << 7) | (d1r & 0x1F)); // AM/D1R
        int d2r = JsonUnhexInt(pj_object_get(o, "smpsVcDecayRate2"), 0);
        op_regs[op][3] = (uint8_t)(d2r & 0x1F); // D2R
        int d1l = JsonUnhexInt(pj_object_get(o, "smpsVcDecayLevel"), 0);
        int rr = JsonUnhexInt(pj_object_get(o, "smpsVcReleaseRate"), 0);
        op_regs[op][4] = (uint8_t)(((d1l & 0xF) << 4) | (rr & 0xF)); // D1L/RR

        uint8_t base_tl = (uint8_t)(JsonUnhexInt(pj_object_get(o, "smpsVcTotalLevel"), 0) & 0x7F);
        sch->fm_base_tl[op] = base_tl;
        int add = FM_IsCarrier(sch->feedback_algo, op) ? sch->volume : 0;
        op_regs[op][5] = FM_ClampTL(base_tl + add); // TL
    }
    if (getenv("SONIC_FM_TRACE"))
        fprintf(stdout, "[%u] FMVOICELOAD channel_index=%d alg_fb=$%02X\n", sound_trace_frame, channel_index, byte0);
    YM2612_LoadVoice(fm, channel_index - SOUND_CHANNEL_FM_BASE, byte0, op_regs);
}

// Pan direction is whole-channel only -- hard left, hard right, center
// (both), or silent (neither) -- not a continuous position. panLeft/
// panRight/panCentre/panNone in smps.h already match the L/R bits of
// register $B4 directly, and the SAME byte's bits5-4 (AMS) are a real,
// supported register field too, so the coordination-flag parameter byte is
// written through unmasked instead of zeroing them. AMS only has an
// audible effect once something enables the chip-wide LFO (register $22)
// -- no coordination flag writes that yet, so today AMS bits are silently
// inert, exactly as they'd be on real hardware. bits2-0 (FMS/PMS, pitch
// modulation depth) are a deliberate fixed 0/off, not a gap -- see
// YM2612_FMCore.c's own $B4-$B6 write handler comment for why.
static void FM_SetPan(YM2612 *fm, int channel_index, uint8_t value) {
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    FM_WriteReg(fm, port, (uint8_t)(0xB4 + ch), value);
}

static void FM_KeyOnOff(YM2612 *fm, int channel_index, int on) {
    // Diagnostic isolation only (SonicSoundWav): SONIC_FM_CHANNEL=N (0-5)
    // silences every other FM channel's key-on so the real ROM voice/note
    // data for just that one channel can be inspected/heard alone.
    const char *only = getenv("SONIC_FM_CHANNEL");
    if (only && (channel_index - SOUND_CHANNEL_FM_BASE) != atoi(only))
        on = 0;
    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    int chan_code = ch + ((channel_index - SOUND_CHANNEL_FM_BASE >= 3) ? 4 : 0);
    uint8_t op_mask = on ? 0xF0 : 0x00; // all 4 operators on/off together
    if (getenv("SONIC_FM_TRACE"))
        fprintf(stdout, "[%u] FMKEY channel_index=%d chan_code=$%02X on=%d\n", sound_trace_frame, channel_index, chan_code, on);
    YM2612_Write(fm, 0, 0x28);
    YM2612_Write(fm, 1, (uint8_t)(op_mask | chan_code));
}

// Real hardware F-Number table (12 semitones, block-0), cross-referenced
// against ValleyBell's SMPSPlay (github.com/sonicretro/SKC-SMPSOUT's
// SMPSPlay submodule, Engine/smps.c GetNote + SMPSPlay-DLL.cpp's
// DEF_FMFREQ_VAL) -- the real driver's own quantized per-semitone table,
// not a from-scratch pow()/log2() formula, which was producing audibly
// slightly-off tuning versus a real Genesis. The table is indexed starting
// from B, not C (13th entry is the next octave's B, unused here since we
// wrap via %12 ourselves) -- SMPSPlay's own GetNote compensates for this by
// adding 1 to the note number for FM specifically (FMBASEN_B) before
// indexing; we do the same below rather than reordering the table, so it
// stays a direct, checkable transcription of the source values.
static const uint16_t FM_FNUM_TABLE[12] = {0x25E, 0x284, 0x2AB, 0x2D3, 0x2FE, 0x32D,
                                            0x35C, 0x38F, 0x3C5, 0x3FF, 0x43C, 0x47C};

static void FM_SetFrequency(YM2612 *fm, int channel_index, int note_index, int mod_val) {
    if (note_index < 0)
        note_index = 0;
    int shifted = note_index + 1; // FMBASEN_B: realigns note_index==0 (nC0) onto the B-based table
    int octave = shifted / 12;
    int fnum = (int)FM_FNUM_TABLE[shifted % 12] + mod_val;
    if (octave > 7)
        octave = 7; // Defensive only -- real driver (FMOctWrap=0) trusts song data to never exceed this
    if (fnum < 0)
        fnum = 0;
    if (fnum > 2047)
        fnum = 2047;

    int port, ch;
    FMPortChannel(channel_index, &port, &ch);
    FM_WriteReg(fm, port, (uint8_t)(0xA4 + ch), (uint8_t)((octave << 3) | (fnum >> 8)));
    FM_WriteReg(fm, port, (uint8_t)(0xA0 + ch), (uint8_t)(fnum & 0xFF));
}

// Real hardware PSG period table (6 octaves), same source as FM_FNUM_TABLE
// above (DEF_PSGFREQ_68K_VAL) -- PSGBaseNote defaults to PSGBASEN_C (no
// index shift needed, unlike FM's table-starts-at-B quirk). The trailing 0
// is the real table's own terminator entry, kept here so an out-of-range
// clamp (matching GetNote's "Note >= PSGFreqCnt -> PSGFreqCnt-1") lands on
// the same value real hardware would.
static const uint16_t PSG_PERIOD_TABLE[70] = {
    0x356, 0x326, 0x2F9, 0x2CE, 0x2A5, 0x280, 0x25C, 0x23A, 0x21A, 0x1FB, 0x1DF, 0x1C4, 0x1AB, 0x193, 0x17D,
    0x167, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0EF, 0x0E2, 0x0D6, 0x0C9, 0x0BE, 0x0B4, 0x0A9, 0x0A0,
    0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071, 0x06B, 0x065, 0x05F, 0x05A, 0x055, 0x050, 0x04B, 0x047, 0x043,
    0x040, 0x03C, 0x039, 0x036, 0x033, 0x030, 0x02D, 0x02B, 0x028, 0x026, 0x024, 0x022, 0x020, 0x01F, 0x01D,
    0x01B, 0x01A, 0x018, 0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x000};

// note_index: 0 == C0, 1 semitone per step (already includes transpose).
static uint16_t PSGPeriodForNote(int note_index) {
    if (note_index < 0)
        note_index = 0;
    if (note_index >= (int)(sizeof(PSG_PERIOD_TABLE) / sizeof(PSG_PERIOD_TABLE[0])))
        note_index = (int)(sizeof(PSG_PERIOD_TABLE) / sizeof(PSG_PERIOD_TABLE[0])) - 1;
    uint16_t period = PSG_PERIOD_TABLE[note_index];
    if (period < 1)
        period = 1;
    if (period > 1023)
        period = 1023;
    return period;
}

// ---------------------------------------------------------------------
// Song/SFX header loading
// ---------------------------------------------------------------------

static uint16_t ReadWord(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static void ResetChannel(SoundChannel *ch) { memset(ch, 0, sizeof(*ch)); }

static void StartChannel(SoundChipSet *cs, SoundChannel *ch, const uint8_t *song_base, const uint8_t *data,
                          int8_t transpose, uint8_t volume, const uint8_t *voice_bank) {
    ResetChannel(ch);
    ch->song_base = song_base;
    ch->data_ptr = data;
    ch->transpose = transpose;
    ch->volume = volume;
    ch->tempo_divider = cs->duration_mult; // Per-track duration multiplier, overridable by $E5
    ch->active = 1;
    ch->duration_timeout = 0; // read the first command immediately on the next tick
    ch->voice_bank = voice_bank;
}

// JSON-engine equivalent of StartChannel -- events is a real PJValue*
// pointing directly at the block's events array (resolved by the caller
// via JsonResolveBlock), not a byte pointer. playlist is stashed on the
// CHANNEL (not read from cs) so later jump/loop/call resolution always
// targets whichever song/SFX THIS channel is actually playing -- see
// SoundChannel::json_playlist's own comment.
static void StartChannelJSON(SoundChipSet *cs, SoundChannel *ch, const PJValue *playlist, const PJValue *voices,
                              const PJValue *events, int8_t transpose, uint8_t volume) {
    ResetChannel(ch);
    ch->json_active = 1;
    ch->json_playlist = playlist;
    ch->json_voices = voices;
    ch->json_events = events;
    ch->json_event_index = 0;
    ch->json_loop_idx = -1; // not memset-safe -- 0 is a real loop slot, unlike every other JSON field here
    ch->transpose = transpose;
    ch->volume = volume;
    ch->tempo_divider = cs->duration_mult;
    ch->active = 1;
    ch->duration_timeout = 0;
}

// "cFM5"-style channel-ID names, as used by smpsHeaderSFXChannel -- same
// numeric space as SND_cFM3/etc above, just string-keyed since JSON author
// content spells them out rather than using the raw byte value directly.
static int JsonChannelIdLookup(const char *name) {
    static const struct { const char *name; int value; } table[] = {
        {"cFM1", 0x00}, {"cFM2", 0x01}, {"cFM3", SND_cFM3}, {"cFM4", SND_cFM4}, {"cFM5", SND_cFM5},
        {"cPSG1", SND_cPSG1}, {"cPSG2", SND_cPSG2}, {"cPSG3", SND_cPSG3}, {"cNoise", SND_cNoise},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(name, table[i].name) == 0)
            return table[i].value;
    return -1;
}

// Jumps to an existing named block ("jumpTo"/"smpsCall"/"cfJumpTo"/etc.
// targets, and smpsLoop/smpsJump's INLINE bodies once folded to a
// synthetic name -- see below) -- a real PJValue* pointer, resolved fresh
// via pj_object_get each time (playlists are small, tens of blocks; not
// worth a persistent cache yet, see the plan's own "TBD" note).
static const PJValue *JsonResolveBlock(const PJValue *playlist, const char *name) {
    return pj_object_get(playlist, name);
}

// JSON-engine equivalent of LoadMusic below -- reads the same schema's
// "header" array (smpsHeaderChan/Tempo/DAC/FM/PSG) but self-describing
// rather than count-encoded: each smpsHeaderFM/PSG entry's own PRESENCE is
// what populates that channel, not a derived fm_count-1/fm_count==7 trick
// (that trick existed only to make the real hardware's fixed-size binary
// header layout work; JSON has no such constraint). `song` is a fully
// parsed, owned PJValue tree -- this function takes ownership (frees the
// chip set's previous one first, if any -- the actual mechanism fixing
// "music leaks", see the plan's own Context section).
static void LoadMusicJSON(SoundChipSet *cs, PJValue *song, uint8_t music_id, uint8_t driver_version) {
    // smpsPSGform ($E7 etc.) redirects the noise generator's own mode
    // (shift rate/FB, see SN76489.c) -- a chip-wide resource, not per-
    // channel, and real hardware never actually guarantees it resets
    // between songs either. But unlike real hardware (which re-applies a
    // per-track-stored noise byte whenever a track regains control of its
    // channel -- see s1.sounddriver.asm's cfSetPSGNoise/StopSpecialSFX),
    // nothing here was re-asserting it, so once ANY song/SFX used PSGform,
    // every later song/SFX that also touches the noise channel without its
    // own PSGform inherited that stale sync-mode setting -- audible as an
    // unrelated sound now ringing/tracking a tone channel's pitch instead
    // of playing clean noise. Reset to the default (periodic, lowest rate)
    // whenever a fresh song loads; if it wants something else, its own
    // event stream sets that moments later during normal playback.
    SN76489_Write(&cs->psg, 0xE0);
    if (cs->json_song)
        pj_free(cs->json_song);
    cs->json_song = song;
    cs->json_voices = pj_object_get(song, "voices");
    cs->json_playlist = pj_object_get(song, "SMPSplaylist");
    cs->driver_version = driver_version;
    cs->spindash_rev = 0;
    cs->halt_flag = 0;
    cs->continuous_sfx_flag = 0;
    cs->cont_sfx_loop_cnt = 0;
    cs->fade_to_prev_flag = 0;
    cs->dac_pitch_override_sample = -1;
    cs->dac_timpani_variant = -1;
    cs->main_tempo = 1; // overwritten by smpsHeaderTempo below if present

    const char *dac_loc = NULL;
    int dac_pitch = 0, dac_vol = 0, has_dac = 0;
    const char *fm_loc[6] = {0};
    int fm_pitch[6] = {0}, fm_vol[6] = {0}, fm_seen = 0;
    const char *psg_loc[SOUND_CHANNELS_PSG] = {0};
    int psg_pitch[SOUND_CHANNELS_PSG] = {0}, psg_vol[SOUND_CHANNELS_PSG] = {0}, psg_voice[SOUND_CHANNELS_PSG] = {0};
    int psg_seen = 0;

    const PJValue *header = pj_object_get(song, "header");
    for (size_t i = 0; i < pj_array_size(header); i++) {
        const PJValue *entry = pj_array_get(header, i);
        const char *macro = pj_object_first_key(entry);
        if (!macro)
            continue;
        const PJValue *args = pj_object_get(entry, macro);
        if (strcmp(macro, "smpsHeaderTempo") == 0) {
            cs->duration_mult = (uint8_t)JsonUnhexInt(pj_array_get(args, 0), 0);
            cs->main_tempo = (uint8_t)JsonUnhexInt(pj_array_get(args, 1), 0);
        } else if (strcmp(macro, "smpsHeaderDAC") == 0) {
            dac_loc = pj_get_string(pj_array_get(args, 0), "");
            if (pj_array_size(args) > 1) {
                dac_pitch = JsonUnhexInt(pj_array_get(args, 1), 0);
                dac_vol = JsonUnhexInt(pj_array_get(args, 2), 0);
            }
            has_dac = 1;
        } else if (strcmp(macro, "smpsHeaderFM") == 0 && fm_seen < 6) {
            fm_loc[fm_seen] = pj_get_string(pj_array_get(args, 0), "");
            fm_pitch[fm_seen] = JsonUnhexInt(pj_array_get(args, 1), 0);
            fm_vol[fm_seen] = JsonUnhexInt(pj_array_get(args, 2), 0);
            fm_seen++;
        } else if (strcmp(macro, "smpsHeaderPSG") == 0 && psg_seen < SOUND_CHANNELS_PSG) {
            psg_loc[psg_seen] = pj_get_string(pj_array_get(args, 0), "");
            psg_pitch[psg_seen] = JsonUnhexInt(pj_array_get(args, 1), 0);
            psg_vol[psg_seen] = JsonUnhexInt(pj_array_get(args, 2), 0);
            // arg 3 (modulation scratch byte) intentionally unused -- see
            // LoadMusic's own comment on the equivalent byte-format field.
            const PJValue *voicev = pj_array_get(args, 4);
            const char *vs = pj_get_string(voicev, "");
            const char *underscore = strrchr(vs, '_');
            psg_voice[psg_seen] = underscore ? (int)strtol(underscore + 1, NULL, 16) : JsonUnhexInt(voicev, 0);
            psg_seen++;
        }
    }
    cs->psg_count = (uint8_t)psg_seen;
    cs->current_music_id = music_id;
    cs->base_main_tempo = cs->main_tempo;
    cs->tempo_timeout = cs->main_tempo;

    const PJValue *playlist = cs->json_playlist;
    const PJValue *voices = cs->json_voices;
    if (has_dac && dac_loc) {
        StartChannelJSON(cs, &cs->channels[SOUND_CHANNEL_DAC], playlist, voices, JsonResolveBlock(playlist, dac_loc),
                          (int8_t)dac_pitch, (uint8_t)dac_vol);
        cs->dac_pan = 0xC0;
    } else {
        ResetChannel(&cs->channels[SOUND_CHANNEL_DAC]);
    }
    for (int i = 0; i < 6; i++) {
        SoundChannel *ch = &cs->channels[SOUND_CHANNEL_FM_BASE + i];
        if (i < fm_seen)
            StartChannelJSON(cs, ch, playlist, voices, JsonResolveBlock(playlist, fm_loc[i]), (int8_t)fm_pitch[i],
                              (uint8_t)fm_vol[i]);
        else
            ResetChannel(ch);
    }
    for (int i = 0; i < SOUND_CHANNELS_PSG; i++) {
        SoundChannel *ch = &cs->channels[SOUND_CHANNEL_PSG_BASE + i];
        if (i < psg_seen) {
            StartChannelJSON(cs, ch, playlist, voices, JsonResolveBlock(playlist, psg_loc[i]), (int8_t)psg_pitch[i],
                              (uint8_t)psg_vol[i]);
            ch->voice_index = (uint8_t)psg_voice[i];
            if (i == 3)
                ch->psg_noise = 1; // dedicated 4th-slot noise channel, same as LoadMusic's own comment
        } else {
            ResetChannel(ch);
        }
    }
    cs->dac_playing = 0;
}

// JSON-engine equivalent of LoadSFX below -- smpsHeaderTempoSFX/ChanSFX/
// SFXChannel instead of Tempo/Chan/DAC/FM/PSG. Same "populate only the
// channels a real smpsHeaderSFXChannel entry names" self-describing
// pattern as LoadMusicJSON. Unlike music, an SFX must NOT reset channels
// it doesn't itself target (see LoadSFX's own comment) -- this doesn't
// touch cs->json_song at all, since SFX trees are cached independently by
// the caller (Sound_DebugLoadSFXJSON), not owned per-chip-set the way a
// single "current song" is.
static void LoadSFXJSON(SoundChipSet *cs, const PJValue *song, const PJValue *playlist, uint8_t driver_version) {
    // See LoadMusicJSON's own comment on this -- same stale noise-control-
    // register leak applies to SFX (arguably more visibly, since SFX play
    // back to back far more often than songs load). Chip-wide, not a
    // per-channel reset, so it doesn't conflict with this function's own
    // "don't reset channels this SFX doesn't target" rule below.
    SN76489_Write(&cs->psg, 0xE0);
    cs->driver_version = driver_version;
    const PJValue *voices = pj_object_get(song, "voices");
    const PJValue *header = pj_object_get(song, "header");
    for (size_t i = 0; i < pj_array_size(header); i++) {
        const PJValue *entry = pj_array_get(header, i);
        const char *macro = pj_object_first_key(entry);
        if (!macro)
            continue;
        const PJValue *args = pj_object_get(entry, macro);
        if (strcmp(macro, "smpsHeaderTempoSFX") == 0) {
            cs->duration_mult = (uint8_t)JsonUnhexInt(pj_array_get(args, 0), 0);
            cs->main_tempo = 0;
            cs->tempo_timeout = 0;
        } else if (strcmp(macro, "smpsHeaderSFXChannel") == 0) {
            int chanid = JsonChannelIdLookup(pj_get_string(pj_array_get(args, 0), ""));
            int idx = chanid >= 0 ? SFXChannelIndex((uint8_t)chanid) : -1;
            if (idx < 0)
                continue;
            const char *loc = pj_get_string(pj_array_get(args, 1), "");
            int pitch = JsonUnhexInt(pj_array_get(args, 2), 0);
            int vol = JsonUnhexInt(pj_array_get(args, 3), 0);
            StartChannelJSON(cs, &cs->channels[idx], playlist, voices, JsonResolveBlock(playlist, loc), (int8_t)pitch,
                              (uint8_t)vol);
        }
    }
}

static void LoadMusic(SoundChipSet *cs, const uint8_t *song, uint8_t music_id, uint8_t driver_version) {
    SN76489_Write(&cs->psg, 0xE0); // see LoadMusicJSON's comment on this (byte-VM equivalent)
    const uint8_t *p = song;
    uint16_t voice_off = ReadWord(p);
    p += 2;
    const uint8_t *voice_bank = song + voice_off;
    cs->driver_version = driver_version;
    cs->spindash_rev = 0;
    cs->halt_flag = 0;
    cs->continuous_sfx_flag = 0;
    cs->cont_sfx_loop_cnt = 0;
    cs->fade_to_prev_flag = 0;
    // A DAC percussion override ($88-$91, see the coordination-flag switch
    // below) only ever gets set by a song's own DAC track and is never
    // implicitly cleared by anything else -- without resetting it here, a
    // new song's plain kick/snare notes silently keep playing whatever
    // sample (e.g. bongo/timpani variant) the PREVIOUS song last selected,
    // since this song's own DAC track has no reason to re-select the
    // default it never knew was overridden.
    cs->dac_pitch_override_sample = -1;
    cs->dac_timpani_variant = -1;
    uint8_t fm_count = *p++;
    uint8_t psg_count = *p++;
    cs->psg_count = psg_count; // Cached for $F3's "only if psg_count<4" gate
    cs->duration_mult = *p++;
    cs->main_tempo = *p++;
    cs->base_main_tempo = cs->main_tempo; // Cached so SlowDownMusic can restore it
    cs->current_music_id = music_id;      // For SpeedUpMusic's per-song SpeedUpIndex lookup
    cs->tempo_timeout = cs->main_tempo;

    // The byte stream's DAC/FM region always has exactly fm_count total
    // 4-byte blocks: the FIRST is always the DAC block (word ptr + pitch +
    // vol), matching the real driver's FMDACInitBytes table (DAC's control
    // byte is literally the first entry, loaded on the loop's first pass);
    // the remaining fm_count-1 blocks are FM1..FM(fm_count-1) in order.
    // fm_count==7 is not a different layout -- it's the same DAC-first
    // rule, just with all 6 FM blocks also present. Real songs that don't
    // want DAC output in that mode encode the DAC block's own data as a
    // bare smpsStop by convention (see Mus89_Special_Stage); the real
    // driver additionally clears the YM2612's $2B DAC-enable register as a
    // hardware-level safety net, which our independent software DAC mixer
    // doesn't need to model since it never shares state with FM6.
    // fm_count==0 means there's no DAC-position block in the stream at
    // all -- not even one. The whole FM/DAC region is only present when
    // fm_count>0 (this is what "0000" -- an empty voice-bank-only song --
    // means: skip straight to PSG parsing, no read here at all).
    if (fm_count > 0) {
        uint16_t dac_off = ReadWord(p);
        p += 2;
        int8_t dac_pitch = (int8_t)*p++;
        uint8_t dac_vol = *p++;
        StartChannel(cs, &cs->channels[SOUND_CHANNEL_DAC], song, song + dac_off, dac_pitch, dac_vol, voice_bank);
        cs->dac_pan = 0xC0; // Default: full L+R, until/unless the DAC track's own $E0 changes it
    } else {
        ResetChannel(&cs->channels[SOUND_CHANNEL_DAC]);
    }

    // FM6 only gets real track data when fm_count==7 (matching the real
    // driver's loop, which only reaches FM6's slot on a 7th pass); for
    // any fm_count<7 it's explicitly force-silenced here, same as the real
    // driver's .silencefm6 register writes for every non-7 fm_count value
    // (not just fm_count==6).
    int real_fm_blocks = (fm_count > 0) ? fm_count - 1 : 0;
    for (int i = 0; i < 6; i++) {
        SoundChannel *ch = &cs->channels[SOUND_CHANNEL_FM_BASE + i];
        if (i < real_fm_blocks) {
            uint16_t off = ReadWord(p);
            p += 2;
            int8_t pitch = (int8_t)*p++;
            uint8_t vol = *p++;
            StartChannel(cs, ch, song, song + off, pitch, vol, voice_bank);
        } else {
            ResetChannel(ch);
        }
    }

    for (int i = 0; i < SOUND_CHANNELS_PSG; i++) {
        SoundChannel *ch = &cs->channels[SOUND_CHANNEL_PSG_BASE + i];
        if (i < psg_count) {
            uint16_t off = ReadWord(p);
            p += 2;
            int8_t pitch = (int8_t)*p++;
            uint8_t vol = *p++;
            uint8_t mod = *p++;
            uint8_t voice = *p++;
            StartChannel(cs, ch, song, song + off, pitch, vol, voice_bank);
            (void)mod; // Real driver reads this byte into a scratch register and never stores it anywhere -- the
                       // envelope index always starts wherever it was left (immediately overwritten by the first
                       // real note/rest/bare-duration event's own reset anyway, see FinishTrackUpdate).
            ch->voice_index = voice;
            // The 4th PSG slot (psg_count==4) is a dedicated hardware noise
            // channel, not a redirectable tone channel -- its own note data
            // always drives SN76489 channel 3's noise control, same as a
            // $F3-redirected PSG3 track, just without needing the $F3 flag
            // (which is why $F3 is a no-op whenever psg_count==4: this slot
            // already covers it).
            if (i == 3)
                ch->psg_noise = 1;
            SOUND_TRACE("psg%d: off=$%04X first bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n", i, off,
                        ch->data_ptr[0], ch->data_ptr[1], ch->data_ptr[2], ch->data_ptr[3], ch->data_ptr[4],
                        ch->data_ptr[5], ch->data_ptr[6], ch->data_ptr[7]);
        } else {
            ResetChannel(ch);
        }
    }

    cs->dac_playing = 0; // any DPCM sample mid-playback belongs to the old song
}

static int SFXChannelIndex(uint8_t chanid) {
    switch (chanid) {
        case SND_cPSG1: return SOUND_CHANNEL_PSG_BASE + 0;
        case SND_cPSG2: return SOUND_CHANNEL_PSG_BASE + 1;
        case SND_cPSG3: return SOUND_CHANNEL_PSG_BASE + 2;
        case SND_cNoise: return SOUND_CHANNEL_PSG_BASE + 3; // the extra 4th PSG slot -- see Sound.h
        case SND_cFM3: return SOUND_CHANNEL_FM_BASE + 2;
        case SND_cFM4: return SOUND_CHANNEL_FM_BASE + 3;
        case SND_cFM5: return SOUND_CHANNEL_FM_BASE + 4;
        default: return -1;
    }
}

// SFX only ever occupy a handful of channels (never all of them) -- unlike
// music, a new SFX must NOT reset channels it doesn't itself target, since
// those may belong to whatever's already playing (e.g. music sharing the
// chip set, or another SFX's other channels).
static void LoadSFX(SoundChipSet *cs, const uint8_t *song, uint8_t driver_version) {
    SN76489_Write(&cs->psg, 0xE0); // see LoadMusicJSON's comment on this (byte-VM equivalent)
    const uint8_t *p = song;
    uint16_t voice_off = ReadWord(p);
    p += 2;
    const uint8_t *voice_bank = song + voice_off;
    cs->driver_version = driver_version;
    // SFX headers only carry a dividing-timing (duration multiplier) byte
    // -- no main_tempo/periodic-correction byte at all, so SFX playback
    // just runs at an uncorrected 1:1 frame rate scaled by that multiplier
    // (main_tempo=0 disables the periodic +1 correction in TickChipSet).
    cs->duration_mult = *p++;
    cs->main_tempo = 0;
    cs->tempo_timeout = 0;
    uint8_t chan_count = *p++;

    for (int i = 0; i < chan_count; i++) {
        p++; // marker byte, always 0x80
        uint8_t chanid = *p++;
        uint16_t off = ReadWord(p);
        p += 2;
        int8_t pitch = (int8_t)*p++;
        uint8_t vol = *p++;

        int idx = SFXChannelIndex(chanid);
        if (idx < 0)
            continue;
        StartChannel(cs, &cs->channels[idx], song, song + off, pitch, vol, voice_bank);
    }
}

void QueueSound1(uint8_t id) { sound_music.queue[SOUND_QUEUE_NORMAL] = id; }
void QueueSound2(uint8_t id) { sound_sfx.queue[SOUND_QUEUE_SPECIAL] = id; }

static void DispatchQueue(SoundChipSet *cs, int slot) {
    uint8_t id = cs->queue[slot];
    if (id == 0)
        return;
    cs->queue[slot] = 0;

    const uint8_t *song = sound_table[id];
    if (!song)
        return; // unmapped ID (a Snd*/Mus* file this project doesn't have, or a driver-2-only ID)

    uint8_t driver_version = sound_table_driver_ver[id];
    if (id >= SOUND_ID_MUSIC_FIRST && id <= SOUND_ID_MUSIC_LAST)
        LoadMusic(cs, song, id, driver_version);
    else
        LoadSFX(cs, song, driver_version);
}

// ---------------------------------------------------------------------
// High-level API: PlayMusic/PlaySound, special $F0-$F4 commands, pause
// ---------------------------------------------------------------------

#define SOUND_ID_SFX_FIRST     sfx_Jump
#define SOUND_ID_SFX_LAST      sfx_Signpost
// Continuous SFX (Sound.h's SOUND_ID_CONTINUOUS_SFX_FIRST/LAST enum members)
// sits between SFX_LAST and SPECIAL_FIRST -- reserved, empty range, see
// that enum's own comment. Nothing here needs to reference it directly;
// sound_priorities/PlaySound's own range checks span straight from
// SFX_FIRST to SPECIAL_LAST and rely on the (currently zero-width) gap
// simply contributing no IDs of its own.
#define SOUND_ID_SPECIAL_FIRST sfx_Waterfall
#define SOUND_ID_SPECIAL_LAST  sfx_Waterfall
#define SOUND_ID_FLAG_FIRST    bgm_Fade
#define SOUND_ID_FLAG_LAST     bgm_Stop

// Matches SoundPriorities: higher wins. Indexed by id - SOUND_ID_SFX_FIRST,
// covering regular SFX through the (currently empty) continuous-SFX range
// to special SFX -- purely a numeric priority-arbitration table, one byte
// per sound ID, mechanically transcribed. Content unchanged by the ID
// renumbering (same 49 values, same relative order -- only the base ID
// each position corresponds to shifted).
static const uint8_t sound_priorities[SOUND_ID_SPECIAL_LAST - SOUND_ID_SFX_FIRST + 1] = {
    0x80, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x68, 0x70, 0x70, 0x70, 0x60, 0x70, // sfx_Jump+
    0x70, 0x60, 0x70, 0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x7F, // (cont.)
    0x60, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, // (cont.)
    0x80,                                                                                           // sfx_Waterfall
};

void PlayMusic(uint8_t id) {
    if (id == 0) {
        // Silence sentinel (real hardware's own $80 -- now just plain 0,
        // same value as "nothing queued", see Sound.h's enum comment) --
        // treat like a stop rather than queuing.
        StopAllSound();
        return;
    }
    QueueSound1(id);
}

void StopAllSound(void) {
    memset(&sound_music.channels, 0, sizeof(sound_music.channels));
    memset(&sound_sfx.channels, 0, sizeof(sound_sfx.channels));
    sound_music.dac_playing = 0;
    sound_sfx.dac_playing = 0;
    // Deliberately NOT touching pcm_playing here: on real hardware, the
    // Z80 busy-waits on the "SEGA!" clip, so any command sent from the 68K
    // side (including a title-screen StopAllSound) effectively queues up
    // behind it and only takes effect once the Z80 gets back to checking
    // for new commands -- the clip always finishes playing regardless of
    // what the 68K does next. This port has no such serial blocking, so
    // letting StopAllSound() leave an in-progress PCM clip alone is what
    // reproduces that behavior instead.
    sound_sfx.current_priority = 0;
    for (int i = 0; i < 3; i++)
        PSG_SetAttenuation(&sound_music.psg, i, 0x0F);
    for (int i = 0; i < 4; i++)
        PSG_SetAttenuation(&sound_sfx.psg, i, 0x0F);
}

void FadeOutMusic(void) {
    // TODO: real behavior ramps volume down over several frames via a
    // dedicated fade coordination step; this just cuts it immediately.
    memset(&sound_music.channels, 0, sizeof(sound_music.channels));
    sound_music.dac_playing = 0;
    for (int i = 0; i < 3; i++)
        PSG_SetAttenuation(&sound_music.psg, i, 0x0F);
}

void PlaySegaSound(void) {
    // Real hardware plays this on sound_sfx's physical DAC, exclusively --
    // it busy-waits the Z80 for the clip's duration so nothing else can use
    // the DAC meanwhile. We don't need the busy-wait (this just triggers
    // playback and returns; PCM_Generate steps it per-frame), but route it
    // through sound_sfx to match which chip set owns it.
    PlaySegaSound_Trigger(&sound_sfx);
}

// Real driver swaps in a separate stored "speedup tempo" constant here,
// not derived from the song's own main_tempo -- approximated by nudging
// main_tempo instead, since we don't track that separate constant.
// Higher main_tempo = fewer periodic +1 corrections = faster playback.
// SpeedUpIndex: real per-song replacement main_tempo values used while
// speed shoes are active, indexed by (music_id - bgm_GHZ). Only the first
// 8 songs (GHZ..ExtraLife) have real entries in the original driver -- for
// any song past that, the real table read continues past SpeedUpIndex's
// end into MusicIndex's own pointer bytes (an actual out-of-bounds read on
// real hardware). That's not something worth reproducing in a
// memory-safe port, so SpeedUpMusic() just leaves main_tempo unchanged
// for those songs instead of reading garbage.
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

void SpeedUpMusic(void) {
    int index = sound_music.current_music_id - bgm_GHZ;
    if (index < 0 || index >= (int)(sizeof(speedup_index) / sizeof(speedup_index[0])))
        return; // Out of SpeedUpIndex's real range -- leave tempo alone (see comment above)
    sound_music.main_tempo = speedup_index[index];
    if (sound_music.tempo_timeout > sound_music.main_tempo)
        sound_music.tempo_timeout = sound_music.main_tempo;
}

void SlowDownMusic(void) {
    sound_music.main_tempo = sound_music.base_main_tempo;
    if (sound_music.tempo_timeout > sound_music.main_tempo)
        sound_music.tempo_timeout = sound_music.main_tempo;
}

void Sound_Pause(void) {
    sound_music.paused = 1;
    // Real hardware's own PauseMusic doesn't just freeze the tempo governor --
    // it explicitly key-offs every FM channel (reg $28) and silences every
    // PSG channel before halting (s1.sounddriver.asm). Without this, whatever
    // note was sounding at the moment of pause just keeps ringing indefinitely
    // (TickChipSet returns immediately while paused, so nothing ever reaches
    // the note's own natural release/duration-out). SFX (sound_sfx) are
    // intentionally left untouched -- this project keeps sound_music and
    // sound_sfx as fully independent chip sets (see Sound.h), so "pause
    // music" only ever needs to reach sound_music's own channels.
    for (int i = 0; i < 6; i++) {
        SoundChannel *ch = &sound_music.channels[SOUND_CHANNEL_FM_BASE + i];
        ch->key_on = 0;
        FM_KeyOnOff(sound_music.fm, SOUND_CHANNEL_FM_BASE + i, 0);
    }
    for (int i = 0; i < 4; i++)
        PSG_SetAttenuation(&sound_music.psg, i, 0x0F);
}
void Sound_Resume(void) { sound_music.paused = 0; }

void PlaySound(uint8_t id) {
    if (id == 0) {
        StopAllSound();
        return;
    }

    if (id >= SOUND_ID_FLAG_FIRST && id <= SOUND_ID_FLAG_LAST) {
        switch (id) {
            case bgm_Fade: FadeOutMusic(); break;
            case sfx_Sega: PlaySegaSound(); break;
            case bgm_Speedup: SpeedUpMusic(); break;
            case bgm_Slowdown: SlowDownMusic(); break;
            case bgm_Stop: StopAllSound(); break;
        }
        return;
    }

    if (id >= SOUND_ID_MUSIC_FIRST && id <= SOUND_ID_MUSIC_LAST) {
        PlayMusic(id);
        return;
    }

    if (id < SOUND_ID_SFX_FIRST || id > SOUND_ID_SPECIAL_LAST)
        return; // Not a valid ID (matches the real driver's silent reject)

    uint8_t priority = sound_priorities[id - SOUND_ID_SFX_FIRST];
    if (priority < sound_sfx.current_priority)
        return; // Lower priority than what's already committed to play -- ignored
    sound_sfx.current_priority = priority;
    QueueSound2(id);
}

// ---------------------------------------------------------------------
// Per-channel byte-stream interpreter
// ---------------------------------------------------------------------

// -1 for FM/DAC channels (no PSG register writes -- ymfm isn't wired up
// yet, so those channels' data still advances correctly for timing/looping
// purposes, they just don't make sound).
static int PSGRegisterChannel(int channel_index) {
    if (channel_index >= SOUND_CHANNEL_PSG_BASE && channel_index < SOUND_CHANNEL_PSG_BASE + 4)
        return channel_index - SOUND_CHANNEL_PSG_BASE;
    return -1;
}

static void SilenceIfPSG(SN76489 *psg, int psg_chan, int is_noise) {
    if (psg_chan >= 0)
        PSG_SetAttenuation(psg, psg_chan, 0x0F);
    // A noise-redirected track (smpsPSGform) writes its own note frequencies
    // to physical channel 2 (PSG3's tone period doubles as the noise
    // generator's "sync to tone 2" pitch source -- see PSG_SetTonePeriod's
    // own ch->psg_noise special-case), but every attenuation/volume write
    // for such a track -- including this one -- only ever targeted the
    // redirected noise channel (psg_chan, already 3 here). Channel 2's own
    // attenuation register was never touched by a noise-mode track at all,
    // so whatever it was last left at just kept ringing at that track's
    // final frequency indefinitely, with nothing in the track's own
    // lifecycle (not even its own smpsStop) ever able to silence it.
    if (is_noise)
        PSG_SetAttenuation(psg, 2, 0x0F);
}

// Real driver's FinishTrackUpdate: every track update that isn't a
// smpsNoAttack ($E7) re-arms modulation from its own stored raw parameter
// bytes, but only if modulation is currently active ($F0 auto-enables it,
// $F4 turns it back off without forgetting the parameters).
static void ResetModulationIfActive(SoundChannel *ch) {
    if (!ch->mod_active || !ch->modulation_ptr)
        return;
    const uint8_t *p = ch->modulation_ptr;
    ch->modulation_wait = p[0];
    ch->modulation_speed = p[1];
    ch->modulation_delta = (int8_t)p[2];
    ch->modulation_steps = (uint8_t)(p[3] >> 1); // halved, same as $F0's own initial store
    ch->modulation_val = 0;
}

// Real driver's DoModulation: a triangle-wave stepper gated by a per-note
// wait, then a re-arming speed countdown, then step count before flipping
// direction. Note the asymmetry versus ResetModulationIfActive above: the
// *first* half-cycle after a note-on uses the halved step count, but every
// later reload here uses the raw, unhalved byte straight from the stream --
// that's not a bug, it's what the real disassembly does (loc_71DFE reads
// 3(a0) directly, never re-halving it).
static void StepModulation(SoundChannel *ch) {
    if (!ch->mod_active || !ch->modulation_ptr)
        return;
    if (ch->modulation_wait > 0) {
        ch->modulation_wait--;
        return;
    }
    if (--ch->modulation_speed != 0)
        return;
    const uint8_t *p = ch->modulation_ptr;
    ch->modulation_speed = p[1];
    if (ch->modulation_steps != 0) {
        ch->modulation_steps--;
        ch->modulation_val = (int16_t)(ch->modulation_val + ch->modulation_delta);
    } else {
        ch->modulation_steps = p[3];
        ch->modulation_delta = (int8_t)(-ch->modulation_delta);
    }
}

static int debug_isolate_voice = -1; // see Sound_DebugIsolateVoice's own comment in Sound.h

// Driver-version-3 (Sonic 3/Flamedriver-compatible) coordination-flag
// dispatch -- called from TickChannel in place of the driver-version-1
// switch below when cs->driver_version>=3, for exactly one flag byte `b`
// (already consumed from ch->data_ptr by the caller). Byte values and
// semantics come from Flamedriver.asm's zCoordFlagSwitchTable/
// zExtraCoordFlagSwitchTable (see the SMPS driver-version-3 plan for the
// full researched table) -- genuinely different assignments than the
// driver-version-1 switch below for most of this range, which is why this
// is a fully separate function rather than extra cases merged into that one.
//
// Three flags are documented, correct-but-inert stubs (byte stream parsed
// exactly right, state stored where sensible, zero synthesis effect) rather
// than real implementations, because the underlying feature genuinely
// doesn't exist in this engine yet, not because it was skipped:
// cfSetSSGEG (fmcore has no SSG-EG register model), cfFM3SpecialMode
// (fmcore has no per-operator-frequency-within-one-channel concept), and
// cfCopyData (this engine's track data is `const uint8_t *`, real
// hardware's self-modifying trick has no equivalent here). cfFadeInToPrevious
// is a fourth stub, but that's consistent with driver-version-1's own
// smpsFade/FadeOutMusic() -- real crossfade was never implemented for
// EITHER table.
static void TickChannel_FlagsV3(SoundChipSet *cs, SoundChannel *ch, int channel_index, int is_fm, int is_dac,
                                 int psg_chan, int psg_reg_chan, uint8_t b) {
    switch (b) {
        case 0xE0: { // cfPanningAMSFMS -- FM and DAC; no-op for PSG (can't set panning), same as v1's $E0
            uint8_t v = *ch->data_ptr++;
            if (is_fm)
                FM_SetPan(cs->fm, channel_index, v);
            else if (is_dac)
                cs->dac_pan = v & 0xC0;
            break;
        }
        case 0xE1: // cfDetune -- absolute set (distinct from Transpose; see the detune-aware
                   // frequency calc this flag feeds, added alongside modulation_val above)
            ch->detune = (int8_t)*ch->data_ptr++;
            break;
        case 0xE2: // cfFadeInToPrevious -- stored, not acted on (documented stub, see this function's own comment)
            cs->fade_to_prev_flag = *ch->data_ptr++;
            break;
        case 0xE3: // cfSilenceStopTrack -- silences then stops, same end state as cfStopTrack ($F2) below
            ch->active = 0;
            SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
            if (is_fm)
                FM_KeyOnOff(cs->fm, channel_index, 0);
            if (is_dac)
                cs->dac_playing = 0;
            return;
        case 0xE4: { // cfSetVolume -- absolute set. "cpl" (one's complement) matches Flamedriver's own encoding.
            uint8_t raw = *ch->data_ptr++;
            if (is_fm) {
                ch->volume = (uint8_t)(~raw & 0x7F);
                FM_ApplyVolume(cs->fm, ch, channel_index);
            } else if (psg_chan >= 0) {
                ch->volume = (uint8_t)((~raw >> 3) & 0x0F);
            }
            break;
        }
        case 0xE5: { // cfChangeVolume2 -- discards the 1st byte, 2nd is the real delta; same effect as cfChangeVolume, FM only
            ch->data_ptr++; // discarded byte
            int8_t delta = (int8_t)*ch->data_ptr++;
            if (is_fm) {
                int v = (int)ch->volume + delta;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > 0x7F ? 0x7F : v));
                FM_ApplyVolume(cs->fm, ch, channel_index);
            }
            break;
        }
        case 0xE6: { // cfChangeVolume -- FM only, effective immediately
            int8_t delta = (int8_t)*ch->data_ptr++;
            if (is_fm) {
                int v = (int)ch->volume + delta;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > 0x7F ? 0x7F : v));
                FM_ApplyVolume(cs->fm, ch, channel_index);
            }
            break;
        }
        // 0xE7 (cfPreventAttack) is handled earlier, unconditionally, shared
        // with v1's smpsNoAttack -- see that code's own comment.
        case 0xE8: // cfNoteFill -- same field/semantics as v1's $E8
            ch->note_timeout_master = *ch->data_ptr++;
            break;
        case 0xE9: // cfSpindashRev -- adds a global escalating counter into Transpose, ticks the counter at exactly +0x10
            ch->transpose = (int8_t)(ch->transpose + cs->spindash_rev);
            if (ch->transpose == 0x10)
                cs->spindash_rev++;
            break;
        case 0xEA: // cfPlayDACSample -- fires a DAC hit from a non-DAC track, same trigger path the DAC track's own note bytes use
            DAC_TriggerByNoteByte(cs, *ch->data_ptr++);
            break;
        case 0xEB: { // cfConditionalJump -- shared loop_counters[] pool with cfRepeatAtPos ($F7) below
            uint8_t idx = *ch->data_ptr++;
            const uint8_t *word_pos = ch->data_ptr;
            int16_t rel = (int16_t)ReadWord(word_pos);
            const uint8_t *target = word_pos + 1 + rel;
            ch->data_ptr = word_pos + 2;
            if (idx < 3) {
                if (ch->loop_counters[idx] > 0)
                    ch->loop_counters[idx]--;
                if (ch->loop_counters[idx] == 0)
                    ch->data_ptr = target;
            }
            break;
        }
        case 0xEC: { // cfChangePSGVolume -- PSG only
            int8_t delta = (int8_t)*ch->data_ptr++;
            if (psg_chan >= 0) {
                if (ch->vol_env_index > 0)
                    ch->vol_env_index--;
                int v = (int)ch->volume + delta;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > 0x0F ? 0x0F : v));
            }
            break;
        }
        case 0xED: // cfSetKey -- absolute set, offset-encoded
            ch->transpose = (int8_t)(*ch->data_ptr++ - 0x40);
            break;
        case 0xEE: { // cfSendFMI -- always "part I" (port 0) regardless of which channel issued it -- a real hardware quirk (FM4-6 mostly unreachable through this flag), not a bug here
            uint8_t reg = *ch->data_ptr++;
            uint8_t data = *ch->data_ptr++;
            FM_WriteReg(cs->fm, 0, reg, data);
            break;
        }
        case 0xEF: { // cfSetVoice -- 2-byte form (foreign voice bank) not supported; low 7 bits still used as a same-bank fallback
            uint8_t v = *ch->data_ptr++;
            uint8_t idx = v & 0x7F;
            if (v & 0x80)
                ch->data_ptr++; // foreign-song voice-bank byte -- consumed, not acted on
            ch->voice_index = idx;
            if (is_fm && ch->voice_bank)
                FM_LoadVoice(cs->fm, ch, channel_index, ch->voice_bank + (size_t)idx * 25);
            break;
        }
        case 0xF0: // cfModulation -- identical shape/semantics to v1's $F0
            ch->modulation_ptr = ch->data_ptr;
            ch->modulation_wait = *ch->data_ptr++;
            ch->modulation_speed = *ch->data_ptr++;
            ch->modulation_delta = (int8_t)*ch->data_ptr++;
            ch->modulation_steps = (uint8_t)(*ch->data_ptr++ >> 1);
            ch->modulation_val = 0;
            ch->mod_active = 1;
            break;
        case 0xF1: { // cfAlterModulation -- PSG uses the 1st byte, FM the 2nd; approximated as an on/off set (see cfSetModulation's own comment)
            uint8_t b1 = *ch->data_ptr++;
            uint8_t b2 = *ch->data_ptr++;
            uint8_t chosen = (psg_chan >= 0) ? b1 : b2;
            ch->mod_active = (chosen != 0);
            break;
        }
        case 0xF2: // cfStopTrack
            ch->active = 0;
            SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
            if (is_fm)
                FM_KeyOnOff(cs->fm, channel_index, 0);
            if (is_dac)
                cs->dac_playing = 0;
            return;
        case 0xF3: { // cfSetPSGNoise -- PSG only; param 0 explicitly clears noise mode (v1's $F3 has no such clear case)
            uint8_t v = *ch->data_ptr++;
            if (psg_chan >= 0) {
                if (v == 0) {
                    ch->psg_noise = 0;
                    SilenceIfPSG(&cs->psg, 3, 1); // 1: leaving noise mode -- also clean up channel 2 (see SilenceIfPSG's own comment)
                } else if (psg_chan == 2 && cs->psg_count < 4) {
                    SN76489_Write(&cs->psg, v);
                    ch->psg_noise = 1;
                }
            }
            break;
        }
        case 0xF4: { // cfSetModulation -- absolute on/off set (this engine only models mod_active, not the fuller ModulationCtrl bitfield)
            uint8_t v = *ch->data_ptr++;
            ch->mod_active = (v != 0);
            break;
        }
        case 0xF5: { // cfSetPSGVolEnv -- PSG-only gate; real code falls through into a plain voice-index store despite the name
            uint8_t v = *ch->data_ptr++;
            if (psg_chan >= 0)
                ch->voice_index = v;
            break;
        }
        case 0xF6: { // cfJumpTo -- identical to v1's $F6
            const uint8_t *word_pos = ch->data_ptr;
            int16_t rel = (int16_t)ReadWord(word_pos);
            ch->data_ptr = word_pos + 1 + rel;
            break;
        }
        case 0xF7: { // cfRepeatAtPos -- same shared loop_counters[] pool/shape as v1's $F7 smpsLoop
            uint8_t idx = *ch->data_ptr++;
            uint8_t count = *ch->data_ptr++;
            const uint8_t *word_pos = ch->data_ptr;
            int16_t rel = (int16_t)ReadWord(word_pos);
            const uint8_t *target = word_pos + 1 + rel;
            ch->data_ptr = word_pos + 2;
            if (idx < 3) {
                if (ch->loop_counters[idx] == 0)
                    ch->loop_counters[idx] = count;
                ch->loop_counters[idx]--;
                if (ch->loop_counters[idx] != 0)
                    ch->data_ptr = target;
            }
            break;
        }
        case 0xF8: { // cfJumpToGosub -- identical to v1's $F8 smpsCall (see its own comment on the abs-vs-rel-1 fix)
            const uint8_t *word_pos = ch->data_ptr;
            uint16_t target_off = ReadWord(word_pos);
            const uint8_t *target = ch->song_base + target_off;
            if (ch->return_sp < 2)
                ch->return_stack[ch->return_sp++] = word_pos + 2;
            ch->data_ptr = target;
            break;
        }
        case 0xF9: // cfJumpReturn -- identical to v1's $E3 smpsReturn
            if (ch->return_sp > 0)
                ch->data_ptr = ch->return_stack[--ch->return_sp];
            break;
        case 0xFA: // cfDisableModulation
            ch->mod_active = 0;
            break;
        case 0xFB: // cfChangeTransposition -- relative add (contrast with cfSetKey's absolute set)
            ch->transpose = (int8_t)(ch->transpose + (int8_t)*ch->data_ptr++);
            break;
        case 0xFC: { // cfLoopContinuousSFX -- global (chip-set-wide) continuous-SFX state
            const uint8_t *word_pos = ch->data_ptr;
            int16_t rel = (int16_t)ReadWord(word_pos);
            const uint8_t *target = word_pos + 1 + rel;
            ch->data_ptr = word_pos + 2;
            if (cs->continuous_sfx_flag) {
                if (cs->cont_sfx_loop_cnt > 0)
                    cs->cont_sfx_loop_cnt--;
                if (cs->cont_sfx_loop_cnt == 0)
                    cs->continuous_sfx_flag = 0;
                ch->data_ptr = target;
            }
            break;
        }
        case 0xFD: // cfToggleAltFreqMode -- stored, not yet interpreted by the note-byte decoder (documented simplification: the
                   // raw-16-bit-frequency note encoding this enables is a distinct note-byte format this engine doesn't parse yet)
            ch->alt_freq_mode = (*ch->data_ptr++ == 1) ? 1 : 0;
            break;
        case 0xFE: // cfFM3SpecialMode -- documented stub, see this function's own comment
            ch->data_ptr += 4;
            break;
        case 0xFF: { // cfMetaCF -- extended table, dispatched by a 2nd (sub-index) byte
            uint8_t sub = *ch->data_ptr++;
            switch (sub) {
                case 0x00: // cfSetTempo
                    cs->main_tempo = *ch->data_ptr++;
                    break;
                case 0x01: // cfPlaySFXByIndex
                    PlaySound(*ch->data_ptr++);
                    break;
                case 0x02: { // cfHaltSound -- reuses the existing Sound_Pause/Resume tempo-freeze mechanism
                    uint8_t v = *ch->data_ptr++;
                    cs->halt_flag = v;
                    cs->paused = (v != 0);
                    break;
                }
                case 0x03: // cfCopyData -- documented stub, see this function's own comment
                    ch->data_ptr += 3;
                    break;
                case 0x04: { // cfSetTempoDivider -- global broadcast to every channel
                    uint8_t v = *ch->data_ptr++;
                    cs->duration_mult = v;
                    for (int i = 0; i < SOUND_CHANNELS; i++)
                        cs->channels[i].tempo_divider = v;
                    break;
                }
                case 0x05: // cfSetSSGEG -- documented stub, see this function's own comment
                    ch->data_ptr += 4;
                    break;
                case 0x06: // cfFMVolEnv -- see FMStepVolEnv, applied per-tick in TickChannel
                    ch->fm_vol_env_index = *ch->data_ptr++;
                    ch->fm_vol_env_mask = *ch->data_ptr++;
                    ch->fm_vol_env_pos = 0;
                    break;
                case 0x07: // cfResetSpindashRev
                    cs->spindash_rev = 0;
                    break;
                case 0x08: // cfChanSetTempoDivider -- current track only (contrast with $04's global broadcast)
                    ch->tempo_divider = *ch->data_ptr++;
                    break;
                case 0x09: { // cfChanFMCommand -- part I or II picked by the CURRENT channel (contrast with $EE's always-part-I)
                    uint8_t reg = *ch->data_ptr++;
                    uint8_t data = *ch->data_ptr++;
                    if (is_fm) {
                        int port, local;
                        FMPortChannel(channel_index, &port, &local);
                        FM_WriteReg(cs->fm, port, reg, data);
                    }
                    break;
                }
                case 0x0A: // cfNoteFillSet -- absolute set (no tempo-divider multiply), contrast with $E8's computed version
                    ch->note_timeout_master = *ch->data_ptr++;
                    ch->note_timeout = ch->note_timeout_master;
                    break;
                case 0x0B: { // cfPitchSlide
                    uint8_t v = *ch->data_ptr++;
                    ch->pitch_slide_active = (v != 0);
                    if (!v)
                        ch->detune = 0;
                    break;
                }
                case 0x0C: { // cfSetLFO -- LFO register is chip-wide (global), pan byte is per-track
                    uint8_t lfo_byte = *ch->data_ptr++;
                    uint8_t pan_byte = *ch->data_ptr++;
                    FM_WriteReg(cs->fm, 0, 0x22, lfo_byte);
                    if (is_fm)
                        FM_SetPan(cs->fm, channel_index, pan_byte);
                    else if (is_dac)
                        cs->dac_pan = pan_byte & 0xC0;
                    break;
                }
                case 0x0D: // cfPlayMusicByIndex
                    PlayMusic(*ch->data_ptr++);
                    break;
                default:
                    break; // Unknown extended sub-byte -- no further bytes can be safely skipped (no guaranteed length)
            }
            break;
        }
        default: ch->data_ptr++; break; // Unknown/reserved flag -- consume one arg byte defensively, same fallback as v1's own default case
    }
}

// ---------------------------------------------------------------------
// JSON-tree-walking playback engine (driver-version-1 events only, this
// pass -- see the "SMPS runtime: walk JSON directly" plan). Mirrors
// TickChannel/TickChannel_FlagsV3's own dispatch above almost exactly,
// reusing every FM/PSG helper those use, just reading PJValue nodes
// instead of stream bytes. Selected per chip set by cs->json_song != NULL
// (see TickChipSet).
// ---------------------------------------------------------------------

// Pushes a resume point (return from smpsCall, or fall through to after an
// inline smpsLoop/smpsJump body once it's done) -- see SoundChannel's own
// json_return_stack comment for why calls and inline-body falls-through
// share one stack.
static void JsonPushReturn(SoundChannel *ch, const PJValue *events, size_t index) {
    if (ch->json_return_sp < 4) {
        ch->json_return_stack[ch->json_return_sp].events = events;
        ch->json_return_stack[ch->json_return_sp].index = index;
        // Saves the CURRENT context's own in_jump_body state (the one being
        // paused, not the one about to start) so JsonPopReturn can restore
        // it correctly -- needed for e.g. an inline smpsLoop nested inside
        // an inline smpsJump body: once the inner loop's own repeat count
        // is exhausted and it pops back out, the outer context is still a
        // "wrap forever" jump body and needs that flag set again.
        ch->json_return_stack[ch->json_return_sp].in_jump_body = ch->json_in_jump_body;
        ch->json_return_stack[ch->json_return_sp].loop_idx = ch->json_loop_idx;
        ch->json_return_stack[ch->json_return_sp].loop_body_start = ch->json_loop_body_start;
        ch->json_return_sp++;
    }
}

static void JsonPopReturn(SoundChipSet *cs, SoundChannel *ch, int channel_index, int is_fm, int is_dac, int psg_reg_chan) {
    if (ch->json_return_sp > 0) {
        ch->json_return_sp--;
        ch->json_events = ch->json_return_stack[ch->json_return_sp].events;
        ch->json_event_index = ch->json_return_stack[ch->json_return_sp].index;
        ch->json_in_jump_body = ch->json_return_stack[ch->json_return_sp].in_jump_body;
        ch->json_loop_idx = ch->json_return_stack[ch->json_return_sp].loop_idx;
        ch->json_loop_body_start = ch->json_return_stack[ch->json_return_sp].loop_body_start;
    } else {
        // Stack underflow -- nothing to return to, matches real hardware
        // running off the end. Every other channel-terminating path
        // (smpsStop/cfStopTrack/smpsStopSpecial/smpsFade) explicitly keys
        // off and silences PSG before deactivating; this one was silently
        // skipping that, so a track that ends via a bare smpsReturn instead
        // of an explicit stop event left its last note ringing forever.
        ch->active = 0;
        SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
        if (is_fm)
            FM_KeyOnOff(cs->fm, channel_index, 0);
        if (is_dac)
            cs->dac_playing = 0;
    }
}

// Real hardware has no bounds checking on a track's data pointer -- a block
// with no terminating smpsStop/smpsJump/smpsReturn simply falls through
// into whatever bytes the compiler placed right after it, which for a
// top-level (non-nested) block is the NEXT block in the SMPSplaylist's own
// insertion order (compiler.c emits them in exactly that order -- see its
// own "Playlist block order comes directly from the object's own insertion
// order" comment). This is REAL, disassembly-confirmed content, not a
// conversion bug: SndB4_Bumper's FM4 track has no smpsStop after its rest
// and audibly falls through into its neighboring Jump00 block's own A4
// note on real hardware -- found via A/B testing against the byte-VM.
static const PJValue *JsonNextBlockAfter(const PJValue *playlist, const PJValue *events) {
    size_t n = pj_object_size(playlist);
    for (size_t i = 0; i < n; i++) {
        if (pj_object_value_at(playlist, i) == events)
            return (i + 1 < n) ? pj_object_value_at(playlist, i + 1) : NULL;
    }
    return NULL;
}

static void TickChannelJSON(SoundChipSet *cs, SoundChannel *ch, int channel_index) {
    int psg_chan = PSGRegisterChannel(channel_index);
    int psg_reg_chan = (psg_chan >= 0 && ch->psg_noise) ? 3 : psg_chan;
    int is_dac = (channel_index == SOUND_CHANNEL_DAC);
    int is_fm = (channel_index >= SOUND_CHANNEL_FM_BASE && channel_index < SOUND_CHANNEL_FM_BASE + 6);

    while (ch->active && ch->duration_timeout == 0) {
        if (!ch->json_events || ch->json_event_index >= pj_array_size(ch->json_events)) {
            if (ch->json_in_jump_body) {
                // Inline smpsJump body, exhausted -- "no count = forever"
                // per the schema: wrap back to its own start rather than
                // falling through/returning.
                ch->json_event_index = 0;
                continue;
            }
            if (ch->json_loop_idx >= 0) {
                // Inline smpsLoop body, exhausted -- decrement/check HERE
                // (see json_loop_idx's own comment in Sound.h for why not
                // on entry), matching the byte-VM's own $F7 sitting after
                // the body and deciding whether to jump back to repeat it.
                int idx = ch->json_loop_idx;
                int repeat = 0;
                if (idx < 3) {
                    ch->loop_counters[idx]--;
                    repeat = (ch->loop_counters[idx] != 0);
                }
                if (repeat) {
                    ch->json_event_index = ch->json_loop_body_start;
                    continue;
                }
                ch->json_loop_idx = -1; // exhausted -- fall through to JsonPopReturn below, same as any other resume point
            }
            // Ran off the end of the current array with no explicit stop.
            // At the top level (no pending call/loop-resume context), real
            // content can legitimately do this on purpose -- fall through
            // to the next playlist block, matching real hardware (see
            // JsonNextBlockAfter's own comment). Only at the top level: a
            // smpsCall'd/looped block running off the end still just
            // returns, matching the real driver's own return-address
            // mechanism for those cases.
            if (ch->json_return_sp == 0) {
                const PJValue *next = JsonNextBlockAfter(ch->json_playlist, ch->json_events);
                if (next) {
                    ch->json_events = next;
                    ch->json_event_index = 0;
                    continue;
                }
            }
            JsonPopReturn(cs, ch, channel_index, is_fm, is_dac, psg_reg_chan);
            if (!ch->active || !ch->json_events)
                return;
            continue;
        }
        const PJValue *event = pj_array_get(ch->json_events, ch->json_event_index++);

        // Bare-string zero-arg events.
        if (pj_type(event) == PJ_STRING) {
            const char *s = pj_get_string(event, "");
            if (strcmp(s, "smpsNoAttack") == 0) {
                // The compiler emits smpsNoAttack as a single opcode byte
                // with no argument of its own, but the byte-VM's runtime
                // handler PEEKS at whatever byte immediately follows in the
                // stream and, if <0x80, greedily consumes it as an inline
                // duration override -- all in the same dispatch/frame (same
                // trick a real note byte uses, see its own peek below). A
                // JSON "tie"/"inheritedNote" node always compiles to
                // exactly that kind of bare duration byte, so when one
                // immediately follows smpsNoAttack in the SAME array, it
                // has to be consumed here too, or the JSON engine spends an
                // extra frame reading it as its own separate event next
                // frame -- found via A/B testing (SndD0_Waterfall's
                // NoAttack+tie loop played at half real speed).
                const PJValue *next =
                    ch->json_event_index < pj_array_size(ch->json_events)
                        ? pj_array_get(ch->json_events, ch->json_event_index)
                        : NULL;
                if (next && pj_type(next) != PJ_STRING &&
                    (pj_object_has(next, "tie") || pj_object_has(next, "inheritedNote"))) {
                    int scaled = JsonUnhexInt(pj_object_get(next, "duration"), 0) * (int)ch->tempo_divider;
                    ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
                    ch->json_event_index++;
                }
                uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
                ch->duration_timeout = dur;
                ch->note_timeout = ch->note_timeout_master;
                ch->key_on = 1;
                return;
            }
            if (strcmp(s, "smpsStop") == 0 || strcmp(s, "smpsStopSpecial") == 0 || strcmp(s, "smpsFade") == 0) {
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                if (is_dac)
                    cs->dac_playing = 0;
                return;
            }
            if (strcmp(s, "smpsReturn") == 0) {
                JsonPopReturn(cs, ch, channel_index, is_fm, is_dac, psg_reg_chan);
                continue;
            }
            if (strcmp(s, "smpsClearPush") == 0 || strcmp(s, "smpsWeirdD1LRR") == 0)
                continue; // no-ops, same as the byte-VM
            continue; // unrecognized bare event -- skip rather than desync
        }

        // Note / tie / inheritedNote / rest.
        const PJValue *v;
        if ((v = pj_object_get(event, "note")) != NULL) {
            const char *name = pj_get_string(v, "");
            int raw = ps_note_value(name);
            const PJValue *durv = pj_object_get(event, "duration");
            if (durv) {
                int scaled = JsonUnhexInt(durv, 0) * (int)ch->tempo_divider;
                ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
            }
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;

            if (raw < 0) {
                continue; // unrecognized note name -- skip rather than crash
            } else if (raw == 0x80) { // rest
                SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                ch->key_on = 0;
                if (psg_chan >= 0)
                    ch->vol_env_index = 0;
                ResetModulationIfActive(ch);
                return;
            } else if (is_dac) {
                DAC_TriggerByNoteByte(cs, (uint8_t)raw);
                ch->key_on = 1;
                ch->note_timeout = dur;
                return;
            } else {
                int note_index = (raw - 0x81) + ch->transpose;
                ch->note_index = note_index;
                ResetModulationIfActive(ch);
                if (psg_chan >= 0 && !ch->psg_noise) {
                    PSG_SetTonePeriod(&cs->psg, psg_reg_chan, (uint16_t)((int)PSGPeriodForNote(note_index) - ch->detune));
                    PSG_SetAttenuation(&cs->psg, psg_reg_chan, ch->debug_muted ? 0x0F : ch->volume);
                    ch->vol_env_index = 0;
                } else if (psg_chan >= 0) { // noise-redirected -- PSG2's period register, see TickChannel's own comment
                    PSG_SetTonePeriod(&cs->psg, 2, PSGPeriodForNote(note_index));
                    PSG_SetAttenuation(&cs->psg, psg_reg_chan, ch->debug_muted ? 0x0F : ch->volume);
                    ch->vol_env_index = 0;
                } else if (is_fm) {
                    FM_SetFrequency(cs->fm, channel_index, note_index, ch->modulation_val + ch->detune);
                    if (debug_isolate_voice < 0 || ch->voice_index == debug_isolate_voice)
                        FM_KeyOnOff(cs->fm, channel_index, 1);
                }
                ch->key_on = 1;
                ch->note_timeout = ch->note_timeout_master;
                return;
            }
        }
        if (pj_object_has(event, "tie") || pj_object_has(event, "inheritedNote")) {
            const PJValue *durv = pj_object_get(event, "duration");
            int scaled = JsonUnhexInt(durv, 0) * (int)ch->tempo_divider;
            ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;
            ch->note_timeout = ch->note_timeout_master;
            if (psg_chan >= 0)
                ch->vol_env_index = 0;
            ResetModulationIfActive(ch);
            return;
        }

        // Coordination flags.
        const char *key = pj_object_first_key(event);
        if (!key)
            continue;
        v = pj_object_get(event, key);

        if (strcmp(key, "smpsPan") == 0) {
            const char *dir = pj_get_string(pj_array_get(v, 0), "");
            static const struct { const char *name; int val; } pans[] = {
                {"panLeft", 0x80}, {"panRight", 0x40}, {"panCentre", 0xC0}, {"panCenter", 0xC0}, {"panNone", 0x00},
            };
            int base = 0;
            for (size_t i = 0; i < sizeof(pans) / sizeof(pans[0]); i++)
                if (strcmp(dir, pans[i].name) == 0)
                    base = pans[i].val;
            uint8_t pv = (uint8_t)(base + JsonUnhexInt(pj_array_get(v, 1), 0));
            if (is_fm)
                FM_SetPan(cs->fm, channel_index, pv);
            else if (is_dac)
                cs->dac_pan = pv & 0xC0;
        } else if (strcmp(key, "smpsMod") == 0) {
            ch->mod_active = pj_get_bool(v, false) ? 1 : 0;
        } else if (strcmp(key, "smpsModSet") == 0) {
            // Stashed in json_modulation_raw (not read directly from JSON
            // again) so ResetModulationIfActive's re-arm-on-every-note-on
            // behavior works for JSON channels too -- see modulation_ptr's
            // own comment in Sound.h.
            ch->json_modulation_raw[0] = (uint8_t)JsonUnhexInt(pj_array_get(v, 0), 0);
            ch->json_modulation_raw[1] = (uint8_t)JsonUnhexInt(pj_array_get(v, 1), 0);
            ch->json_modulation_raw[2] = (uint8_t)JsonUnhexInt(pj_array_get(v, 2), 0);
            ch->json_modulation_raw[3] = (uint8_t)JsonUnhexInt(pj_array_get(v, 3), 0);
            ch->modulation_ptr = ch->json_modulation_raw;
            ch->modulation_wait = ch->json_modulation_raw[0];
            ch->modulation_speed = ch->json_modulation_raw[1];
            ch->modulation_delta = (int8_t)ch->json_modulation_raw[2];
            ch->modulation_steps = (uint8_t)(ch->json_modulation_raw[3] >> 1);
            ch->modulation_val = 0;
            ch->mod_active = 1;
        } else if (strcmp(key, "smpsPSGvoice") == 0) {
            const char *name = pj_get_string(v, "");
            const char *underscore = strrchr(name, '_');
            ch->voice_index = (uint8_t)strtol(underscore ? underscore + 1 : name, NULL, 16);
        } else if (strcmp(key, "smpsChanTempoDiv") == 0) {
            ch->tempo_divider = (uint8_t)JsonUnhexInt(v, 0);
        } else if (strcmp(key, "smpsAlterVol") == 0) {
            // Signed delta, encoded as its raw unsigned byte value (e.g.
            // "0xF2" means int8_t -14, not +242) -- must cast to int8_t
            // BEFORE adding, not after: the real bug found via A/B testing
            // against the byte-VM (GHZ's own FM5 smpsAlterVol "0xF2" was
            // computing volume+242, clamping to max instead of the real
            // volume-14). Unlike smpsAlterPitch/smpsChangeTransposition
            // below, this one has an explicit clamp in between, so it can't
            // rely on the cast-after-add's modular arithmetic to self-correct.
            int max = is_fm ? 0x7F : 0x0F;
            int val = (int)ch->volume + (int8_t)JsonUnhexInt(v, 0);
            ch->volume = (uint8_t)(val < 0 ? 0 : (val > max ? max : val));
            if (is_fm)
                FM_ApplyVolume(cs->fm, ch, channel_index);
        } else if (strcmp(key, "smpsNoteFill") == 0) {
            ch->note_timeout_master = (uint8_t)JsonUnhexInt(v, 0);
        } else if (strcmp(key, "smpsAlterPitch") == 0 || strcmp(key, "smpsChangeTransposition") == 0) {
            // Cast-before-add here too (see smpsAlterVol's own comment) --
            // this one happened to self-correct without it (plain modular
            // addition then truncation gives the same answer either order),
            // but writing it explicitly is one less thing to reason about.
            ch->transpose = (int8_t)(ch->transpose + (int8_t)JsonUnhexInt(v, 0));
        } else if (strcmp(key, "smpsAlterNote") == 0 || strcmp(key, "smpsDetune") == 0) {
            ch->transpose = (int8_t)JsonUnhexInt(v, 0);
        } else if (strcmp(key, "smpsNop") == 0) {
            // no-op, argument already consumed via the generic pj_object_get above
        } else if (strcmp(key, "smpsPSGform") == 0) {
            int form = JsonUnhexInt(v, 0);
            if (getenv("SONIC_PSG_TRACE")) fprintf(stdout, "JSON smpsPSGform ch%d psg_chan=%d psg_count=%u form=%d\n", channel_index, psg_chan, cs->psg_count, form);
            if (psg_chan == 2 && cs->psg_count < 4) {
                SN76489_Write(&cs->psg, (uint8_t)form);
                ch->psg_noise = 1;
            }
        } else if (strcmp(key, "smpsSetvoice") == 0 || strcmp(key, "smpsFMvoice") == 0) {
            uint8_t voice_index = (uint8_t)JsonUnhexInt(v, 0);
            ch->voice_index = voice_index;
            if (is_fm && ch->json_voices) {
                if (getenv("SONIC_VOL_TRACE")) fprintf(stdout, "JSON ch%d voice_index=%u volume=%u\n", channel_index, voice_index, ch->volume);
                const PJValue *voice = pj_array_get(ch->json_voices, voice_index);
                if (voice)
                    FM_LoadVoiceJSON(cs->fm, ch, channel_index, voice);
            }
        } else if (strcmp(key, "smpsPSGAlterVol") == 0) {
            int max = is_fm ? 0x7F : 0x0F;
            int val = (int)ch->volume + (int8_t)JsonUnhexInt(v, 0);
            ch->volume = (uint8_t)(val < 0 ? 0 : (val > max ? max : val));
        } else if (strcmp(key, "smpsSetTempoDiv") == 0) {
            uint8_t val = (uint8_t)JsonUnhexInt(v, 0);
            cs->duration_mult = val;
            for (int i = 0; i < SOUND_CHANNELS; i++)
                cs->channels[i].tempo_divider = val;
        } else if (strcmp(key, "smpsSetTempoMod") == 0) {
            cs->main_tempo = (uint8_t)JsonUnhexInt(v, 0);
        } else if (strcmp(key, "smpsCall") == 0) {
            const PJValue *target = JsonResolveBlock(ch->json_playlist, pj_get_string(v, ""));
            if (target) {
                JsonPushReturn(ch, ch->json_events, ch->json_event_index);
                ch->json_events = target;
                ch->json_event_index = 0;
                ch->json_in_jump_body = 0; // a called block is a normal track, never itself an unclosed "forever" body
                // Also clear the OUTER context's loop-body state (already
                // saved into the pushed return entry above) -- otherwise a
                // smpsCall issued from inside an inline smpsLoop body (a
                // real pattern, see Mus92_Drowning's own FM2) leaves
                // json_loop_idx pointing at the CALLER's loop slot while
                // running the callee, so the callee's own "ran off the
                // end" (it has no explicit terminator either, matching the
                // schema's "block end IS the implicit return" convention)
                // gets misread as "this loop body is exhausted" instead of
                // a normal return -- found via A/B testing.
                ch->json_loop_idx = -1;
            }
        } else if (strcmp(key, "jumpTo") == 0) {
            const PJValue *target = JsonResolveBlock(ch->json_playlist, pj_get_string(v, ""));
            const PJValue *loopArgs = pj_object_get(event, "smpsLoopArgs");
            if (loopArgs) {
                uint8_t idx = (uint8_t)JsonUnhexInt(pj_array_get(loopArgs, 0), 0);
                uint8_t cnt = (uint8_t)JsonUnhexInt(pj_array_get(loopArgs, 1), 0);
                if (idx < 3) {
                    if (ch->loop_counters[idx] == 0)
                        ch->loop_counters[idx] = cnt;
                    ch->loop_counters[idx]--;
                    if (ch->loop_counters[idx] == 0)
                        continue; // loop exhausted -- fall through to whatever's next in THIS array, no jump
                }
            }
            if (target) {
                ch->json_events = target;
                ch->json_event_index = 0;
                // A cross-block jumpTo target is always a real named block
                // with its own explicit terminating instruction (never
                // needs the inline-body "wrap to index 0 forever" fallback
                // -- see json_in_jump_body's own comment), even for the
                // "smpsJump <OwnTrackName>" self-referencing idiom.
                ch->json_in_jump_body = 0;
                ch->json_loop_idx = -1; // see smpsCall's own comment above -- same stale-context hazard
            }
        } else if (strcmp(key, "smpsLoop") == 0 || strcmp(key, "smpsJump") == 0) {
            // Inline body -- per your direction, walked directly via a
            // nesting resume point rather than pre-flattened into a
            // synthetic named block at load time (see json_return_stack's
            // own comment; pj_array's append-only API makes in-place
            // splicing awkward, and this achieves the identical playback
            // result without mutating the loaded tree at all).
            //
            // Always entered, exactly once per encounter of this node --
            // the byte-VM's own $F7 sits AFTER its loop body in the
            // compiled stream (body plays through inline first, THEN the
            // loop instruction decides whether to jump back), so the
            // count check has to happen when the body is exhausted, not
            // here on entry (see json_loop_idx's own comment in Sound.h).
            int is_loop = (strcmp(key, "smpsLoop") == 0);
            size_t body_start = is_loop ? 2 : 0;
            uint8_t idx = is_loop ? (uint8_t)JsonUnhexInt(pj_array_get(v, 0), 0) : 0;
            uint8_t cnt = is_loop ? (uint8_t)JsonUnhexInt(pj_array_get(v, 1), 0) : 0;
            if (is_loop && idx < 3)
                ch->loop_counters[idx] = cnt;
            if (is_loop) // an inline smpsJump body never falls through (see json_in_jump_body below), so never needs a return point
                JsonPushReturn(ch, ch->json_events, ch->json_event_index);
            ch->json_events = v;
            ch->json_event_index = body_start;
            // An inline smpsJump body ("no count" = forever) wraps back to
            // its own start when it runs out, not fall through/return --
            // json_in_jump_body is what the "ran off the end" handler
            // (below the main event-read loop) checks to do that. An
            // inline smpsLoop body is finite (checked via json_loop_idx
            // there instead once its own counter hits 0), so clears it.
            ch->json_in_jump_body = is_loop ? 0 : 1;
            ch->json_loop_idx = is_loop ? (int8_t)idx : -1;
            ch->json_loop_body_start = body_start;
        }
        // Unrecognized flag key: argument already implicitly "consumed" by
        // virtue of being one event object -- just move on, matching the
        // byte-VM's own defensive default case in spirit (skip, don't crash).
    }
}

static void TickChannel(SoundChipSet *cs, int channel_index) {
    SoundChannel *ch = &cs->channels[channel_index];
    if (!ch->active)
        return;

    int psg_chan = PSGRegisterChannel(channel_index);
    // $F3 (smpsPSGform) redirects this track to drive the real hardware
    // noise channel (SN76489 channel 3) instead of its own tone channel --
    // psg_reg_chan is which register slot to actually write to, while
    // psg_chan stays the track's own logical slot (needed for $F3's own
    // "only PSG3" gate).
    int psg_reg_chan = (psg_chan >= 0 && ch->psg_noise) ? 3 : psg_chan;
    int is_dac = (channel_index == SOUND_CHANNEL_DAC);
    int is_fm = (channel_index >= SOUND_CHANNEL_FM_BASE && channel_index < SOUND_CHANNEL_FM_BASE + 6);

    // Note-fill release tail: gate off early, before the command stream
    // advances to the next byte. Matches the real driver's
    // NoteTimeoutUpdate exactly: note_timeout==0 means the feature is
    // inactive (never decremented, never fires) -- it is NOT "already
    // expired, fire now". Only a nonzero note_timeout counts down, and
    // firing happens the instant it reaches 0 from that countdown, not on
    // every subsequent frame it happens to read as 0.
    if (ch->note_timeout > 0) {
        ch->note_timeout--;
        if (ch->note_timeout == 0 && ch->key_on) {
            ch->key_on = 0;
            SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
            if (is_fm)
                FM_KeyOnOff(cs->fm, channel_index, 0);
        }
    }

    // Volume envelope: steps every governed tick regardless of whether a
    // new note/command is being read this tick, same as the real driver's
    // PSGUpdateVolFX running independently of PSGUpdateTrack.
    if (psg_chan >= 0 && ch->key_on) {
        int vol = PSGStepEnvelope(ch);
        if (vol >= 0)
            PSG_SetAttenuation(&cs->psg, psg_reg_chan, ch->debug_muted ? 0x0F : (uint8_t)vol);
    }

    // FM volume-flutter envelope (driver-version-3's cfFMVolEnv) -- steps
    // every governed tick same as the PSG one above, independent of flag
    // processing. fm_vol_env_index defaults to 0 (disabled) for every
    // driver-version-1 channel, so this is a no-op there.
    if (is_fm && ch->key_on && ch->fm_vol_env_index > 0) {
        if (FMStepVolEnv(ch))
            FM_ApplyVolume(cs->fm, ch, channel_index);
    }

    if (ch->duration_timeout > 0) {
        ch->duration_timeout--;
        // Modulation (vibrato/pitch-bend, $F0-$F1/$F4) only steps while a
        // note is being held, not on the frame it's (re)triggered -- matches
        // the real driver only calling DoModulation from its "notegoing"
        // path, never from the fresh-note-on path.
        if (ch->mod_active) {
            StepModulation(ch);
            if (ch->key_on) {
                if (psg_chan >= 0 && !ch->psg_noise) {
                    int period = (int)PSGPeriodForNote(ch->note_index) + ch->modulation_val - ch->detune;
                    if (period < 1)
                        period = 1;
                    if (period > 1023)
                        period = 1023;
                    PSG_SetTonePeriod(&cs->psg, psg_reg_chan, (uint16_t)period);
                } else if (is_fm) {
                    // detune subtracted (not added) here to match PSG's own
                    // sign above -- PSG period and FM frequency move in
                    // OPPOSITE directions for the same "higher pitch" (lower
                    // period, higher fnum), so a single detune field needs
                    // opposite signs to sound like the same pitch offset on
                    // both chip types (driver-version-3 only -- always 0 for
                    // driver-version-1 channels, so no behavior change there).
                    FM_SetFrequency(cs->fm, channel_index, ch->note_index, ch->modulation_val + ch->detune);
                }
            }
        }
        return;
    }

    // JSON-tree-walking engine takes over here once the shared per-tick
    // governor logic above (note-fill, envelopes, modulation) has run --
    // see TickChannelJSON's own comment. Dispatch is per-CHANNEL
    // (ch->json_active, set by StartChannelJSON), not per-chip-set: SFX
    // are loaded via LoadSFXJSON, which deliberately never sets
    // cs->json_song (SFX trees aren't chip-set-owned), so a
    // cs->json_song-based check here left every JSON-loaded SFX channel
    // falling through to the byte-VM with a NULL data_ptr.
    if (ch->json_active) {
        TickChannelJSON(cs, ch, channel_index);
        return;
    }

    while (ch->active && ch->duration_timeout == 0) {
        uint8_t b = *ch->data_ptr++;

        if (b == 0xE7) { // smpsNoAttack (v1) / cfPreventAttack (v3) -- hold the current pitch, don't retrigger.
                          // Real driver-version-3 semantics are actually "set a
                          // flag that suppresses the NEXT note's own retrigger",
                          // not an immediate hold action -- but both produce the
                          // same audible result (no fresh attack/envelope reset)
                          // for how this flag is actually used in real songs
                          // (always immediately followed by a note), so v3
                          // channels deliberately share this exact v1 code path
                          // rather than threading a separate pending-flag through
                          // the shared note-on logic.
            uint8_t raw = *ch->data_ptr;
            if (raw < 0x80) {
                // "Dividing timing": the raw duration byte from the stream
                // gets multiplied here, once, and the *multiplied* value is
                // what's kept/reused (SavedDuration is post-multiplication
                // on real hardware) -- matches the real driver's
                // "move.b SMPS_Track.SavedDuration,DurationTimeout" reuse
                // path, which does no further multiplying.
                int scaled = (int)raw * (int)ch->tempo_divider;
                ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
                ch->data_ptr++;
            }
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;
            // Note fill ($E8) is the number of frames the note is *allowed
            // to play*, not a release-tail subtracted from duration -- 0
            // means disabled, and must stay exactly 0 (not fall back to
            // dur), or the note-timeout gate above would still fire right
            // at the note's own natural end and cut off the PSG envelope's
            // decay tail even with note-fill "off". Matches FinishTrackUpdate's
            // unconditional "move.b NoteTimeoutMaster,NoteTimeout".
            ch->note_timeout = ch->note_timeout_master;
            ch->key_on = 1;
            SOUND_TRACE("ch%d: $E7 no-attack dur=%u\n", channel_index, dur);
            break;
        }

        if (b < 0x80) { // Bare duration, no note prefix: keep playing the
                         // currently-held note/pitch (no new frequency write --
                         // this does NOT retrigger the note), just change how
                         // long for.
            int scaled = (int)b * (int)ch->tempo_divider;
            ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;
            ch->note_timeout = ch->note_timeout_master; // see the real note-on branch below for why not `dur`
            // FinishTrackUpdate resets VolEnvIndex unconditionally on every
            // track update except smpsNoAttack ($E7) -- including this bare-
            // duration case. The pitch doesn't retrigger, but the volume
            // envelope DOES restart from the top here, producing a fresh
            // attack-decay swell at the same held pitch -- this is what
            // actually produces LZ's "echo" character on a held note broken
            // into several bare-duration continuation bytes (e.g.
            // "nE6, $0C, $0C, $0C, $06"): four envelope swells at one
            // pitch, not four separate note-on events.
            if (psg_chan >= 0)
                ch->vol_env_index = 0;
            ResetModulationIfActive(ch);
            SOUND_TRACE("ch%d: bare-duration raw=$%02X dur=%u\n", channel_index, b, dur);
            break;
        }

        if (b < 0xE0) { // Note byte: 0x80 = rest, else pitch relative to C0
            uint8_t raw = *ch->data_ptr;
            if (raw < 0x80) {
                int scaled = (int)raw * (int)ch->tempo_divider;
                ch->saved_duration = (uint8_t)(scaled > 0xFF ? 0xFF : scaled);
                ch->data_ptr++;
            }
            uint8_t dur = ch->saved_duration ? ch->saved_duration : 1;
            ch->duration_timeout = dur;

            if (b == 0x80) {
                ch->key_on = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                ch->note_timeout = ch->note_timeout_master; // see the note-on branch below for why not `dur`
                if (psg_chan >= 0)
                    ch->vol_env_index = 0; // FinishTrackUpdate resets this unconditionally, rests included
                ResetModulationIfActive(ch);
                SOUND_TRACE("ch%d: rest dur=%u\n", channel_index, dur);
            } else if (is_dac) {
                // Sonic1PC's own EXTENDED DAC scheme (per your direction) --
                // a superset of real Sonic 1's own note-byte layout,
                // incorporating Sonic 2's four extra percussion samples
                // (Scratch/Clap/Tom/Bongo) at renumbered slots so both fit
                // without collision (real Sonic 2 uses $83 for Clap and $87
                // for Bongo, which real Sonic 1 already uses for Timpani and
                // the "SEGA!" PCM clip respectively -- moved to $85 and $DF
                // here instead). $81-$87 are the 7 base DPCM samples (Kick/
                // Snare/Timpani/Scratch/Clap/Tom/Bongo). $88-$91 are
                // pitch-shifted variants of Timpani/Tom/Bongo -- same DPCM
                // data as their own base sample, just played back at a
                // different rate (real driver's DACUpdateTrack: any sample
                // byte with bit 3 set gets rewritten to its base sample
                // after stashing a rate into zTimpani_Pitch). $DF is the
                // "SEGA!" PCM clip, given the highest possible note-byte
                // slot ($E0+ is coordination-flag territory) so it never
                // has to move again. See dac_notes above for every note's
                // own sample/rate. Pitch overrides persist across other
                // samples playing in between, same as real hardware's "this
                // affects the raw pitch of the base sample, meaning it will
                // use this value from then on" -- see
                // cs->dac_pitch_override_sample/rate. NOTE: this project
                // models that persistence with a single shared
                // override-sample/rate pair (matching real hardware's own
                // apparent single-register redirect-and-restash mechanism,
                // as best understood from the disassembly), so interleaving
                // Timpani/Tom/Bongo variants (e.g. a Bongo variant right
                // after a Timpani one) means only the MOST RECENT family's
                // override is remembered -- not independently verified
                // against real hardware for the 3-family case, since real
                // Sonic 1 only ever had Timpani to test this with.
                DAC_TriggerByNoteByte(cs, b);
                ch->key_on = 1;
                ch->note_timeout = dur;
            } else {
                int note_index = (int)(b - 0x81) + ch->transpose;
                ch->note_index = note_index;
                ResetModulationIfActive(ch); // reset before this note's first frequency write, so modulation_val is 0 for it
                if (psg_chan >= 0) {
                    // A noise-redirected track's own notes DO still set a
                    // period -- just not its own (real hardware has no
                    // period register on the noise channel at all). Real
                    // driver's PSGUpdateFreq: when VoiceControl marks a
                    // track as noise-redirected ($E0), it overrides the
                    // channel-select bits to $C0 ("PSG channel 2") before
                    // writing the frequency, unconditionally -- i.e. it
                    // hijacks PSG channel 2's own period register. That's
                    // exactly the register SN76489's noise generator reads
                    // from in "sync" mode (shift_rate==3, which $F3's real
                    // $E7 usage always selects -- see SN76489.c), so this is
                    // how a composer actually controls noise pitch: not via
                    // a noise-specific register (there isn't one), but by
                    // repurposing PSG2's period register through whichever
                    // track got redirected. Skipping this (as this code
                    // used to) leaves the noise generator reading whatever
                    // stale/unrelated value PSG2's own track last set,
                    // independent of the noise track's own authored notes
                    // -- audibly, every noise hit ends up the same pitch
                    // regardless of what note is written.
                    PSG_SetTonePeriod(&cs->psg, ch->psg_noise ? 2 : psg_reg_chan, (uint16_t)((int)PSGPeriodForNote(note_index) - ch->detune));
                    PSG_SetAttenuation(&cs->psg, psg_reg_chan, ch->debug_muted ? 0x0F : ch->volume);
                    ch->vol_env_index = 0; // Every new note restarts its volume envelope from the top
                } else if (is_fm) {
                    FM_SetFrequency(cs->fm, channel_index, note_index, ch->modulation_val + ch->detune);
                    if (debug_isolate_voice < 0 || ch->voice_index == debug_isolate_voice)
                        FM_KeyOnOff(cs->fm, channel_index, 1);
                }
                ch->key_on = 1;
                // Note fill ($E8) is the number of frames the note is
                // *allowed to play*, not a release-tail subtracted from
                // duration -- must stay exactly note_timeout_master (0 =
                // disabled, never fires), not fall back to `dur`, or the
                // gate above would fire right at the note's own natural
                // end even with note-fill "off", cutting off the PSG
                // envelope before it can restart on the next bare-duration
                // continuation. Matches FinishTrackUpdate's unconditional
                // "move.b NoteTimeoutMaster,NoteTimeout".
                ch->note_timeout = ch->note_timeout_master;
                SOUND_TRACE("ch%d: note byte=$%02X note_index=%d dur=%u vol=%u transpose=%d\n", channel_index, b,
                            note_index, dur, ch->volume, (int)ch->transpose);
            }
            break;
        }

        // Coordination flags ($E0-$FF, except $E7 handled above). Driver-
        // version-3 channels dispatch through a completely separate table
        // (TickChannel_FlagsV3, defined above) -- the byte values below mean
        // something else entirely on that table for most of this range, so
        // this switch stays exactly as it always has for driver-version-1
        // (the default, everything currently shipped) with zero risk of
        // behavior change.
        if (cs->driver_version >= 3) {
            TickChannel_FlagsV3(cs, ch, channel_index, is_fm, is_dac, psg_chan, psg_reg_chan, b);
            continue;
        }
        switch (b) {
            case 0xE0: { // smpsPan -- FM and DAC; no-op for PSG (can't set panning)
                uint8_t v = *ch->data_ptr++;
                if (is_fm)
                    FM_SetPan(cs->fm, channel_index, v);
                else if (is_dac)
                    cs->dac_pan = v & 0xC0;
                break;
            }
            case 0xE1: ch->transpose = (int8_t)*ch->data_ptr++; break; // smpsDetune -- SETS the channel key displacement (same field the header's pitch byte and $E9 use)
            case 0xE2: ch->data_ptr++; break; // smpsNop -- argument byte, no effect
            case 0xE3: // smpsReturn
                if (ch->return_sp > 0)
                    ch->data_ptr = ch->return_stack[--ch->return_sp];
                break;
            case 0xE4: // smpsFade -- stops this track; fade-in-to-previous-song isn't implemented
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                if (is_dac)
                    cs->dac_playing = 0;
                return;
            case 0xE5: ch->tempo_divider = *ch->data_ptr++; break; // smpsChanTempoDiv -- per-track duration multiplier override
            case 0xE6: { // smpsAlterVol -- designed for FM, effective immediately
                int max = is_fm ? 0x7F : 0x0F; // FM TL is 7-bit; PSG attenuation only uses the low nibble
                int v = (int)ch->volume + (int8_t)*ch->data_ptr++;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > max ? max : v));
                if (is_fm)
                    FM_ApplyVolume(cs->fm, ch, channel_index);
                break;
            }
            case 0xE8: ch->note_timeout_master = *ch->data_ptr++; break; // smpsNoteFill
            case 0xE9: ch->transpose = (int8_t)(ch->transpose + (int8_t)*ch->data_ptr++); break; // smpsChangeTransposition
            case 0xEA: cs->main_tempo = *ch->data_ptr++; break; // smpsSetTempoMod -- chip-set-wide, music only
            case 0xEB: { // smpsSetTempoDiv -- chip-set-wide duration multiplier, propagates to every track's own copy
                uint8_t v = *ch->data_ptr++;
                cs->duration_mult = v;
                for (int i = 0; i < SOUND_CHANNELS; i++)
                    cs->channels[i].tempo_divider = v;
                break;
            }
            case 0xEC: { // smpsPSGAlterVol -- designed for PSG; for FM only takes effect on the next $EF voice load
                int max = is_fm ? 0x7F : 0x0F;
                int v = (int)ch->volume + (int8_t)*ch->data_ptr++;
                ch->volume = (uint8_t)(v < 0 ? 0 : (v > max ? max : v));
                break;
            }
            case 0xED: break; // smpsClearPush -- clears sfx_Push's "pushing block" game-state flag, unrelated to the sound engine
            case 0xEE: // smpsStopSpecial
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                if (is_dac)
                    cs->dac_playing = 0;
                return;
            case 0xEF: { // smpsFMvoice / smpsSetvoice
                uint8_t voice_index = *ch->data_ptr++;
                ch->voice_index = voice_index;
                if (is_fm && ch->voice_bank) {
                    if (getenv("SONIC_VOL_TRACE")) fprintf(stdout, "BYTE ch%d voice_index=%u volume=%u\n", channel_index, voice_index, ch->volume);
                    FM_LoadVoice(cs->fm, ch, channel_index, ch->voice_bank + (size_t)voice_index * 25);
                }
                break;
            }
            case 0xF0: // smpsModSet -- also immediately activates modulation on real
                       // hardware (cfModulation's own bset #3), no separate $F1 needed
                ch->modulation_ptr = ch->data_ptr; // raw stream bytes, reread later by StepModulation's step-count reload
                ch->modulation_wait = *ch->data_ptr++;
                ch->modulation_speed = *ch->data_ptr++;
                ch->modulation_delta = (int8_t)*ch->data_ptr++;
                ch->modulation_steps = (uint8_t)(*ch->data_ptr++ >> 1); // halved, matches the real driver's lsr.b #1
                ch->modulation_val = 0;
                ch->mod_active = 1;
                break;
            case 0xF1: ch->mod_active = 1; break; // smpsModOn -- re-enable without resetting stored parameters
            case 0xF2: // smpsStop
                ch->active = 0;
                SilenceIfPSG(&cs->psg, psg_reg_chan, ch->psg_noise);
                if (is_fm)
                    FM_KeyOnOff(cs->fm, channel_index, 0);
                if (is_dac)
                    cs->dac_playing = 0;
                return;
            case 0xF3: { // smpsPSGform -- irreversibly redirects this track to drive the real
                         // hardware noise channel (SN76489 channel 3) instead of its own tone
                         // channel. Only safe on PSG3 ("without bugs" per the real driver's own
                         // documentation), and only when the 4th PSG slot isn't already dedicated
                         // to noise on its own (psg_count==4).
                uint8_t v = *ch->data_ptr++;
                if (psg_chan == 2 && cs->psg_count < 4) {
                    SN76489_Write(&cs->psg, v); // v is already a valid $E0-$E7 noise-control byte
                    ch->psg_noise = 1;
                }
                break;
            }
            case 0xF4: ch->mod_active = 0; break; // smpsModOff
            case 0xF5: ch->voice_index = *ch->data_ptr++; break; // smpsPSGvoice
            case 0xF6: { // smpsJump
                const uint8_t *word_pos = ch->data_ptr;
                int16_t rel = (int16_t)ReadWord(word_pos);
                ch->data_ptr = word_pos + 1 + rel;
                break;
            }
            case 0xF7: { // smpsLoop
                uint8_t index = *ch->data_ptr++;
                uint8_t loops = *ch->data_ptr++;
                const uint8_t *word_pos = ch->data_ptr;
                int16_t rel = (int16_t)ReadWord(word_pos);
                const uint8_t *target = word_pos + 1 + rel;
                ch->data_ptr = word_pos + 2;
                if (index < 3) {
                    if (ch->loop_counters[index] == 0)
                        ch->loop_counters[index] = loops;
                    ch->loop_counters[index]--;
                    if (ch->loop_counters[index] != 0)
                        ch->data_ptr = target;
                }
                break;
            }
            case 0xF8: { // smpsCall -- ABSOLUTE offset from song start (the compiler's own "abs" patch kind,
                         // json_to_header.py's emit_event: `em.word_placeholder(value, "abs")`), NOT relative
                         // like smpsJump/smpsLoop's "rel-1" -- found via the JSON-engine cross-verification
                         // effort: GHZ's own FM1 smpsCall into Call02 was silently landing on the WRONG (but
                         // coincidentally plausible-sounding) nearby data under the old word_pos+1+rel math.
                const uint8_t *word_pos = ch->data_ptr;
                uint16_t target_off = ReadWord(word_pos);
                const uint8_t *target = ch->song_base + target_off;
                if (ch->return_sp < 2)
                    ch->return_stack[ch->return_sp++] = word_pos + 2;
                ch->data_ptr = target;
                break;
            }
            case 0xF9: break; // smpsMaxRelRate -- FM-only, no-op for PSG
            default: ch->data_ptr++; break; // Unknown/reserved flag -- consume one arg byte defensively
        }
    }
}

// ---------------------------------------------------------------------
// Frame tick
// ---------------------------------------------------------------------

static void TickChipSet(SoundChipSet *cs) {
    if (cs->paused) // Sound_Pause() -- only sound_music ever sets this
        return;

    // Shared tempo governor -- matches the real driver's TempoWait exactly
    // (s1.sounddriver.asm): every track's DurationTimeout gets an
    // unconditional "addq.b #1", regardless of whether that track is
    // active, and as a plain 8-bit add it wraps on overflow (255+1->0)
    // rather than clamping. Both properties matter: skipping inactive
    // channels or clamping at 0xFF (as this used to do) makes a channel
    // that ever hits max duration diverge permanently from real hardware's
    // timing by a fixed offset from that point on -- exactly the kind of
    // per-channel desync (not a gradual drift) this was found to cause.
    //
    // This must run BEFORE the queue is dispatched below -- the real
    // driver's main loop (UpdateMusic) does subq.b/TempoWait first, and
    // only afterward calls CycleSoundQueue/PlaySoundID to load any newly
    // queued song/SFX. Doing it in the other order (as this used to) means
    // a freshly-loaded song's very first frame either does or doesn't get
    // an extra tempo correction depending purely on our call order, not on
    // anything the song data says -- a real source of startup-frame skew
    // for whichever song/SFX happens to get dispatched this tick.
    if (cs->main_tempo != 0) {
        if (cs->tempo_timeout > 0)
            cs->tempo_timeout--;
        if (cs->tempo_timeout == 0) {
            cs->tempo_timeout = cs->main_tempo;
            for (int i = 0; i < SOUND_CHANNELS; i++)
                cs->channels[i].duration_timeout++; // uint8_t: wraps naturally, matching addq.b
        }
    }

    DispatchQueue(cs, SOUND_QUEUE_NORMAL);
    DispatchQueue(cs, SOUND_QUEUE_SPECIAL);

    for (int i = 0; i < SOUND_CHANNELS; i++)
        TickChannel(cs, i);

    // Once everything in this chip set has finished playing, release any
    // priority claim -- matches v_sndprio only mattering while something's
    // actually still occupying the channels it was set for.
    if (cs->current_priority != 0) {
        int any_active = 0;
        for (int i = 0; i < SOUND_CHANNELS; i++)
            if (cs->channels[i].active)
                any_active = 1;
        if (!any_active)
            cs->current_priority = 0;
    }
}

void Sound_Frame(void) {
    TickChipSet(&sound_music);
    TickChipSet(&sound_sfx);
    sound_trace_frame++;
}

// `out` is interleaved stereo (2*count entries: L,R,L,R,...), additive
// (callers must clear it first). Real SN76489 hardware has no panning
// capability at all, so PSG is generated into a mono scratch buffer and
// duplicated equally into both channels.
#define SOUND_SCRATCH_MAX 4096

void Sound_Generate(int32_t *out, uint32_t count, uint32_t sample_rate) {
    static int32_t psg_scratch[SOUND_SCRATCH_MAX];
    uint32_t n = count < SOUND_SCRATCH_MAX ? count : SOUND_SCRATCH_MAX;
    memset(psg_scratch, 0, n * sizeof(int32_t));
    if (!getenv("SONIC_FM_ONLY") && debug_isolate_voice < 0) {
        SN76489_Generate(&sound_music.psg, psg_scratch, n, sample_rate, SOUND_PSG_CLOCK);
        SN76489_Generate(&sound_sfx.psg, psg_scratch, n, sample_rate, SOUND_PSG_CLOCK);
    }
    for (uint32_t i = 0; i < n; i++) {
        out[2 * i + 0] += psg_scratch[i];
        out[2 * i + 1] += psg_scratch[i];
    }

    // Multiplies FM's contribution to the mix by a fixed gain before adding
    // it in -- real TL/attenuation register math is unaffected, this only
    // scales FM's overall level in the final mix relative to PSG/DAC, which
    // is otherwise measurably ~5-6x quieter than PSG+DAC even when playing
    // fully correct, byte-verified voice data (see the RMS comparison this
    // was based on). SONIC_FM_GAIN=N overrides the default for A/B testing.
    const char *fm_gain_env = getenv("SONIC_FM_GAIN");
    int fm_gain = fm_gain_env ? atoi(fm_gain_env) : 10;
    if (fm_gain != 1) {
        static int32_t fm_scratch[2 * SOUND_SCRATCH_MAX];
        uint32_t fn = count < SOUND_SCRATCH_MAX ? count : SOUND_SCRATCH_MAX;
        memset(fm_scratch, 0, 2 * fn * sizeof(int32_t));
        YM2612_Generate(sound_music.fm, fm_scratch, fn, sample_rate, SOUND_FM_CLOCK);
        YM2612_Generate(sound_sfx.fm, fm_scratch, fn, sample_rate, SOUND_FM_CLOCK);
        for (uint32_t i = 0; i < 2 * fn; i++)
            out[i] += fm_scratch[i] * fm_gain;
    } else {
        YM2612_Generate(sound_music.fm, out, count, sample_rate, SOUND_FM_CLOCK);
        YM2612_Generate(sound_sfx.fm, out, count, sample_rate, SOUND_FM_CLOCK);
    }
    if (!getenv("SONIC_FM_ONLY") && debug_isolate_voice < 0) {
        // sound_sfx has no DAC-sample track -- real hardware's SFX RAM
        // layout (v_sfx_track_ram) never allocates one (only FM3-5/PSG1-3),
        // and SFXChannelIndex() never routes any SFX channel ID to
        // SOUND_CHANNEL_DAC either, so sound_sfx.channels[DAC] can never
        // become active. DAC_Generate(&sound_sfx,...) was a guaranteed-inert
        // call every frame; PCM_Generate stays for both, since the separate
        // "SEGA!" PCM clip ($E1/$87) can legitimately trigger from either
        // chip set's own track data.
        DAC_Generate(&sound_music, out, count, sample_rate);
        PCM_Generate(&sound_music, out, count, sample_rate);
        PCM_Generate(&sound_sfx, out, count, sample_rate);
    }
}

// Debug/tooling only (ParadoxComposer): raw pointer to a song/SFX's
// compiled data by ID, NULL if unused. Lets a standalone tool read the
// voice bank and header directly without going through PlayMusic/LoadMusic.
const uint8_t *Sound_DebugGetSongData(uint8_t id) { return sound_table[id]; }

// Debug/tooling only (ParadoxComposer): play an arbitrary, not-yet-baked-in
// compiled song/SFX buffer -- e.g. one just produced by json_to_header.py
// from an edited .jsonc, with no ID in sound_table at all. PlayMusic/
// PlaySound only accept an ID looked up in that compile-time table, so this
// bypasses the lookup and calls straight into the same LoadMusic/LoadSFX
// logic DispatchQueue uses, letting the tool audition arbitrary song bytes
// through the real sequencer without recompiling the game.
void Sound_DebugPlayRawSong(const uint8_t *data, uint8_t is_sfx, uint8_t driver_version) {
    if (is_sfx)
        LoadSFX(&sound_sfx, data, driver_version);
    else
        LoadMusic(&sound_music, data, 0, driver_version);
}

// Debug/tooling only: JSON-engine equivalent of Sound_DebugPlayRawSong --
// json_text is a whole .jsonc song/SFX file's source text (comments
// stripped/parsed via pj_parse, same as the real compiler). Verification
// aid for the "SMPS runtime: walk JSON directly" plan's own staged
// rollout: lets a throwaway tool A/B this engine against the byte-VM
// without needing the (not-yet-built) resource-compiler/loader.
int Sound_DebugPlayRawSongJSON(const char *json_text, uint8_t is_sfx, uint8_t driver_version) {
    char err[256] = {0};
    PJValue *song = pj_parse(json_text, err, sizeof(err));
    if (!song) {
        fprintf(stderr, "Sound_DebugPlayRawSongJSON: parse error: %s\n", err);
        return 0;
    }
    if (is_sfx) {
        const PJValue *playlist = pj_object_get(song, "SMPSplaylist");
        LoadSFXJSON(&sound_sfx, song, playlist, driver_version);
        // Deliberately NOT freed here: LoadSFXJSON doesn't take ownership
        // (SFX trees aren't chip-set-owned), but the channels it just
        // started hold live PJValue* pointers straight into this tree
        // (json_playlist/json_voices/json_events) that stay in use for the
        // SFX's whole playback -- freeing it immediately after load was a
        // real use-after-free (caught via ASan: pj_array_size reading a
        // freed block from TickChannelJSON on a later frame). The real
        // production path needs a proper per-SoundID cache that owns each
        // SFX's tree for the process lifetime (see the JSON-engine plan's
        // "cache each SFX's parsed tree permanently... freed never" design
        // note) -- this debug entry point doesn't have one, so it leaks
        // intentionally rather than use-after-free, matching that same
        // "freed never" precedent.
    } else {
        LoadMusicJSON(&sound_music, song, 0, driver_version); // takes ownership
    }
    return 1;
}

// Real (non-debug) entry point for the JSON engine: plays a music/SFX ID
// from sound_table_json instead of sound_table, i.e. through
// TickChannelJSON instead of the byte-VM. Not wired into the normal
// QueueSound1/2 -> DispatchQueue path -- callers that want this engine
// call it directly (currently just the level-select sound test).
//
// SFX trees need to survive repeated re-triggering (the sound test can
// replay the same one many times in a row) without leaking a fresh
// pj_parse() on every press -- unlike Sound_DebugPlayRawSongJSON's
// intentional single-shot leak, this builds the real per-SoundID cache
// that was only ever a design note before: parsed once, kept for the rest
// of the process's life (matches DispatchQueue's own byte-VM data, which
// is just as permanently resident, being compiled straight into the
// binary).
static PJValue *sound_json_sfx_cache[0x100];

void Sound_PlayFromJSON(uint8_t id) {
    // Stamps which sound ID just triggered into the SONIC_FM_TRACE/
    // SONIC_PSG_TRACE log, so a trace file can be positively tied to a
    // specific repro attempt instead of relying on frame numbers alone --
    // two different runs of the same song/SFX can otherwise produce
    // identical-looking tails purely by coincidence (looping content).
    if (getenv("SONIC_FM_TRACE") || getenv("SONIC_PSG_TRACE"))
        fprintf(stdout, "[%u] SOUNDTEST id=$%02X\n", sound_trace_frame, id);
    const char *json_text = sound_table_json[id];
    if (!json_text)
        return; // unmapped ID -- same "clean skip" contract as DispatchQueue's NULL sound_table check
    uint8_t driver_version = sound_table_driver_ver[id];

    if (id >= SOUND_ID_MUSIC_FIRST && id <= SOUND_ID_MUSIC_LAST) {
        char err[256] = {0};
        PJValue *song = pj_parse(json_text, err, sizeof(err));
        if (!song) {
            fprintf(stderr, "Sound_PlayFromJSON: parse error for id $%02X: %s\n", id, err);
            return;
        }
        LoadMusicJSON(&sound_music, song, id, driver_version); // takes ownership
        return;
    }

    PJValue *song = sound_json_sfx_cache[id];
    if (!song) {
        char err[256] = {0};
        song = pj_parse(json_text, err, sizeof(err));
        if (!song) {
            fprintf(stderr, "Sound_PlayFromJSON: parse error for id $%02X: %s\n", id, err);
            return;
        }
        sound_json_sfx_cache[id] = song;
    }
    const PJValue *playlist = pj_object_get(song, "SMPSplaylist");
    LoadSFXJSON(&sound_sfx, song, playlist, driver_version);
}

void Sound_DebugSetChannelMuted(int channel_index, uint8_t muted) {
    if (channel_index < 0 || channel_index >= SOUND_CHANNELS)
        return;
    sound_music.channels[channel_index].debug_muted = muted;
}

void Sound_DebugSetLadderEffect(int enabled) {
    YM2612_SetLadderEffect(sound_music.fm, enabled);
}

// Dedicated scratch chip-set for standalone DAC preview -- DAC_Trigger/
// DAC_Generate only ever touch a SoundChipSet's own dac_*/timpani_pitch_rate
// fields, so a zero-initialized instance used for nothing else is safe.
// dac_pan defaults to 0 (both channels off, i.e. silent) like any other
// zero-initialized SoundChipSet -- set explicitly to center below, same
// reasoning as YM2612_FMCore's own pan-default fix (a caller with no pan
// control of its own should still be audible).
static SoundChipSet dac_preview_chipset;
static int dac_preview_pan_initialized = 0;

static void EnsureDacPreviewPanInitialized(void) {
    if (dac_preview_pan_initialized)
        return;
    dac_preview_chipset.dac_pan = 0xC0;
    // -1, not the zero-initialized (static storage) default -- 0 is a real
    // DAC_SAMPLE_KICK, which would otherwise make the very first preview
    // ever triggered incorrectly think an override applies (matching the
    // sound_music/sound_sfx init in Sound_Init).
    dac_preview_chipset.dac_pitch_override_sample = -1;
    dac_preview_pan_initialized = 1;
}

void Sound_DebugPreviewDacSample(int sampleId) {
    EnsureDacPreviewPanInitialized();
    // Each direct-sample preview is independent -- doesn't inherit a
    // pitch-variant override from a previous Sound_DebugPreviewDacVariant
    // call, same as dac_timpani_variant being reset here too.
    dac_preview_chipset.dac_pitch_override_sample = -1;
    dac_preview_chipset.dac_timpani_variant = -1;
    DAC_Trigger(&dac_preview_chipset, sampleId);
}

void Sound_DebugPreviewDacVariant(int sampleId, int variant) {
    // Same note-byte mapping as the main dispatch (SND_dKick's own
    // dispatch comment) -- Timpani/Tom/Bongo variants are notes
    // $88-$8B/$8C-$8E/$8F-$91, rate/data now sourced from dac_notes
    // directly instead of a separate per-family array.
    int note_byte;
    if (sampleId == DAC_SAMPLE_TIMPANI && variant >= 0 && variant < 4) {
        note_byte = 0x88 + variant;
    } else if (sampleId == DAC_SAMPLE_TOM && variant >= 0 && variant < 3) {
        note_byte = 0x8C + variant;
    } else if (sampleId == DAC_SAMPLE_BONGO && variant >= 0 && variant < 3) {
        note_byte = 0x8F + variant;
    } else {
        return; // not a real family/variant combination -- nothing to trigger
    }
    int note = note_byte - SND_dKick;
    if (note < 0 || note >= DAC_NOTE_COUNT || !dac_notes[note].data)
        return;
    EnsureDacPreviewPanInitialized();
    dac_preview_chipset.dac_pitch_override_sample = sampleId;
    dac_preview_chipset.dac_pitch_override_rate = dac_notes[note].rate;
    // Debug tooling only (SMPSInspector) -- still Timpani-specific by
    // design (see its own declaration comment in Sound.h); left at -1 for
    // Tom/Bongo previews since that inspector visualization doesn't cover
    // them.
    dac_preview_chipset.dac_timpani_variant = (sampleId == DAC_SAMPLE_TIMPANI) ? variant : -1;
    DAC_TriggerNote(&dac_preview_chipset, &dac_notes[note], sampleId);
}

uint8_t Sound_DebugHasDacNote(int note_byte) {
    int note = note_byte - SND_dKick;
    return (note >= 0 && note < DAC_NOTE_COUNT && dac_notes[note].data) ? 1 : 0;
}

void Sound_DebugPreviewDacNote(int note_byte) {
    int note = note_byte - SND_dKick;
    if (note < 0 || note >= DAC_NOTE_COUNT || !dac_notes[note].data)
        return;
    EnsureDacPreviewPanInitialized();
    // No pitch-override bookkeeping here -- a preview click always wants to
    // hear exactly the rate baked into this slot, not whatever override an
    // earlier click left active.
    dac_preview_chipset.dac_pitch_override_sample = -1;
    dac_preview_chipset.dac_timpani_variant = -1;
    DAC_TriggerNote(&dac_preview_chipset, &dac_notes[note], -1);
}

void Sound_DebugGenerateDacPreview(int32_t *out, uint32_t count, uint32_t sample_rate) {
    DAC_Generate(&dac_preview_chipset, out, count, sample_rate);
}

int Sound_DebugIsDacPreviewPlaying(void) { return dac_preview_chipset.dac_playing; }

void Sound_DebugIsolateVoice(int voice_index) {
    debug_isolate_voice = voice_index;
    // Only gating *new* note-on events left an already-ringing channel
    // audible until its own natural note-off -- switching targets live
    // would briefly bleed the old voice into the new selection. Force any
    // currently-active, non-matching FM channel silent immediately instead.
    if (voice_index < 0)
        return;
    SoundChipSet *sets[2] = {&sound_music, &sound_sfx};
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < 6; i++) {
            SoundChannel *fch = &sets[s]->channels[SOUND_CHANNEL_FM_BASE + i];
            if (fch->key_on && fch->voice_index != voice_index) {
                fch->key_on = 0;
                FM_KeyOnOff(sets[s]->fm, SOUND_CHANNEL_FM_BASE + i, 0);
            }
        }
    }
}
