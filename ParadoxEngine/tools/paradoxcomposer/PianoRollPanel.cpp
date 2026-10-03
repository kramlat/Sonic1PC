#include "PianoRollPanel.h"
#include "PianoRollWidgets.h"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace {
constexpr int kKeysWidth = 56; // must match PianoRollWidgets.cpp's own kKeysWidth
} // namespace

PianoRollPanel::PianoRollPanel(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);

    // Toolbar: Play/Fit/Cut, per your Qt Designer reference. Not added to
    // mainLayout below -- MainWindow places it into the owning
    // DockableContainer's header row instead (see toolbar() above), so it
    // doesn't take a whole separate row of its own.
    m_toolbar = new QToolBar(this);
    auto *toolbar = m_toolbar;
    m_playButton = new QPushButton(style()->standardIcon(QStyle::SP_MediaPlay), "", this);
    m_playButton->setCheckable(true);
    connect(m_playButton, &QPushButton::toggled, this, &PianoRollPanel::togglePlay);
    toolbar->addWidget(m_playButton);
    // "Fit" returns to auto-fit horizontal zoom (undoing any manual zoom
    // from the horizontal scrollbar's thumb-edge drag, see below) and
    // scrolls back to the start.
    auto *fitButton = new QPushButton(this);
    fitButton->setIcon(QIcon::fromTheme("zoom-fit-best"));
    fitButton->setToolTip("Zoom to fit / scroll to start");
    connect(fitButton, &QPushButton::clicked, this, [this]() {
        m_horizontalManualZoom = false;
        updateAutoFit();
        m_scrollArea->horizontalScrollBar()->setValue(0);
        m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum() / 2);
    });
    toolbar->addWidget(fitButton);
    auto *cutButton = new QPushButton(QIcon::fromTheme("edit-cut"), "", this);
    connect(cutButton, &QPushButton::clicked, this, [this]() { m_grid->deleteSelected(); });
    toolbar->addWidget(cutButton);
    // Note length: the duration (raw SMPS ticks, same unit as everywhere
    // else in this schema) a left click draws, per your direction -- wired
    // to the grid once it exists, below.
    toolbar->addWidget(new QLabel("Length:", this));
    auto *noteLengthSpin = new QSpinBox(this);
    noteLengthSpin->setRange(1, 255);
    noteLengthSpin->setValue(12);
    toolbar->addWidget(noteLengthSpin);
    // No block-name label here -- redundant with whatever host is showing
    // this panel's own title ("Piano Roll: <name>") right above.

    // Conflict-resolution dropdown (per your direction) -- hidden until
    // MainWindow calls setOwnerChoices() for a genuinely conflicting
    // shared block (see this class's own header comment). Wrapped in a
    // QAction so hiding/showing it as a unit is one call
    // (QAction::setVisible), not separately hiding a label alongside it.
    m_ownerContextCombo = new QComboBox(this);
    m_ownerContextCombo->setToolTip("This block is shared with a conflicting voice/channel elsewhere -- "
                                     "pick which owner's context to preview under");
    m_ownerContextAction = toolbar->addWidget(m_ownerContextCombo);
    m_ownerContextAction->setVisible(false);

    // Ruler row: a spacer matching the keys column width, then the ruler.
    auto *rulerRow = new QHBoxLayout();
    auto *rulerSpacer = new QWidget(this);
    rulerSpacer->setFixedWidth(kKeysWidth);
    m_ruler = new PianoRulerWidget(this);
    rulerRow->addWidget(rulerSpacer);
    rulerRow->addWidget(m_ruler, 1);
    mainLayout->addLayout(rulerRow);

    // Content row: synced piano keys + scrollable note grid.
    auto *contentRow = new QHBoxLayout();
    m_keys = new PianoKeysWidget(this);
    m_grid = new PianoRollGridWidget(this);
    m_grid->setNoteLength(noteLengthSpin->value());
    connect(noteLengthSpin, QOverload<int>::of(&QSpinBox::valueChanged), m_grid, &PianoRollGridWidget::setNoteLength);
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(m_grid);
    m_scrollArea->setWidgetResizable(false);
    // Always visible (not the default ScrollBarAsNeeded) -- these are also
    // the zoom controls now (thumb-edge drag, below), so they must stay
    // reachable even when the content already fits and a normal scrollbar
    // would otherwise hide/disable itself.
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    // Custom scrollbars whose thumb can also be resized by dragging its far
    // edge, repurposed as zoom controls per your direction: the vertical
    // one does vertical zoom (row height), the horizontal one does
    // horizontal zoom (pixels-per-tick) -- "bigger the scroll thumb, the
    // smaller the display" in both cases.
    auto *vZoomBar = new ZoomScrollBar(Qt::Vertical, m_scrollArea);
    auto *hZoomBar = new ZoomScrollBar(Qt::Horizontal, m_scrollArea);
    m_scrollArea->setVerticalScrollBar(vZoomBar);
    m_scrollArea->setHorizontalScrollBar(hZoomBar);
    connect(hZoomBar, &QScrollBar::valueChanged, m_ruler, &PianoRulerWidget::setScrollOffset);
    connect(vZoomBar, &QScrollBar::valueChanged, m_keys, &PianoKeysWidget::setScrollOffset);
    connect(vZoomBar, &ZoomScrollBar::thumbEdgeDragged, this, [this](int delta) {
        const int newHeight = m_grid->rowHeight() - delta;
        m_grid->setRowHeight(newHeight);
        m_keys->setRowHeight(newHeight);
        // setRowHeight() alone only tells Qt the widget's sizeHint changed
        // (updateGeometry()) -- with setWidgetResizable(false), the scroll
        // area won't act on that itself, so the actual widget bounds stay
        // stale unless we resize explicitly. Without this, zooming in grows
        // the content but not the widget, clipping it instead of becoming
        // scrollable to reach the newly-larger rows.
        updateAutoFit();
    });
    connect(hZoomBar, &ZoomScrollBar::thumbEdgeDragged, this, [this](int delta) {
        m_horizontalManualZoom = true;
        const double newPixelsPerTick = m_grid->pixelsPerTick() - delta * 0.15;
        m_grid->setPixelsPerTick(newPixelsPerTick);
        m_ruler->setPixelsPerTick(newPixelsPerTick);
        m_flagLane->setPixelsPerTick(newPixelsPerTick);
        updateAutoFit(); // same reasoning as the vertical handler above; m_horizontalManualZoom keeps this from re-fitting the scale itself
    });
    contentRow->addWidget(m_keys);
    contentRow->addWidget(m_scrollArea, 1);
    mainLayout->addLayout(contentRow, 1);

    // Flag lane: a separate strip below the scroll area (per your
    // direction), not part of the vertically-scrollable note area, but
    // still horizontally synced (scroll offset + zoom) with the grid/ruler
    // above -- same spacer-then-stretch layout as the ruler row.
    auto *flagLaneRow = new QHBoxLayout();
    auto *flagLaneSpacer = new QWidget(this);
    flagLaneSpacer->setFixedWidth(kKeysWidth);
    m_flagLane = new PianoFlagLaneWidget(this);
    flagLaneRow->addWidget(flagLaneSpacer);
    flagLaneRow->addWidget(m_flagLane, 1);
    mainLayout->addLayout(flagLaneRow);
    connect(hZoomBar, &QScrollBar::valueChanged, m_flagLane, &PianoFlagLaneWidget::setScrollOffset);
    connect(m_grid, &PianoRollGridWidget::flagsChanged, m_flagLane, &PianoFlagLaneWidget::setFlags);
    connect(m_grid, &PianoRollGridWidget::selectionChanged, m_flagLane, &PianoFlagLaneWidget::setSelectedIndices);
    connect(m_flagLane, &PianoFlagLaneWidget::flagClicked, m_grid, &PianoRollGridWidget::selectEvent);
    connect(m_flagLane, &PianoFlagLaneWidget::flagDoubleClicked, m_grid, &PianoRollGridWidget::editEvent);
    connect(m_flagLane, &PianoFlagLaneWidget::contextMenuRequested, m_grid, &PianoRollGridWidget::showFlagMenuAt);

    connect(m_grid, &PianoRollGridWidget::eventsChanged, this, [this](const QJsonArray &events) {
        m_ruler->setTotalTicks(m_grid->totalTicks());
        m_flagLane->setTotalTicks(m_grid->totalTicks());
        updateAutoFit();
        emit eventsChanged(m_blockName, events);
    });
    connect(m_grid, &PianoRollGridWidget::activeRowsChanged, m_keys, &PianoKeysWidget::setActiveRows);
    connect(m_keys, &PianoKeysWidget::notePressed, this, &PianoRollPanel::notePreviewRequested);
    connect(m_keys, &PianoKeysWidget::noteReleased, this, &PianoRollPanel::notePreviewStopRequested);
    // Placing/moving a note briefly plays it too (per your direction) --
    // same relay, same MainWindow-side voice/attenuation resolution.
    connect(m_grid, &PianoRollGridWidget::notePreviewRequested, this, &PianoRollPanel::notePreviewRequested);
    connect(m_grid, &PianoRollGridWidget::notePreviewStopRequested, this, &PianoRollPanel::notePreviewStopRequested);

    m_playbackTimer = new QTimer(this);
    connect(m_playbackTimer, &QTimer::timeout, this, &PianoRollPanel::stepPlayback);
}

