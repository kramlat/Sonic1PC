#pragma once

#include <stdint.h>
#include <stddef.h>

#include "Backend/SN76489.h"
#include "Backend/YM2612.h"

// PJValue (libparadoxsmps) -- the JSON-tree-walking playback engine's
// channel/chip-set state below points directly at nodes in a loaded song's
// parsed tree (see the "SMPS runtime: walk JSON directly" plan's own
// rollout section, staged alongside the original byte-stream engine).
#include "json.h"

// Sound driver state. Modeled on the real SMPS driver's per-channel RAM
// (SMPS_Track in s1.sounddriver.ram.asm) and its 3-slot sound request queue
// (v_soundqueue0-2) -- simplified into one flat channel array per chip set
// rather than the original's separate music/SFX/special-SFX track RAM
// banks sharing one physical chip, since channel-stealing between tiers
// can just as well be done with a priority check at runtime instead of
// mirroring the original's fixed Z80 RAM layout.
//
// Channel counts are deliberately over-provisioned past the real chips'
// hardware limits, on purpose, to safely reproduce two RAM layout quirks
// the original relies on rather than letting them read/write out of bounds:
//
//   - The extra ("7th") FM slot: on real hardware, a Sonic 1 song with no
//     DAC part still has real DAC track data present -- it's just an SMPS
//     jump command that loops the track back to its own start, forever,
//     rather than actually being empty. The RAM this occupies is what a
//     6th *music* FM channel's track data gets unpacked into instead, and
//     that borrowed track plays on the real YM2612's hardware FM channel 6
//     (via a 07/03 register setup) -- the DAC hardware itself outputs
//     nothing in this mode, it's purely the DAC *track's RAM* being reused
//     to hold a second logical track that targets FM6. Tracked here as its
//     own array slot rather than aliasing it onto the DAC channel's memory.
//   - The 4th PSG slot: the same trick, deliberately extended. The
//     SN76489 itself fully supports all 3 tone channels plus the noise
//     channel running simultaneously (enabled by a 07/04 setup, same idea
//     as FM6's 07/03 above) -- it's purely that the original driver never
//     allocated RAM for a 4th concurrent PSG track, not a hardware limit --
//     one that simply doesn't exist here, since a PC has plenty of RAM to
//     spare for a track struct the size of this one. The extra slot just
//     allocates that room, so SMS tracks (which do use tone+noise
//     together) work for modders instead of the two contending over one
//     slot.
#define SOUND_CHANNELS_PSG 4
#define SOUND_CHANNELS_FM  7 // 6 FM channels + DAC (the 6th FM channel's "borrowed" DAC-slot trick is its own array entry, not aliased)
#define SOUND_CHANNELS     (SOUND_CHANNELS_PSG + SOUND_CHANNELS_FM)

#define SOUND_CHANNEL_PSG_BASE 0
#define SOUND_CHANNEL_FM_BASE  SOUND_CHANNELS_PSG
#define SOUND_CHANNEL_DAC      (SOUND_CHANNEL_FM_BASE + 6)

