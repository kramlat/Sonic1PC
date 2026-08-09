#!/usr/bin/env python3
"""ParadoxSMPS JSON-to-C-header compiler -- reads a ParadoxSMPS JSONC song/SFX
file (see project_paradoxsmps_schema memory / tools/paradoxsmps/asm_to_json.py
for the format this consumes) and emits a C header with the compiled byte
array directly, skipping the separate binary+bin2h step entirely (user's own
suggestion -- "a straight header compiler to skip the step of binary").

Byte-level format matches the real SonicDriverVer=1 SMPS compiled format
exactly (see sound/_smps2asm_inc.asm), so the output is readable by the
EXISTING Sound.c/FM_LoadVoice pipeline unchanged -- this compiler produces
the same bytes the original .asm+smps2asmc pipeline would, just from JSON
input instead of macro calls.

Usage: json_to_header.py input.jsonc output.h ArrayName
"""

import json
import re
import sys

# ---------------------------------------------------------------------------
# JSONC -> dict: strip // and /* */ comments (JSONC's only extension over
# plain JSON), then a straight json.loads. Also unwraps this project's
# quoted-hex-string convention ("0x1B" -> 0x1B) back into real ints.
# ---------------------------------------------------------------------------


def strip_jsonc_comments(text):
    # Line comments and block comments, skipping over string literals so a
    # ';' or '//' inside a quoted note name etc. is never misread. No
    # backslash-escape handling needed beyond \" -- this format never
    # contains anything more exotic in its strings.
    out = []
    i = 0
    in_string = False
    while i < len(text):
        c = text[i]
        if in_string:
            out.append(c)
            if c == "\\" and i + 1 < len(text):
                out.append(text[i + 1])
                i += 2
                continue
            if c == '"':
                in_string = False
            i += 1
            continue
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue
        if c == "/" and i + 1 < len(text) and text[i + 1] == "/":
            while i < len(text) and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < len(text) and text[i + 1] == "*":
            i += 2
            while i + 1 < len(text) and not (text[i] == "*" and text[i + 1] == "/"):
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def unhex(value):
    if isinstance(value, str) and re.match(r"^-?0x[0-9A-Fa-f]+$", value):
        return int(value, 16)
    if isinstance(value, str) and re.match(r"^-?\d+$", value):
        return int(value)
    if isinstance(value, str) and value in CHANNEL_ID:
        return CHANNEL_ID[value]
    if isinstance(value, str) and re.match(r"^fTone_[0-9A-Fa-f]+$", value):
        # fTone_XX names encode their own value (see emit_event's smpsPSGvoice
        # handling for the same trick) -- also appears as smpsHeaderPSG's
        # "voice" arg (its initial tone), not just in-track smpsPSGvoice events.
        return int(value.rsplit("_", 1)[-1], 16)
    return value


def load_jsonc(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.loads(strip_jsonc_comments(f.read()))


# ---------------------------------------------------------------------------
# Note-name -> raw byte value. Computed mathematically from the standard
# 12-semitone-per-octave chromatic layout (matches sound/_smps2asm_inc.asm's
# enum sequence exactly: nRst=$80, nC0=$81, chromatic upward from there,
# enharmonic spellings -- Db/Cs etc. -- landing on the identical value by
# construction) rather than hand-transcribing the enum's alias list.
# ---------------------------------------------------------------------------

NOTE_BASE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}

# smpsHeaderSFXChannel's chanid argument -- named channel-ID constants (see
# _smps2asm_inc.asm's own EQUs). FM1/FM2 aren't given named constants there
# (no real file converted so far uses them via SFXChannel), inferred here as
# 0/1 continuing the FM3=2 sequence backward -- flagged as unverified should
# a real file ever need them; every other value below is a direct
# transcription of the source's own EQU, not inferred.
CHANNEL_ID = {
    "cFM1": 0x00, "cFM2": 0x01,  # inferred, not directly confirmed by any EQU in the source
    "cFM3": 0x02, "cFM4": 0x04, "cFM5": 0x05, "cFM6": 0x06,
    "cPSG1": 0x80, "cPSG2": 0xA0, "cPSG3": 0xC0, "cNoise": 0xE0,
}
DAC_NAMES = {  # SonicDriverVer=1 only (see _smps2asm_inc.asm's DAC Equates switch)
    "Kick": 0x81, "Snare": 0x82, "Timpani": 0x83,
    "HiTimpani": 0x88, "MidTimpani": 0x89, "LowTimpani": 0x8A, "VLowTimpani": 0x8B,
}
PSG_MAX_NOTE = "A5"  # nMaxPSG == nA5 for SonicDriverVer<=2 (see _smps2asm_inc.asm)


