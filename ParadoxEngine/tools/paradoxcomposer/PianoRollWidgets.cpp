#include "PianoRollWidgets.h"
#include "NativeCompiler.h"
#include "SongDocument.h"

#include <QAction>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QStyleOptionSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {
// Inverse of NativeCompiler::noteValue() for row 0-95 -- used only to give a
// newly-drawn note a name; NativeCompiler::noteValue() stays the single
// source of truth for the forward direction (name -> row), this is purely
// for note creation and isn't relied on for compiling.
QString noteNameForRow(int row) {
    static const char *kNames[12] = {"C", "Cs", "D", "Ds", "E", "F", "Fs", "G", "Gs", "A", "As", "B"};
    return QString("%1%2").arg(kNames[row % 12]).arg(row / 12);
}
} // namespace

namespace {
constexpr int kMinRowHeight = 6;
constexpr int kMaxRowHeight = 40;
constexpr int kRowHeightForNoteLabel = 18; // rows at least this tall show the note name inside the bar
constexpr int kRowCount = 96; // matches AudioEngine::previewVoice's own 0-95 note_index range
constexpr int kKeysWidth = 56;

// Standard 12-semitone black-key pattern (C#,D#,F#,G#,A#), used both for
// the piano-key sidebar's own key shapes and the grid's row shading.
bool isBlackKeyRow(int row) {
    static const QSet<int> blackSemitones{1, 3, 6, 8, 10};
    return blackSemitones.contains(((row % 12) + 12) % 12);
}

// Normalizes a flag event to one label for icon lookup -- in particular,
// "jumpTo" (the converter's escape-hatch reference, see
// asm_to_json.py/json_to_header.py) represents EITHER a plain jump OR a
// loop depending on whether "smpsLoopArgs" rides along with it (matching
// NativeCompiler::emitEvent's own branch: smpsLoopArgs present -> real
// opcode 0xF7/loop, absent -> 0xF6/jump), so it needs the loop icon in the
// first case, not the jump icon.
QString classifyFlagLabel(const QJsonObject &obj) {
    if (obj.contains("smpsLoop"))
        return "smpsLoop";
    if (obj.contains("smpsJump"))
        return "smpsJump";
    if (obj.contains("jumpTo"))
        return obj.contains("smpsLoopArgs") ? "smpsLoop" : "smpsJump";
    if (obj.contains("smpsCall"))
        return "smpsCall";
    if (obj.contains("smpsPan"))
        return "smpsPan";
    if (obj.contains("smpsModSet"))
        return "smpsModSet";
    if (obj.contains("smpsMod"))
        return "smpsMod";
    if (obj.contains("smpsPSGvoice"))
        return "smpsPSGvoice";
    return obj.isEmpty() ? "?" : obj.keys().first();
}

// Per-flag-type glyph (per your direction -- distinct icon per SMPS
// coordination flag instead of one generic marker). Drawn as Unicode
// symbol text for now (simple, no asset files needed); swap for real
// QPixmap artwork later if you'd rather supply icons.
QString flagGlyph(const QString &label) {
    static const QMap<QString, QString> glyphs{
        {"smpsNoAttack", QString::fromUtf8("\xE2\x9C\x95")},    // X (no-attack/tie marker)
        {"smpsJump", QString::fromUtf8("\xE2\x86\x92")},        // -> (jump)
        {"smpsLoop", QString::fromUtf8("\xE2\x86\xBB")},        // reload/repeat (loop)
        {"smpsStop", QString::fromUtf8("\xE2\x96\xA0")},        // stop square
        {"smpsClearPush", QString::fromUtf8("\xE2\x96\xA0")},
        {"smpsStopSpecial", QString::fromUtf8("\xE2\x96\xA0")},
        {"smpsCall", QString::fromUtf8("\xE2\x86\xAA")},        // call/return arrow
        {"smpsPan", QString::fromUtf8("\xE2\x87\x84")},         // left-right arrows
        {"smpsMod", QString::fromUtf8("\xE2\x88\xBF")},         // wave (modulation)
        {"smpsModSet", QString::fromUtf8("\xE2\x88\xBF")},
        {"smpsFade", QString::fromUtf8("\xE2\x97\x90")},        // half-circle (fade)
        {"smpsPSGvoice", QString::fromUtf8("\xE2\x99\xAA")},    // musical note (PSG envelope voice)
    };
    return glyphs.value(label, QString::fromUtf8("\xE2\x97\x86")); // diamond fallback for anything else
}

constexpr int kThumbEdgeGrabZone = 6; // pixels near the thumb's far edge that grab for resizing instead of scrolling
} // namespace

// ---------------------------------------------------------------------------
// ZoomScrollBar
// ---------------------------------------------------------------------------

ZoomScrollBar::ZoomScrollBar(Qt::Orientation orientation, QWidget *parent) : QScrollBar(orientation, parent) {}

void ZoomScrollBar::mousePressEvent(QMouseEvent *event) {
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    const QRect thumb = style()->subControlRect(QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarSlider, this);
    const int pos = orientation() == Qt::Vertical ? event->pos().y() : event->pos().x();
    const int farEdge = orientation() == Qt::Vertical ? thumb.bottom() : thumb.right();
    if (thumb.isValid() && qAbs(pos - farEdge) <= kThumbEdgeGrabZone) {
        m_resizingThumb = true;
        m_dragLastPos = pos;
        event->accept();
        return;
    }
    QScrollBar::mousePressEvent(event);
}

void ZoomScrollBar::mouseMoveEvent(QMouseEvent *event) {
    if (m_resizingThumb) {
        const int pos = orientation() == Qt::Vertical ? event->pos().y() : event->pos().x();
        const int delta = pos - m_dragLastPos;
        m_dragLastPos = pos;
        if (delta != 0)
            emit thumbEdgeDragged(delta);
        event->accept();
        return;
    }
    QScrollBar::mouseMoveEvent(event);
}

void ZoomScrollBar::mouseReleaseEvent(QMouseEvent *event) {
    if (m_resizingThumb) {
        m_resizingThumb = false;
        event->accept();
        return;
    }
    QScrollBar::mouseReleaseEvent(event);
}

// ---------------------------------------------------------------------------
// PianoKeysWidget
// ---------------------------------------------------------------------------

PianoKeysWidget::PianoKeysWidget(QWidget *parent) : QWidget(parent) {
    setFixedWidth(kKeysWidth);
}

QSize PianoKeysWidget::sizeHint() const { return QSize(kKeysWidth, kRowCount * m_rowHeight); }

void PianoKeysWidget::setScrollOffset(int pixels) {
    m_scrollOffset = pixels;
    update();
}

