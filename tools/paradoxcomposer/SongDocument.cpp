#include "SongDocument.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QTextStream>

QString SongDocument::stripJsonComments(const QString &text) {
    QString out;
    out.reserve(text.size());
    bool inString = false;
    int i = 0;
    const int n = text.size();
    while (i < n) {
        QChar c = text.at(i);
        if (inString) {
            out.append(c);
            if (c == '\\' && i + 1 < n) {
                out.append(text.at(i + 1));
                i += 2;
                continue;
            }
            if (c == '"')
                inString = false;
            ++i;
            continue;
        }
        if (c == '"') {
            inString = true;
            out.append(c);
            ++i;
            continue;
        }
        if (c == '/' && i + 1 < n && text.at(i + 1) == '/') {
            while (i < n && text.at(i) != '\n')
                ++i;
            continue;
        }
        if (c == '/' && i + 1 < n && text.at(i + 1) == '*') {
            i += 2;
            while (i + 1 < n && !(text.at(i) == '*' && text.at(i + 1) == '/'))
                ++i;
            i += 2;
            continue;
        }
        out.append(c);
        ++i;
    }
    return out;
}

bool SongDocument::loadFromFile(const QString &path, QString *errorOut) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = f.errorString();
        return false;
    }
    QTextStream in(&f);
    const QString raw = in.readAll();
    const QString stripped = stripJsonComments(raw);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(stripped.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject()) {
        if (errorOut)
            *errorOut = err.errorString();
        return false;
    }
    m_root = doc.object();
    m_playlistOrder = extractPlaylistKeyOrder(stripped);
    m_filePath = path;
    return true;
}

QStringList SongDocument::extractPlaylistKeyOrder(const QString &text) {
    QStringList order;
    const int keyIdx = text.indexOf("\"SMPSplaylist\"");
    if (keyIdx < 0)
        return order;
    int i = text.indexOf('{', keyIdx);
    if (i < 0)
        return order;
    ++i; // past SMPSplaylist's own opening brace
    int depth = 1;
    const int n = text.size();
    while (i < n && depth > 0) {
        const QChar c = text.at(i);
        if (c == '"') {
            // At depth 1 (directly inside SMPSplaylist, not yet descended
            // into any block's own array/object contents), every string is
            // a block name -- SMPSplaylist's own shape never has bare
            // strings at that level otherwise.
            const bool isKey = (depth == 1);
            int j = i + 1;
            QString key;
            while (j < n) {
                if (text.at(j) == '\\' && j + 1 < n) {
                    if (isKey)
                        key.append(text.at(j + 1));
                    j += 2;
                    continue;
                }
                if (text.at(j) == '"')
                    break;
                if (isKey)
                    key.append(text.at(j));
                ++j;
            }
            if (isKey)
                order.append(key);
            i = j + 1;
            continue;
        }
        if (c == '{' || c == '[') {
            ++depth;
        } else if (c == '}' || c == ']') {
            --depth;
            if (depth == 0) {
                ++i;
                break;
            }
        }
        ++i;
    }
    return order;
}

QString SongDocument::toJsonText() const {
    const QString headerText = QString::fromUtf8(QJsonDocument(header()).toJson(QJsonDocument::Compact));
    const QString voicesText = QString::fromUtf8(QJsonDocument(voices()).toJson(QJsonDocument::Compact));

    const QJsonObject pl = playlist();
    QStringList order = m_playlistOrder;
    for (auto it = pl.constBegin(); it != pl.constEnd(); ++it)
        if (!order.contains(it.key()))
            order.append(it.key());

    QStringList blockEntries;
    for (const QString &name : order) {
        if (!pl.contains(name))
            continue;
        const QString eventsText = QString::fromUtf8(QJsonDocument(pl.value(name).toArray()).toJson(QJsonDocument::Compact));
        blockEntries << QString("\"%1\":%2").arg(name, eventsText);
    }

    QString extra;
    if (isVoiceBank())
        extra = "\"paradoxVoiceBank\":true,";

    return QString("{%1\"header\":%2,\"voices\":%3,\"SMPSplaylist\":{%4}}")
        .arg(extra, headerText, voicesText, blockEntries.join(","));
}