def note_value(name):
    if name == "Rst":
        return 0x80
    if name == "MaxPSG":
        return note_value(PSG_MAX_NOTE)
    if name in DAC_NAMES:
        return DAC_NAMES[name]
    m = re.match(r"^([A-G])([sb]?)(\d+)$", name)
    if not m:
        raise ValueError(f"unrecognized note name: {name!r}")
    letter, accidental, octave = m.group(1), m.group(2), int(m.group(3))
    semitone = NOTE_BASE[letter] + (1 if accidental == "s" else -1 if accidental == "b" else 0)
    return 0x81 + octave * 12 + semitone


# ---------------------------------------------------------------------------
# Two-pass emitter: pass 1 walks the JSON structure emitting real bytes for
# everything EXCEPT relative loop/jump/call offsets (those get a 2-byte
# placeholder plus a recorded patch: byte position + target block name +
# whether it's PSG/DAC pitch-relative or a plain track offset). Pass 2, once
# every block's final address is known, patches every recorded reference in
# place. Mirrors how any two-pass assembler handles forward references --
# necessary here because smpsJump/smpsLoop/smpsCall can target labels
# anywhere, including blocks not yet emitted.
# ---------------------------------------------------------------------------


class Emitter:
    def __init__(self):
        self.buf = bytearray()
        self.block_addr = {}  # name -> address (byte offset from song start)
        self.patches = []  # (patch_pos, target_name, kind) -- kind: "rel-1" (loc-*-1) or "abs" (loc-songStart)

    def pos(self):
        return len(self.buf)

    def byte(self, v):
        self.buf.append(v & 0xFF)

    def word_placeholder(self, target_name, kind):
        self.patches.append((len(self.buf), target_name, kind))
        self.buf.append(0)
        self.buf.append(0)

    def word_abs(self, v):
        self.buf.append((v >> 8) & 0xFF)
        self.buf.append(v & 0xFF)

    def mark_block(self, name):
        self.block_addr[name] = len(self.buf)

    def apply_patches(self):
        for patch_pos, target_name, kind in self.patches:
            if target_name not in self.block_addr:
                raise ValueError(f"unresolved reference to block '{target_name}' -- never defined in SMPSplaylist")
            target = self.block_addr[target_name]
            if kind == "rel-1":
                # Matches smpsJump/smpsLoop's real formula: dc.w loc-*-1,
                # where * is the position of the offset word ITSELF.
                value = (target - patch_pos - 1) & 0xFFFF
            elif kind == "abs":
                value = target & 0xFFFF  # already relative to songStart==0 by construction
            else:
                raise ValueError(f"unknown patch kind {kind!r}")
            self.buf[patch_pos] = (value >> 8) & 0xFF
            self.buf[patch_pos + 1] = value & 0xFF


# Coordination-flag byte encodings, SonicDriverVer=1 (see this project's own
# audited catalog in project_paradoxsmps_schema -- only macros confirmed to
# actually appear somewhere in this project's real songs are implemented).
def emit_simple(em, mnemonic, value):
    table = {
        "smpsChanTempoDiv": 0xE5, "smpsAlterVol": 0xE6, "smpsNoteFill": 0xE8,
        "smpsAlterPitch": 0xE9, "smpsAlterNote": 0xE1, "smpsNop": 0xE2,
        "smpsPSGAlterVol": None,  # PSG-specific, handled separately below (real opcode differs by driver rev; see TODO)
        "smpsPSGform": 0xF3, "smpsPSGvoice": 0xF5, "smpsSetvoice": 0xEF,
    }
    op = table.get(mnemonic)
    if op is None:
        raise NotImplementedError(f"{mnemonic}: byte encoding not yet implemented in the compiler")
    em.byte(op)
    em.byte(unhex(value) & 0xFF)


