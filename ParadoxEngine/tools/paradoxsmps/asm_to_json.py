#!/usr/bin/env python3
"""ParadoxSMPS asm-to-json converter -- migration tool, not part of the CMake
build (see project_paradoxsmps_schema memory for the full schema design this
implements). Converts a real Sega/SMPS-style song or SFX .asm file (using the
smps2asm macro system, see sound/_smps2asm_inc.asm) into ParadoxSMPS JSONC.

Deliberately conservative: notes/DAC-samples/tone-names are preserved as
opaque identifier strings exactly as written in the source (see the schema
memory's own reasoning -- resolving them to numeric byte values is the
COMPILER's job, not the converter's), and any control-flow construct that
doesn't cleanly fold into a nested subblock is left as an explicit flagged
"jumpTo" escape hatch rather than silently guessed at.

Usage: asm_to_json.py input.asm > output.jsonc
"""

import json
import re
import sys

# NOTE: standard JSON (and JSONC -- comments are its only extension) has no
# hex numeric literal syntax at all, unlike C's 0x81. To honor "C-style hex,
# not asm $-style" while staying valid JSON, HexInt values are rendered as
# quoted strings ("0x81"), not bare numbers. Flagged explicitly since this
# wasn't resolved during schema design -- easy to switch to plain decimal
# numbers instead if quoted-hex-strings turns out to be the wrong call.

# ---------------------------------------------------------------------------
# Tokenizing: turn the raw .asm text into a flat list of line records, each
# either a label definition, an instruction (mnemonic + args), or blank.
# Comments are captured (not discarded) per the schema's "carry over source
# comments" decision.
# ---------------------------------------------------------------------------


class Line:
    __slots__ = ("label", "mnemonic", "args", "comment", "raw")

    def __init__(self, label=None, mnemonic=None, args=None, comment=None, raw=""):
        self.label = label
        self.mnemonic = mnemonic
        self.args = args or []
        self.comment = comment
        self.raw = raw


def split_comment(text):
    # ';' always starts a comment in this format -- no strings/escapes to
    # worry about in smps2asm source.
    idx = text.find(";")
    if idx == -1:
        return text, None
    return text[:idx], text[idx + 1 :].strip()


def split_args(text):
    # Args are comma-separated; whitespace-trimmed. No nested commas/parens
    # appear in this format's argument lists.
    text = text.strip()
    if not text:
        return []
    return [a.strip() for a in text.split(",")]


# Compile-time conditional-assembly flags this project's own songs actually
# reference (found via direct grep across the whole corpus -- only this one
# flag appears anywhere). Value matches Sonic1PC's own CMakeLists.txt
# default (`option(FIX_BUGS ... OFF)` -> `SONG_FIX_BUGS=0`), so resolving
# conditionals against this produces the exact same branch the real shipped
# game actually builds with, not an arbitrary/guessed choice.
ASSEMBLER_FLAGS = {"FixMusicAndSFXDataBugs": 0}


def _eval_condition(expr):
    # Real conditions seen in this corpus are always either a bare flag name
    # (truthy check) or `flag=N` -- no more complex expressions appear
    # anywhere in the actual song/SFX files, so this deliberately doesn't
    # implement a general expression evaluator.
    m = re.match(r"^(\w+)\s*=\s*(-?\d+)$", expr)
    if m:
        flag, value = m.group(1), int(m.group(2))
        return ASSEMBLER_FLAGS.get(flag, 0) == value
    return bool(ASSEMBLER_FLAGS.get(expr.strip(), 0))


