#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

// In-memory model of a loaded ParadoxSMPS .jsonc file. Deliberately mirrors
// the schema shape 1:1 (header/voices/SMPSplaylist, or paradoxVoiceBank for
// a standalone voice-preset file) rather than a separate internal model --
// see project_paradoxsmps_schema memory / tools/paradoxsmps/*.py, which this
// must always stay structurally compatible with (this class never compiles
// anything itself; CompilePipeline shells out to the real json_to_header.py
// for that, so there's only one source of truth for the schema's meaning).
class SongDocument {
public:
    // Strips // line comments and /* */ block comments, respecting quoted
    // strings, exactly like tools/paradoxsmps/json_to_header.py's
    // strip_jsonc_comments() -- kept in lockstep with that function by hand,
    // since both need to accept the exact same .jsonc dialect.
    static QString stripJsonComments(const QString &text);

    bool loadFromFile(const QString &path, QString *errorOut = nullptr);
    bool saveToFile(const QString &path, QString *errorOut = nullptr) const;

    bool isVoiceBank() const { return m_root.value("paradoxVoiceBank").toBool(false); }

    QJsonObject &root() { return m_root; }
    const QJsonObject &root() const { return m_root; }

    QJsonArray header() const { return m_root.value("header").toArray(); }
    void setHeader(const QJsonArray &header) { m_root["header"] = header; }

    QJsonArray voices() const { return m_root.value("voices").toArray(); }
    void setVoices(const QJsonArray &voices) { m_root["voices"] = voices; }

    QJsonObject playlist() const { return m_root.value("SMPSplaylist").toObject(); }
    void setPlaylist(const QJsonObject &playlist) { m_root["SMPSplaylist"] = playlist; }

    // Authoritative block-emission order for SMPSplaylist, SEPARATE from
    // QJsonObject's own iteration order -- verified empirically that Qt's
    // QJsonObject does NOT preserve source/insertion key order when parsing
    // (a real GHZ file's authored order FM1,Call02,Call07,FM2... comes back
    // alphabetized as Call00,Call01,Call02...DAC on readback). Since block
    // order affects compiled addresses, anything that compiles or displays
    // blocks in a specific sequence MUST use this list, never
    // playlist().keys() or QJsonObject iteration directly.
    QStringList playlistOrder() const { return m_playlistOrder; }
    // Sets both content and order together -- `order` becomes authoritative;
    // any key in `playlist` missing from `order` is appended at the end
    // (e.g. a block added programmatically without updating order first),
    // and any name in `order` no longer present in `playlist` is dropped.
    void setPlaylist(const QJsonObject &playlist, const QStringList &order) {
        m_root["SMPSplaylist"] = playlist;
        m_playlistOrder.clear();
        for (const QString &name : order)
            if (playlist.contains(name))
                m_playlistOrder.append(name);
        for (auto it = playlist.constBegin(); it != playlist.constEnd(); ++it)
            if (!m_playlistOrder.contains(it.key()))
                m_playlistOrder.append(it.key());
    }

    // Scans (already comment-stripped) JSON text for SMPSplaylist's
    // top-level key order via a brace-depth-aware text scan -- the only
    // point where authored order is actually recoverable, since it's lost
    // the moment the text is parsed into a QJsonObject.
    static QStringList extractPlaylistKeyOrder(const QString &strippedJsonText);

    // Serializes the document to JSON text with SMPSplaylist blocks emitted
    // in playlistOrder() sequence (compact, machine-consumption only -- e.g.
    // feeding CompilePipeline's subprocess, so the real Python compiler sees
    // the same block order the native compiler does, keeping "Verify
    // against real compiler" a meaningful comparison instead of comparing
    // two arbitrarily-differently-ordered-but-both-internally-consistent
    // outputs).
    QString toJsonText() const;

    // Builds an empty song document ({"header":[],"voices":[],"SMPSplaylist":{}})
    // or an empty voice bank ({"paradoxVoiceBank":true,"voices":[]}).
    static SongDocument makeEmptySong();
    static SongDocument makeEmptyVoiceBank();

    QString filePath() const { return m_filePath; }

    // The channel attenuation ("volume", third arg) of the smpsHeaderFM
    // entry whose home block (first arg) is `blockName` -- 0 (no extra
    // attenuation) if no such entry exists. Used by PianoRollPanel's block
    // test-play so play-testing a track applies the SAME channel volume the
    // real driver would (per your direction: "with the attenuation the
    // channel points to"), not the raw voice in isolation like the Voice
    // Bank panel's own piano-key audition intentionally does.
    static int fmChannelAttenuationForBlock(const QJsonArray &header, const QString &blockName);