void PianoKeysWidget::setActiveRows(const QSet<int> &rows) {
    m_activeRows = rows;
    update();
}

void PianoKeysWidget::setRowHeight(int rowHeight) {
    m_rowHeight = qBound(kMinRowHeight, rowHeight, kMaxRowHeight);
    updateGeometry();
    update();
}

void PianoKeysWidget::setDacSampleNames(const QMap<int, QString> &sampleNames) {
    m_dacSampleNames = sampleNames;
    update();
}

int PianoKeysWidget::rowAt(int y) const {
    // Row 0 (C0) is drawn at the BOTTOM (highest y), matching a real piano
    // roll's low-to-high-going-up convention.
    const int totalHeight = kRowCount * m_rowHeight;
    const int fromBottom = totalHeight - (y + m_scrollOffset);
    return fromBottom / m_rowHeight;
}

void PianoKeysWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    const int totalHeight = kRowCount * m_rowHeight;
    const bool dacMode = !m_dacSampleNames.isEmpty();

    for (int row = 0; row < kRowCount; row++) {
        const int yBottom = totalHeight - row * m_rowHeight - m_scrollOffset;
        const int yTop = yBottom - m_rowHeight;
        if (yBottom < 0 || yTop > height())
            continue;
        const QRect keyRect(0, yTop, width(), m_rowHeight);
        const bool active = m_activeRows.contains(row);
        // DAC-home block (per your direction): only rows with an actual
        // sample to trigger stay "live" -- the rest are dimmed, since
        // there's nothing there to play.
        const bool isDacRow = dacMode && m_dacSampleNames.contains(row);
        QColor fill;
        if (dacMode)
            fill = isDacRow ? QColor(60, 45, 20) : QColor(25, 25, 27);
        else
            fill = isBlackKeyRow(row) ? QColor(40, 40, 45) : QColor(250, 250, 250);
        if (active)
            fill = QColor(90, 170, 255);
        p.fillRect(keyRect, fill);
        p.setPen(QColor(120, 120, 120));
        p.drawRect(keyRect);

        if (dacMode) {
            if (isDacRow) {
                p.setPen(QColor(255, 210, 140));
                p.drawText(keyRect.adjusted(4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft, m_dacSampleNames.value(row));
            }
            continue;
        }
        // Zoomed in enough to show a note's own name inside its box (per
        // your direction: the same threshold should label every key here
        // too, not just the C rows) -- otherwise just label C notes for
        // orientation (row%12==0 is a C, per note_value()'s own 0x81=C0
        // base), same as before this zoom level existed.
        if (m_rowHeight >= kRowHeightForNoteLabel) {
            p.setPen(isBlackKeyRow(row) ? Qt::white : Qt::black);
            p.drawText(keyRect.adjusted(4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft, noteNameForRow(row));
        } else if (row % 12 == 0) {
            p.setPen(isBlackKeyRow(row) ? Qt::white : Qt::black);
            p.drawText(keyRect.adjusted(4, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft, QString("C%1").arg(row / 12));
        }
    }
}

void PianoKeysWidget::mousePressEvent(QMouseEvent *event) {
    const int row = rowAt(event->pos().y());
    if (row < 0 || row >= kRowCount)
        return;
    // DAC-home block: only rows with an actual sample do anything -- there's
    // nothing meaningful to preview on the rest.
    if (!m_dacSampleNames.isEmpty() && !m_dacSampleNames.contains(row))
        return;
    m_pressedRow = row;
    emit notePressed(m_pressedRow);
}

void PianoKeysWidget::mouseReleaseEvent(QMouseEvent *) {
    if (m_pressedRow >= 0)
        emit noteReleased();
    m_pressedRow = -1;
}

// ---------------------------------------------------------------------------
// PianoRollGridWidget
// ---------------------------------------------------------------------------

PianoRollGridWidget::PianoRollGridWidget(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus); // so a click here can actually receive the Delete/Backspace key, per your direction
}

QSize PianoRollGridWidget::sizeHint() const {
    return QSize(qMax(400, int(m_totalTicks * m_pixelsPerTick)), kRowCount * m_rowHeight);
}

int PianoRollGridWidget::barYBottom(int noteIndex) const { return kRowCount * m_rowHeight - noteIndex * m_rowHeight; }

void PianoRollGridWidget::setPixelsPerTick(double pixelsPerTick) {
    m_pixelsPerTick = qMax(0.5, pixelsPerTick);
    updateGeometry();
    update();
}

void PianoRollGridWidget::setRowHeight(int rowHeight) {
    m_rowHeight = qBound(kMinRowHeight, rowHeight, kMaxRowHeight);
    updateGeometry();
    update();
}

void PianoRollGridWidget::setEvents(const QJsonArray &events) {
    m_events = events;
    m_selectedEventIndex = -1;
    m_selectedIndices.clear();
    relayout();
    emit selectionChanged(m_selectedIndices);
}

// Walks the block's TOP-LEVEL events (see this class's own header comment
// for why nested loop/jump bodies aren't separately laid out in v1),
// tracking cumulative time and the last-seen pitch (for tie/inheritedNote,
// which inherit row placement from context -- same semantics
// NoteStreamState uses in asm_to_json.py).
void PianoRollGridWidget::relayout() {
    m_bars.clear();
    m_flags.clear();
    double tick = 0;
    int lastNote = 48; // arbitrary fallback row if a tie/inheritedNote opens the block with no prior note

    for (int i = 0; i < m_events.size(); i++) {
        const QJsonValue ev = m_events.at(i);
        if (ev.isString()) {
            m_flags.append({tick, i, ev.toString()});
            continue;
        }
        const QJsonObject obj = ev.toObject();
        if (obj.contains("note")) {
            const QString noteName = obj.value("note").toString();
            const double dur = obj.contains("duration") ? hexFieldToInt(obj.value("duration"), 1) : 1;
            if (noteName == "Rst") {
                // Rests render as a gap in the timeline -- a real piano
                // roll's normal convention -- not a visible bar, per your
                // direction. Still consumes its duration so later events
                // land at the right tick position; doesn't touch lastNote.
                tick += dur;
                continue;
            }
            const int nv = NativeCompiler::noteValue(noteName);
            const int row = qBound(0, nv - 0x81, kRowCount - 1);
            lastNote = row;
            m_bars.append({tick, dur, row, i});
            tick += dur;
        } else if (obj.contains("tie") || obj.contains("inheritedNote")) {
            const double dur = hexFieldToInt(obj.value("duration"), 1);
            m_bars.append({tick, dur, lastNote, i});
            tick += dur;
        } else {
            m_flags.append({tick, i, classifyFlagLabel(obj)});
        }
    }
    m_totalTicks = qMax(16.0, tick + 8);
    updateGeometry();
    update();
    emit flagsChanged(m_flags);
}

double PianoRollGridWidget::tickAtIndex(int index) const {
    double tick = 0;
    for (int i = 0; i < index && i < m_events.size(); i++) {
        const QJsonValue ev = m_events.at(i);
        if (ev.isString())
            continue;
        const QJsonObject obj = ev.toObject();
        if (obj.contains("note"))
            tick += obj.contains("duration") ? hexFieldToInt(obj.value("duration"), 1) : 1;
        else if (obj.contains("tie") || obj.contains("inheritedNote"))
            tick += hexFieldToInt(obj.value("duration"), 1);
    }
    return tick;
}

void PianoRollGridWidget::setPlayheadTick(double tick) {
    m_playheadTick = tick;
    QSet<int> active;
    if (tick >= 0) {
        for (const NoteBar &b : m_bars)
            if (tick >= b.startTick && tick < b.startTick + b.duration)
                active.insert(b.noteIndex);
    }
    if (active != m_activeRows) {
        m_activeRows = active;
        emit activeRowsChanged(m_activeRows);
    }
    update();
}

void PianoRollGridWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    const int totalHeight = kRowCount * m_rowHeight;

    // Row shading matching the piano keys (per your direction) -- black-key
    // rows shaded darker, so the grid reads the same way the keys sidebar
    // does at a glance.
    for (int row = 0; row < kRowCount; row++) {
        const int yBottom = totalHeight - row * m_rowHeight;
        const int yTop = yBottom - m_rowHeight;
        p.fillRect(QRect(0, yTop, width(), m_rowHeight), isBlackKeyRow(row) ? QColor(30, 30, 34) : QColor(45, 45, 50));
    }
    p.setPen(QColor(60, 60, 66));
    for (int row = 0; row <= kRowCount; row += 12)
        p.drawLine(0, totalHeight - row * m_rowHeight, width(), totalHeight - row * m_rowHeight);

    // Vertical tick gridlines, every 12 ticks (arbitrary visual spacing) --
    // drawn across the widget's own full width, not just m_totalTicks, so
    // any extra room past the block's own content (see PianoRollPanel::
    // updateAutoFit()) still looks gridded and usable. Purely a visual
    // guide past the real content -- phantom measures, ignored by playback
    // and never written to the actual event array.
    p.setPen(QColor(55, 55, 60));
    const double visibleTicks = width() / m_pixelsPerTick;
    for (int t = 0; t < visibleTicks; t += 12)
        p.drawLine(int(t * m_pixelsPerTick), 0, int(t * m_pixelsPerTick), height());

    // Note bars -- rests never reach here (see relayout()'s own comment:
    // they consume tick time but never append a NoteBar, so they render as
    // a plain gap). Rows tall enough to fit text (per your direction, via
    // the vertical ZoomScrollBar's thumb-edge zoom) also show the actual
    // note name inside the bar.
    const bool showNoteLabels = m_rowHeight >= kRowHeightForNoteLabel;
    for (const NoteBar &b : m_bars) {
        const int x = int(b.startTick * m_pixelsPerTick);
        const int w = qMax(2, int(b.duration * m_pixelsPerTick) - 1);
        const int yBottom = barYBottom(b.noteIndex);
        const QRect barRect(x, yBottom - m_rowHeight, w, m_rowHeight - 1);
        QColor fill = m_selectedIndices.contains(b.eventIndex) ? QColor(255, 200, 60) : QColor(60, 120, 255);
        p.fillRect(barRect, fill);
        p.setPen(QColor(20, 20, 24));
        p.drawRect(barRect);
        if (showNoteLabels) {
            p.setPen(Qt::black);
            p.drawText(barRect.adjusted(3, 0, -2, 0), Qt::AlignVCenter | Qt::AlignLeft, noteNameForRow(b.noteIndex));
        }
    }

    // Flag markers now render in PianoFlagLaneWidget, a separate strip
    // below the scroll area (per your direction) -- see this class's own
    // flagsChanged() signal/flags() getter.

    // Playhead.
    if (m_playheadTick >= 0) {
        const int x = int(m_playheadTick * m_pixelsPerTick);
        p.setPen(QPen(QColor(255, 255, 255), 2));
        p.drawLine(x, 0, x, height());
    }
}