void PianoRollPanel::setBlock(const QString &name, const QJsonArray &events) {
    m_blockName = name;
    m_grid->setEvents(events);
    m_ruler->setTotalTicks(m_grid->totalTicks());
    m_flagLane->setTotalTicks(m_grid->totalTicks());
    updateAutoFit();
    resetPlayhead();
}

void PianoRollPanel::setAvailableBlocks(const QStringList &names) { m_grid->setAvailableBlockNames(names); }

void PianoRollPanel::copySelection() const { m_grid->copySelection(); }
void PianoRollPanel::cutSelection() { m_grid->cutSelection(); }
void PianoRollPanel::pasteAtPlayhead() {
    const double tick = m_grid->playheadTick();
    m_grid->pasteAt(tick >= 0 ? tick : 0);
}

void PianoRollPanel::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    updateAutoFit();
}

// Per your direction: resizing the panel should resize its contents to fit
// the window -- rescales ticks -> pixels so the block's full width always
// matches the scroll area's viewport, instead of a fixed scale you'd need
// to scroll to see all of. Skipped once the user has manually zoomed via
// the horizontal ZoomScrollBar's thumb-edge drag (until "Fit" is pressed
// again), so a resize doesn't silently undo a deliberate zoom.
void PianoRollPanel::updateAutoFit() {
    if (!m_horizontalManualZoom) {
        const int viewportWidth = m_scrollArea->viewport()->width();
        const double totalTicks = qMax(1.0, m_grid->totalTicks());
        const double pixelsPerTick = qMax(2.0, viewportWidth / totalTicks);
        m_grid->setPixelsPerTick(pixelsPerTick);
        m_ruler->setPixelsPerTick(pixelsPerTick);
        m_flagLane->setPixelsPerTick(pixelsPerTick);
    }
    // setWidgetResizable(false) means the scroll area won't auto-resize the
    // grid to its new size hint on its own -- do it explicitly. Never
    // SMALLER than the viewport, in either direction, even past the end of
    // the block's own content/pitch range -- so there's always room to
    // click past the current end and add to it, and zooming out doesn't
    // leave a chunk of dead, unusable space beyond the grid widget's own
    // edge. This doesn't rescale anything (no stretch-to-fit) -- it just
    // grows the widget when its natural content size is smaller than the
    // viewport.
    const QSize hint = m_grid->sizeHint();
    const QSize viewportSize = m_scrollArea->viewport()->size();
    m_grid->resize(qMax(hint.width(), viewportSize.width()), qMax(hint.height(), viewportSize.height()));
}