int SongDocument::fmChannelAttenuationForBlock(const QJsonArray &header, const QString &blockName) {
    for (const QJsonValue &entryValue : header) {
        const QJsonObject entry = entryValue.toObject();
        if (!entry.contains("smpsHeaderFM"))
            continue;
        const QJsonArray args = entry.value("smpsHeaderFM").toArray();
        if (args.size() > 0 && args.at(0).toString() == blockName)
            return args.size() > 2 ? hexFieldToInt(args.at(2)) : 0;
    }
    return 0;
}

QMap<int, QString> SongDocument::dacSampleNamesForBlock(const QJsonArray &header, const QString &blockName) {
    for (const QJsonValue &entryValue : header) {
        const QJsonObject entry = entryValue.toObject();
        if (!entry.contains("smpsHeaderDAC"))
            continue;
        const QJsonArray args = entry.value("smpsHeaderDAC").toArray();
        if (args.size() > 0 && args.at(0).toString() == blockName) {
            // Row numbering matches AudioEngine::previewDacSample and
            // Sound.c's own dac_notes[] table exactly: row N is note byte
            // $81+N (SND_dKick and up). Keep this list in sync with
            // dac_notes[] (src/Sound.c) whenever a note gets added, moved,
            // or re-pointed there -- there's no shared source for the two
            // right now, this is a manual mirror.
            QMap<int, QString> names;
            names[0] = "Kick";
            names[1] = "Snare";
            names[2] = "Timpani";
            names[3] = "Scratch";
            names[4] = "Clap";
            names[5] = "Tom";
            names[6] = "Bongo";
            names[7] = "HiTimpani";
            names[8] = "MidTimpani";
            names[9] = "LowTimpani";
            names[10] = "FloorTimpani";
            names[11] = "MidTom";
            names[12] = "LowTom";
            names[13] = "FloorTom";
            names[14] = "HiBongo";
            names[15] = "MidBongo";
            names[16] = "LowBongo";
            names[17] = "S3Snare";
            names[18] = "S3HiTom";
            names[19] = "S3MidTom";
            names[20] = "S3LowTom";
            names[21] = "S3FloorTom";
            names[22] = "S3Kick";
            names[23] = "MuffledSnare";
            names[24] = "Crash";
            names[25] = "Ride";
            names[26] = "LowMetalHit";
            names[27] = "FloorMetalHit";
            names[28] = "HighMetalHit";
            names[29] = "HigherMetalHit";
            names[30] = "MidMetalHit";
            names[31] = "S3Clap";
            names[32] = "ElectricHiTom";
            names[33] = "ElectricMidTom";
            names[34] = "ElectricLowTom";
            names[35] = "ElectricFloorTom";
            names[36] = "TightSnare";
            names[37] = "MidPitchedSnare";
            names[38] = "LooseSnare";
            names[39] = "LooserSnare";
            names[40] = "S3HiTimpani";
            names[41] = "S3LowTimpani";
            names[42] = "S3MidTimpani";
            names[43] = "QuickLooseSnare";
            names[44] = "Click";
            names[45] = "PowerKick";
            names[46] = "QuickGlassCrash";
            names[47] = "GlassCrashSnare";
            names[48] = "GlassCrash";
            names[49] = "GlassCrashKick";
            names[50] = "QuietGlassCrash";
            names[51] = "OddSnareKick";
            names[52] = "Claves";
            names[53] = "DanceSnare";
            names[54] = "LooseKick";
            names[55] = "HandDrum";
            names[56] = "PowerTom";
            names[57] = "HiWoodBlock";
            names[58] = "LowWoodBlock";
            names[59] = "HiConga";
            names[60] = "HiConga2";
            names[61] = "GavelHitDrum";
            names[62] = "GavelHitDrum2";
            names[63] = "GunshotHitDrum";
            names[64] = "GunshotHitDrum2";
            names[65] = "HitDrum3A";
            names[66] = "HitDrum3B";
            names[67] = "HitDrum3C";
            names[68] = "HitDrum3D";
            names[69] = "HitDrum3E";
            names[70] = "MetalCrashHit";
            names[71] = "EchoClap";
            names[72] = "LowEchoClap";
            names[73] = "HipHopKick";
            names[74] = "HipHopKickLow"; // same sample as row 73, lower rate
            names[75] = "DanceKick";
            names[76] = "HipHopKick2"; // different sample, sounds like a record scratch
            names[77] = "HipHopKick3";
            names[78] = "DeepHit";
            names[79] = "WoodBlock3";
            names[80] = "Unused";
            names[81] = "ReverseCymbal";
            names[82] = "PsytranceKick";
            names[83] = "PsytranceKickHi";
            names[84] = "PsytranceKickLow";
            names[85] = "PsytranceKickFloor";
            names[86] = "PsytranceSnare";
            names[87] = "PsytranceSnareHi";
            names[88] = "PsytranceSnareLow";
            names[89] = "PsytranceSnareFloor";
            names[90] = "Cowbell";
            names[91] = "Rimshot";
            names[92] = "Cuica";
            names[93] = "Guiro";
            return names;
        }
    }
    return {};
}