int PianoRollGridWidget::resizeHandleAt(const QPoint &pos, bool *isLeftEdgeOut) const {
    constexpr int kHandleSlop = 4;
    for (const NoteBar &b : m_bars) {
        const int yBottom = barYBottom(b.noteIndex);
        if (pos.y() < yBottom - m_rowHeight || pos.y() >= yBottom)
            continue;
        const int rightEdgeX = int((b.startTick + b.duration) * m_pixelsPerTick);
        if (qAbs(pos.x() - rightEdgeX) <= kHandleSlop) {
            if (isLeftEdgeOut)
                *isLeftEdgeOut = false;
            return b.eventIndex;
        }
        const int leftEdgeX = int(b.startTick * m_pixelsPerTick);
        if (qAbs(pos.x() - leftEdgeX) <= kHandleSlop) {
            if (isLeftEdgeOut)
                *isLeftEdgeOut = true;
            return b.eventIndex;
        }
    }
    return -1;
}

int PianoRollGridWidget::rowAtY(int y) const {
    return qBound(0, (kRowCount * m_rowHeight - y) / m_rowHeight, kRowCount - 1);
}

int PianoRollGridWidget::insertionIndexForTick(double tick) const {
    int best = m_events.size();
    for (const NoteBar &b : m_bars)
        if (b.startTick >= tick && b.eventIndex < best)
            best = b.eventIndex;
    for (const FlagMarker &f : m_flags)
        if (f.tick >= tick && f.eventIndex < best)
            best = f.eventIndex;
    return best;
}

void PianoRollGridWidget::insertFlagAt(double tick, const QJsonValue &value) {
    const int idx = insertionIndexForTick(tick);
    m_events.insert(idx, value);
    selectEvent(idx, false);
    relayout();
    emit eventsChanged(m_events);
}

void PianoRollGridWidget::selectEvent(int eventIndex, bool addToSelection) {
    if (addToSelection) {
        if (m_selectedIndices.contains(eventIndex))
            m_selectedIndices.remove(eventIndex);
        else
            m_selectedIndices.insert(eventIndex);
    } else {
        m_selectedIndices = {eventIndex};
    }
    m_selectedEventIndex = eventIndex;
    emit selectionChanged(m_selectedIndices);
}

void PianoRollGridWidget::briefPreview(int row) {
    emit notePreviewRequested(row);
    QTimer::singleShot(150, this, [this]() { emit notePreviewStopRequested(); });
}

