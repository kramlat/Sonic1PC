#pragma once

#include <QJsonObject>
#include <QMultiMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// Which real chip channel a resolved state ultimately traces back to, per
// the header's own smpsHeaderFM/PSG/DAC home-block pointers -- Unknown
// only if a block's ownership chain never reaches any header entry at all
// (shouldn't normally happen for a block that's actually reachable from
// playback, but guards against a malformed/incomplete document).
enum class ChannelKind { Unknown, FM, PSG, DAC };

// One owner's resolved playback state at the tick it calls/jumps into a
// particular block -- see BlockMetadata::checkSharedConflict.
struct ResolvedBlockState {
    ChannelKind kind = ChannelKind::Unknown;
    int voiceOrTone = -1;    // FM voice index (kind==FM) or PSG tone index (kind==PSG); unused for DAC
    bool noiseActive = false; // PSG only -- whether a smpsPSGform tick has been crossed on this path

    // Equality is deliberately narrower than a plain memberwise compare --
    // fields that don't apply to `kind` (e.g. noiseActive for an FM state)
    // never contribute to a mismatch, per your direction that only ACTUAL
    // divergence in the state that matters for this channel kind counts as
    // a conflict.
    bool operator==(const ResolvedBlockState &other) const {
        if (kind != other.kind)
            return false;
        if (kind == ChannelKind::FM)
            return voiceOrTone == other.voiceOrTone;
        if (kind == ChannelKind::PSG)
            return voiceOrTone == other.voiceOrTone && noiseActive == other.noiseActive;
        return true; // DAC/Unknown -- channel kind match is the whole comparison
    }
};

// One smpsSetvoice coordination flag found while scanning a block's own
// event stream -- tick is that block's OWN local tick position (same
// convention as SongDocument::eventTickDuration/resolveVoiceAtTick), not
// an absolute/song-wide position.
struct BlockVoiceChange {
    double tick;
    int voice;
};

// One smpsPSGform occurrence found while scanning a block's own event
// stream -- a bare flag (no value), same tick convention as
// BlockVoiceChange. Mirrors SongDocument::resolvePsgStateAtTick's own
// "once seen, stays noise for the rest of the block" semantics: a PSG
// channel plays its configured tone up until the first smpsPSGform tick,
// periodic noise from there on.
struct BlockPsgFormChange {
    double tick;
};

// One smpsPSGvoice change found while scanning a block's own event stream
// -- PSG's own equivalent of BlockVoiceChange/smpsSetvoice (a tone/envelope
// selection, see SongDocument::parsePsgToneValue). Per your direction, a
// shared PSG-owned block can conflict the same two ways an FM-owned one
// can: a different smpsPSGvoice change here, OR the owners' respective PSG
// CHANNELS (via the header's smpsHeaderPSG entries, not tracked by this
// class -- see SongDocument::psgChannelInfoForBlock) having a different
// starting/default tone even with no in-block smpsPSGvoice change at all.
// That second case is a header-level comparison the future resolve step
// needs to make alongside this data, not something this per-block scan can
// capture on its own.
struct BlockPsgVoiceChange {
    double tick;
    int tone;
};

// One smpsCall/jumpTo occurrence reaching a particular target block --
// ownerBlock is where it was written, tick is ITS OWN local position
// within ownerBlock's stream at the moment of the call/jump. Deliberately
// NOT deduplicated the way ownersOf()/targetsOf() are (see BlockMetadata's
// own rebuild() comment) -- the same owner calling the same target twice
// from two different points is two distinct call sites, each with
// potentially a different voice active at that point.
struct BlockCallSite {
    QString ownerBlock;
    double tick;
};