void PianoRollPanel::togglePlay(bool playing) {
    if (playing) {
        m_playbackTick = 0;
        m_playbackTimer->start(1000 / 30);
        m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    } else {
        m_playbackTimer->stop();
        if (m_playingRow >= 0)
            emit notePreviewStopRequested();
        m_playingRow = -1;
        m_playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        resetPlayhead();
    }
}

void PianoRollPanel::stepPlayback() {
    m_playbackTick += (1000.0 / 30.0) / (m_framesPerTick * (1000.0 / 60.0));
    m_grid->setPlayheadTick(m_playbackTick);
    followPlayhead(m_playbackTick);

    const QSet<int> active = m_grid->activeRowsAtPlayhead();
    const int newRow = active.isEmpty() ? -1 : *active.begin();
    if (newRow != m_playingRow) {
        if (m_playingRow >= 0)
            emit notePreviewStopRequested();
        if (newRow >= 0)
            emit notePreviewRequested(newRow);
        m_playingRow = newRow;
    }

    if (active.isEmpty() && m_playbackTick > 0 && m_grid->events().size() > 0)
        ; // keep running -- silence between notes is normal, not end-of-block
}

void PianoRollPanel::advancePlayhead(quint64 frameCount) {
    const double tick = double(frameCount) / m_framesPerTick;
    m_grid->setPlayheadTick(tick);
    followPlayhead(tick);
}