void PianoRollGridWidget::mousePressEvent(QMouseEvent *event) {
    const int x = event->pos().x(), y = event->pos().y();
    const bool isLeftClick = event->button() == Qt::LeftButton;
    const bool ctrl = event->modifiers() & Qt::ControlModifier; // per your direction: ctrl-click selects multiples

    bool isLeftEdge = false;
    const int handle = isLeftClick ? resizeHandleAt(event->pos(), &isLeftEdge) : -1;
    if (handle >= 0) {
        m_resizingEventIndex = handle;
        m_resizingLeftEdge = isLeftEdge;
        selectEvent(handle, ctrl);
        if (isLeftEdge) {
            // Fixed reference points for the whole gesture: the note's own
            // end tick (held fixed while its start moves), and the true
            // left boundary this drag is free to eat into (end of
            // whatever precedes it, past any leading rest of its own,
            // which this drag can freely resize/remove).
            double endTick = 0;
            for (const NoteBar &b : m_bars)
                if (b.eventIndex == handle)
                    endTick = b.startTick + b.duration;
            m_resizeFixedEndTick = endTick;
            int gapIdx = handle;
            if (gapIdx > 0 && m_events.at(gapIdx - 1).toObject().value("note").toString() == "Rst")
                gapIdx--;
            m_resizeGapStart = tickAtIndex(gapIdx);
        }
        update();
        return;
    }

    const double tick = double(x) / m_pixelsPerTick;
    int hitBar = -1;
    for (const NoteBar &b : m_bars) {
        const int yBottom = barYBottom(b.noteIndex);
        if (tick >= b.startTick && tick < b.startTick + b.duration && y >= yBottom - m_rowHeight && y < yBottom) {
            hitBar = b.eventIndex;
            break;
        }
    }
    if (hitBar >= 0 && isLeftClick) {
        selectEvent(hitBar, ctrl);
        if (!ctrl) {
            // Body-drag candidate (not yet committed -- see mouseMoveEvent's
            // own threshold check, so a plain click still just selects).
            // Ctrl-click stays a pure selection toggle -- dragging doesn't
            // have a sensible single meaning across a multi-selection yet.
            m_draggingEventIndex = hitBar;
            m_dragPressPos = event->pos();
            m_dragMoved = false;
            m_dragLastPreviewRow = -1;
            // Multi-note group drag (per your direction: "multimove") --
            // capture every selected note's current position/content once,
            // up front, so performGroupDragMove() has a stable reference
            // each move rather than a mutating array. Only kicks in when
            // the note being dragged is itself part of a >1-note selection.
            m_dragGroupAnchors.clear();
            if (m_selectedIndices.size() > 1 && m_selectedIndices.contains(hitBar)) {
                for (const NoteBar &b : m_bars) {
                    if (!m_selectedIndices.contains(b.eventIndex))
                        continue;
                    const QJsonObject data = m_events.at(b.eventIndex).toObject();
                    const bool canRepitch = data.contains("note") && data.value("note").toString() != "Rst";
                    m_dragGroupAnchors.append({b.eventIndex, b.startTick, b.noteIndex, canRepitch, data});
                    if (b.eventIndex == hitBar) {
                        m_dragPrimaryAnchorTick = b.startTick;
                        m_dragPrimaryAnchorRow = b.noteIndex;
                    }
                }
            }
        }
        update();
        return;
    }

    // Below the last pitch row (e.g. the "phantom" viewport-filling area
    // past the block's own content) -- nothing lives here anymore now that
    // flags moved to their own strip (PianoFlagLaneWidget), so there's
    // nothing to select or draw.
    const int totalHeight = kRowCount * m_rowHeight;
    if (y >= totalHeight) {
        if (!ctrl) {
            m_selectedIndices.clear();
            m_selectedEventIndex = -1;
            emit selectionChanged(m_selectedIndices);
        }
        update();
        return;
    }

    if (!ctrl) {
        m_selectedIndices.clear();
        m_selectedEventIndex = -1;
        emit selectionChanged(m_selectedIndices);
    }

    // Nothing hit -- draw a new note of the toolbar-selected length here
    // (per your direction: a left click places a note of that length at
    // the exact place clicked, snapped to the nearest whole tick -- "SMPS
    // value positioning" -- rather than the old click-drag-to-set-duration
    // gesture), and briefly plays it. Any gap between where the previous
    // event ends and the click position becomes an explicit, auto-inserted
    // rest, since the format has no other way to represent silence (see
    // relayout()'s own comment on rests rendering as a gap). Left-click
    // only, and not while ctrl is held (that stays purely about selection,
    // never accidentally creates a note) -- a right-click here opens the
    // note context menu (Copy/Cut/Paste/Edit/Delete) instead, and must not
    // also draw a note underneath it.
    if (isLeftClick && !ctrl) {
        const int row = rowAtY(y);
        const double clickedTick = qMax(0.0, (double)qRound(tick));
        const int insertIdx = insertionIndexForTick(clickedTick);
        const double gapStart = tickAtIndex(insertIdx);

        int slot = insertIdx;
        if (clickedTick > gapStart + 0.5) {
            QJsonObject rest;
            rest["note"] = "Rst";
            rest["duration"] = intToHexField(int(qRound(clickedTick - gapStart)));
            m_events.insert(slot, QJsonValue(rest));
            slot++;
        }
        QJsonObject note;
        note["note"] = noteNameForRow(row);
        note["duration"] = intToHexField(m_noteLength);
        m_events.insert(slot, QJsonValue(note));
        selectEvent(slot, false);
        relayout();
        emit eventsChanged(m_events);
        briefPreview(row);
        return;
    }
    update();
}

void PianoRollGridWidget::mouseMoveEvent(QMouseEvent *event) {
    if (m_resizingEventIndex >= 0) {
        if (m_resizingLeftEdge) {
            performLeftEdgeResize(event->pos());
            return;
        }
        // Find the bar's own startTick (already-laid-out position) so
        // dragging computes an absolute new duration, not a delta -- avoids
        // drift if relayout() shifts things slightly between moves.
        double startTick = 0;
        for (const NoteBar &b : m_bars)
            if (b.eventIndex == m_resizingEventIndex)
                startTick = b.startTick;
        const double newEndTick = double(event->pos().x()) / m_pixelsPerTick;
        const int newDuration = qMax(1, int(qRound(newEndTick - startTick)));
        if (m_resizingEventIndex < m_events.size()) {
            QJsonObject obj = m_events.at(m_resizingEventIndex).toObject();
            obj["duration"] = intToHexField(newDuration);
            m_events[m_resizingEventIndex] = obj;
            relayout();
        }
        return;
    }
    if (m_draggingEventIndex >= 0) {
        if (!m_dragMoved) {
            if ((event->pos() - m_dragPressPos).manhattanLength() < 4)
                return; // below the drag threshold -- still just a click-in-progress
            m_dragMoved = true;
        }
        performDragMove(event->pos());
        return;
    }
    setCursor(resizeHandleAt(event->pos()) >= 0 ? Qt::SizeHorCursor : Qt::ArrowCursor);
}