// DAW-only relational index over one song's SMPSplaylist blocks: tracks
// which block(s) "own" (smpsCall or jumpTo) which other block(s), including
// many-to-many -- a block can legitimately be shared/called from more than
// one place (e.g. a shared sub-block reused across several instrument
// lines) -- plus, per your direction, which block sets what voice (via the
// smpsSetvoice coordination flag) and when (tick position), and exactly
// where each owner's call/jump into a shared block happens. Together these
// let a future consumer (the piano roll, once its own work resumes --
// currently on hold per your direction until the other editors are done)
// resolve the correct active voice for an OWNED block by chasing into
// every distinct owner's own voice-change history up to that owner's
// call-site tick, instead of SongDocument::resolveVoiceAtTick's older
// "first caller found" approximation -- that resolution algorithm itself
// isn't built yet, only the raw data it needs.
//
// A SHARED block (isSharedBlock() true) can conflict across its distinct
// owners in exactly two ways, per your direction: (1) different voice --
// each owner's own voiceChangesOf() (or, for a PSG-owned block,
// psgVoiceChangesOf()/the owning channel's own header-configured default
// tone) resolves to a different active voice/tone at its respective
// call-site tick, or (2) different channel type -- the owners ultimately
// trace back to different channel kinds entirely (FM vs PSG vs DAC, via
// the header's smpsHeaderFM/PSG/DAC home-block pointers), not just a
// different voice within the same kind. Per your direction, having
// multiple owners is NOT itself a conflict -- if every distinct owner
// resolves to the SAME state (same voice, same channel type, same flags
// set), there's nothing to prompt about, EVEN IF the owners' call-site
// ticks themselves differ -- the comparison is purely over the resolved
// VALUES each owner's path produces, never over the tick numbers. Applies
// equally to FM (e.g. two owners both landing on smpsSetvoice=5, just at
// different ticks -- no conflict) and PSG (e.g. both owners' paths having
// crossed a smpsPSGform noise tick, or neither having -- no conflict).
// Only a genuine conflict (by that stricter definition) needs the piano
// roll's planned toolbar dropdown (pick which owner's context to preview
// under, session-only, per your direction) -- a shared
// block whose owners all agree needs no such prompt.
//
// Purely a ParadoxComposer-side concept, per your direction -- NEVER part
// of the compiled song format or its JSON schema (the compiler,
// libparadoxsmps/compiler.c, never reads any of this).
//
// Ownership is established ONLY by smpsCall and jumpTo -- NOT by a bare
// smpsLoop/smpsJump key, which in this schema is an INLINE nested loop/jump
// body embedded in the SAME block (see compiler.c's emit_event: it mints a
// synthetic label and stays within the block), not a reference to another
// block. jumpTo is the actual cross-block form the converter emits for a
// real smpsJump/smpsLoop that targets an existing named block instead of
// folding into a nested body. Both smpsLoop's and smpsJump's own inline
// bodies are still scanned recursively (a smpsCall/jumpTo or smpsSetvoice
// can appear nested inside one), attributed to the same owning block
// they're written in (and the SAME running tick position, since an inline
// body's events still play in sequence as part of that block's own
// timeline), not to a new block of their own.
class BlockMetadata {
public:
    // Rebuilds the whole index from scratch by scanning every block's
    // events (including nested smpsLoop/smpsJump bodies) for smpsCall/
    // jumpTo references and smpsSetvoice changes. Call whenever the
    // playlist changes.
    void rebuild(const QJsonObject &playlist);

    // Every DISTINCT block that calls/jumps to `blockName` -- empty if none
    // (e.g. a song's own top-level FM/PSG/DAC home block, which nothing
    // else ever calls into). Deduplicated by block name, even if that
    // owner calls/jumps to `blockName` from more than one point in its own
    // stream -- see callSitesOf() for the non-deduplicated, tick-aware
    // version of the same relationship.
    QStringList ownersOf(const QString &blockName) const;
    // Every block `blockName` itself calls/jumps to (deduplicated the same
    // way as ownersOf()).
    QStringList targetsOf(const QString &blockName) const;
    // True if more than one DISTINCT block owns `blockName` -- the specific
    // case your direction called out ("tracking blocks owned by more than
    // one block").
    bool isSharedBlock(const QString &blockName) const;

    // Every smpsCall/jumpTo occurrence that reaches `blockName`, each with
    // its own owner + local tick -- unlike ownersOf(), NOT deduplicated:
    // the same owner calling `blockName` twice from two different points
    // yields two separate entries here, since each has its own call-site
    // tick and therefore potentially a different active voice.
    QVector<BlockCallSite> callSitesOf(const QString &blockName) const;