void PianoRollPanel::resetPlayhead() { m_grid->setPlayheadTick(-1); }

// Scrolls just enough to keep the playhead visible once it reaches the
// viewport's edge -- not a continuous re-center, so scrubbing/reading
// around the current position while playback is running doesn't fight the
// user's own scroll position until the playhead actually catches up to it.
void PianoRollPanel::followPlayhead(double tick) {
    if (tick < 0)
        return;
    QScrollBar *hbar = m_scrollArea->horizontalScrollBar();
    const int x = int(tick * m_grid->pixelsPerTick());
    const int viewLeft = hbar->value();
    const int viewWidth = m_scrollArea->viewport()->width();
    if (x < viewLeft || x >= viewLeft + viewWidth)
        hbar->setValue(x);
}

double PianoRollPanel::currentPlayheadTick() const { return m_grid->playheadTick(); }

void PianoRollPanel::setTempo(int durationMult, int mainTempo) {
    // duration_mult is the dominant, exact factor (Sound.c's
    // ch->duration_timeout = raw_duration * duration_mult); main_tempo adds
    // a periodic +1-frame correction every main_tempo frames (TickChipSet),
    // approximated here as its average per-tick contribution rather than
    // simulated frame-by-frame -- consistent with this playhead already
    // being a visual approximation, not a claim of frame-exact sync.
    const double mult = qMax(1, durationMult);
    m_framesPerTick = mainTempo > 0 ? mult + mult / (double)mainTempo : mult;
}

void PianoRollPanel::setDacSampleNames(const QMap<int, QString> &sampleNames) { m_keys->setDacSampleNames(sampleNames); }

void PianoRollPanel::setOwnerChoices(const QStringList &owners, const QStringList &descriptions) {
    m_ownerContextCombo->clear();
    for (int i = 0; i < owners.size(); i++) {
        const QString description = i < descriptions.size() ? descriptions.at(i) : QString();
        const QString label = description.isEmpty() ? owners.at(i) : QString("%1 (%2)").arg(owners.at(i), description);
        m_ownerContextCombo->addItem(label, owners.at(i)); // item data = the owner name itself, see header comment
    }
    m_ownerContextAction->setVisible(!owners.isEmpty());
}

void PianoRollPanel::clearOwnerChoices() {
    m_ownerContextCombo->clear();
    m_ownerContextAction->setVisible(false);
}

QString PianoRollPanel::selectedOwnerContext() const {
    return m_ownerContextAction->isVisible() ? m_ownerContextCombo->currentData().toString() : QString();
}