void PianoRollGridWidget::mouseReleaseEvent(QMouseEvent *) {
    if (m_resizingEventIndex >= 0) {
        // Resizing (either edge) never plays a preview, per your direction
        // -- only placing and moving do.
        m_resizingEventIndex = -1;
        m_resizingLeftEdge = false;
        emit eventsChanged(m_events);
    }
    if (m_draggingEventIndex >= 0) {
        if (m_dragMoved)
            emit eventsChanged(m_events);
        m_draggingEventIndex = -1;
        m_dragMoved = false;
        m_dragLastPreviewRow = -1;
        m_dragGroupAnchors.clear();
    }
}

// Repositions the dragged note in time (whole-tick snapped) and, for real
// notes (not tie/inheritedNote, which have no pitch of their own to move),
// in pitch -- auto-managing the leading rest needed to land it at the new
// tick, same "auto-calculated" rest logic as mousePressEvent's draw-new-
// note path. No polyphony (per your direction: the chipset can only ever
// hold one note per channel) -- but rather than redirecting the dragged
// note around whatever it lands on, whatever's in the way moves out of the
// way instead: every later top-level event's own tick position is just the
// running total of everything before it, so simply choosing where in the
// array the dragged note belongs (via insertionIndexForTick(), same as a
// fresh draw) and inserting it there automatically pushes whatever used to
// occupy that time later -- no separate "push" step needed, in either
// direction.
void PianoRollGridWidget::performDragMove(const QPoint &pos) {
    if (!m_dragGroupAnchors.isEmpty()) {
        performGroupDragMove(pos);
        return;
    }
    if (m_draggingEventIndex < 0 || m_draggingEventIndex >= m_events.size())
        return;
    const QJsonObject dragged = m_events.at(m_draggingEventIndex).toObject();
    const bool canRepitch = dragged.contains("note") && dragged.value("note").toString() != "Rst";

    // Remove the dragged note, and an immediately-preceding plain rest if
    // one exists (system-managed padding, safe to fold back into the
    // recompute below).
    int slot = m_draggingEventIndex;
    m_events.removeAt(slot);
    if (slot > 0 && m_events.at(slot - 1).toObject().value("note").toString() == "Rst") {
        m_events.removeAt(slot - 1);
        slot--;
    }
    relayout(); // m_bars/m_flags now reflect positions with the dragged note removed

    const double target = qMax(0.0, (double)qRound(pos.x() / m_pixelsPerTick));

    // If the target lands inside an existing bar's own span (not just
    // before it), THAT bar is what needs to move out of the way -- insert
    // right in its place so it (and everything after) gets pushed later by
    // the dragged note's own footprint. Otherwise fall back to the normal
    // "which gap does this tick fall in" lookup used for a fresh draw.
    int insertIdx = -1;
    for (const NoteBar &b : m_bars) {
        if (target >= b.startTick && target < b.startTick + b.duration) {
            insertIdx = b.eventIndex;
            break;
        }
    }
    if (insertIdx < 0)
        insertIdx = insertionIndexForTick(target);
    const double gapStart = tickAtIndex(insertIdx);

    QJsonObject note = dragged;
    int row = -1;
    if (canRepitch) {
        row = rowAtY(pos.y());
        note["note"] = noteNameForRow(row);
    }

    slot = insertIdx;
    if (target > gapStart + 0.5) {
        QJsonObject rest;
        rest["note"] = "Rst";
        rest["duration"] = intToHexField(int(qRound(target - gapStart)));
        m_events.insert(slot, QJsonValue(rest));
        slot++;
    }
    m_events.insert(slot, QJsonValue(note));
    m_draggingEventIndex = slot;
    m_selectedIndices = {slot};
    m_selectedEventIndex = slot;
    relayout();
    emit selectionChanged(m_selectedIndices);

    // Briefly play it as it moves (per your direction), re-triggered only
    // when the pitch actually changes -- not on every micro-movement -- so
    // dragging through several rows sounds like scrubbing through pitches
    // rather than a stutter.
    if (canRepitch && row != m_dragLastPreviewRow) {
        m_dragLastPreviewRow = row;
        briefPreview(row);
    }
}

