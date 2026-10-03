#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QScrollBar>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>

// A QScrollBar whose thumb can also be resized by dragging its own far
// edge -- repurposed here as a zoom control (per your direction): install
// one in place of a QScrollArea's default scrollbar via
// setVerticalScrollBar()/setHorizontalScrollBar(), then listen for
// thumbEdgeDragged() and adjust whatever "zoom level" that axis controls
// (row height for vertical, pixels-per-tick for horizontal in
// PianoRollPanel's case) -- this widget itself knows nothing about the
// piano roll. Falls through to normal QScrollBar behavior (thumb-drag-
// scroll, track clicks, wheel, keyboard) whenever the press isn't within
// the edge-grab zone.
class ZoomScrollBar : public QScrollBar {
    Q_OBJECT
public:
    explicit ZoomScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);

signals:
    // Raw mouse-movement delta (pixels) since the last emission during an
    // edge-drag -- positive means the thumb's far edge moved further from
    // the scrollbar's own origin (down for vertical, right for
    // horizontal), i.e. the thumb grew. Per your direction ("bigger the
    // scroll thumb, the smaller the display"), the caller should shrink
    // its zoom level as the thumb grows and enlarge it as the thumb
    // shrinks -- this signal only reports the raw drag, not that mapping.
    void thumbEdgeDragged(int deltaPixels);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool m_resizingThumb = false;
    int m_dragLastPos = 0; // last raw mouse coordinate along the scrollbar's own axis
};

// Vertical piano-key strip, kept in sync with PianoRollGridWidget's own
// vertical scroll (see PianoRollPanel, which owns both and forwards scroll
// position between them). Clicking a key previews that pitch; keys
// currently sounding at the playhead are highlighted (set via
// setActiveRows(), driven by PianoRollGridWidget's own note-layout data --
// no separate real-engine introspection needed, since the piano roll
// already knows every note's start/duration from laying out the bars).
class PianoKeysWidget : public QWidget {
    Q_OBJECT
public:
    explicit PianoKeysWidget(QWidget *parent = nullptr);
    void setScrollOffset(int pixels);
    void setActiveRows(const QSet<int> &rows);
    // Row height in pixels -- kept in sync with PianoRollGridWidget's own
    // (see PianoRollPanel, driven by the vertical ZoomScrollBar's thumb-edge
    // drag) so the two widgets' rows always line up.
    void setRowHeight(int rowHeight);
    int rowHeight() const { return m_rowHeight; }
    // DAC-home block mode (per your direction): labels the keys with DAC
    // sample names (Kick/Snare/Timpani/...) instead of pitches, on exactly
    // the rows real DAC note bytes map to -- other rows go blank, since
    // there's no sample there to trigger. sampleNames maps row -> label,
    // empty/absent rows aren't DAC-triggerable. Pass an empty map to return
    // to normal pitch-keyboard mode.
    void setDacSampleNames(const QMap<int, QString> &sampleNames);
    QSize sizeHint() const override;

signals:
    void notePressed(int noteIndex); // noteIndex: 0-95, same convention as AudioEngine::previewVoice
    void noteReleased();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    int rowAt(int y) const;

    int m_scrollOffset = 0;
    int m_rowHeight = 14;
    QMap<int, QString> m_dacSampleNames; // row -> sample label, empty = normal pitch-keyboard mode
    QSet<int> m_activeRows;
    int m_pressedRow = -1;
};

// The note grid itself: lays out a playlist block's event array as
// horizontal bars (note/tie/inheritedNote events) plus small flag markers
// for every other SMPS coordination flag (smpsLoop, smpsJump, smpsPan,
// etc.) at their time position. V1 scope: only TOP-LEVEL events become
// bars/markers -- events nested inside a smpsLoop/smpsJump body are not
// separately laid out (would need path-tracking through nested structure
// for editing), but the loop/jump's own flag marker is still shown and can
// be double-clicked to edit its full JSON, including that nested content,
// as text (same "edit the schema's own shape directly" approach used
// elsewhere in this tool).
class PianoRollGridWidget : public QWidget {
    Q_OBJECT
public:
    // A flag marker (smpsLoop/smpsJump/smpsPan/etc) -- public since
    // PianoFlagLaneWidget (below) draws these itself, in its own separate
    // strip below the note grid's scroll area, per your direction.
    struct FlagMarker {
        double tick;
        int eventIndex;
        QString label;
    };