def tokenize(path):
    lines = []
    # Conditional-assembly state: a stack of booleans, one per nested
    # if/else/endif, tracking whether the CURRENT branch's lines should be
    # kept. This is purely a compile-time source-selection mechanism (which
    # branch existed in the .asm at all) -- it doesn't need to survive into
    # the JSON, so resolved lines from the untaken branch are dropped
    # entirely rather than represented as some runtime construct.
    active_stack = []

    def currently_active():
        return all(active_stack)

    with open(path, "r", encoding="utf-8") as f:
        for raw_line in f:
            code, comment = split_comment(raw_line.rstrip("\n"))
            stripped = code.strip()

            m_if = re.match(r"^if\s+(.+)$", stripped)
            m_else = re.match(r"^else\s*$", stripped)
            m_endif = re.match(r"^endif\s*$", stripped)
            if m_if:
                active_stack.append(_eval_condition(m_if.group(1)))
                continue
            if m_else:
                if active_stack:
                    active_stack[-1] = not active_stack[-1]
                continue
            if m_endif:
                if active_stack:
                    active_stack.pop()
                continue
            if not currently_active():
                continue  # skip every line in a not-taken conditional branch

            if not stripped:
                if comment is not None:
                    lines.append(Line(comment=comment, raw=raw_line))
                continue

            label = None
            # A bare "Name:" line (label definition, possibly with nothing
            # else on the line -- the common case in these files).
            m = re.match(r"^(\w+):\s*$", stripped)
            if m:
                lines.append(Line(label=m.group(1), comment=comment, raw=raw_line))
                continue

            # Otherwise: <mnemonic> <args...>  (mnemonic is the first
            # whitespace-delimited token; smps2asm always puts exactly one
            # instruction per line in this codebase's style).
            parts = stripped.split(None, 1)
            mnemonic = parts[0]
            args = split_args(parts[1]) if len(parts) > 1 else []
            lines.append(Line(mnemonic=mnemonic, args=args, comment=comment, raw=raw_line))
    return lines


# ---------------------------------------------------------------------------
# Numeric literal handling -- source uses asm-style $XX hex; schema wants
# C-style 0xXX. Decimal stays decimal.
# ---------------------------------------------------------------------------


def parse_number(tok):
    if tok.startswith("$"):
        return int(tok[1:], 16)
    if re.match(r"^-?\d+$", tok):
        return int(tok)
    return None  # not a plain number -- caller should treat as an identifier


def num_to_json(n):
    # Render as the schema's C-style hex convention when it reads more
    # naturally that way (matches how these values are always written as
    # hex in the source); small values still fine as hex too, JSON doesn't
    # care either way, this is purely for human readability of the output.
    if n < 0:
        return n
    return n  # emit as plain JSON number; a custom encoder below renders hex


class HexInt(int):
    """Marks an int that should be rendered as 0x.. in the output JSON."""


def hx(tok_or_int):
    if isinstance(tok_or_int, str):
        n = parse_number(tok_or_int)
        if n is None:
            return tok_or_int  # not actually numeric -- pass through as-is (e.g. a named constant)
        return HexInt(n)
    return HexInt(tok_or_int)


# ---------------------------------------------------------------------------
# Macro catalog -- shape of every smps* macro this converter understands,
# per the audited catalog in project_paradoxsmps_schema. "multi" = positional
# array; "single" = bare value; "zero" = bare string; "block" = control-flow
# (handled separately, not through this table).
# ---------------------------------------------------------------------------

ZERO_ARG = {
    "smpsStop",
    "smpsClearPush",
    "smpsStopSpecial",
    "smpsWeirdD1LRR",
    "smpsFade",
    "smpsReturn",
    "smpsNoAttack",  # not really a macro (bare $E7 token) but handled the same way
}
SINGLE_ARG = {
    "smpsChanTempoDiv",
    "smpsSetTempoDiv",
    "smpsSetTempoMod",
    "smpsAlterPitch",
    "smpsNop",
    "smpsAlterVol",
    "smpsPSGAlterVol",
    "smpsAlterNote",
    "smpsNoteFill",
    "smpsPSGform",
    "smpsPSGvoice",
    "smpsSetvoice",
}
MULTI_ARG = {
    "smpsPan",
    "smpsModSet",
}
BLOCK_MACROS = {"smpsLoop", "smpsJump", "smpsCall"}

