#include "BlockMetadata.h"
#include "SongDocument.h" // eventTickDuration/hexFieldToInt -- shared with SongDocument's own tick math, see BlockMetadata.h's header comment

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

void BlockMetadata::rebuild(const QJsonObject &playlist) {
    m_ownersByBlock.clear();
    m_targetsByBlock.clear();
    m_callSitesByTarget.clear();
    m_voiceChangesByBlock.clear();
    m_psgFormChangesByBlock.clear();
    m_psgVoiceChangesByBlock.clear();
    for (auto it = playlist.constBegin(); it != playlist.constEnd(); ++it) {
        double tick = 0;
        scanEvents(it.key(), it.value().toArray(), tick);
    }
}

void BlockMetadata::scanEvents(const QString &ownerBlock, const QJsonArray &events, double &tick) {
    for (const QJsonValue &ev : events) {
        if (!ev.isObject())
            continue;
        const QJsonObject obj = ev.toObject();
        if (obj.contains("smpsSetvoice")) {
            m_voiceChangesByBlock[ownerBlock].append({tick, hexFieldToInt(obj.value("smpsSetvoice"), 0)});
        } else if (obj.contains("smpsPSGform")) {
            m_psgFormChangesByBlock[ownerBlock].append(BlockPsgFormChange{tick});
        } else if (obj.contains("smpsPSGvoice")) {
            m_psgVoiceChangesByBlock[ownerBlock].append(
                BlockPsgVoiceChange{tick, parsePsgToneValue(obj.value("smpsPSGvoice").toString())});
        } else if (obj.contains("smpsCall")) {
            addEdge(ownerBlock, obj.value("smpsCall").toString(), tick);
        } else if (obj.contains("jumpTo")) {
            // The actual cross-block form of a real smpsJump/smpsLoop --
            // see this class's header comment.
            addEdge(ownerBlock, obj.value("jumpTo").toString(), tick);
        } else if (obj.contains("smpsLoop")) {
            // [idx, cnt, ...body] -- an INLINE loop, not a block reference;
            // still owned by `ownerBlock` itself, just scanned recursively
            // (same running `tick`, since the body plays inline as part of
            // this block's own timeline) in case a smpsCall/jumpTo/
            // smpsSetvoice is nested inside it.
            const QJsonArray body = obj.value("smpsLoop").toArray();
            QJsonArray inner;
            for (int i = 2; i < body.size(); i++)
                inner.append(body.at(i));
            scanEvents(ownerBlock, inner, tick);
        } else if (obj.contains("smpsJump")) {
            // [...body] -- same inline case as smpsLoop, no idx/cnt prefix.
            scanEvents(ownerBlock, obj.value("smpsJump").toArray(), tick);
        }
        tick += eventTickDuration(obj);
    }
}

void BlockMetadata::addEdge(const QString &owner, const QString &target, double tick) {
    if (target.isEmpty())
        return;
    // Same owner can legitimately smpsCall/jumpTo the same target from more
    // than one point in its own event stream (confirmed against real data,
    // e.g. GHZ's FM1 calling Call07 twice) -- that's still only ONE
    // distinct owner, not sharing, so skip inserting a duplicate edge into
    // the deduplicated owners/targets maps. callSitesOf() below is the
    // non-deduplicated version that keeps every occurrence, since each has
    // its own tick and therefore potentially a different active voice.
    if (!m_targetsByBlock.values(owner).contains(target)) {
        m_targetsByBlock.insert(owner, target);
        m_ownersByBlock.insert(target, owner);
    }
    m_callSitesByTarget.insert(target, BlockCallSite{owner, tick});
}

QStringList BlockMetadata::ownersOf(const QString &blockName) const { return m_ownersByBlock.values(blockName); }
QStringList BlockMetadata::targetsOf(const QString &blockName) const { return m_targetsByBlock.values(blockName); }
bool BlockMetadata::isSharedBlock(const QString &blockName) const { return m_ownersByBlock.count(blockName) > 1; }

QVector<BlockCallSite> BlockMetadata::callSitesOf(const QString &blockName) const {
    const QList<BlockCallSite> sites = m_callSitesByTarget.values(blockName);
    return QVector<BlockCallSite>(sites.cbegin(), sites.cend());
}

QVector<BlockVoiceChange> BlockMetadata::voiceChangesOf(const QString &blockName) const {
    return m_voiceChangesByBlock.value(blockName);
}