ZERO_ARG_OPS = {
    "smpsStop": 0xF2, "smpsClearPush": 0xED, "smpsStopSpecial": 0xEE,
    "smpsWeirdD1LRR": 0xF9, "smpsFade": 0xE4, "smpsNoAttack": 0xE7,
}

_synthetic_label_counter = [0]


def _new_synthetic_label():
    _synthetic_label_counter[0] += 1
    return f"__synthetic_{_synthetic_label_counter[0]}"


def emit_event(em, event):
    if isinstance(event, str):
        if event in ZERO_ARG_OPS:
            em.byte(ZERO_ARG_OPS[event])
            return
        raise NotImplementedError(f"bare event {event!r}: not implemented")
    if "note" in event:
        em.byte(note_value(event["note"]))
        if "duration" in event:
            em.byte(unhex(event["duration"]) & 0xFF)
        return
    if "tie" in event or "inheritedNote" in event:
        # Both compile identically -- just the raw duration byte, no note
        # byte at all. The audible difference between them (no retrigger
        # for "tie" vs. retrigger-at-inherited-pitch for "inheritedNote")
        # comes entirely from whether a smpsNoAttack opcode was emitted
        # immediately before this, which is already its own separate event
        # in the stream -- not something this byte itself encodes. See
        # asm_to_json.py's NoteStreamState for the real-driver tracing this
        # distinction is based on.
        em.byte(unhex(event["duration"]) & 0xFF)
        return
    if "smpsMod" in event:
        em.byte(0xF1 if event["smpsMod"] else 0xF4)  # SonicDriverVer=1: both are bare, no data byte
        return
    if "smpsPan" in event:
        direction_name, amsfms = event["smpsPan"]
        pan_values = {"panLeft": 0x80, "panRight": 0x40, "panCentre": 0xC0, "panCenter": 0xC0, "panNone": 0x00}
        em.byte(0xE0)
        em.byte((pan_values[direction_name] + unhex(amsfms)) & 0xFF)
        return
    if "smpsModSet" in event:
        em.byte(0xF0)
        for v in event["smpsModSet"]:
            em.byte(unhex(v) & 0xFF)
        return
    if "smpsPSGvoice" in event:
        # fTone_XX names encode their own numeric value in the name itself
        # (fTone_01=$01, fTone_02=$02, ... sequential -- see
        # _smps2asm_inc.asm's PSG volume envelope equates) -- no separate
        # lookup table needed, just parse the trailing hex digits.
        em.byte(0xF5)
        em.byte(int(event["smpsPSGvoice"].rsplit("_", 1)[-1], 16) & 0xFF)
        return
    if "smpsLoop" in event or "smpsJump" in event:
        is_loop = "smpsLoop" in event
        value = event["smpsLoop"] if is_loop else event["smpsJump"]
        if is_loop:
            idx, cnt, *body = value
        else:
            body = value
        # A nested block compiles as: [the body's bytes, with their own
        # start marked as a synthetic label] followed by the real
        # smpsLoop/smpsJump instruction pointing back at that label --
        # exactly mirroring how the real .asm source places the label
        # BEFORE the body and the loop/jump instruction AFTER it, just
        # without needing an author-visible name for it.
        label = _new_synthetic_label()
        em.mark_block(label)
        for e in body:
            emit_event(em, e)
        if is_loop:
            em.byte(0xF7)
            em.byte(unhex(idx) & 0xFF)
            em.byte(unhex(cnt) & 0xFF)
        else:
            em.byte(0xF6)
        em.word_placeholder(label, "rel-1")
        return
    for mnemonic, value in event.items():
        if mnemonic in ("smpsChanTempoDiv", "smpsAlterVol", "smpsNoteFill", "smpsAlterPitch", "smpsAlterNote",
                         "smpsNop", "smpsPSGform", "smpsSetvoice"):
            emit_simple(em, mnemonic, value)
            return
        if mnemonic == "smpsPSGAlterVol":
            em.byte(0xEC)  # real op differs by driver rev in general, but fixed for SonicDriverVer=1
            em.byte(unhex(value) & 0xFF)
            return
        if mnemonic == "smpsSetTempoDiv":
            em.byte(0xEB)
            em.byte(unhex(value) & 0xFF)
            return
        if mnemonic == "smpsSetTempoMod":
            em.byte(0xEA)
            em.byte(unhex(value) & 0xFF)
            return
        if mnemonic == "smpsCall":
            em.byte(0xF8)
            em.word_placeholder(value, "abs")
            return
        if mnemonic == "jumpTo":
            # Escape-hatch reference from the converter (a cross-block
            # reference, or anything else that didn't fold into a nested
            # subblock) -- compiles the same as a real smpsJump/smpsLoop,
            # just targeting an EXISTING named block instead of a synthetic
            # one, exactly like smpsCall does.
            if "smpsLoopArgs" in event:
                idx, cnt = event["smpsLoopArgs"]
                em.byte(0xF7)
                em.byte(unhex(idx) & 0xFF)
                em.byte(unhex(cnt) & 0xFF)
            else:
                em.byte(0xF6)
            em.word_placeholder(value, "rel-1")
            return
    raise NotImplementedError(f"unrecognized event: {event!r}")