# ---------------------------------------------------------------------------
# SMPS driver-version-3 (Sonic 3/Flamedriver-compatible) mnemonics -- same
# name/shape conventions as json_to_header.py's own SIMPLE_OPS_V3/
# ZERO_ARG_OPS_V3 (bare string / single value / positional array), so a real
# Flamedriver-driven .asm source (matching this project's naming exactly,
# see /home/deck/Projects/flamedriver/Flamedriver.asm) converts straight
# through the SAME machinery below as the driver-version-1 sets above.
#
# NOT included here: cfPanningAMSFMS/cfModulation/cfSetVoice (need the
# same special-cased handling smpsPan/smpsModSet/smpsSetvoice-with-variant-
# arg-count get elsewhere -- see convert_instruction's own v3 branch below),
# and the 5 jump/loop/gosub-target flags (cfConditionalJump, cfRepeatAtPos,
# cfJumpTo, cfJumpToGosub, cfLoopContinuousSFX) -- those need the same
# control-flow-folding treatment smpsLoop/smpsJump/smpsCall get below, which
# is currently hardcoded to those 3 mnemonic names specifically (not
# parameterized by BLOCK_MACROS membership) across several functions in this
# file. Generalizing that fold is real future work, deferred until an actual
# driverVersion-3 source file needs importing -- until then, those 5
# mnemonics will fall through to convert_instruction's "unrecognized macro"
# passthrough if encountered, which is honest (visibly flags the gap) rather
# than silently producing wrong output.
ZERO_ARG_V3 = {
    "cfPreventAttack",
    "cfSilenceStopTrack",
    "cfStopTrack",
    "cfJumpReturn",
    "cfDisableModulation",
    "cfResetSpindashRev",
}
SINGLE_ARG_V3 = {
    "cfDetune",
    "cfFadeInToPrevious",
    "cfSetVolume",
    "cfChangeVolume",
    "cfNoteFill",
    "cfPlayDACSample",
    "cfChangePSGVolume",
    "cfSetKey",
    "cfSetPSGNoise",
    "cfSetModulation",
    "cfSetPSGVolEnv",
    "cfChangeTransposition",
    "cfToggleAltFreqMode",
    "cfSetTempo",
    "cfPlaySFXByIndex",
    "cfHaltSound",
    "cfSetTempoDivider",
    "cfChanSetTempoDivider",
    "cfNoteFillSet",
    "cfPitchSlide",
    "cfPlayMusicByIndex",
}
MULTI_ARG_V3 = {
    "cfChangeVolume2",  # [discarded_byte, delta]
    "cfSendFMI",  # [reg, data]
    "cfAlterModulation",  # [byte_for_psg, byte_for_fm]
    "cfFM3SpecialMode",  # 4 raw bytes
    "cfSetSSGEG",  # 4 raw bytes
    "cfFMVolEnv",  # [envIndex, operatorMask]
    "cfChanFMCommand",  # [reg, data]
    "cfSetLFO",  # [lfoByte, panByte]
}
ALL_MNEMONICS_V3 = ZERO_ARG_V3 | SINGLE_ARG_V3 | MULTI_ARG_V3 | {
    "cfPanningAMSFMS", "cfModulation", "cfSetVoice",
    "cfConditionalJump", "cfRepeatAtPos", "cfJumpTo", "cfJumpToGosub", "cfLoopContinuousSFX",
}

# Voice-bank macros (op1..op4 positional, matches VoiceParams::op[] order).
VOICE_MULTI = {
    "smpsVcDetune",
    "smpsVcCoarseFreq",
    "smpsVcRateScale",
    "smpsVcAttackRate",
    "smpsVcAmpMod",
    "smpsVcDecayRate1",
    "smpsVcDecayRate2",
    "smpsVcDecayLevel",
    "smpsVcReleaseRate",
    "smpsVcTotalLevel",
}
VOICE_SINGLE = {"smpsVcAlgorithm", "smpsVcFeedback", "smpsVcUnusedBits"}