QVector<BlockPsgFormChange> BlockMetadata::psgFormChangesOf(const QString &blockName) const {
    return m_psgFormChangesByBlock.value(blockName);
}

QVector<BlockPsgVoiceChange> BlockMetadata::psgVoiceChangesOf(const QString &blockName) const {
    return m_psgVoiceChangesByBlock.value(blockName);
}

// Matches SongDocument::fmChannelAttenuationForBlock/psgChannelInfoForBlock/
// dacSampleNamesForBlock's own header-scanning convention (args[0] ==
// blockName) -- duplicated rather than reused since those three each
// return different payloads (attenuation/tone/sample names) this only
// needs the KIND from, not their values.
ChannelKind BlockMetadata::homeChannelKind(const QString &blockName, const QJsonArray &header) const {
    for (const QJsonValue &entryValue : header) {
        const QJsonObject entry = entryValue.toObject();
        auto matches = [&](const char *key) {
            const QJsonArray args = entry.value(key).toArray();
            return entry.contains(key) && args.size() > 0 && args.at(0).toString() == blockName;
        };
        if (matches("smpsHeaderFM"))
            return ChannelKind::FM;
        if (matches("smpsHeaderPSG"))
            return ChannelKind::PSG;
        if (matches("smpsHeaderDAC"))
            return ChannelKind::DAC;
    }
    return ChannelKind::Unknown;
}

ResolvedBlockState BlockMetadata::resolveStateAtTick(const QString &blockName, double cutoffTick,
                                                       const QJsonArray &header) const {
    QSet<QString> visited;
    return resolveStateAtTick(blockName, cutoffTick, header, visited);
}

ResolvedBlockState BlockMetadata::resolveStateAtTick(const QString &blockName, double cutoffTick,
                                                       const QJsonArray &header, QSet<QString> &visited) const {
    if (visited.contains(blockName))
        return ResolvedBlockState{}; // cycle guard -- matches SongDocument::resolveVoiceAtTick's own
    visited.insert(blockName);

    ResolvedBlockState state;
    state.kind = homeChannelKind(blockName, header);

    // Latest local smpsSetvoice at or before cutoffTick (both change lists
    // are already tick-ordered from the scan, so the last one that
    // qualifies wins -- matches SongDocument::lastVoiceChangeBeforeTick).
    int voice = -1;
    for (const BlockVoiceChange &vc : voiceChangesOf(blockName))
        if (vc.tick <= cutoffTick)
            voice = vc.voice;

    int tone = -1;
    for (const BlockPsgVoiceChange &pc : psgVoiceChangesOf(blockName))
        if (pc.tick <= cutoffTick)
            tone = pc.tone;

    // smpsPSGform is sticky (SongDocument::resolvePsgStateAtTick's own
    // "once seen, stays noise" semantics) -- true if ANY occurrence is at
    // or before cutoffTick, not just the latest.
    bool noise = false;
    for (const BlockPsgFormChange &pf : psgFormChangesOf(blockName))
        if (pf.tick <= cutoffTick)
            noise = true;

    if (voice >= 0) {
        state.voiceOrTone = voice;
        if (state.kind == ChannelKind::Unknown)
            state.kind = ChannelKind::FM;
    }
    if (tone >= 0) {
        state.voiceOrTone = tone;
        if (state.kind == ChannelKind::Unknown)
            state.kind = ChannelKind::PSG;
    }
    state.noiseActive = noise;

    if (voice < 0 && tone < 0 && state.kind == ChannelKind::Unknown) {
        // Nothing local, and not itself a header home block -- chase into
        // the ownership chain, same "keep walking backward" idea as
        // SongDocument::resolveVoiceAtTick. Picks the first distinct owner
        // found here (this is resolving ONE path at a time -- the actual
        // per-DISTINCT-owner fan-out that matters for conflict detection
        // already happens one level up, in checkSharedConflict's own loop
        // over ownersOf() for the block a conflict is being checked for).
        const QStringList upOwners = ownersOf(blockName);
        if (!upOwners.isEmpty()) {
            const QString &upOwner = upOwners.first();
            double upTick = -1;
            for (const BlockCallSite &site : callSitesOf(blockName))
                if (site.ownerBlock == upOwner && (upTick < 0 || site.tick < upTick))
                    upTick = site.tick;
            return resolveStateAtTick(upOwner, upTick, header, visited);
        }
    }
    return state;
}