    explicit PianoRollGridWidget(QWidget *parent = nullptr);

    void setEvents(const QJsonArray &events);
    QJsonArray events() const { return m_events; }
    // Block names offered by the right-click menu's "Jump to block..." /
    // "Loop to block..." entries (per your direction) -- set from
    // SongDocument::playlistOrder() via PianoRollPanel, so picking a target
    // doesn't require hand-typing a block name into the JSON edit dialog.
    void setAvailableBlockNames(const QStringList &names) { m_availableBlockNames = names; }

    void setPlayheadTick(double tick); // -1 hides the playhead
    double playheadTick() const { return m_playheadTick; }
    QSet<int> activeRowsAtPlayhead() const { return m_activeRows; }

    void deleteSelected(); // erases every selected event -- also bound to the Delete/Backspace keys

    // Horizontal scale, ticks -> pixels -- variable (not a fixed constant)
    // so the panel can auto-fit the block's full width to whatever space is
    // available on resize (see PianoRollPanel::resizeEvent), or be zoomed
    // manually via the horizontal ZoomScrollBar's thumb-edge drag.
    void setPixelsPerTick(double pixelsPerTick);
    double pixelsPerTick() const { return m_pixelsPerTick; }
    double totalTicks() const { return m_totalTicks; }

    // Row height in pixels -- zoomed via the vertical ZoomScrollBar's
    // thumb-edge drag (see PianoRollPanel). Large enough rows (see
    // paintEvent()) show the actual note name inside each bar.
    void setRowHeight(int rowHeight);
    int rowHeight() const { return m_rowHeight; }

    // Duration (ticks) a left-click places a new note at -- set from the
    // toolbar's note-length control (PianoRollPanel), per your direction.
    void setNoteLength(int length) { m_noteLength = qMax(1, length); }

    // Flags live here (this class owns m_events, the single source of
    // truth) but render/interact in PianoFlagLaneWidget, a separate strip
    // below the scroll area -- these let that widget mirror this class's
    // own data/selection without duplicating any mutation logic.
    QVector<FlagMarker> flags() const { return m_flags; }
    QSet<int> selectedIndices() const { return m_selectedIndices; }
    void insertFlagAt(double tick, const QJsonValue &value);
    void selectEvent(int eventIndex, bool addToSelection);
    void editEvent(int eventIndex) { editEventDialog(eventIndex); }
    // Builds and shows the flag-insertion/edit/delete context menu at
    // `tick` -- shared by this widget's own contextMenuEvent() and
    // PianoFlagLaneWidget's forwarded right-clicks.
    void showFlagMenuAt(double tick, const QPoint &globalPos);

    // Clipboard (per your direction: "since we have multiselect") -- the
    // selected NOTE events (rests aren't independently selectable, having
    // no bar of their own, so they're never part of a copy), pasted back
    // to back starting at `tick` on paste, auto-padded with a leading rest
    // the same way a fresh draw or a drag is.
    void copySelection() const;
    void cutSelection();
    void pasteAt(double tick);

    QSize sizeHint() const override;

signals:
    void eventsChanged(const QJsonArray &events);
    void activeRowsChanged(const QSet<int> &rows);
    void scrolled(); // geometry changed enough that the caller should re-sync scroll offsets
    // Brief audition, per your direction ("placing a note briefly plays it
    // and so does moving it") -- NOT emitted by edge-resizing. Mirrors
    // PianoKeysWidget's own notePressed/noteReleased pair so PianoRollPanel
    // can relay both through the exact same preview path.
    void notePreviewRequested(int noteIndex);
    void notePreviewStopRequested();
    // Flags/selection changed -- PianoFlagLaneWidget mirrors both to redraw
    // itself, since it holds no event data of its own.
    void flagsChanged(const QVector<FlagMarker> &flags);
    void selectionChanged(const QSet<int> &indices);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct NoteBar {
        double startTick;
        double duration;
        int noteIndex; // 0-95
        int eventIndex; // index into m_events, for edit/delete
    };
    // One selected note's position/content, captured once at the start of
    // a multi-note group drag (per your direction: "multimove") so every
    // group member's delta is computed against a stable reference, not a
    // mutating array.
    struct DragAnchor {
        int eventIndex;
        double tick;
        int row;
        bool canRepitch;
        QJsonObject data;
    };