HEADER_MACROS = {
    "smpsHeaderStartSong",
    "smpsHeaderVoice",
    "smpsHeaderChan",
    "smpsHeaderTempo",
    "smpsHeaderDAC",
    "smpsHeaderFM",
    "smpsHeaderPSG",
    "smpsHeaderTempoSFX",
    "smpsHeaderChanSFX",
    "smpsHeaderSFXChannel",
}


def args_to_json(args):
    return [hx(a) for a in args]


def note_event(tok):
    # "nE7" -> {"note": "E7"}; "dKick" -> {"note": "kick", "dac": true}-ish --
    # keep it simple: strip the leading n/d, preserve the rest verbatim. DAC
    # sample names and FM/PSG note names share the same "note" field per the
    # schema (same event shape, different value vocabulary by channel type).
    return {"note": tok[1:]}


def is_note_token(tok):
    return bool(re.match(r"^[nd][A-Za-z0-9]", tok))


def is_number_token(tok):
    return parse_number(tok) is not None


# ---------------------------------------------------------------------------
# dc.b/dc.w note-stream -> events. Implements the note/duration/tie model
# from project_paradoxsmps_schema: bare-duration tokens (no preceding note in
# this immediate position) are collapsed into an explicit repeated note at
# the last-seen pitch, matching what the real driver actually does (traced
# against FMDoNext/PSGDoNext/DACUpdateTrack -- always retriggers, never a
# silent tie) rather than inventing a distinct "duration-only" event type.
# ---------------------------------------------------------------------------


# (Stateful across multiple dc.b/dc.w lines within one block -- see
# NoteStreamState below, which implements this same logic while carrying
# `last_note` continuity across line boundaries, matching how the real
# driver reads the whole track as one continuous byte stream regardless of
# where the source's dc.b lines happen to break.)


# ---------------------------------------------------------------------------
# Per-instruction-line conversion (everything except dc.b/dc.w and control
# flow, which are handled by their own passes).
# ---------------------------------------------------------------------------


def convert_instruction(line):
    m = line.mnemonic
    if m in ZERO_ARG:
        return m
    if m in SINGLE_ARG:
        return {m: hx(line.args[0]) if line.args else None}
    if m in MULTI_ARG:
        return {m: args_to_json(line.args)}
    if m == "smpsMod":  # not a real source mnemonic; smpsModOn/Off map to it below
        pass
    if m == "smpsModOn":
        return {"smpsMod": True}
    if m == "smpsModOff":
        return {"smpsMod": False}
    # SMPS driver-version-3 mnemonics -- see ALL_MNEMONICS_V3's own comment
    # for what's NOT covered here (the 5 jump/loop/gosub-target flags).
    if m in ZERO_ARG_V3:
        return m
    if m in SINGLE_ARG_V3:
        return {m: hx(line.args[0]) if line.args else None}
    if m in MULTI_ARG_V3:
        return {m: args_to_json(line.args)}
    if m == "cfPanningAMSFMS":
        return {m: args_to_json(line.args)}
    if m == "cfModulation":
        return {m: args_to_json(line.args)}
    if m == "cfSetVoice":
        return {m: args_to_json(line.args) if len(line.args) > 1 else hx(line.args[0])}
    # Unknown mnemonic -- pass through raw so nothing silently vanishes; a
    # human needs to look at this rather than have data quietly dropped.
    return {"_unrecognized_macro": m, "_args": args_to_json(line.args)}