bool BlockMetadata::checkSharedConflict(const QString &blockName, const QJsonArray &header) const {
    const QStringList owners = ownersOf(blockName);
    if (owners.size() < 2)
        return false; // not shared at all -- can't conflict, per your direction

    QVector<ResolvedBlockState> states;
    for (const QString &owner : owners) {
        // This owner's EARLIEST call site into blockName -- the state
        // active at the moment control first reaches the shared block via
        // this particular owner.
        double cutoff = -1;
        for (const BlockCallSite &site : callSitesOf(blockName))
            if (site.ownerBlock == owner && (cutoff < 0 || site.tick < cutoff))
                cutoff = site.tick;
        states.append(resolveStateAtTick(owner, cutoff, header));
    }
    for (int i = 1; i < states.size(); i++)
        if (!(states[i] == states[0]))
            return true; // actual divergence -- a genuine conflict, per your direction
    return false; // every distinct owner agrees -- no conflict even though it's shared
}

QString BlockMetadata::metadataPathFor(const QString &songFilePath) {
    const QFileInfo info(songFilePath);
    // "song.jsonc" -> "song.metadata.json" -- inserted before the LAST
    // extension only, so "song.jsonc" doesn't become "song.jsonc.metadata.json".
    return info.absolutePath() + "/" + info.completeBaseName() + ".metadata.json";
}

bool BlockMetadata::saveToFile(const QString &songFilePath, QString *errorOut) const {
    QJsonObject blocks;
    // Every block name any edge or voice change touches, so the union
    // covers blocks with no owners/targets too (still worth listing, with
    // empty arrays, so the file documents the full playlist, not just the
    // shared/voiced subset).
    QSet<QString> allNames;
    for (auto it = m_ownersByBlock.constBegin(); it != m_ownersByBlock.constEnd(); ++it) {
        allNames.insert(it.key());
        allNames.insert(it.value());
    }
    for (auto it = m_targetsByBlock.constBegin(); it != m_targetsByBlock.constEnd(); ++it) {
        allNames.insert(it.key());
        allNames.insert(it.value());
    }
    for (auto it = m_voiceChangesByBlock.constBegin(); it != m_voiceChangesByBlock.constEnd(); ++it)
        allNames.insert(it.key());
    for (auto it = m_psgFormChangesByBlock.constBegin(); it != m_psgFormChangesByBlock.constEnd(); ++it)
        allNames.insert(it.key());
    for (auto it = m_psgVoiceChangesByBlock.constBegin(); it != m_psgVoiceChangesByBlock.constEnd(); ++it)
        allNames.insert(it.key());

    QStringList sortedNames = allNames.values();
    sortedNames.sort();
    for (const QString &name : sortedNames) {
        QJsonObject entry;
        QStringList owners = ownersOf(name);
        owners.sort();
        QStringList targets = targetsOf(name);
        targets.sort();
        entry["owners"] = QJsonArray::fromStringList(owners);
        entry["targets"] = QJsonArray::fromStringList(targets);
        entry["shared"] = owners.size() > 1;

        QJsonArray callSites;
        for (const BlockCallSite &site : callSitesOf(name)) {
            QJsonObject siteObj;
            siteObj["owner"] = site.ownerBlock;
            siteObj["tick"] = site.tick;
            callSites.append(siteObj);
        }
        entry["callSites"] = callSites;

        QJsonArray voiceChanges;
        for (const BlockVoiceChange &change : voiceChangesOf(name)) {
            QJsonObject changeObj;
            changeObj["tick"] = change.tick;
            changeObj["voice"] = change.voice;
            voiceChanges.append(changeObj);
        }
        entry["voiceChanges"] = voiceChanges;

        QJsonArray psgFormChanges;
        for (const BlockPsgFormChange &change : psgFormChangesOf(name)) {
            QJsonObject changeObj;
            changeObj["tick"] = change.tick;
            psgFormChanges.append(changeObj);
        }
        entry["psgFormChanges"] = psgFormChanges;

        QJsonArray psgVoiceChanges;
        for (const BlockPsgVoiceChange &change : psgVoiceChangesOf(name)) {
            QJsonObject changeObj;
            changeObj["tick"] = change.tick;
            changeObj["tone"] = change.tone;
            psgVoiceChanges.append(changeObj);
        }
        entry["psgVoiceChanges"] = psgVoiceChanges;

        blocks[name] = entry;
    }

    QJsonObject root;
    root["blocks"] = blocks;

    QFile f(metadataPathFor(songFilePath));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut = f.errorString();
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