double eventTickDuration(const QJsonObject &obj) {
    if (obj.contains("note"))
        return obj.contains("duration") ? hexFieldToInt(obj.value("duration"), 1) : 1;
    if (obj.contains("tie") || obj.contains("inheritedNote"))
        return hexFieldToInt(obj.value("duration"), 1);
    return 0;
}

namespace {
// Last smpsSetvoice at or before `tickCutoff` within `events` -- -1 if none.
int lastVoiceChangeBeforeTick(const QJsonArray &events, double tickCutoff) {
    double tick = 0;
    int found = -1;
    for (const QJsonValue &ev : events) {
        if (!ev.isObject())
            continue;
        const QJsonObject obj = ev.toObject();
        if (obj.contains("smpsSetvoice")) {
            if (tick > tickCutoff)
                break;
            found = hexFieldToInt(obj.value("smpsSetvoice"), 0);
        }
        tick += eventTickDuration(obj);
    }
    return found;
}

// A block that smpsCall's into `target`, and that call's own tick position
// within the caller -- the FIRST one found (playlist order isn't
// authoritative here; a block can legitimately have multiple call sites
// with different active voices at each, so this is a best-effort play-test
// approximation, not a compiler-level guarantee).
bool findCaller(const QJsonObject &playlist, const QString &target, QString *callerOut, double *tickOut) {
    for (auto it = playlist.constBegin(); it != playlist.constEnd(); ++it) {
        double tick = 0;
        for (const QJsonValue &ev : it.value().toArray()) {
            if (ev.isObject()) {
                const QJsonObject obj = ev.toObject();
                if (obj.contains("smpsCall") && obj.value("smpsCall").toString() == target) {
                    *callerOut = it.key();
                    *tickOut = tick;
                    return true;
                }
                tick += eventTickDuration(obj);
            }
        }
    }
    return false;
}
} // namespace

int SongDocument::resolveVoiceAtTick(const QString &blockName, double tickCutoff) const {
    const QJsonObject pl = playlist();
    QSet<QString> visited;
    QString name = blockName;
    double cutoff = tickCutoff;
    while (!visited.contains(name)) {
        visited.insert(name);
        const int v = lastVoiceChangeBeforeTick(pl.value(name).toArray(), cutoff);
        if (v >= 0)
            return v;
        QString caller;
        double callerTick;
        if (!findCaller(pl, name, &caller, &callerTick))
            return 0; // nothing anywhere in the chain -- default voice 0
        name = caller;
        cutoff = callerTick;
    }
    return 0; // cycle guard
}