# ---------------------------------------------------------------------------
# Control-flow folding: flat (label-or-None, event) sequence -> nested
# subblocks, via a stack matching smpsLoop/self-smpsJump targets against the
# most recently opened, not-yet-closed label -- same discipline as balanced
# parentheses -- BUT a label's position is remembered persistently (never
# discarded once "closed"), not popped off a stack. This is what makes
# RE-ENTRANT labels work: GHZ's PSG1 has `Loop13` closed once by its own
# smpsLoop (an inner bounded repeat), then referenced AGAIN later by an
# unconditional smpsJump that means "repeat forever, starting from Loop13's
# original position" -- which by then includes the already-folded inner
# smpsLoop block as part of its own body. A pop-and-discard stack can't
# represent that (the first WORKING version of this function couldn't --
# found via real GHZ output, see the conversation this came from); a
# persistent position table naturally does, because folding a label always
# collapses "everything from its remembered position to the current end of
# the list" into one item IN PLACE, and simply leaves that same position
# recorded afterward. If the label is never referenced again, the recorded
# position is just unused history. Provably safe against staleness: since
# processing is a single forward pass and every fold only ever replaces a
# SUFFIX of the list (position -> current end, never a mid-list slice), any
# earlier-recorded label position is never touched by a later fold -- list
# slice-assignment past a given index cannot move anything before it.
def fold_control_flow(flat_items, block_name):
    # flat_items: list of ("label", name) | ("event", value) | ("loop", [idx,cnt,target]) | ("jump", target)
    items = []
    label_pos = {}  # name -> index into `items` where that label's scope currently begins
    warnings = []

    for kind, payload in flat_items:
        if kind == "label":
            label_pos[payload] = len(items)
        elif kind == "event":
            items.append(payload)
        elif kind == "loop":
            index, count, target = payload
            start = label_pos.get(target)
            if start is None or start > len(items):
                warnings.append(f"{block_name}: smpsLoop target '{target}' has no matching label in this block "
                                 f"-- emitting flat jumpTo fallback, needs manual review")
                items.append({"jumpTo": target, "smpsLoopArgs": [hx(index), hx(count)]})
            else:
                body = items[start:]
                items[start:] = [{"smpsLoop": [hx(index), hx(count)] + body}]
                # label_pos[target] intentionally left pointing at `start` --
                # a later re-reference should scoop up this fold result too.
        elif kind == "jump":
            target = payload
            start = label_pos.get(target)
            if start is None or start > len(items):
                warnings.append(f"{block_name}: smpsJump target '{target}' has no matching label in this block "
                                 f"-- emitting flat jumpTo fallback, needs manual review")
                items.append({"jumpTo": target})
            else:
                body = items[start:]
                items[start:] = [{"smpsJump": body}]  # no count = forever

    return items, warnings


# ---------------------------------------------------------------------------
# Header + voice bank parsing.
# ---------------------------------------------------------------------------


def parse_header(lines, idx):
    header = []
    while idx < len(lines):
        line = lines[idx]
        if line.label is not None:
            break  # any label (the first real track/voice-bank label) ends the header section
        if line.mnemonic is None:
            idx += 1  # blank/comment-only line inside the header block -- skip, keep looking
            continue
        if line.mnemonic not in HEADER_MACROS:
            break
        header.append({line.mnemonic: args_to_json(line.args)})
        idx += 1
    return header, idx


VOICE_ORDER = [
    "smpsVcAlgorithm", "smpsVcFeedback", "smpsVcUnusedBits", "smpsVcDetune", "smpsVcCoarseFreq",
    "smpsVcRateScale", "smpsVcAttackRate", "smpsVcAmpMod", "smpsVcDecayRate1", "smpsVcDecayRate2",
    "smpsVcDecayLevel", "smpsVcReleaseRate", "smpsVcTotalLevel",
]


def _skip_blank(lines, idx):
    while idx < len(lines) and lines[idx].mnemonic is None and lines[idx].label is None:
        idx += 1
    return idx


