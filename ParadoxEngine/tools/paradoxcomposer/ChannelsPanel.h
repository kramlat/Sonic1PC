#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QVBoxLayout;
class ChannelVUWidget;

// Channel Rack-style editor over the document's header entries (per your
// Qt Designer reference -- "resembles the FL Studio one just reworked for
// a smps editor"): one strip per smpsHeaderDAC/FM/PSG channel (label,
// Displacement/Vol knobs, "Select Home" block picker, a mini piano-roll
// thumbnail of that channel's home block), grouped into "FM + DAC" and
// "PSG" (matching the real byte format -- smpsHeaderChan's fm_count counts
// DAC+FM entries together as one group, psg_count is separate), each group
// with +/- buttons to change its channel count.
class ChannelsPanel : public QWidget {
    Q_OBJECT
public:
    explicit ChannelsPanel(QWidget *parent = nullptr);

    void setHeader(const QJsonArray &header);
    QJsonArray header() const { return m_header; }
    // Needed for the "Select Home" picker's choices and the thumbnail's
    // note data -- call whenever SongDocument's playlist changes.
    void setPlaylist(const QJsonObject &playlist, const QStringList &order);

public slots:
    // From AudioEngine::channelActivityUpdated, forwarded by MainWindow --
    // updates each strip's VU bar(s) live during playback.
    void setChannelActivity(const QVector<float> &fmLevelsL, const QVector<float> &fmLevelsR,
                             const QVector<float> &psgLevels);

signals:
    void headerChanged(const QJsonArray &header);
    // Emitted when a +Channel button needs a fresh home block -- MainWindow
    // adds it to the document's playlist (a single smpsStop event) so the
    // new channel is immediately valid/compilable, not a dangling pointer.
    void blockNeeded(const QString &name, const QJsonArray &events);
    // channelIndex matches Sound_DebugSetChannelMuted's own indexing (PSG
    // 0-3, then FM/DAC) -- emitted once per affected channel whenever a
    // mute or solo checkbox changes (solo affects every OTHER channel's
    // effective mute too, not just the one clicked).
    void channelMuteChanged(int channelIndex, bool muted);

private:
    struct ChannelEntry {
        QString macro; // smpsHeaderDAC / smpsHeaderFM / smpsHeaderPSG
        int headerIndex;
        QString label; // "DAC", "FM1".., "PSG1"..
        int runtimeChannelIndex; // Sound_DebugSetChannelMuted's own indexing -- PSG 0-3, then FM/DAC
    };

    void rebuild();
    QWidget *buildStrip(const ChannelEntry &entry);
    void writeArg(int headerIndex, int argIndex, const QJsonValue &value);
    void addChannel(bool isFmGroup);
    void removeChannel(bool isFmGroup);
    QString freshBlockName(const QString &hint) const;
    void setChanCounts(int fmCount, int psgCount);
    QPair<int, int> chanCounts() const; // (fm_count, psg_count) from smpsHeaderChan
    void applyMuteSolo(); // recomputes effective mute for every known channel and emits channelMuteChanged

    QJsonArray m_header;
    QJsonObject m_playlist;
    QStringList m_playlistOrder;

    QVBoxLayout *m_fmStripsLayout;
    QVBoxLayout *m_psgStripsLayout;

    // Keyed by label ("FM1", "PSG2", ...) so mute/solo state survives
    // rebuild() recreating the strip widgets (e.g. after an unrelated arg
    // edit elsewhere in the document).
    QSet<QString> m_mutedLabels;
    QSet<QString> m_soloedLabels;
    QMap<QString, int> m_labelToChannelIndex; // refreshed each rebuild()
    QMap<QString, ChannelVUWidget *> m_labelToVU; // refreshed each rebuild()
};