typedef struct {
    uint8_t playback_control;
    uint8_t voice_control;
    uint8_t tempo_divider;
    const uint8_t *data_ptr;
    // The song/SFX's own byte 0 -- needed to resolve smpsCall ($F8) targets,
    // which the compiler encodes as an ABSOLUTE offset from song start
    // ("abs" patch kind, unlike smpsJump/smpsLoop's relative "rel-1"
    // encoding). Per-channel, not per-chip-set, since sound_sfx's channels
    // can each be playing a different SFX (different song_base) at once --
    // same reasoning as SoundChannel's own json_playlist field.
    const uint8_t *song_base;
    int8_t transpose;
    uint8_t volume;
    uint8_t ams_fms_pan;   // FM/DAC only
    uint8_t voice_index;   // FM/PSG only
    uint8_t vol_env_index; // PSG only
    uint8_t stack_pointer;
    uint8_t duration_timeout;
    uint8_t saved_duration; // also doubles as SavedDAC on the DAC channel
    uint16_t freq;
    uint8_t note_timeout;
    uint8_t note_timeout_master; // persists across notes -- set by smpsNoteFill ($E8)
    const uint8_t *modulation_ptr;
    // Backing store for modulation_ptr when this channel is running the
    // JSON engine: ResetModulationIfActive/StepModulation (shared with the
    // byte-VM) re-arm modulation by re-reading 4 raw bytes through
    // modulation_ptr on every note-on, not from the wait/speed/delta/steps
    // fields directly (those get mutated while stepping) -- the byte-VM
    // points it straight into the compiled stream; the JSON engine has no
    // such stream, so smpsModSet copies its 4 args in here and points
    // modulation_ptr at this instead, keeping the governor engine-agnostic.
    uint8_t json_modulation_raw[4];
    uint8_t modulation_wait;
    uint8_t modulation_speed;
    int8_t modulation_delta;
    uint8_t modulation_steps;
    int16_t modulation_val;
    int note_index; // last real note-on's pitch (post-transpose), so modulation can recompute frequency each frame without a note retrigger
    int8_t detune;
    uint8_t psg_noise;    // PSG only
    uint8_t fm_base_tl[4]; // FM only -- each operator's TL as loaded from the
                            // voice bank, before ch->volume is added as an
                            // offset (see FM_LoadVoice/FM_ApplyVolume)
    uint8_t feedback_algo; // FM only
    const uint8_t *voice_ptr; // FM SFX only
    uint32_t loop_counters[3];

    // Driver-version-3 (Sonic 3/Flamedriver-compatible flags, see
    // TickChannel_FlagsV3 in Sound.c) only -- unused/zero for driver_version
    // 1 channels. return_stack/return_sp and loop_counters above are shared
    // as-is (Flamedriver's own gosub-stack and shared loop-counter-pool
    // mechanisms match these fields exactly, no v3-specific duplicates
    // needed).
    uint8_t alt_freq_mode;    // cfToggleAltFreqMode ($FD) -- raw 16-bit-frequency note encoding, not yet interpreted by the note-byte decoder (see TickChannel_FlagsV3's own comment)
    uint8_t pitch_slide_active; // cfPitchSlide ($FF,$0B)
    int fm_vol_env_index;     // cfFMVolEnv ($FF,$06) -- 0 = no envelope active, else 1-based index into psg_envelopes (same table FM borrows for its own "flutter" effect)
    uint8_t fm_vol_env_mask;  // cfFMVolEnv's operator bitmask (%00004231 format) -- stored for completeness, not yet used to restrict which operators the flutter applies to (whole-channel TL only, see FM_ApplyVolume)
    uint8_t fm_vol_env_pos;   // Current step within the envelope pointed to by fm_vol_env_index -- separate from vol_env_index above, which is PSG-only and driven by a different flag ($EC v1 / $E6 v1)

    // Runtime interpreter state (not modeled on SMPS_Track RAM -- this
    // engine's byte-stream interpreter needs a real return-address stack
    // and explicit active/key-on flags where the original just relied on
    // fixed Z80 RAM layout and unconditional jumps).
    const uint8_t *return_stack[2]; // smpsCall/smpsReturn ($F8/$E3) -- 2 levels deep, more than any real song nests
    uint8_t return_sp;
    uint8_t active;  // 0 once smpsStop/smpsStopSpecial ($F2/$EE) is hit, or channel unused by the loaded song
    uint8_t key_on;  // 1 while gated on (audible); cleared during a note-fill release tail or on a rest
    uint8_t mod_active; // smpsModOn/smpsModOff ($F1/$F4) -- parameters stored above are not yet applied per-frame (TODO)
    uint8_t debug_muted; // Debug/tooling only (ParadoxComposer): forces silence regardless of ch->volume -- see Sound_DebugSetChannelMuted

    // JSON-tree-walking playback engine (SoundJSON.c) -- staged ALONGSIDE
    // data_ptr/return_stack above, not replacing them yet (see the "SMPS
    // runtime: walk JSON directly" plan's rollout section). A channel uses
    // exactly one of the two engines at a time, selected by whether the
    // owning SoundChipSet's json_song is non-NULL. events/event_index are
    // this engine's equivalent of data_ptr: a direct pointer to the current
    // block's events array (real PJValue*, resolved once at load time --
    // never a byte offset, per your direction) plus a position within it.
    // This channel's own SMPSplaylist object, for jump/loop/call target
    // resolution (JsonResolveBlock). Per-channel, NOT per-chip-set --
    // sound_music's channels all share one song's playlist, but sound_sfx's
    // channels can each be playing a DIFFERENT SFX concurrently (e.g. one
    // channel mid-Jump SFX while another starts Spring), so a single
    // chip-set-wide "current playlist" would resolve the wrong SFX's block
    // names for whichever channel didn't trigger most recently.
    const PJValue *json_playlist;
    const PJValue *json_voices; // this channel's own "voices" array -- same per-channel-not-per-chip-set reasoning as json_playlist above
    const PJValue *json_events;
    size_t json_event_index;
    // Doubles as both smpsCall's real return stack AND the implicit
    // "resume the outer array here" point an inline smpsLoop body needs
    // once its own repeat count is exhausted (see TickChannelJSON's own
    // comment) -- both are strictly-nested push/pop resume points, so one
    // stack serves both rather than needing two. Sized a little deeper
    // than the byte-VM's return_stack[2] to allow one level of each
    // simultaneously, a real pattern (a smpsCall target block that itself
    // contains an inline smpsLoop).
    struct {
        const PJValue *events;
        size_t index;
        uint8_t in_jump_body; // the CONTEXT being resumed's own json_in_jump_body value, saved/restored across nesting (see JsonPushReturn/JsonPopReturn)
        int8_t loop_idx;      // ditto for json_loop_idx/json_loop_body_start
        size_t loop_body_start;
    } json_return_stack[4];
    uint8_t json_return_sp;
    // True while ch->json_events points at an inline smpsJump body ("no
    // count = forever" per the schema) -- running off the end of one wraps
    // back to its own start instead of popping json_return_stack, since an
    // inline smpsJump body never falls through to anything (unlike an
    // inline smpsLoop body, which does once its repeat count hits 0).
    uint8_t json_in_jump_body;
    // >=0 while ch->json_events points at an inline smpsLoop body -- which
    // loop_counters[] slot to check/decrement once the body runs off its
    // own end (-1 = not currently in a loop body). Matches the byte-VM's
    // own $F7, which sits AFTER its loop body in the compiled stream and
    // jumps BACKWARD to repeat it -- the JSON schema instead embeds the
    // body inline right after the smpsLoop node, so the equivalent
    // decrement/repeat-or-fall-through check has to happen when the body
    // is EXHAUSTED, not when the smpsLoop node is first entered (an early
    // version got this backwards: checking on entry meant the body only
    // ever played once, since re-entering it required re-reading the
    // smpsLoop node itself, which nothing does once inside the body).
    int8_t json_loop_idx;
    size_t json_loop_body_start;
    // Which engine THIS channel uses -- set 1 by StartChannelJSON, 0 by
    // StartChannel. Dispatch must be per-channel, not per-chip-set: SFX
    // are loaded via LoadSFXJSON, which deliberately never touches
    // cs->json_song (SFX trees aren't chip-set-owned -- see LoadSFXJSON's
    // own comment), so a cs->json_song-based dispatch check left every
    // JSON-loaded SFX channel silently falling through to the byte-VM
    // with a NULL data_ptr (the "active=1, data_ptr=NULL" crash).
    uint8_t json_active;
} SoundChannel;