# ---------------------------------------------------------------------------
# Voice bank -- straightforward fixed 25-byte-per-voice layout, matching
# Sound.c's FM_LoadVoice / smps.c's smpsVcTotalLevel exactly (already fully
# validated earlier this session): byte0 = algorithm(+high bit for
# ParadoxFM's 8-15 extension)/feedback/unused-bits, then 4 rows of 4
# operator bytes each (op1,op3,op2,op4 physical write order), then D2R row,
# D1L/RR row, TL row.
# ---------------------------------------------------------------------------

OP_WRITE_ORDER = [0, 2, 1, 3]  # natural op index -> physical write-order position (self-inverse)


def emit_voice(em, voice):
    alg = unhex(voice.get("smpsVcAlgorithm", 0))
    fb = unhex(voice.get("smpsVcFeedback", 0))
    ub = unhex(voice.get("smpsVcUnusedBits", 0))
    alg_ext = (alg >> 3) & 1
    em.byte((alg_ext << 7) | ((ub & 1) << 6) | ((fb & 7) << 3) | (alg & 7))

    ops = voice["operators"]

    def op_field(key):
        return [unhex(ops[i].get(key, 0)) for i in range(4)]

    dt, mul = op_field("smpsVcDetune"), op_field("smpsVcCoarseFreq")
    rs, ar = op_field("smpsVcRateScale"), op_field("smpsVcAttackRate")
    am, d1r = op_field("smpsVcAmpMod"), op_field("smpsVcDecayRate1")
    d2r = op_field("smpsVcDecayRate2")
    d1l, rr = op_field("smpsVcDecayLevel"), op_field("smpsVcReleaseRate")
    tl = op_field("smpsVcTotalLevel")

    # TL's bit7 is a byte-format marker baked into the STORED/compiled byte
    # (not stripped until Sound.c's FM_LoadVoice masks it off at load time,
    # `& 0x7F`, right before the real hardware write) -- found by diffing
    # this compiler's output against the real songbuild_SndB5_Ring header
    # byte-for-byte; matches smps.c's own tl_mask formula exactly (natural
    # op order: op1 always set, op2/op3/op4 depend on algorithm).
    tl_mask = [0x80, 0x80 if alg >= 5 else 0, 0x80 if alg >= 4 else 0, 0x80 if alg == 7 else 0]

    for pos in OP_WRITE_ORDER:
        em.byte(((dt[pos] & 7) << 4) | (mul[pos] & 0xF))
    for pos in OP_WRITE_ORDER:
        em.byte(((rs[pos] & 3) << 6) | (ar[pos] & 0x1F))
    for pos in OP_WRITE_ORDER:
        em.byte(((am[pos] & 1) << 7) | (d1r[pos] & 0x1F))
    for pos in OP_WRITE_ORDER:
        em.byte(d2r[pos] & 0x1F)
    for pos in OP_WRITE_ORDER:
        em.byte(((d1l[pos] & 0xF) << 4) | (rr[pos] & 0xF))
    for pos in OP_WRITE_ORDER:
        em.byte((tl[pos] & 0x7F) | tl_mask[pos])


# ---------------------------------------------------------------------------
# Header emission. Layout choice: [voice-bank-offset word][header entries in
# JSON order][playlist blocks in JSON order][voice bank]. Header pointer
# fields (channel/DAC `loc` args, and the voice-bank offset itself) are
# "abs" patches (offset from songStart==0); internal control flow within a
# block uses "rel-1" patches (matches smpsJump/smpsLoop's own `loc-*-1`).
# ---------------------------------------------------------------------------