// Multi-note group drag (per your direction: "multimove") -- applies the
// SAME tick/row delta the primary dragged note moves by to every other
// selected note, computed against the stable anchors captured once at
// mousePressEvent (not a mutating array, so deltas can't drift as notes
// get removed/reinserted below). Since every member moves by the identical
// delta, and they didn't overlap each other before the drag, they can
// never collide with EACH other afterward -- only with unselected
// ("foreign") notes, resolved per-member the same push-out-of-the-way way
// as a single-note drag. Processed in ascending target-tick order so each
// member's own insertion sees the correct, already-updated positions of
// any earlier group members inserted this same move.
void PianoRollGridWidget::performGroupDragMove(const QPoint &pos) {
    if (m_dragGroupAnchors.isEmpty())
        return;

    const double targetPrimary = qMax(0.0, (double)qRound(pos.x() / m_pixelsPerTick));
    const double deltaTick = targetPrimary - m_dragPrimaryAnchorTick;
    int deltaRow = 0;
    bool primaryCanRepitch = false;
    for (const DragAnchor &a : m_dragGroupAnchors)
        if (a.eventIndex == m_draggingEventIndex && a.canRepitch) {
            deltaRow = rowAtY(pos.y()) - m_dragPrimaryAnchorRow;
            primaryCanRepitch = true;
        }

    // Remove every group member from m_events, highest index first so
    // removing one never shifts another still-pending one out from under
    // it. Leading rests aren't cleaned up here (unlike the single-note
    // path) -- with several members possibly adjacent to each other, which
    // rest belongs to which member gets ambiguous; any left behind is just
    // harmless extra silence, not a correctness problem.
    QVector<DragAnchor> byIndexDesc = m_dragGroupAnchors;
    std::sort(byIndexDesc.begin(), byIndexDesc.end(),
              [](const DragAnchor &a, const DragAnchor &b) { return a.eventIndex > b.eventIndex; });
    for (const DragAnchor &a : byIndexDesc)
        if (a.eventIndex < m_events.size())
            m_events.removeAt(a.eventIndex);
    relayout();

    QVector<DragAnchor> byTickAsc = m_dragGroupAnchors;
    std::sort(byTickAsc.begin(), byTickAsc.end(), [](const DragAnchor &a, const DragAnchor &b) { return a.tick < b.tick; });

    QSet<int> newSelection;
    int newDraggingIndex = -1;
    int primaryRow = -1;
    for (const DragAnchor &a : byTickAsc) {
        double target = qMax(0.0, (double)qRound(a.tick + deltaTick));

        int insertIdx = -1;
        for (const NoteBar &b : m_bars) {
            if (target >= b.startTick && target < b.startTick + b.duration) {
                insertIdx = b.eventIndex;
                break;
            }
        }
        if (insertIdx < 0)
            insertIdx = insertionIndexForTick(target);
        const double gapStart = tickAtIndex(insertIdx);

        QJsonObject note = a.data;
        int row = a.row;
        if (a.canRepitch) {
            row = qBound(0, a.row + deltaRow, kRowCount - 1);
            note["note"] = noteNameForRow(row);
        }

        int slot = insertIdx;
        if (target > gapStart + 0.5) {
            QJsonObject rest;
            rest["note"] = "Rst";
            rest["duration"] = intToHexField(int(qRound(target - gapStart)));
            m_events.insert(slot, QJsonValue(rest));
            slot++;
        }
        m_events.insert(slot, QJsonValue(note));
        newSelection.insert(slot);
        if (a.eventIndex == m_draggingEventIndex) {
            newDraggingIndex = slot;
            primaryRow = row;
        }
        relayout(); // so the next member's collision scan sees this one already placed
    }

    m_draggingEventIndex = newDraggingIndex;
    m_selectedIndices = newSelection;
    m_selectedEventIndex = newDraggingIndex;
    emit selectionChanged(m_selectedIndices);

    // Only the primary note briefly plays, per your direction: "you only
    // hear the note of the first one being moved".
    if (primaryCanRepitch && primaryRow != m_dragLastPreviewRow) {
        m_dragLastPreviewRow = primaryRow;
        briefPreview(primaryRow);
    }
}

// Left-edge resize: keeps the note's own end tick fixed and moves its
// start, recomputing duration -- the mirror image of the right-edge
// resize's own fixed-start/moving-end logic. Auto-manages the leading rest
// the same way a body drag does, but doesn't displace neighbors -- clamped
// to m_resizeGapStart, the boundary captured once at mousePressEvent.
void PianoRollGridWidget::performLeftEdgeResize(const QPoint &pos) {
    if (m_resizingEventIndex < 0 || m_resizingEventIndex >= m_events.size())
        return;

    double newStart = qMax(m_resizeGapStart, (double)qRound(pos.x() / m_pixelsPerTick));
    newStart = qMin(newStart, m_resizeFixedEndTick - 1); // keep at least 1 tick of duration
    const int newDuration = qMax(1, int(qRound(m_resizeFixedEndTick - newStart)));

    int noteIdx = m_resizingEventIndex;
    const bool hasRest = noteIdx > 0 && m_events.at(noteIdx - 1).toObject().value("note").toString() == "Rst";
    const double restNeeded = newStart - m_resizeGapStart;

    if (restNeeded > 0.5) {
        QJsonObject rest;
        rest["note"] = "Rst";
        rest["duration"] = intToHexField(int(qRound(restNeeded)));
        if (hasRest) {
            m_events[noteIdx - 1] = rest;
        } else {
            m_events.insert(noteIdx, QJsonValue(rest));
            noteIdx++;
        }
    } else if (hasRest) {
        m_events.removeAt(noteIdx - 1);
        noteIdx--;
    }

    QJsonObject note = m_events.at(noteIdx).toObject();
    note["duration"] = intToHexField(newDuration);
    m_events[noteIdx] = note;
    m_resizingEventIndex = noteIdx;
    relayout();
}

void PianoRollGridWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    mousePressEvent(event);
    if (m_selectedEventIndex >= 0)
        editEventDialog(m_selectedEventIndex);
}

void PianoRollGridWidget::editEventDialog(int eventIndex) {
    if (eventIndex < 0 || eventIndex >= m_events.size())
        return;
    const QJsonDocument doc(QJsonArray{m_events.at(eventIndex)});
    // Wrapped in a 1-element array so plain values (bare event strings like
    // "smpsStop") and objects both round-trip through the same text editor
    // uniformly.
    QString text = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    text = text.mid(text.indexOf('\n') + 1); // drop the wrapping "[\n" ... "]" lines for a cleaner edit box
    text = text.left(text.lastIndexOf('\n', text.lastIndexOf('\n') - 1));

    bool ok = false;
    const QString edited =
        QInputDialog::getMultiLineText(this, "Edit event", QString("Event %1 (JSON):").arg(eventIndex), text, &ok);
    if (!ok)
        return;

    QJsonParseError err;
    const QJsonDocument parsed = QJsonDocument::fromJson(("[" + edited + "]").toUtf8(), &err);
    if (!parsed.isArray() || parsed.array().size() != 1)
        return; // silently ignore a bad edit rather than crash -- same "don't guess" spirit as elsewhere
    m_events[eventIndex] = parsed.array().at(0);
    relayout();
    emit eventsChanged(m_events);
}