// Matches v_soundqueue0-2: one pending request per priority tier, each
// overwritten (not FIFO'd) by the next QueueSoundN call before the driver
// consumes it.
#define SOUND_QUEUE_NORMAL  0 // v_soundqueue0 -- QueueSound1
#define SOUND_QUEUE_SPECIAL 1 // v_soundqueue1 -- QueueSound2
#define SOUND_QUEUE_UNUSED  2 // v_soundqueue2 -- QueueSound3 (unused in the original unless FixBugs is on)
#define SOUND_QUEUE_SIZE    3

// One chip set: a full complement of PSG+FM channel state plus its own
// request queue. Real hardware only has one physical YM2612 and one
// physical SN76489 -- music and SFX have to fight over the same 4+7
// channels, which is the entire reason the original driver's priority
// system (v_sndprio) and channel-stealing between music/SFX/special-SFX
// RAM banks exists at all. A PC has no such constraint: nothing stops us
// from emulating a second, completely independent set of chips -- one
// dedicated to music, one dedicated to SFX -- and downmixing their two
// output streams into one at the very end, in software, right before
// handing samples to SDL. Something impossible on a real Genesis, free
// here.
typedef struct {
    SoundChannel channels[SOUND_CHANNELS];
    uint8_t queue[SOUND_QUEUE_SIZE];
    SN76489 psg;
    struct YM2612 *fm; // Backend/YM2612.h -- one physical chip per chip set, same dual-chip-set idea as psg
    const uint8_t *voice_bank; // Current song's FM voice table (25 bytes/voice) -- see smpsSetvoice/$EF

    // JSON-tree-walking playback engine (SoundJSON.c) -- staged alongside
    // the byte-VM fields above, see SoundChannel's own comment. json_song
    // is this chip set's currently-loaded song's root PJValue (owned --
    // freed and replaced wholesale on the next JSON-path load, which is
    // the actual fix for the "music leaks" this whole redesign is for: no
    // shared backing store two songs' state could ever alias into). NULL
    // means this chip set is using the classic byte-VM instead.
    PJValue *json_song;
    PJValue *json_voices;   // "voices" array within json_song, cached (FM_LoadVoice-equivalent reads this by index)
    PJValue *json_playlist; // "SMPSplaylist" object within json_song, cached (post-normalization -- see SoundJSON.c)

    // Shared tempo governor -- one per chip set (not per channel; see
    // smpsChanTempoDiv's comment in Sound.c for why per-channel overrides
    // aren't independently timed yet). Matches the real driver's
    // TempoWait/v_main_tempo_timeout mechanism, NOT a simple accumulator:
    // every frame, every channel's duration_timeout decrements by 1
    // unconditionally; separately, tempo_timeout counts down from
    // main_tempo each frame, and when it hits 0 it resets to main_tempo AND
    // adds 1 back to every channel's duration_timeout (a periodic "delay by
    // one extra frame" correction -- "duty cycle" = how many of every
    // main_tempo frames actually advance vs. get delayed). duration_mult
    // is unrelated to that -- it's a flat multiplier applied to a note's
    // raw duration byte the moment it's read (header offset $04,
    // "dividing timing"), not part of the per-frame governor at all.
    uint8_t duration_mult;
    uint8_t main_tempo;
    uint8_t base_main_tempo;   // main_tempo as loaded from the header, cached so SlowDownMusic can restore it
    uint8_t current_music_id;  // Currently-loaded music ID, for SpeedUpMusic's per-song SpeedUpIndex lookup
    uint8_t psg_count;         // Cached from the header, for $F3's "only if psg_count<4" gate
    uint8_t tempo_timeout;

    // 0 (default) = driver-version-1 (Sonic 1/2-compatible) coordination-
    // flag table, exactly as implemented before this field existed -- every
    // existing song stays on this path unconditionally, since sound_table's
    // parallel driver-version array (Sound.c) defaults every entry to 0.
    // >=3 = driver-version-3 (Sonic 3/Flamedriver-compatible) table, see
    // TickChannel_FlagsV3. Set once per LoadMusic/LoadSFX call, from
    // whichever song was just loaded.
    //
    // Deliberately NOT a byte-exact match for Clownacy's Sonic 2 Clone
    // Driver v2 (~/Downloads/Sonic-2-Clone-Driver-v2-master) -- that driver
    // uses a genuinely different physical encoding (EVERY coordination flag,
    // even pan/detune, goes through a universal $FF,<sub-op> 2-byte prefix;
    // see its own engine/_smps2asm_inc.asm macro bodies), not this project's
    // Flamedriver-style direct-$E0-$FE-plus-small-$FF-extended-subset
    // scheme, even when Clone Driver v2's own SourceDriver>=3 conditionals
    // are active. Per your direction, music authored for Clone Driver v2
    // with SourceDriver>=3 is approximated as driver-version-3 here for now
    // (same target feature set, same macro names in spirit) rather than
    // getting its own distinct byte-exact table -- if that ever needs to
    // change, Clone Driver v2 support means a REAL 4th table (its own
    // sub-op numbering, read from that file), not a value tweak here.
    uint8_t driver_version;

    // Driver-version-3 only, chip-set-wide (matches Flamedriver's own
    // global-not-per-track scope for these -- e.g. zSpindashRev/zHaltFlag
    // are single shared Z80 RAM bytes in the real driver, not one per
    // track). Unused/zero when driver_version==0.
    int spindash_rev;          // cfSpindashRev ($E9) / cfResetSpindashRev ($FF,$07)
    uint8_t halt_flag;         // cfHaltSound ($FF,$02)
    uint8_t continuous_sfx_flag; // cfLoopContinuousSFX ($FC)
    uint8_t cont_sfx_loop_cnt;   // cfLoopContinuousSFX ($FC)
    uint8_t fade_to_prev_flag;   // cfFadeInToPrevious ($E2) -- stored but not acted on, same documented stub status as the existing v1 smpsFade/FadeOutMusic()

    // DAC/DPCM percussion sample playback (kick/snare/timpani only so far
    // -- see DACSampleID). One sample plays at a time per chip set, matching
    // the real driver's single physical DAC.
    const uint8_t *dac_data;
    uint32_t dac_nibble_pos;
    uint32_t dac_nibble_count;
    uint8_t dac_accum;   // 8-bit DPCM decode accumulator, centered at 0x80
    uint8_t dac_playing;
    int dac_sample_id;   // which of dac_samples[]/DAC_SAMPLE_* triggered the current dac_data -- debug tooling only (SMPSInspector), nothing in the driver itself reads this back
    // Generalized pitch-override mechanism (per your direction, Sonic 2's
    // driver does the exact same "one base DPCM sample, multiple
    // pitch-shifted note bytes" trick for Tom and Bongo too, not just
    // Timpani) -- dac_pitch_override_sample is which DAC_SAMPLE_* the
    // override applies to (-1 = none), dac_pitch_override_rate is the last
    // pitch-shifted-variant rate selected for it, sticky across other
    // samples playing in between (matches real hardware's "this affects
    // the raw pitch of the base sample, meaning it will use this value
    // from then on" -- see the DAC dispatch comment in Sound.c). Was
    // Timpani-only (timpani_pitch_rate); generalized rather than adding a
    // near-duplicate pair of fields per family now that there are three.
    uint32_t dac_pitch_override_rate;
    int dac_pitch_override_sample;
    int dac_timpani_variant; // -1 = N/A (a non-Timpani sample, or plain $83, is the most recent trigger); 0-3 = Hi/Mid/Low/Floor, whichever $88-$8B was the most recent trigger -- debug tooling only (SMPSInspector)
    uint32_t dac_phase; // fixed-point phase accumulator for dac_rate->output-rate step
    uint32_t dac_rate;  // Hz -- the currently-triggered sample's own real rate (see DACSample::rate)

    // Raw PCM playback (the "SEGA!" boot voice clip, res/PCM/sega) -- unlike
    // the DPCM percussion above, this is plain unsigned 8-bit samples with
    // no header and no delta-decode, played back directly (see
    // PlaySegaSound). Independent of dac_playing/dac_data.
    uint32_t pcm_pos;
    uint8_t pcm_playing;
    uint32_t pcm_phase;

    // DAC/PCM output pan -- real hardware shares this with FM channel 6's
    // own $B4 register (DAC replaces channel 6's audio when DAC_EN is set,
    // but still uses channel 6's stereo bits); this port gives the DAC an
    // independent pan state instead of literally sharing a register with
    // FM6, which is simpler and has the same audible effect. Set via $E0
    // on the DAC track; top 2 bits are the L/R enable bits, same encoding
    // as FM_SetPan. Shared by both DPCM percussion and the raw PCM path
    // (Sega sound), since they're the same physical DAC.
    uint8_t dac_pan;

    uint8_t paused; // Sound_Pause()/Sound_Resume() -- freezes tempo advancement (sound_music only)

    // v_sndprio equivalent -- tracks the priority of whatever's currently
    // occupying sound_sfx's channels, so a lower-priority PlaySound() can't
    // cut it off (matches CycleSoundQueue's ".blo .nextinput" reject). Not
    // meaningful for sound_music (music always just replaces outright).
    uint8_t current_priority;
} SoundChipSet;