def parse_voice_bank(lines, start_idx):
    """lines[start_idx] should be the voice bank's own label; voices follow
    as repeated 13-macro groups (each preceded by a blank/comment run -- the
    hex-dump-style ";  Voice $00 / ; $04 / ..." block every real voice has)
    until a non-voice mnemonic or EOF."""
    idx = _skip_blank(lines, start_idx + 1)  # skip the bank's own label line, then any comment block
    voices = []
    while True:
        idx = _skip_blank(lines, idx)
        if idx >= len(lines) or lines[idx].mnemonic != "smpsVcAlgorithm":
            break
        group = {}
        for expected in VOICE_ORDER:
            if idx >= len(lines) or lines[idx].mnemonic != expected:
                break
            group[expected] = args_to_json(lines[idx].args)
            idx += 1
        alg = group.pop("smpsVcAlgorithm", [0])[0]
        fb = group.pop("smpsVcFeedback", [0])[0]
        ub = group.pop("smpsVcUnusedBits", [0])[0]
        ops = [{} for _ in range(4)]
        for key, values in group.items():
            for op_i in range(4):
                ops[op_i][key] = values[op_i]
        voices.append({"smpsVcAlgorithm": alg, "smpsVcFeedback": fb, "smpsVcUnusedBits": ub, "operators": ops})
    return voices, idx


# ---------------------------------------------------------------------------
# Main driver
# ---------------------------------------------------------------------------


def find_header_start_labels(header):
    starts = set()
    for entry in header:
        for macro_name, args in entry.items():
            if macro_name in ("smpsHeaderFM", "smpsHeaderPSG", "smpsHeaderDAC") and args:
                starts.add(str(args[0]))
            elif macro_name == "smpsHeaderSFXChannel" and len(args) > 1:
                starts.add(str(args[1]))
    return starts


class NoteStreamState:
    def __init__(self):
        self.last_note = None

    def consume(self, tokens):
        events = []
        i = 0
        while i < len(tokens):
            tok = tokens[i]
            if is_note_token(tok):
                self.last_note = tok
                if i + 1 < len(tokens) and is_number_token(tokens[i + 1]):
                    events.append({"note": tok[1:], "duration": hx(tokens[i + 1])})
                    i += 2
                else:
                    events.append({"note": tok[1:]})
                    i += 1
            elif is_number_token(tok):
                # A bare duration immediately after smpsNoAttack is a
                # genuinely different construct from an ordinary bare
                # duration: NoAttack suppresses the retrigger, and with no
                # note byte at all there's no frequency update either --
                # real hardware just extends whatever note is ALREADY
                # sounding (inherited from the calling context, since
                # smpsCall never resets "current note") by this many more
                # ticks. It doesn't need -- and in general (e.g. a smpsCall
                # target reached from multiple different callers with
                # different active notes each time) CAN'T have -- a single
                # resolvable note name, unlike an ordinary bare duration.
                # Found via real data: Mus83_MZ_Call03 opens with exactly
                # this (smpsNoAttack then bare $03), called from two
                # different points in FM3 with different preceding notes.
                if events and events[-1] == "smpsNoAttack":
                    events.append({"tie": True, "duration": hx(tok)})
                elif self.last_note is None:
                    # A bare duration with genuinely no local note anywhere
                    # in this block (not even earlier in the SAME block) --
                    # NOT an error, same root cause as "tie" above minus the
                    # smpsNoAttack suppression: this block is a smpsCall
                    # target, and smpsCall never resets "current note", so
                    # this retriggers at whatever pitch the CALLING context
                    # left active. Genuinely can't resolve to one static
                    # note name (found via real data: Mus82_LZ_Call06 and
                    # Mus91_Credits_Call0F both open with several of these,
                    # called from multiple different points with different
                    # active notes each time) -- unlike "tie", this DOES
                    # retrigger, it just inherits its pitch at playback time
                    # instead of compile time.
                    events.append({"inheritedNote": True, "duration": hx(tok)})
                else:
                    events.append({"note": self.last_note[1:], "duration": hx(tok)})
                i += 1
            elif tok == "smpsNoAttack":
                events.append("smpsNoAttack")
                i += 1
            else:
                events.append({"_unrecognized_token": tok})
                i += 1
        return events