// Flag-insertion/edit/delete menu -- shared by this widget's own
// right-click (when nothing else is more specifically hit) and
// PianoFlagLaneWidget's forwarded right-clicks (per your direction: the
// flag lane moved to its own strip, but insertion still needs this same
// menu content, so it's factored out here rather than duplicated).
void PianoRollGridWidget::showFlagMenuAt(double tick, const QPoint &globalPos) {
    tick = qMax(0.0, tick);
    QMenu menu(this);

    menu.addAction("Rest", this, [this, tick]() {
        insertFlagAt(tick, QJsonObject{{"note", "Rst"}, {"duration", intToHexField(1)}});
    });
    menu.addAction("Stop", this, [this, tick]() { insertFlagAt(tick, "smpsStop"); });
    menu.addAction("No Attack (tie)", this, [this, tick]() { insertFlagAt(tick, "smpsNoAttack"); });
    menu.addAction("Fade", this, [this, tick]() { insertFlagAt(tick, "smpsFade"); });
    menu.addSeparator();
    menu.addAction("Pan...", this, [this, tick]() {
        insertFlagAt(tick, QJsonObject{{"smpsPan", QJsonArray{"panCentre", "0x0"}}});
        // Has real params (direction/level) beyond what the menu label
        // itself specifies, per your direction -- pop the edit dialog
        // immediately so it can be configured in one motion. "Modulation
        // On/Off" and the bare Stop/NoAttack/Fade flags skip this since the
        // menu choice already fully specifies them, and Jump/Loop/Call
        // already got their own picker dialog above.
        editEventDialog(m_selectedEventIndex);
    });
    menu.addAction("Modulation On", this, [this, tick]() { insertFlagAt(tick, QJsonObject{{"smpsMod", true}}); });
    menu.addAction("Modulation Off", this, [this, tick]() { insertFlagAt(tick, QJsonObject{{"smpsMod", false}}); });
    menu.addSeparator();
    // Per your direction: pick the target block from a list instead of
    // hand-typing a name.
    QAction *jumpAction = menu.addAction("Jump to block...");
    jumpAction->setEnabled(!m_availableBlockNames.isEmpty());
    connect(jumpAction, &QAction::triggered, this, [this, tick]() {
        bool ok = false;
        const QString target =
            QInputDialog::getItem(this, "Jump to block", "Target:", m_availableBlockNames, 0, false, &ok);
        if (ok)
            insertFlagAt(tick, QJsonObject{{"jumpTo", target}});
    });
    QAction *loopAction = menu.addAction("Loop to block...");
    loopAction->setEnabled(!m_availableBlockNames.isEmpty());
    connect(loopAction, &QAction::triggered, this, [this, tick]() {
        bool ok = false;
        const QString target =
            QInputDialog::getItem(this, "Loop to block", "Target:", m_availableBlockNames, 0, false, &ok);
        if (ok)
            insertFlagAt(tick, QJsonObject{{"jumpTo", target}, {"smpsLoopArgs", QJsonArray{"0x0", "0x2"}}});
    });
    menu.addAction("Call block...", this, [this, tick]() {
        bool ok = false;
        const QString target =
            QInputDialog::getItem(this, "Call block", "Target:", m_availableBlockNames, 0, false, &ok);
        if (ok)
            insertFlagAt(tick, QJsonObject{{"smpsCall", target}});
    });

    if (m_selectedEventIndex >= 0) {
        menu.addSeparator();
        menu.addAction("Edit selected event...", this, [this]() { editEventDialog(m_selectedEventIndex); });
        menu.addAction("Delete selected event", this, [this]() { deleteSelected(); });
    }

    menu.exec(globalPos);
}

void PianoRollGridWidget::contextMenuEvent(QContextMenuEvent *event) {
    // Note-focused menu (per your direction: flag insertion moved
    // exclusively to PianoFlagLaneWidget's own right-click, below the
    // scrollbar) -- clipboard plus edit/delete for whatever's selected.
    const double tick = qMax(0.0, double(event->pos().x()) / m_pixelsPerTick);
    QMenu menu(this);

    QAction *copyAction = menu.addAction("Copy", this, [this]() { copySelection(); });
    copyAction->setEnabled(!m_selectedIndices.isEmpty());
    QAction *cutAction = menu.addAction("Cut", this, [this]() { cutSelection(); });
    cutAction->setEnabled(!m_selectedIndices.isEmpty());
    menu.addAction("Paste", this, [this, tick]() { pasteAt(tick); });

    if (m_selectedEventIndex >= 0) {
        menu.addSeparator();
        menu.addAction("Edit selected event...", this, [this]() { editEventDialog(m_selectedEventIndex); });
        menu.addAction("Delete selected event", this, [this]() { deleteSelected(); });
    }

    menu.exec(event->globalPos());
}

void PianoRollGridWidget::deleteSelected() {
    if (m_selectedIndices.isEmpty())
        return;
    // Highest index first, so removing one doesn't shift the still-pending
    // ones out from under it.
    QList<int> indices = m_selectedIndices.values();
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    for (int idx : indices)
        if (idx >= 0 && idx < m_events.size())
            m_events.removeAt(idx);
    m_selectedIndices.clear();
    m_selectedEventIndex = -1;
    relayout();
    emit selectionChanged(m_selectedIndices);
    emit eventsChanged(m_events);
}

void PianoRollGridWidget::copySelection() const {
    if (m_selectedIndices.isEmpty())
        return;
    QList<int> indices = m_selectedIndices.values();
    std::sort(indices.begin(), indices.end());
    QJsonArray arr;
    for (int idx : indices)
        if (idx >= 0 && idx < m_events.size())
            arr.append(m_events.at(idx));
    QGuiApplication::clipboard()->setText(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void PianoRollGridWidget::cutSelection() {
    copySelection();
    deleteSelected();
}

// Pastes the clipboard's events back to back starting at `tick`, per your
// direction ("since we have multiselect") -- auto-padded with a leading
// rest the same way a fresh draw or a drag is, since the format has no
// other way to represent the gap. Selects everything just pasted.
void PianoRollGridWidget::pasteAt(double tick) {
    const QString text = QGuiApplication::clipboard()->text();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
    if (!doc.isArray() || doc.array().isEmpty())
        return;
    const QJsonArray toPaste = doc.array();

    const double clickedTick = qMax(0.0, (double)qRound(tick));
    const int insertIdx = insertionIndexForTick(clickedTick);
    const double gapStart = tickAtIndex(insertIdx);

    int slot = insertIdx;
    if (clickedTick > gapStart + 0.5) {
        QJsonObject rest;
        rest["note"] = "Rst";
        rest["duration"] = intToHexField(int(qRound(clickedTick - gapStart)));
        m_events.insert(slot, QJsonValue(rest));
        slot++;
    }
    QSet<int> newSelection;
    for (const QJsonValue &v : toPaste) {
        m_events.insert(slot, v);
        newSelection.insert(slot);
        slot++;
    }
    m_selectedIndices = newSelection;
    m_selectedEventIndex = newSelection.isEmpty() ? -1 : *newSelection.begin();
    relayout();
    emit selectionChanged(m_selectedIndices);
    emit eventsChanged(m_events);
}

void PianoRollGridWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) {
        deleteSelected();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        cutSelection();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteAt(m_playheadTick >= 0 ? m_playheadTick : 0);
        return;
    }
    QWidget::keyPressEvent(event);
}

// ---------------------------------------------------------------------------
// PianoFlagLaneWidget
// ---------------------------------------------------------------------------

PianoFlagLaneWidget::PianoFlagLaneWidget(QWidget *parent) : QWidget(parent) {}

void PianoFlagLaneWidget::setScrollOffset(int pixels) {
    m_scrollOffset = pixels;
    update();
}

void PianoFlagLaneWidget::setPixelsPerTick(double pixelsPerTick) {
    m_pixelsPerTick = qMax(0.5, pixelsPerTick);
    update();
}

void PianoFlagLaneWidget::setTotalTicks(double totalTicks) {
    m_totalTicks = totalTicks;
    update();
}