extern SoundChipSet sound_music; // Dedicated chip set for music
extern SoundChipSet sound_sfx;   // Dedicated chip set for sound effects

// Genesis PSG/FM clock: both chips are fed a divided-down version of the
// same ~53.69MHz (NTSC) master clock the 68000 runs from.
#define SOUND_PSG_CLOCK 3579545

// The 7 base DPCM percussion samples (res/DAC/*) in Sonic1PC's own EXTENDED
// DAC scheme (per your direction) -- Kick/Snare/Timpani are real Sonic 1's
// own set; Scratch/Clap/Tom/Bongo are Sonic 2's four extra ones, folded in
// at renumbered note-byte slots (see the $81-$92 dispatch comment in
// Sound.c) so both games' samples coexist without collision. Each has its
// own pitch-shiftable variants too except Scratch/Clap -- see
// timpani_variant_rate_hz/tom_variant_rate_hz/bongo_variant_rate_hz in
// Sound.c. The "SEGA!" boot voice PCM sample (res/PCM/sega, a different
// codec entirely -- see zPlay_SegaPCM in the real driver) is separate from
// this DPCM sample set.
#define DAC_SAMPLE_KICK    0
#define DAC_SAMPLE_SNARE   1
#define DAC_SAMPLE_TIMPANI 2
#define DAC_SAMPLE_SCRATCH 3
#define DAC_SAMPLE_CLAP    4
#define DAC_SAMPLE_TOM     5
#define DAC_SAMPLE_BONGO   6
#define DAC_SAMPLE_S3SNARE 7 // Sonic 3's snare -- $92, temporarily taking over the slot SEGA! used to trigger (see Sound.c's $92 dispatch comment)

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

    // Continuous SFX -- new category, no members yet (see this enum's own
    // comment above). sound_table has nothing registered anywhere in this
    // range, so any ID here resolves to a NULL pointer and the driver
    // silently skips it (DispatchQueue's `if (!song) return;`) until real
    // entries exist.
    SOUND_ID_CONTINUOUS_SFX_FIRST = sfx_Signpost + 1,
    SOUND_ID_CONTINUOUS_SFX_LAST = SOUND_ID_CONTINUOUS_SFX_FIRST - 1, // empty range (LAST < FIRST)

    sfx_Waterfall = SOUND_ID_CONTINUOUS_SFX_LAST + 1, // collapses onto CONTINUOUS_SFX_FIRST while that range stays empty

    bgm_Fade = 0xF0,
    sfx_Sega,
    bgm_Speedup,
    bgm_Slowdown,
    bgm_Stop,
};
#define DAC_SAMPLE_COUNT   8

