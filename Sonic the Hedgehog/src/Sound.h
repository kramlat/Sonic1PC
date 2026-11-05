#pragma once

#include "EngineSound.h"

// Sound IDs. Originally a direct transcription of the real disasm's
// bgm_*/sfx_* equates (_Constants.asm), which start music at $81 (a real
// hardware convention: $80 itself means "silence"/stop, so the real driver's
// own first playable song is one past that) and pack SFX into $A0-$D0 with
// a further gap up to $E0-$E4's handful of special top-level commands.
// Renumbered (per your direction) into one contiguous enum, freeing up that
// space for a new "continuous SFX" category (Sonic 3-style, cfLoopContinuousSFX)
// that doesn't exist yet -- reserved here as an empty range so a future
// Sonic 3 content port has somewhere to grow into without ANOTHER
// renumbering pass. Layout, in order: 0 = silence/stop (was $80), music
// ($01+), regular SFX, continuous SFX (empty for now), special SFX (just
// Waterfall), then the 5 top-level commands fixed at $F0-$F4. Each
// constant's NAME is still exactly what the real disasm calls it and (for
// music) still matches its resource filename number (Mus81 = bgm_GHZ, even
// though bgm_GHZ's own ID value is no longer literally 0x81) -- only the
// numeric ID space changed, not the identity of anything.
#ifdef SOUND_IDS_CUSTOM // a game built on this one with its own sound ids (Sonic 2): it defines the whole enum SoundID
#include "SoundIds.h"
#else
enum SoundID {
    bgm_GHZ = 1, // 0 is reserved -- id==0 means silence/stop, same as the real $80 sentinel PlayMusic used to special-case
    bgm_LZ,
    bgm_MZ,
    bgm_SLZ,
    bgm_SYZ,
    bgm_SBZ,
    bgm_Invincible,
    bgm_ExtraLife,
    bgm_SS,
    bgm_Title,
    bgm_Ending,
    bgm_Boss,
    bgm_FZ,
    bgm_GotThrough,
    bgm_GameOver,
    bgm_Continue,
    bgm_Credits,
    bgm_Drowning,
    bgm_Emerald,
    bgm_SSRG, // SCP_SPLASH-only in sound_table (see Sound.c), but the ID slot itself is always reserved so the rest of the ID space doesn't shift depending on that build option

    sfx_Jump = bgm_SSRG + 1,
    sfx_Lamppost,
    sfx_Unk_A2, // SndA2 in sound_table -- no real disasm sfx_ name (never referenced by ID in game code, then or now)
    sfx_Death,
    sfx_Skid,
    sfx_Unk_A5, // SndA5, same as sfx_Unk_A2 above
    sfx_HitSpikes,
    sfx_Push,
    sfx_SSGoal,
    sfx_SSItem,
    sfx_Splash,
    sfx_Unk_AB, // SndAB
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
    sfx_Unk_B8, // SndB8
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
    sfx_Signpost,
    sfx_SpindashRev, // Sonic 2's spin dash rev ($E0): its pitch climbs with each rev, see DispatchSfx

    // Continuous SFX -- new category, no members yet (see this enum's own
    // comment above). sound_table has nothing registered anywhere in this
    // range, so any ID here resolves to a NULL pointer and the driver
    // silently skips it (DispatchQueue's `if (!song) return;`) until real
    // entries exist.
    SOUND_ID_CONTINUOUS_SFX_FIRST = sfx_SpindashRev + 1,
    SOUND_ID_CONTINUOUS_SFX_LAST = SOUND_ID_CONTINUOUS_SFX_FIRST - 1, // empty range (LAST < FIRST)

    sfx_Waterfall = SOUND_ID_CONTINUOUS_SFX_LAST + 1, // collapses onto CONTINUOUS_SFX_FIRST while that range stays empty

    bgm_Fade = 0xF0,
    sfx_Sega,
    bgm_Speedup,
    bgm_Slowdown,
    bgm_Stop,
};
#endif