def _walk_segments(lines, idx, voice_bank_idx, top_level_labels, on_label, on_jump_or_loop):
    # Shared walk used by the cross-segment-target detector below: tracks
    # "current segment name" exactly the way convert_file's real
    # segment-splitting pass does (a label starts a new segment iff it's in
    # top_level_labels), skipping over the voice bank region, and invokes a
    # callback for each label definition and each smpsJump/smpsLoop
    # instruction encountered.
    current = None
    i = idx
    while i < len(lines):
        line = lines[i]
        if i == voice_bank_idx:
            _, next_i = parse_voice_bank(lines, voice_bank_idx)
            i = next_i
            continue
        if line.label is not None:
            if line.label in top_level_labels:
                current = line.label
            on_label(line.label, current)
            i += 1
            continue
        if line.mnemonic == "smpsJump" and line.args:
            on_jump_or_loop(current, line.args[0])
        elif line.mnemonic == "smpsLoop" and len(line.args) >= 3:
            on_jump_or_loop(current, line.args[2])
        i += 1


# Cross-channel jump sharing: real SMPS tracks sometimes have one channel
# smpsJump straight into a label that lives inside a DIFFERENT channel's
# already-written data (found via Mus82_LZ: FM4 ends with
# `smpsJump Mus82_LZ_Jump01`, a label that's physically inside FM3's block --
# the real space-saving trick of reusing another channel's already-compiled
# tail instead of duplicating it). A label like that can't fold into its
# defining segment as an ordinary nested subblock, because content jumped to
# from OUTSIDE that segment needs to be its own named SMPSplaylist entry (so
# the other segment's "jumpTo" can resolve against it by name, same as any
# other cross-block reference in this schema -- see "start" pointers in the
# header). This promotes any such label to a top-level segment boundary,
# iterating to a fixpoint since promoting one label can shift a segment's
# tail into a new segment, which can itself turn a previously in-segment
# jump into a newly cross-segment one.
def promote_cross_segment_targets(lines, idx, voice_bank_idx, top_level_labels):
    top_level_labels = set(top_level_labels)
    while True:
        label_segment = {}
        _walk_segments(lines, idx, voice_bank_idx, top_level_labels,
                        on_label=lambda name, seg: label_segment.__setitem__(name, seg),
                        on_jump_or_loop=lambda seg, target: None)
        extra = set()

        def on_jump_or_loop(seg, target):
            target_seg = label_segment.get(target)
            if target_seg is not None and target_seg != seg:
                extra.add(target)

        _walk_segments(lines, idx, voice_bank_idx, top_level_labels,
                        on_label=lambda name, seg: None,
                        on_jump_or_loop=on_jump_or_loop)
        new_labels = extra - top_level_labels
        if not new_labels:
            return top_level_labels
        top_level_labels |= new_labels