void Sound_Init(void);

// Queues a sound ID for the driver to pick up on its next Sound_Frame()
// tick -- matches the real QueueSound1 (normal/music) and QueueSound2
// (special/priority SFX). Each overwrites its slot rather than FIFO'ing.
// Low-level primitives -- PlayMusic()/PlaySound() below are what game code
// should actually call; they route through these.
void QueueSound1(uint8_t id);
void QueueSound2(uint8_t id);

// Plays a music/SFX ID through the JSON tree-walking engine instead of the
// byte-VM QueueSound1/2 -> DispatchQueue routes through -- see its own
// comment in Sound.c. Immediate, not queued (unlike QueueSound1/2, which
// wait for the next Sound_Frame() tick): the caller is expected to already
// be OK with side effects landing this frame, matching how the level-select
// sound test (its only caller so far) already works.
void Sound_PlayFromJSON(uint8_t id);

// High-level entry points -- matches the real PlaySoundID's dispatch by ID
// range (bgm__First-Last $81-$93, sfx__First-Last $A0-$CF, spec__First-Last
// $D0, flg__First-Last $E0-$E4). Game code should call these, not
// QueueSound1/2 directly.
void PlayMusic(uint8_t id);  // $81-$93, or $00/$80 to stop
void PlaySound(uint8_t id);  // $A0-$D0 (SFX/special SFX), or an $E0-$E4 command below