    void relayout();
    void editEventDialog(int eventIndex);
    // Returns the eventIndex of a bar whose left or right edge is within a
    // few pixels of `pos` (a resize-handle hit test), or -1. isLeftEdgeOut
    // (if non-null) reports which edge was hit.
    int resizeHandleAt(const QPoint &pos, bool *isLeftEdgeOut = nullptr) const;
    // Array index to insert a new top-level event at so it lands
    // approximately at `tick` -- before the first existing bar/flag whose
    // own tick is >= it, or at the end if none.
    int insertionIndexForTick(double tick) const;
    int rowAtY(int y) const;
    // Y (bottom edge) for a bar at the given (real, 0-95) noteIndex --
    // rests never reach this (see relayout()'s own comment: they consume
    // tick time but never become a NoteBar, rendering as a plain gap
    // instead, per your direction).
    int barYBottom(int noteIndex) const;
    // Cumulative tick position just before m_events[index] -- same
    // duration-accumulation rules as relayout() (note/tie/inheritedNote,
    // including Rst, consume ticks; everything else is instantaneous).
    // Used to figure out how much leading rest a new/moved note needs.
    double tickAtIndex(int index) const;
    // Single-note body-drag (not the resize handle): repositions
    // m_draggingEventIndex in time (snapping to whole ticks -- "SMPS value
    // positioning") and, for real notes, in pitch (clamped to the 0-95 row
    // range -- "pitch boundaries"), auto-managing the leading rest needed
    // to land it there. No polyphony (per your direction): whatever's in
    // the way moves out of the way instead of blocking the drag.
    void performDragMove(const QPoint &pos);
    // Multi-note group drag (per your direction: "multimove") -- applies
    // the SAME tick/row delta the primary dragged note moves by to every
    // other selected note, preserving their relative spacing (so members
    // never collide with EACH other, only ever with unselected notes,
    // resolved the same push-out-of-the-way way per member). Only the
    // primary note briefly plays (per your direction: "you only hear the
    // note of the first one being moved").
    void performGroupDragMove(const QPoint &pos);
    // Left-edge resize: keeps the note's own END tick fixed and moves its
    // START (recomputing duration), auto-managing the leading rest the
    // same way performDragMove() does -- distinct from the right-edge
    // resize handled inline in mouseMoveEvent, which keeps the start fixed
    // instead. Does not push/displace neighbors (unlike a body drag) --
    // clamped to not go earlier than whatever precedes it.
    void performLeftEdgeResize(const QPoint &pos);
    // Emits notePreviewRequested(row) then, after a short delay,
    // notePreviewStopRequested() -- the "briefly plays it" audition for
    // placing/moving a note (per your direction), never for resizing.
    void briefPreview(int row);

    QJsonArray m_events;
    QStringList m_availableBlockNames;
    int m_noteLength = 12; // duration ticks a left-click draws -- see setNoteLength()
    QVector<NoteBar> m_bars;
    QVector<FlagMarker> m_flags;
    double m_totalTicks = 16;
    double m_pixelsPerTick = 8.0;
    int m_rowHeight = 14;
    // Multi-select (per your direction: ctrl-click adds to the selection).
    // m_selectedEventIndex is the "primary"/most-recently-touched one of
    // the set -- used for single-target operations (resize handles,
    // double-click edit) where multiple targets don't make sense.
    QSet<int> m_selectedIndices;
    int m_selectedEventIndex = -1;
    double m_playheadTick = -1;
    QSet<int> m_activeRows;