// "fTone_05" -> 5, or a bare hex/decimal value directly (smpsPSGvoice
// in-track events may use either form -- see compiler.c's own
// smpsPSGvoice handling for the same duality; the suffix is always parsed
// as hex there, which is a no-op distinction for the single digits 1-9
// this ever actually holds).
int parsePsgToneValue(const QString &s) {
    const int us = s.lastIndexOf('_');
    const QString numPart = us >= 0 ? s.mid(us + 1) : s;
    bool ok = false;
    const int v = numPart.toInt(&ok, 16);
    return ok ? v : 0;
}

bool SongDocument::psgChannelInfoForBlock(const QJsonArray &header, const QString &blockName, int *toneIndexOut,
                                           int *attenuationOut) {
    for (const QJsonValue &entryValue : header) {
        const QJsonObject entry = entryValue.toObject();
        if (!entry.contains("smpsHeaderPSG"))
            continue;
        const QJsonArray args = entry.value("smpsHeaderPSG").toArray();
        if (args.size() > 0 && args.at(0).toString() == blockName) {
            if (toneIndexOut)
                *toneIndexOut = args.size() > 3 ? parsePsgToneValue(args.at(3).toString()) : 0;
            if (attenuationOut)
                *attenuationOut = args.size() > 2 ? qBound(0, hexFieldToInt(args.at(2)), 0x0F) : 0;
            return true;
        }
    }
    return false;
}

void SongDocument::resolvePsgStateAtTick(const QString &blockName, double tickCutoff, int defaultTone,
                                          int *toneIndexOut, bool *noiseOut) const {
    int tone = defaultTone;
    bool noise = false;
    double tick = 0;
    for (const QJsonValue &ev : playlist().value(blockName).toArray()) {
        if (!ev.isObject())
            continue;
        const QJsonObject obj = ev.toObject();
        if (tick > tickCutoff)
            break;
        if (obj.contains("smpsPSGvoice"))
            tone = parsePsgToneValue(obj.value("smpsPSGvoice").toString());
        if (obj.contains("smpsPSGform"))
            noise = true; // irreversible on real hardware -- once seen, stays noise for the rest of the block
        tick += eventTickDuration(obj);
    }
    if (toneIndexOut)
        *toneIndexOut = tone;
    if (noiseOut)
        *noiseOut = noise;
}

bool SongDocument::saveToFile(const QString &path, QString *errorOut) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = f.errorString();
        return false;
    }
    const QJsonDocument doc(m_root);
    f.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

SongDocument SongDocument::makeEmptySong() {
    SongDocument doc;
    doc.m_root = QJsonObject{
        {"header", QJsonArray{}},
        {"voices", QJsonArray{}},
        {"SMPSplaylist", QJsonObject{}},
    };
    return doc;
}

SongDocument SongDocument::makeEmptyVoiceBank() {
    SongDocument doc;
    doc.m_root = QJsonObject{
        {"paradoxVoiceBank", true},
        {"voices", QJsonArray{}},
    };
    return doc;
}

int hexFieldToInt(const QJsonValue &value, int fallback) {
    if (value.isDouble())
        return static_cast<int>(value.toDouble());
    if (value.isString()) {
        const QString s = value.toString();
        bool ok = false;
        int result;
        if (s.startsWith("0x", Qt::CaseInsensitive))
            result = s.mid(2).toInt(&ok, 16);
        else
            result = s.toInt(&ok, 10);
        if (ok)
            return result;
    }
    return fallback;
}

QJsonValue intToHexField(int value) {
    // Matches _hexify()'s own rendering in asm_to_json.py: "0x" + uppercase
    // hex, no padding beyond the natural digit count.
    return QJsonValue(QString("0x%1").arg(value, 0, 16).toUpper());
}