// The $E0-$E4 commands PlaySound() dispatches to directly (also callable on
// their own):
void StopAllSound(void);  // $E4 (and $00)
void FadeOutMusic(void);  // $E0
void PlaySegaSound(void); // $E1 -- NOT implemented yet (needs the "SEGA!" PCM
                           // sample decoded through a different codec path
                           // than the DPCM percussion -- see zPlay_SegaPCM)
void SpeedUpMusic(void);  // $E2
void SlowDownMusic(void); // $E3

// Matches PauseMusic/ResumeMusic: freezes/unfreezes sound_music's tempo
// governor (so it stops advancing playback) without touching sound_sfx --
// SFX keep playing while paused, same as the real driver.
void Sound_Pause(void);
void Sound_Resume(void);

// Advances playback by one 60Hz frame: drains the sound queues, ticks the
// tempo governor, and steps every active channel's byte-stream interpreter
// (writing PSG registers / decoding DPCM samples as it goes). Call this
// once per frame, before Sound_Generate().
void Sound_Frame(void);

// Diagnostic: when enabled, logs every note/duration/coordination-flag
// event TickChannel decodes to stderr, so a real song's playback can be
// verified directly against its own .asm source. Off by default.
void Sound_SetTrace(int enabled);

// Renders `count` mono samples at `sample_rate`, downmixing sound_music and
// sound_sfx additively into `out` (which is NOT cleared first by this
// function -- callers must zero it, since it's meant to be summed with
// whatever else shares the output buffer).
void Sound_Generate(int32_t *out, uint32_t count, uint32_t sample_rate);

