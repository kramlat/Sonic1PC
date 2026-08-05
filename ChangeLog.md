# Changelog

Session notes capturing context, rationale, and outstanding work that isn't
already recorded in code comments or commit history — written so this
survives conversation summarization/compaction. Newest work first.

## Music/SFX pipeline (smps2asmc)

**Status: core pipeline done and verified; only 2 songs imported; playback driver not started.**

Built a from-scratch translator + runtime for Flamewing's SMPS2ASM format
(the human-readable macro language real Sonic 1 songs are written in),
specialized to `SonicDriverVer=1`/`SourceDriver=1` (the real Sonic 1 driver,
no cross-driver conversion needed).

- **`smps2asmc/smps2asmc.c`**: translates a `.asm` song/SFX file into a
  small C program that `#include`s `smps.h` and calls its functions in the
  same order/arguments as the original file's macro invocations. Only does
  *syntax* translation (macro names, `$XX`→`0xXX` literals, label lines →
  `SMPS_Label()` calls) — all real SMPS-format semantics live in smps.h/.c.
  This is viable specifically because SMPS2ASM song files are themselves
  just straight-line macro-call sequences with no AS-specific control flow
  (`switch`/`function`/`eval`/etc. — those only appear in the shared macro
  *package*, not in individual songs).
- **`smps2asmc/smps.h` + `smps.c`**: the C reimplementation of
  smps2asm.asm's macros for `SonicDriverVer=1`. Byte-length/encoding for
  every macro was hand-derived from the real macro source (pasted into
  chat by the user) by manually collapsing each macro's conditional logic
  to the `SonicDriverVer==SourceDriver==1` branch. Includes:
  - The full note-name→pitch-byte table (166 names), generated
    *mechanically* from the enum block (not hand-transcribed — a scripted
    parse of the `enum`/`nextenum`/alias sequence), specifically to rule
    out a transcription slip silently shifting every note past some point.
  - A single-pass byte buffer + deferred patch list for label references
    (forward refs are common — e.g. a song's header references its FM/PSG
    channel labels before they're defined later in the same file). Two
    patch kinds: `PATCH_SONG_RELATIVE` (`dc.w loc-songStart`, used by
    header pointers) and `PATCH_PC_RELATIVE` (`dc.w loc-*-1`, used by
    `smpsJump`/`smpsLoop`/`smpsCall`).
  - FM voice bank macros (`smpsVc*`), including the `SourceSMPS2ASM==0`
    interpretation of `smpsVcAmpMod` (`op<<5`, not `op<<7` — see that
    function's comment) and the `SonicDriverVer!=2` operator write order
    (4,3,2,1).
- **Verification**: not just "compiles" — cross-checked against
  hand-computed expected output. `SndA0 - Jump.asm` (SFX) matches
  byte-for-byte against manual derivation. `Mus81 - GHZ.asm` (full 627-line
  real song) verified via multiple independent pointer cross-checks: the
  Voice pointer, DAC pointer, and FM1 pointer all landed exactly where
  hand-computed, and a `smpsCall` PC-relative reference resolved exactly to
  its target's first note byte.
- **Two bugs found and fixed against the real GHZ file** (not caught by the
  synthetic test): `smpsVcUnusedBits` takes 1 *or* 5 args (d1r1-4 optional,
  default 0) — was hardcoded to require 5. Missing constants `nMaxPSG`
  (=`nA5`) and the DAC sample IDs (`dKick`, `dSnare`, etc.) from the
  "DAC Equates" section of smps2asm.asm.
- **CMake wiring**: `add_smps_song(asm_file, array_name)` function in the
  main CMakeLists chains three steps — translate (smps2asmc_tool) → build
  (per-song `songbuild_<name>` executable linking the generated `.c` +
  `smps.c`) → run (produces `Resource/Music/<name>.h`, same shape as every
  other resource). Two songs wired in as proof-of-concept:
  `Mus81 - GHZ.asm` → `Mus81_GHZ.h`, `SndA0 - Jump.asm` → `SndA0_Jump.h`.
- **Known gap**: unlike bin2h/clownassembler (built via `ExternalProject_Add`
  specifically so they're host-native even when Sonic itself is
  cross-compiled), `smps2asmc_tool` and the per-song `songbuild_*` targets
  are plain `add_executable()` — fine for this project's only configuration
  so far (native build) but would need the same isolation before trusting
  this pipeline under cross-compilation.
- **Not started**: importing the rest of the song/SFX catalog (only 2 of
  ~20+ tracks done). The actual **playback driver** — code that walks a
  song's byte stream frame-by-frame, updates `Sound.h`'s `sound_channels[]`
  state, and writes chip registers — does not exist yet. This is the
  critical missing piece before anything can actually be heard, and before
  `SonicSoundTest` (below) can do anything meaningful.

## Sound chip emulation

**Status: SN76489 (PSG) done and audible; YM2612 (FM) vendored but not wired up.**

- **`src/Backend/SN76489.h`/`.c`**: full SN76489 PSG emulator (3 tone + 1
  noise channel), standard latch/data write protocol, 2dB-step attenuation
  table, LFSR-based noise (white: tap bits 0+3; periodic: tap bit 0; resets
  to 0x8000 on every noise-control write, matching real hardware). Fixed
  point tick accumulator carries fractional cycles between
  `SN76489_Generate()` calls so successive per-frame calls don't drift or
  click at the boundary.
- **`ymfm` submodule** (root of repo, BSD-3-Clause, github.com/aaronsgiles/ymfm):
  vendored for YM2612 (FM) emulation. Chosen over e.g. Nuked-OPN2 because
  those are GPL and would put a copyleft license on this project. **Not
  wired up yet** — no C++/C interop wrapper exists, `Sound.h`'s
  `SoundChipSet` has no FM state, `Sound_Generate()` only calls the PSG
  path. This project is pure C99 today; wiring ymfm in will be this
  project's first C++ compilation unit, needing CMake changes to add a C++
  toolchain requirement plus an `extern "C"` bridge.
- **`src/Sound.h`/`.c`**: the driver-level channel/track state, deliberately
  designed around **two independent emulated chip sets** — one dedicated to
  music (`sound_music`), one to SFX (`sound_sfx`) — each with a full
  complement of channels, downmixed together in software right before
  handing samples to SDL. This is *impossible on real Genesis hardware*
  (only one physical YM2612 + one SN76489 exist, so music and SFX have to
  fight over the same channels — the entire reason the original driver's
  priority system and channel-stealing between music/SFX/special-SFX RAM
  banks exists at all), but free on a PC. User's explicit direction, not an
  invented simplification.
  - Channel counts are **deliberately over-provisioned** past real hardware
    limits, on purpose, to safely reproduce two RAM-reuse quirks the
    original relies on instead of letting them read/write out of bounds:
    - 7 "FM" slots instead of 6: on real hardware, a Sonic 1 song with no
      DAC part still has real DAC track data (just a self-looping
      `smpsJump`) — the RAM that occupies is what a 6th *music* FM
      channel's track data gets unpacked into instead, and that borrowed
      track plays on real YM2612 hardware FM channel 6 (via a 07/03
      register setup; the DAC hardware itself outputs nothing in this
      mode). Tracked as its own array slot rather than aliased onto the
      DAC channel's memory.
      - **Correction from user**: initially described as "6th FM channel
        works because DAC is a no-op" — actually the DAC track always has
        real track data (the self-loop), and the 6th-FM-channel trick
        plays on real FM6 hardware via 07/03, not some separate emulated
        path.
    - 4th PSG slot: same trick, deliberately extended. The SN76489 chip
      itself fully supports all 3 tone channels + noise running
      simultaneously (enabled by a 07/04 setup) — it's purely that the
      original driver never allocated RAM for a 4th concurrent PSG track,
      not a hardware limit. Doesn't exist as a constraint here since a PC
      has plenty of RAM to spare. Added specifically so SMS tracks (which
      do use tone+noise together) work for modders.
  - `sound_queue[3]` matches `v_soundqueue0-2` (`SOUND_QUEUE_NORMAL`/
    `SPECIAL`/`UNUSED`, i.e. `QueueSound1/2/3`).
- **`src/Backend/SDL2/Audio.h`/`.c`**: SDL audio device opened **passively**
  (no callback) — samples generated and pushed via `SDL_QueueAudio()` once
  per frame from `Audio_Update()`, matching the game's own frame-driven
  (60Hz VBlank) architecture instead of fighting it with a separate
  callback thread's timing. Hooked into `Backend/VDP.c`'s per-frame
  `Input_HandleEvents()` call site (same place, same pattern). Caps queued
  backlog at 4 frames and drops excess on a stall, rather than drifting out
  of sync. `SDL_INIT_AUDIO` added to `System_Init`'s flags.
- Quick smoke-tested: `./bin/Sonic` runs 3s with no audio-device errors;
  full `ctest` suite (7/7, includes headless `SDL_AUDIODRIVER=dummy`) still
  passes after every change in this section.

## Enigma decompression (EniDec)

**Status: done, verified byte-exact against known-good data.**

- **`src/Enigma.c`/`.h`**: line-for-line C translation of the disassembly's
  `EniDec` (used for 16x16 block mappings and similar tilemap-like assets).
  Deliberately *not* restructured/"cleaned up" — the bit-level tricks (the
  mid-word rotate when refilling with a bit deficit, the shared bitstream
  between the format-list reader and the per-tile render-flag bits) are
  easy to subtly break by simplifying control flow, so it mirrors the
  original's register/state usage closely.
- **Resource swap**: `res/Map16/{GHZ,LZ,MZ,SBZ,SLZ,SYZ}` were previously
  *pre-decompressed* binary blobs (a port-time simplification predating
  this session). Swapped back to the genuine compressed `.eni` originals
  from the disassembly, decompressed at runtime via `EniDec`, matching how
  Nemesis/Kosinski assets already worked. `LevelHeader.map16_size` (marked
  `//TEMP` in the struct) removed — no longer needed, since `EniDec` finds
  its own end. Both load sites in `Level.c` switched from `memcpy` to
  `EniDec(header->map16, level_map16, 0)`.
- **Verification**: wrote a standalone harness linking `Enigma.c` directly,
  ran it against all 6 zones' compressed data, diffed output byte-for-byte
  against the previously-known-correct decompressed data (pulled from git
  history) — exact match on all 6, and the source pointer lands exactly at
  each compressed file's true end (proving the trailing
  pointer-alignment/rewind logic is also correct, not just the main
  decode loop).

## Level select

**Status: done.**

Ported `LevSelTextLoad`/`LevelSelect`/`Tit_ChkLevSel` from the
disassembly into `GM_Title.c`. Reuses the `Art_Text` font asset that was
already sitting unused in the resource pipeline (also used by
`HUD_WriteHex` for digit rendering) — no new asset extraction needed. 21
rows (all zones/acts + Special Stage + a present-but-inert Sound Select,
since sound wasn't playable at the time this was written), Up/Down
navigation, A/B/C/Start confirm.

Gated behind the real cheat code in release builds (Up, Down, Left, Right,
then hold A + Start — via `TitleCheatStep()`, a small state machine that
also restarts cleanly on a partial-match retry rather than requiring a
hard reset). **Debug builds** (`#ifndef NDEBUG`) skip the D-pad sequence
entirely — just hold A + Start.

Also fixed `PlayLevel()`: it had a "hold A → Special Stage" shortcut that
predated level select's existence (a stand-in shortcut). Once A became
required just to *enter* level select, that check would misfire on every
level picked from the menu (A is always held by the time `PlayLevel()`
runs). Removed — now matches the disassembly exactly (always a normal
level; Special Stage is its own row, same as real hardware).

## Debug/object-placement mode

**Status: cheat entry + minimal DebugMode() core done. Full feature NOT done.**

- Cheat entry: `C,C,C,C,Up,Down,Left,Right` (this project's own entry
  gesture — the original instead counts total C presses during the level
  select code to layer this on top; not replicated since slow motion,
  which shares that mechanism, isn't implemented) sets `debug_cheat`.
  `GM_Level.c`/`GM_Special.c` already had dormant `debug_cheat`-gated code
  from before this session; this made it reachable.
- Debug builds: `debug_cheat` forced on, and the "hold A" requirement for
  activating `debug_mode` on level start is also dropped — any level start
  enables it, for faster iteration while testing.
- **`Object/Sonic.c`'s `DebugMode()`**: was a stub
  (`// DebugMode(); return;` — Sonic simply vanished when `debug_use`
  activated, since nothing drew anything in its place). Implemented a
  minimal core: on entry, temporarily unlocks level Y-boundaries and shows
  a ring sprite in place of Sonic (item 0 is always a ring in every real
  zone's debug item list, per the disassembly's `DebugList` — used as a
  placeholder since item cycling isn't implemented yet); B exits back to
  normal Sonic; D-pad free-flies at a fixed speed (not yet the real speed
  ramp-up from held input).
- **Not implemented**: cycling through the object list (A/C presses),
  spawning objects, the per-zone `DebugList` item tables (the disassembly
  has ~15-30 entries per zone, each referencing this port's existing
  object mapping/art constants — real data-entry work, not just logic),
  and the user's specific design ideas for this port: right-stick L/R to
  change subtype, Y button to traverse backwards, and conditional HUD
  switching (normal ring/score HUD when Sonic — i.e. no object attached —
  is on screen, debug object-ID HUD when an object is).

## Pause + frame-advance

**Status: done.**

`PauseGame()` (new, shared between `GM_Level.c` and `GM_Special.c` via
`GM_Level.h`) ported from the disassembly: pressing Start (with ≥1 life,
so a game-over screen can't be paused) blocks in a loop running VBlank
routine `0x10` (Paused) until Start is pressed again. Frame-advance
(`Pause_SlowMo` in the original): holding B or pressing C while paused
runs exactly one real frame then re-pauses without needing Start again
(returns from `PauseGame()` with `pause_state` left `true`, so the very
next call re-enters the paused loop immediately). The original only allows
this behind the slow-motion cheat (not implemented — see above); made
available unconditionally here instead of locking it behind a cheat that
doesn't exist.

`pause_state` existed already (defined, reset at level start, checked once
in `AnimateLevelGfx` to gate animation updates) but nothing had ever set it
— pause was entirely non-functional before this.

## Input / controls

**Status: done.**

- **Gamepad support** (`Backend/SDL2/Input.c`): `SDL_GameController` (not
  raw `SDL_Joystick`), specifically so Steam Input's virtual controller —
  which SDL sees as a standard Xbox-style pad regardless of physical
  hardware or the user's own Steam Input remapping — works automatically.
  Hotplug-aware (device add/remove events), since Steam Input's virtual
  device can appear or disappear after startup (e.g. overlay taking it
  over). Mapping: D-pad + left stick → Genesis D-pad, X→A, A→B, B→C,
  Start/Options→Start.
- **Keyboard scheme reworked**: WASD (alongside arrows, no conflict) for
  movement, J/K/L for A/B/C — replacing the old A/S/D-for-jump scheme,
  which collided with using A/D for movement (likely why "jump always
  fails" was reported — a UX/keybinding mismatch, not a logic bug, though
  the fix generalized further per user request). Later user-edited (mid
  session, outside my changes) to B/N/M for A/B/C instead of J/K/L because
   it clashed with something else — respected as intentional, not reverted.
- **Real bug fixed**: `GM_Title.c` had `if (jpad1_press1 &= JPAD_START)`
  (assignment, not comparison) in two places on the title screen's
  Start-detection — a stray `&=` instead of `&`, corrupting the flags word
  as a side effect (harmless in that exact call site since the next
  frame's read overwrites it, but a real bug regardless).
- **VDP peek debug overlay** (VRAM/CRAM viewer): Tab or Select/Back toggles
  it — edge-detected (was previously un-debounced, flickering every frame
  while held). LB/RB/LT/RT pick CRAM palette 0-3, right stick (or O/L
  keys) pages through VRAM. **Compiled out entirely in release builds**
  (`#ifndef NDEBUG`) — a release build shouldn't expose a VRAM/CRAM dump to
  players.
- **`tests/joypad_test_main.c`** (`SonicJoypadTest`): standalone SDL2 tool,
  a row of 8 icons (4 direction triangles, A/B/C circles, Start square)
  that light from dim to full brightness while held. Reads input through
  the exact same `Joypad_GetState1()`/`Input_HandleEvents()` path the real
  game uses, so it's a live check of both the keyboard scheme and any
  connected gamepad. Not registered as a ctest (interactive tool).

## Background rendering fixes

**Status: done.**

- **MZ/SBZ backgrounds**: the port's `MZ_ScrollArray`/`SBZ_ScrollArray`
  tables were fabricated data that didn't match the real ROM's
  `BG_ScrollBlockMap_MZ`/`_SBZ` tables at all. Also missing: MZ's real
  `-512` row-index bias, the `+1` table-pointer offset both MZ and SBZ use,
  and the diagonal-scroll "column scan" path (`DrawBG_ColumnForBGIndex`)
  had its X/Y axes swapped and was unimplemented for SBZ entirely.
  Rewrote against the disassembly; tables padded with safe zero-bytes
  past their real length, since the real ROM's column scan can walk past
  the nominal table end into whatever adjacent ROM bytes follow (a
  hardware quirk not reproducible/desirable in a clean C port).
- **Universal fallback-path bug** (affected GHZ, MZ, SBZ alike): the
  "background row index 0" fallback drew via `DrawBlocks_LR_2` at a full
  `PLANE_WIDTH` (64 blocks = 1024px) instead of `PLANE_WIDTH/2` (32 blocks
  = 512px, the plane's real width) — double-wrapping the destination VRAM
  position and drawing over itself with the wrong tiles. This was the
  direct cause of a user-reported screenshot showing a wavy/wrong texture
  in MZ's sky. Fixed to use `DrawBlocks_LR_3` (REV01's VRAM-wrapping
  variant) at the correct width, matching the disassembly's
  `DrawBG_RowForBGIndex` `.bgXPos0` branch exactly — and matching every
  other "draw a full-width strip" call already in the same file.

## LZ water features

**Status: partial — surface sway/ripple done, water height scripting not done.**

`LZWaterFeatures()` was referenced (called from `GM_Level.c`'s main loop)
but never implemented — the call was commented out, which is why LZ's
water surface never animated. Implemented the surface-sway portion
(`src/LZWaterFeatures.c`, new file): a small continuous bob layered on top
of the water height via oscillator 0, updating `hbla_counter`/the VDP's
h-int line and `wtr_state` (whole-screen-underwater flag) every frame.

**Not implemented**: `LZWindTunnels`/`LZWaterSlides`/`LZDynamicWater` — the
per-act scripted water height changes, wind tunnels, and water slides.
`wtr_pos2` (actual, un-swayed water height) just stays wherever level init
left it (0) until those are ported.

## Known follow-ups, roughly in dependency order

1. **Sound playback driver** — reads `sound_channels[]`/song byte streams
   frame-by-frame, drives `SN76489_Write`-equivalent register updates.
   Nothing is audible yet without this, regardless of how correct the
   encoded song data or chip emulation are.
2. **ymfm C++ wrapper** — needed for FM instruments/music to sound right
   (PSG-only songs, like most SFX, don't need it).
3. **`SonicSoundTest`** tool — depends on (1). User's spec: piano-key-style
   display, keys light on note-on events; hex/drum pad flashes red on DAC
   sample-trigger events. Reference screenshot showed FM1-6/PSG1-3/DAC
   channel activity bars plus a song selector.
4. Full song/SFX catalog import (2 of ~20+ done).
5. Full `DebugMode()` — object cycling/spawning, per-zone `DebugList`
   tables, RStick subtype/Y-reverse/conditional-HUD ideas.
6. Demo-recorder debug HUD tweak (show debug hex overlay without the
   "RINGS"/"TIME"/"SCOR" static labels) — was in progress, not finished;
   the label graphics come from a separate PLC-loaded `Art_HUD` asset
   (VRAM `0xD940`), not `hud_cmd_base` as first assumed.
7. `smps2asmc`/song-builder cross-compilation isolation (see pipeline
   section above).