HEADER_PTR_FIELD = {  # macro -> which arg index is a block-name pointer (None = no pointer arg)
    "smpsHeaderVoice": 0, "smpsHeaderDAC": 0, "smpsHeaderFM": 0, "smpsHeaderPSG": 0,
}


def compile_song(data):
    em = Emitter()

    voice_bank_name = None
    header = data.get("header", [])
    for entry in header:
        for macro in entry:
            if macro == "smpsHeaderVoice":
                voice_bank_name = entry[macro][0]

    # smpsHeaderVoice's own word (voice_off = voiceBank - songStart) is
    # ALWAYS the first 2 bytes of a real compiled song (see
    # smpsHeaderVoice's own macro body -- smpsHeaderStartSong emits zero
    # bytes, so Voice's word is unconditionally byte 0-1). Sound_DebugGetSongData
    # / FM_LoadVoice read this directly.
    em.word_placeholder(voice_bank_name, "abs")

    for entry in header:
        for macro, args in entry.items():
            if macro in ("smpsHeaderStartSong", "smpsHeaderVoice"):
                continue  # StartSong emits nothing; Voice's word already emitted above
            if macro == "smpsHeaderSFXChannel":
                # Real macro body: dc.b $80,chanid ; CheckedChannelPointer loc ; dc.b pitch ; dc.b vol
                # -- the leading $80 is a fixed constant, not derived from
                # chanid, easy to miss (found via direct byte-layout
                # verification against SndB5's real compiled bytes).
                chanid, loc, pitch, vol = args
                em.byte(0x80)
                em.byte(unhex(chanid) & 0xFF)
                em.word_placeholder(loc, "abs")
                em.byte(unhex(pitch) & 0xFF)
                em.byte(unhex(vol) & 0xFF)
                continue
            if macro == "smpsHeaderDAC":
                # Real macro body has a branch the generic loop below can't
                # express: pointer, THEN either (pitch byte [+ vol byte, or
                # $00 if vol omitted]) if pitch was given, OR a flat `dc.w
                # $00` (two zero bytes, not one) if pitch was omitted
                # entirely -- e.g. GHZ's own `smpsHeaderDAC Mus81_GHZ_DAC`
                # has no pitch/vol at all. Found via direct byte-layout diff
                # against the real compiled GHZ header (my compiler was
                # silently dropping 2 bytes here).
                loc = args[0]
                em.word_placeholder(loc, "abs")
                if len(args) > 1:
                    em.byte(unhex(args[1]) & 0xFF)  # pitch
                    em.byte(unhex(args[2]) & 0xFF if len(args) > 2 else 0)  # vol, default 0
                else:
                    em.word_abs(0)
                continue
            ptr_field = HEADER_PTR_FIELD.get(macro)
            for i, a in enumerate(args):
                if i == ptr_field:
                    em.word_placeholder(a, "abs")
                else:
                    em.byte(unhex(a) & 0xFF)

    for name, events in data.get("SMPSplaylist", {}).items():
        em.mark_block(name)
        for e in events:
            emit_event(em, e)

    if voice_bank_name is not None:
        em.mark_block(voice_bank_name)
        for voice in data.get("voices", []):
            emit_voice(em, voice)

    em.apply_patches()
    return bytes(em.buf)


def to_c_header(data_bytes, array_name):
    # Matches smps2asmc's own generated-header convention exactly (#pragma
    # once, uint8_t) since this compiler is meant to replace that pipeline --
    # same consumption pattern in Sound.c (a single direct #include per
    # song), so the two should be interchangeable at the include site.
    lines = ["#pragma once", "", "#include <stdint.h>", "", f"const uint8_t {array_name}[] = {{"]
    for i in range(0, len(data_bytes), 16):
        chunk = data_bytes[i : i + 16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: json_to_header.py input.jsonc output.h ArrayName", file=sys.stderr)
        sys.exit(1)
    song = load_jsonc(sys.argv[1])
    compiled = compile_song(song)
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write(to_c_header(compiled, sys.argv[3]))
    print(f"Wrote {len(compiled)} bytes to {sys.argv[2]}", file=sys.stderr)