    // The voice (index into voices()) that would be "current" at
    // `tickCutoff` ticks into `blockName`, per your direction: walk
    // backward from the cursor for the most recent smpsSetvoice in this
    // block; if none has happened yet, smpsCall never resets "current
    // voice" (same principle as note inheritance across smpsCall --  see
    // asm_to_json.py's NoteStreamState comment), so the search continues
    // into whichever OTHER block last called into this one, at ITS tick
    // position at the point of that call, and so on. Falls back to voice 0
    // (not "whatever's selected in the Voice Bank panel") if the whole
    // chain never sets one -- the sensible zero-state default for a track
    // that's never explicitly changed voice. Used identically for the
    // piano-roll sidebar's manual key-click preview (against the current
    // playhead/cursor position) and for block test-play (against the
    // advancing playhead), so both "hear" the same voice a real playthrough
    // would at that point.
    int resolveVoiceAtTick(const QString &blockName, double tickCutoff) const;

    // PSG channel info for a home block -- false if `blockName` isn't a
    // smpsHeaderPSG entry's home block (first arg). toneIndexOut: the
    // header's own default tone (1-9, from its "fTone_NN" 4th arg -- 0 if
    // unrecognized/absent). attenuationOut: the channel's configured base
    // volume (0-15, third arg).
    static bool psgChannelInfoForBlock(const QJsonArray &header, const QString &blockName, int *toneIndexOut,
                                        int *attenuationOut);

    // PSG "voice" (tone envelope) and noise-mode ($F3) resolution for
    // block test-play/key-click preview, scoped to THIS block only (unlike
    // resolveVoiceAtTick, no cross-block smpsCall chasing -- not requested
    // for PSG). toneIndexOut: the last smpsPSGvoice at or before
    // tickCutoff, or `defaultTone` if none yet. noiseOut: true if any
    // smpsPSGform ($F3) event exists at or before tickCutoff -- once
    // triggered, real hardware redirects the channel permanently (see
    // Sound.c's own TickChannel comment), so this never "un-triggers" as
    // the cursor moves further forward, per your direction: "if the cursor
    // crosses an F3 tag, switch to noise".
    void resolvePsgStateAtTick(const QString &blockName, double tickCutoff, int defaultTone, int *toneIndexOut,
                                bool *noiseOut) const;

    // DAC sample names for a home block (per your direction: "a dac home...
    // should list DAC samples on keys... and actually play the samples")
    // -- empty if `blockName` isn't a smpsHeaderDAC entry's home block
    // (first arg). Row mapping matches AudioEngine::previewDacSample()'s
    // own convention: 0=Kick, 1=Snare, 2=Timpani, 7-10=Hi/Mid/Low/VLow
    // Timpani (the four pitch-shifted variants, note bytes $88-$8B) -- the
    // only DAC note bytes with a real sample behind them (see Sound.c's
    // own DAC dispatch comment on $84-$87 being invalid/special-cased, not
    // real samples).
    static QMap<int, QString> dacSampleNamesForBlock(const QJsonArray &header, const QString &blockName);

private:
    QJsonObject m_root;
    QStringList m_playlistOrder;
    QString m_filePath;
};

// Schema's hex-value convention ("0x81", matching HexInt in asm_to_json.py /
// _hexify's own rendering) <-> plain int, for widgets that edit a single
// numeric field (e.g. voice-editor knobs) without round-tripping the whole
// document as text.
int hexFieldToInt(const QJsonValue &value, int fallback = 0);
QJsonValue intToHexField(int value);

// How far one event advances the tick position -- matches
// PianoRollGridWidget::relayout()'s own layout exactly (note/tie/
// inheritedNote consume duration ticks, everything else is instantaneous).
// Public (not file-local) so BlockMetadata's own event-stream scan uses the
// exact same tick math instead of a second copy that could drift out of
// sync with this one.
double eventTickDuration(const QJsonObject &obj);

// "fTone_05" -> 5, or a bare hex/decimal value directly (smpsPSGvoice
// in-track events may use either form -- see compiler.c's own smpsPSGvoice
// handling for the same duality). Public for the same reason as
// eventTickDuration above -- shared with BlockMetadata's own scan.
int parsePsgToneValue(const QString &s);