void PianoFlagLaneWidget::setFlags(const QVector<PianoRollGridWidget::FlagMarker> &flags) {
    m_flags = flags;
    update();
}

void PianoFlagLaneWidget::setSelectedIndices(const QSet<int> &indices) {
    m_selectedIndices = indices;
    update();
}

QSize PianoFlagLaneWidget::sizeHint() const {
    return QSize(qMax(400, int(m_totalTicks * m_pixelsPerTick)), 20);
}

int PianoFlagLaneWidget::flagAt(int x) const {
    for (const PianoRollGridWidget::FlagMarker &f : m_flags)
        if (qAbs(int(f.tick * m_pixelsPerTick) - m_scrollOffset - x) < 6)
            return f.eventIndex;
    return -1;
}

void PianoFlagLaneWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 22));
    p.setPen(QColor(150, 150, 150));
    p.drawText(QRect(2, 0, 40, height()), Qt::AlignVCenter | Qt::AlignLeft, "Flags");

    QFont glyphFont = p.font();
    glyphFont.setPointSize(8);
    p.setFont(glyphFont);
    const int centerY = height() / 2;
    for (const PianoRollGridWidget::FlagMarker &f : m_flags) {
        const int x = int(f.tick * m_pixelsPerTick) - m_scrollOffset;
        const QRect badge(x - 6, centerY - 6, 12, 12);
        p.setBrush(m_selectedIndices.contains(f.eventIndex) ? QColor(255, 200, 60) : QColor(230, 120, 60));
        p.setPen(Qt::black);
        p.drawEllipse(badge);
        p.setPen(Qt::black);
        p.drawText(badge, Qt::AlignCenter, flagGlyph(f.label));
    }
}

void PianoFlagLaneWidget::mousePressEvent(QMouseEvent *event) {
    const int hit = flagAt(event->pos().x());
    if (hit >= 0)
        emit flagClicked(hit, event->modifiers() & Qt::ControlModifier);
}

void PianoFlagLaneWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    const int hit = flagAt(event->pos().x());
    if (hit >= 0)
        emit flagDoubleClicked(hit);
}

void PianoFlagLaneWidget::contextMenuEvent(QContextMenuEvent *event) {
    const double tick = (double(event->pos().x()) + m_scrollOffset) / m_pixelsPerTick;
    // Selecting under the cursor first (if a flag's there) so the shared
    // menu's "Edit/Delete selected" options apply to the right target, same
    // as right-clicking directly on a bar in the note grid does.
    const int hit = flagAt(event->pos().x());
    if (hit >= 0)
        emit flagClicked(hit, false);
    emit contextMenuRequested(tick, event->globalPos());
}

// ---------------------------------------------------------------------------
// PianoRulerWidget
// ---------------------------------------------------------------------------

PianoRulerWidget::PianoRulerWidget(QWidget *parent) : QWidget(parent) { setFixedHeight(20); }

QSize PianoRulerWidget::sizeHint() const { return QSize(qMax(400, int(m_totalTicks * m_pixelsPerTick)), 20); }

void PianoRulerWidget::setScrollOffset(int pixels) {
    m_scrollOffset = pixels;
    update();
}

void PianoRulerWidget::setTotalTicks(double totalTicks) {
    m_totalTicks = totalTicks;
    updateGeometry();
    update();
}

void PianoRulerWidget::setPixelsPerTick(double pixelsPerTick) {
    m_pixelsPerTick = qMax(0.5, pixelsPerTick);
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------
// NoteThumbnailWidget
// ---------------------------------------------------------------------------

NoteThumbnailWidget::NoteThumbnailWidget(QWidget *parent) : QWidget(parent) { setMinimumSize(90, 50); }

void NoteThumbnailWidget::setEvents(const QJsonArray &events) {
    m_bars.clear();
    double tick = 0;
    int lastNote = 48;
    for (const QJsonValue &ev : events) {
        if (ev.isString())
            continue;
        const QJsonObject obj = ev.toObject();
        if (obj.contains("note")) {
            const QString name = obj.value("note").toString();
            const double dur = obj.contains("duration") ? hexFieldToInt(obj.value("duration"), 1) : 1;
            if (name == "Rst") {
                // A rest is the absence of anything in that span -- no bar,
                // not even a marker -- just a gap; still consumes the tick.
                tick += dur;
                continue;
            }
            const int row = qBound(0, NativeCompiler::noteValue(name) - 0x81, 95);
            lastNote = row;
            m_bars.append({tick, dur, row});
            tick += dur;
        } else if (obj.contains("tie") || obj.contains("inheritedNote")) {
            const double dur = hexFieldToInt(obj.value("duration"), 1);
            m_bars.append({tick, dur, lastNote});
            tick += dur;
        }
    }
    m_totalTicks = qMax(1.0, tick);
    update();
}

void NoteThumbnailWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(15, 15, 18));
    if (m_bars.isEmpty()) {
        p.setPen(QColor(90, 90, 95));
        p.drawText(rect(), Qt::AlignCenter, "(empty)");
        return;
    }
    int minRow = 95, maxRow = 0;
    for (const Bar &b : m_bars) {
        minRow = qMin(minRow, b.row);
        maxRow = qMax(maxRow, b.row);
    }
    if (maxRow < minRow) {
        minRow = 0;
        maxRow = 1;
    }
    const double xScale = width() / qMax(1.0, m_totalTicks);
    const double rowSpan = qMax(1, maxRow - minRow + 1);
    const double yScale = height() / rowSpan;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(80, 150, 255));
    for (const Bar &b : m_bars) {
        const int x = int(b.startTick * xScale);
        const int w = qMax(1, int(b.duration * xScale));
        const int y = height() - int((b.row - minRow + 1) * yScale);
        p.drawRect(x, y, w, qMax(1, int(yScale) - 1));
    }
}

void PianoRulerWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(35, 35, 40));
    p.setPen(QColor(150, 150, 150));
    // Same phantom-measures-past-the-content reasoning as
    // PianoRollGridWidget::paintEvent()'s own gridlines -- ticks/labels
    // drawn across the widget's actual width, not just m_totalTicks.
    const double visibleTicks = (double(width()) + m_scrollOffset) / m_pixelsPerTick;
    for (int t = 0; t < visibleTicks; t += 12) {
        const int x = int(t * m_pixelsPerTick) - m_scrollOffset;
        if (x < -20 || x > width() + 20)
            continue;
        p.drawLine(x, height() - 6, x, height());
        p.drawText(QRect(x + 2, 0, 40, height() - 6), Qt::AlignLeft | Qt::AlignVCenter, QString::number(t));
    }
}