    // Every smpsSetvoice change within `blockName`'s OWN event stream, in
    // tick order -- does NOT chase into owners (see this class's own
    // header comment for why that resolution step isn't built yet).
    QVector<BlockVoiceChange> voiceChangesOf(const QString &blockName) const;

    // Every smpsPSGform occurrence within `blockName`'s OWN event stream,
    // in tick order -- same "raw data only, no cross-block chase yet"
    // scope as voiceChangesOf(). Per your direction, this is the same
    // inference a future resolve step can use for a PSG/noise-owned block
    // (and, via ownersOf()'s existing ownership chain, for recognizing a
    // block owned by a DAC-home block as DAC context too) -- not built
    // here, just the data it needs.
    QVector<BlockPsgFormChange> psgFormChangesOf(const QString &blockName) const;

    // Every smpsPSGvoice change within `blockName`'s OWN event stream, in
    // tick order -- see BlockPsgVoiceChange's own comment.
    QVector<BlockPsgVoiceChange> psgVoiceChangesOf(const QString &blockName) const;

    // The stricter conflict definition worked out in this class's own
    // header comment: true only if `blockName` has more than one distinct
    // owner AND those owners' resolved states (voice/tone, channel kind,
    // noise-active) actually differ -- not merely because it has more than
    // one owner. `header` is the document's own header array (needed to
    // trace an owner's ownership chain back to whichever smpsHeaderFM/PSG/
    // DAC entry it ultimately belongs to); passed in rather than stored,
    // since BlockMetadata otherwise only ever deals with the playlist.
    bool checkSharedConflict(const QString &blockName, const QJsonArray &header) const;

    // Resolves the state active for `blockName` at `cutoffTick`, chasing
    // up through ownersOf() (via each owner's own EARLIEST call site into
    // whatever it's chasing) when `blockName` has no local voice/tone
    // change of its own before `cutoffTick` and isn't itself a header home
    // block -- the same "chase the ownership chain backward" idea as
    // SongDocument::resolveVoiceAtTick, generalized to cover channel kind
    // and PSG noise too, and (via checkSharedConflict's own per-owner
    // calls) actually exercised once per distinct owner instead of just
    // the first one found.
    ResolvedBlockState resolveStateAtTick(const QString &blockName, double cutoffTick, const QJsonArray &header) const;

    // Sidecar file I/O -- per your direction, a `<song>.metadata.json` file
    // next to the song file, written after each in-memory scan on open
    // (see MainWindow::openFile). Never read back to skip a rescan --
    // rebuild() always runs fresh off the just-loaded document; this file
    // is the scan's own persisted-for-inspection result, not a cache.
    static QString metadataPathFor(const QString &songFilePath);
    bool saveToFile(const QString &songFilePath, QString *errorOut = nullptr) const;

private:
    void scanEvents(const QString &ownerBlock, const QJsonArray &events, double &tick);
    void addEdge(const QString &owner, const QString &target, double tick);
    ChannelKind homeChannelKind(const QString &blockName, const QJsonArray &header) const;
    ResolvedBlockState resolveStateAtTick(const QString &blockName, double cutoffTick, const QJsonArray &header,
                                           QSet<QString> &visited) const; // cycle guard for the recursive owner chase

    QMultiMap<QString, QString> m_ownersByBlock;  // callee -> distinct caller(s)
    QMultiMap<QString, QString> m_targetsByBlock; // caller -> distinct callee(s)
    QMultiMap<QString, BlockCallSite> m_callSitesByTarget; // callee -> every call site reaching it (not deduplicated)
    QMap<QString, QVector<BlockVoiceChange>> m_voiceChangesByBlock; // block -> its own smpsSetvoice changes, tick order
    QMap<QString, QVector<BlockPsgFormChange>> m_psgFormChangesByBlock; // block -> its own smpsPSGform occurrences, tick order
    QMap<QString, QVector<BlockPsgVoiceChange>> m_psgVoiceChangesByBlock; // block -> its own smpsPSGvoice changes, tick order
};