// Debug/tooling only (ParadoxComposer): raw pointer to a song/SFX's
// compiled data by ID (matches PlaySoundID's ID space, e.g. 0x81=GHZ),
// NULL if that ID has no song registered.
const uint8_t *Sound_DebugGetSongData(uint8_t id);

// Debug/tooling only (ParadoxComposer): play an arbitrary, not-yet-baked-in
// compiled song/SFX buffer (e.g. one just produced by json_to_header.py from
// an edited .jsonc) through the real sequencer, bypassing the sound_table ID
// lookup PlayMusic/PlaySound require. is_sfx selects LoadSFX vs LoadMusic.
// driver_version selects the coordination-flag table (0 = driver-version-1/
// Sonic 1-2-compatible, the same value sound_table's songs default to; 3 =
// driver-version-3/Sonic 3-Flamedriver-compatible) -- the caller (e.g.
// ParadoxComposer, via its SongDocument's own "driverVersion" field) must
// pass whichever table the bytes were actually compiled against, since
// nothing in the compiled byte stream itself self-identifies its table.
void Sound_DebugPlayRawSong(const uint8_t *data, uint8_t is_sfx, uint8_t driver_version);

// Debug/tooling only: JSON-tree-walking engine equivalent of the above --
// json_text is a whole .jsonc song/SFX's source text. Returns 0 on parse
// failure, 1 on success. See the "SMPS runtime: walk JSON directly" plan.
int Sound_DebugPlayRawSongJSON(const char *json_text, uint8_t is_sfx, uint8_t driver_version);