def convert_file(path):
    lines = tokenize(path)
    idx = 0
    # Skip leading blank/comment padding, then the file's own top-level
    # "SongName_Header:" label, before the header macros begin.
    while idx < len(lines) and lines[idx].mnemonic is None and lines[idx].label is None:
        idx += 1
    if idx < len(lines) and lines[idx].label is not None:
        idx += 1
    header, idx = parse_header(lines, idx)
    top_level_labels = find_header_start_labels(header)

    # Find the voice bank: the label smpsHeaderVoice points at.
    voice_bank_label = None
    for entry in header:
        if "smpsHeaderVoice" in entry and entry["smpsHeaderVoice"]:
            voice_bank_label = str(entry["smpsHeaderVoice"][0])

    # Pre-scan the rest of the file for every smpsCall target -- those
    # labels are also top-level SMPSplaylist entries, not same-block loop
    # targets.
    for line in lines[idx:]:
        if line.mnemonic == "smpsCall" and line.args:
            top_level_labels.add(line.args[0])

    # Locate the voice bank's own line index (by label) early -- needed by
    # the cross-segment-target promotion pass below, which must also skip
    # over the voice bank region the same way the real segment-splitting
    # pass does.
    voice_bank_idx = None
    for i in range(idx, len(lines)):
        if lines[i].label == voice_bank_label:
            voice_bank_idx = i
            break

    top_level_labels = promote_cross_segment_targets(lines, idx, voice_bank_idx, top_level_labels)

    voices = []
    if voice_bank_idx is not None:
        voices, _ = parse_voice_bank(lines, voice_bank_idx)

    # Build segments: everything from idx to EOF, excluding the voice bank
    # region, split at top-level-entry label boundaries.
    playlist = {}
    warnings = []
    current_name = None
    current_flat = []
    note_state = None
    used_driver_v3 = [False]  # list so the nested loop below can mutate it (no nonlocal needed for a single flag read)

    def flush():
        nonlocal current_name, current_flat, note_state
        if current_name is not None:
            folded, w = fold_control_flow(current_flat, current_name)
            playlist[current_name] = folded
            warnings.extend(w)
        current_flat = []
        note_state = NoteStreamState()

    note_state = NoteStreamState()
    i = idx
    while i < len(lines):
        line = lines[i]
        if i == voice_bank_idx:
            # Skip the whole voice bank region -- already parsed above.
            _, next_i = parse_voice_bank(lines, voice_bank_idx)
            i = next_i
            continue
        if line.label is not None:
            if line.label in top_level_labels:
                flush()
                current_name = line.label
                # Seed the block's own name as a fold target at position 0,
                # same as any internal label -- many real tracks end with
                # `smpsJump <OwnTrackName>` meaning "loop the whole track
                # forever from its own start", which needs this to resolve
                # (found via batch-converting the full song/SFX corpus:
                # without this, every one of those showed up as a false
                # "no matching label" warning).
                current_flat.append(("label", line.label))
            else:
                current_flat.append(("label", line.label))
            i += 1
            continue
        if line.mnemonic is None:
            i += 1
            continue
        if line.mnemonic in ("dc.b", "dc.w"):
            current_flat.extend(("event", e) for e in note_state.consume(line.args))
        elif line.mnemonic == "smpsLoop":
            index, count, target = line.args[0], line.args[1], line.args[2]
            current_flat.append(("loop", (hx(index), hx(count), target)))
        elif line.mnemonic == "smpsJump":
            current_flat.append(("jump", line.args[0]))
        elif line.mnemonic == "smpsCall":
            current_flat.append(("event", {"smpsCall": line.args[0]}))
        elif line.mnemonic == "smpsReturn":
            pass  # block end IS the implicit return -- no event emitted, per schema
        else:
            if line.mnemonic in ALL_MNEMONICS_V3:
                used_driver_v3[0] = True
            current_flat.append(("event", convert_instruction(line)))
        i += 1
    flush()

    for w in warnings:
        print(f"WARNING: {w}", file=sys.stderr)

    result = {"header": header, "voices": voices, "SMPSplaylist": playlist}
    if used_driver_v3[0]:
        # At least one SMPS driver-version-3 (Sonic 3/Flamedriver-compatible)
        # mnemonic was seen -- tag the whole song so json_to_header.py/
        # compiler.c compile it against the matching flag table instead of
        # the driver-version-1 default.
        result["driverVersion"] = 3
    return result


class HexEncoder(json.JSONEncoder):
    def default(self, o):
        if isinstance(o, HexInt):
            return f"0x{o:X}" if o >= 0 else str(o)
        return super().default(o)

    def iterencode(self, o, _one_shot=False):
        # HexInt needs to render as a JSON *string* (quoted), not a bare
        # number -- json's default() hook only fires for genuinely
        # unrecognized types, but HexInt IS an int subclass so the base
        # encoder would emit it as a bare number. Pre-walk and convert.
        return super().iterencode(_hexify(o), _one_shot)


def _hexify(o):
    if isinstance(o, HexInt):
        return f"0x{o:X}" if o >= 0 else str(int(o))
    if isinstance(o, dict):
        return {k: _hexify(v) for k, v in o.items()}
    if isinstance(o, list):
        return [_hexify(v) for v in o]
    return o


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: asm_to_json.py input.asm > output.jsonc", file=sys.stderr)
        sys.exit(1)
    result = convert_file(sys.argv[1])
    print(json.dumps(result, indent=2, cls=HexEncoder))