    int m_resizingEventIndex = -1; // event currently being edge-dragged, -1 if none
    bool m_resizingLeftEdge = false; // which edge, when m_resizingEventIndex >= 0
    double m_resizeFixedEndTick = 0; // left-edge resize only: the note's own end tick, held fixed all gesture
    double m_resizeGapStart = 0; // left-edge resize only: the immovable left boundary (end of whatever precedes it)

    // Body-drag (reposition), separate from the resize-handle drag above.
    int m_draggingEventIndex = -1; // -1 if none
    QPoint m_dragPressPos;
    bool m_dragMoved = false; // becomes true once movement passes a small threshold -- a plain click still just selects
    int m_dragLastPreviewRow = -1; // last row briefPreview() was called for during this drag, so repitching only re-triggers on an actual change
    QVector<DragAnchor> m_dragGroupAnchors; // non-empty only during a multi-note group drag
    double m_dragPrimaryAnchorTick = 0;
    int m_dragPrimaryAnchorRow = 0;
};

// A thin strip below the note grid's scroll area, showing just the flag
// markers (smpsLoop/smpsJump/smpsPan/etc) -- moved out of the note grid
// itself, per your direction, since flags relate to TIME position (synced
// here via setScrollOffset()/setPixelsPerTick(), same as PianoRulerWidget)
// but not to any particular pitch row, so they don't belong inside the
// vertically-scrollable note area. All the underlying event data and
// mutation logic still lives in PianoRollGridWidget (the single source of
// truth for the block's own events) -- this widget only draws whatever
// it's told via setFlags()/setSelectedIndices() and forwards mouse
// interaction back via signals; PianoRollPanel wires the two together.
class PianoFlagLaneWidget : public QWidget {
    Q_OBJECT
public:
    explicit PianoFlagLaneWidget(QWidget *parent = nullptr);
    void setScrollOffset(int pixels);
    void setPixelsPerTick(double pixelsPerTick);
    void setTotalTicks(double totalTicks);
    void setFlags(const QVector<PianoRollGridWidget::FlagMarker> &flags);
    void setSelectedIndices(const QSet<int> &indices);
    QSize sizeHint() const override;

signals:
    void flagClicked(int eventIndex, bool addToSelection); // addToSelection: ctrl held
    void flagDoubleClicked(int eventIndex);
    void contextMenuRequested(double tick, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    int flagAt(int x) const; // eventIndex of the flag badge nearest `x`, within a few pixels, or -1

    int m_scrollOffset = 0;
    double m_pixelsPerTick = 8.0;
    double m_totalTicks = 16;
    QVector<PianoRollGridWidget::FlagMarker> m_flags;
    QSet<int> m_selectedIndices;
};

// Compact, non-interactive piano-roll preview of a block's notes -- reuses
// the same note/tie/inheritedNote layout semantics as
// PianoRollGridWidget::relayout() (simplified: no flags, no nested
// loop/jump body expansion, just a quick visual reference of the note
// pattern). Shared by ChannelsPanel (a channel strip's home-block preview)
// and PlaylistPanel (per-block thumbnails in its card row) so both draw
// blocks identically instead of two separate copies.
class NoteThumbnailWidget : public QWidget {
    Q_OBJECT
public:
    explicit NoteThumbnailWidget(QWidget *parent = nullptr);
    void setEvents(const QJsonArray &events);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Bar {
        double startTick, duration;
        int row;
    };
    QVector<Bar> m_bars;
    double m_totalTicks = 1;
};

// Thin ruler strip shown just under the toolbar, above the note grid --
// marks tick positions along the same horizontal axis/scroll as the grid
// (PianoRollPanel keeps the two in horizontal sync). Not a real musical
// time signature (SMPS duration units aren't measures/beats), just labeled
// tick gridlines for visual reference.
class PianoRulerWidget : public QWidget {
    Q_OBJECT
public:
    explicit PianoRulerWidget(QWidget *parent = nullptr);
    void setScrollOffset(int pixels);
    void setTotalTicks(double totalTicks);
    void setPixelsPerTick(double pixelsPerTick);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_scrollOffset = 0;
    double m_totalTicks = 16;
    double m_pixelsPerTick = 8.0;
};
