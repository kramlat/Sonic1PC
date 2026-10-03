#pragma once

#include <QJsonArray>
#include <QMap>
#include <QWidget>

class PianoKeysWidget;
class PianoRollGridWidget;
class PianoFlagLaneWidget;
class PianoRulerWidget;
class QScrollArea;
class QPushButton;
class QToolBar;
class QComboBox;

// Composite piano-roll editor for one SMPSplaylist block at a time: toolbar
// (Play/Fit/Cut) + ruler + synced piano-key sidebar + note grid. Edits the
// SAME block data PlaylistPanel manages (per your direction -- "use it to
// edit blocks in playlist") -- MainWindow keeps both in sync against
// SongDocument, whichever one last changed.
class PianoRollPanel : public QWidget {
    Q_OBJECT
public:
    explicit PianoRollPanel(QWidget *parent = nullptr);

    void setBlock(const QString &name, const QJsonArray &events);
    QString blockName() const { return m_blockName; }
    // Feeds the right-click menu's block picker (per your direction) --
    // call whenever SongDocument::playlistOrder() changes.
    void setAvailableBlocks(const QStringList &names);
    // Current playhead/cursor position in ticks (-1 if hidden/never set) --
    // MainWindow resolves both the manual key-click preview and block
    // test-play's voice/attenuation against this same position (via
    // SongDocument::resolveVoiceAtTick), so both "hear" whatever the red
    // line is currently sitting on, per your direction.
    double currentPlayheadTick() const;

    // Playhead speed, per your direction ("relative to tempo and zoom"):
    // durationMult/mainTempo are the document's own smpsHeaderTempo fields
    // (Sound.c's cs->duration_mult/cs->main_tempo) -- MainWindow calls this
    // whenever a piano roll opens and whenever the transport tempo changes,
    // for every currently-open panel. Zoom (pixels-per-tick) needs no
    // separate handling here: the playhead's on-screen pixel speed already
    // scales with it automatically, since its x position is always
    // tick*pixelsPerTick.
    void setTempo(int durationMult, int mainTempo);

    // DAC-home block (per your direction: "list DAC samples on keys...").
    // Forwards to the piano-keys sidebar; empty map returns it to normal
    // pitch-keyboard mode. See SongDocument::dacSampleNamesForBlock().
    void setDacSampleNames(const QMap<int, QString> &sampleNames);

    // The Play/Fit/Cut/Length toolbar -- NOT added to this widget's own
    // layout (see the .cpp); MainWindow places it into the owning
    // DockableContainer's header row instead (DockableContainer::setToolBar,
    // a general feature of that window class), so it shares one row with
    // the gear menu instead of taking a separate row underneath.
    QToolBar *toolbar() const { return m_toolbar; }

    // Conflict-resolution dropdown (per your direction), lives in the same
    // toolbar as Play/Fit/Cut/Length -- hidden/empty whenever this block
    // isn't a genuinely conflicting shared block (BlockMetadata::
    // checkSharedConflict), otherwise populated with its distinct owners
    // (BlockMetadata::ownersOf) so the user can pick which owner's context
    // to preview under for this session. MainWindow drives both ends: it
    // calls setOwnerChoices()/clearOwnerChoices() after every setBlock(),
    // and reads back selectedOwnerContext() at preview time.
    //
    // owners[i]/descriptions[i] are parallel arrays -- descriptions is what
    // each owner actually RESOLVES to (per your direction: "fTone5 and
    // Voice 6", not just the bare block name, so a PSG-vs-FM conflict
    // reads as what it actually is at a glance), while the owner name
    // itself stays the combo's underlying item data (what
    // selectedOwnerContext() returns) since that's the string
    // BlockMetadata's own lookups key off of.
    void setOwnerChoices(const QStringList &owners, const QStringList &descriptions);
    void clearOwnerChoices();
    QString selectedOwnerContext() const;

    // Forwarded to the grid's own clipboard/selection ops (see
    // PianoRollGridWidget), so MainWindow's Edit menu Copy/Cut/Paste can
    // target whichever Piano Roll subwindow is currently active, the same
    // way Ctrl+C/X/V already work when the grid itself has focus.
    void copySelection() const;
    void cutSelection();
    void pasteAtPlayhead();

signals:
    void eventsChanged(const QString &blockName, const QJsonArray &events);
    void notePreviewRequested(int noteIndex);
    void notePreviewStopRequested();

public slots:
    // frameCount: AudioEngine::frameTicked's own 60Hz counter, converted
    // here to an approximate tick position (see .cpp -- SMPS duration
    // units aren't a fixed real-time length, this is a visual
    // approximation, not a claim of frame-exact sync with the real driver).
    void advancePlayhead(quint64 frameCount);
    void resetPlayhead();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void togglePlay(bool playing);
    void stepPlayback();
    void updateAutoFit(); // recomputes pixels-per-tick so the block's full width fits the viewport
    // Auto-scrolls the horizontal scrollbar to keep the playhead in view
    // once it reaches the edge of the currently visible area, per your
    // direction -- called after every playhead move during playback (both
    // block test-play and full-song sync), not during manual scrubbing.
    void followPlayhead(double tick);

    QString m_blockName;
    PianoKeysWidget *m_keys;
    PianoRollGridWidget *m_grid;
    PianoFlagLaneWidget *m_flagLane;
    PianoRulerWidget *m_ruler;
    QScrollArea *m_scrollArea;
    QToolBar *m_toolbar;
    QPushButton *m_playButton;
    QComboBox *m_ownerContextCombo;
    class QAction *m_ownerContextAction; // toolbar action wrapping the combo, so it can be hidden as a unit

    class QTimer *m_playbackTimer;
    double m_playbackTick = 0;
    int m_playingRow = -1;
    // Real-time scale for one SMPS duration unit, derived from the
    // document's own tempo (see setTempo()) -- default is a rough fallback
    // for before any real tempo has been supplied.
    double m_framesPerTick = 2.0;
    // Set once the horizontal ZoomScrollBar's thumb has been manually
    // dragged -- updateAutoFit() then stops overriding pixelsPerTick on
    // every resize, until "Fit" is pressed again.
    bool m_horizontalManualZoom = false;
};