// Debug/tooling only (ParadoxComposer): force a channel silent regardless
// of its own computed volume, for the Channels panel's per-strip mute/solo
// checkboxes. Operates on sound_music (what Sound_DebugPlayRawSong(...,
// is_sfx=0) plays through) -- channel_index matches SoundChipSet::channels[]
// indexing: 0..SOUND_CHANNELS_PSG-1 are PSG (3 tone + noise), then
// SOUND_CHANNEL_FM_BASE.. are FM/DAC (see Sound.h's own layout comment).
void Sound_DebugSetChannelMuted(int channel_index, uint8_t muted);

// Debug/tooling only (ParadoxComposer): standalone DAC-sample preview for a
// DAC-home block's own keyboard (per your direction: "list DAC samples on
// keys... and actually play the samples"), independent of any song/SFX
// playback -- uses a dedicated scratch chip-set state, not sound_music/
// sound_sfx, so it never interferes with real playback. sampleId is one of
// the 7 DAC_SAMPLE_* base samples (Kick/Snare/Timpani/Scratch/Clap/Tom/
// Bongo).
void Sound_DebugPreviewDacSample(int sampleId);
// Pitch-shifted variant preview, for any of the 3 families that have them
// -- sampleId is DAC_SAMPLE_TIMPANI/TOM/BONGO, variant indexes that
// family's own rate table in Sound.c (timpani_variant_rate_hz: 0-3 =
// Hi/Mid/Low/Floor, transcribed directly from Sonic 1's own
// DAC_sample_rate/byte_71CC4 disassembly; tom_variant_rate_hz/
// bongo_variant_rate_hz: 0-2, derived from Sonic 2's dac_sample_metadata
// scale factors -- see those tables' own comments). Any other sampleId is
// a no-op. Generalized (was Timpani-only, Sound_DebugPreviewTimpaniVariant)
// once Tom/Bongo needed the exact same preview behavior.
void Sound_DebugPreviewDacVariant(int sampleId, int variant);
// Preview by raw DAC note byte ($81-$DE, SND_dKick and up) -- looks straight
// into Sound.c's dac_notes[] table, the same one the real note dispatch
// uses, so this covers every wired-up percussion sample (not just the
// original 7 DAC_SAMPLE_* base ones + their pitch variants) with a single
// function. Preferred over Sound_DebugPreviewDacSample/Variant above for any
// new caller; those two are kept only because existing callers still use
// them. A note byte with no dac_notes[] entry (a gap, or $DF/SEGA -- not a
// real DPCM sample) is a no-op.
void Sound_DebugPreviewDacNote(int note_byte);
// Whether dac_notes[] actually has a real sample at this note byte -- lets
// callers (e.g. ParadoxComposer's piano roll) tell a valid preview click
// apart from a no-op one without needing to know dac_notes[]'s own bounds.
uint8_t Sound_DebugHasDacNote(int note_byte);
// Generates `count` stereo samples of whatever's currently previewing via
// either function above, additive into `out` (same convention as
// Sound_Generate). Call once per output tick while a DAC preview is active.
void Sound_DebugGenerateDacPreview(int32_t *out, uint32_t count, uint32_t sample_rate);
// Whether the standalone DAC preview is still playing -- a DAC sample is a
// one-shot (unlike a held FM/PSG note), so the caller uses this to know
// when it can stop calling Sound_DebugGenerateDacPreview.
int Sound_DebugIsDacPreviewPlaying(void);

// Debug/tooling only (SonicVoiceIsolate): while active, mutes every FM
// channel except one currently loaded with the given voice_index, and mutes
// PSG/DAC entirely -- lets a real song play at real speed/timing with only
// one specific voice audible, to identify which voice index in a bank is
// the "this one sounds right" voice by ear, in context. -1 disables
// isolation (normal playback, the default).
void Sound_DebugIsolateVoice(int voice_index);
