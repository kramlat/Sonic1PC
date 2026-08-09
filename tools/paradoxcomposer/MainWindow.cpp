#include "MainWindow.h"
#include "BytesPanel.h"
#include "ChannelsPanel.h"
#include "NativeCompiler.h"
#include "PianoRollPanel.h"
#include "PlaybackPanel.h"
#include "PlaylistPanel.h"
#include "SourcePanel.h"
#include "VoiceBankPanel.h"
#include "WaveformVUWidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QToolBar>

#include <utility>

MainWindow::MainWindow(const QString &repoRoot, QWidget *parent) : QMainWindow(parent), m_repoRoot(repoRoot) {
    setWindowTitle("ParadoxComposer");
    resize(1100, 800);

    m_sourcePanel = new SourcePanel(this);
    m_bytesPanel = new BytesPanel(this);
    m_voiceBankPanel = new VoiceBankPanel(this);
    m_playlistPanel = new PlaylistPanel(this);
    m_playbackPanel = new PlaybackPanel(this);
    m_channelsPanel = new ChannelsPanel(this);

    // MDI layout, per your Qt Designer reference -- each panel is its own
    // freely arranged/resized subwindow instead of a fixed tab, with a
    // Window menu (Tile/Cascade/Close All) for managing them. Piano Roll
    // windows are NOT part of this default set -- per your direction, one
    // only exists once a block is actually opened with it (double-click in
    // the Playlist panel), see openPianoRollFor().
    m_mdiArea = new QMdiArea(this);
    setCentralWidget(m_mdiArea);

    // Channel Rack -- part of the same managed panel system as everything
    // else below (it was left on a separate, raw QDockWidget path when that
    // system was first built, which is why it never got the gear menu /
    // Docked-Detached toggle the others have). Docked to the left by
    // default, matching FL Studio's own persistent side-panel placement.
    createManagedPanel(m_channelsPanel, "Channels", "channels", Qt::LeftDockWidgetArea);
    setPanelMode("channels", PanelMode::Docked, /*forceRebuild=*/true);

    // FL Studio-style dockable/floating panels (per your direction): each
    // one defaults to a contained "Mdi" window (exactly how these always
    // looked), with its own header buttons to dock it to a main-window edge
    // or pop it out to a real desktop window. Voice Bank and Playback are
    // "undockable" -- they must stay fixed-size (see
    // feedback_paradoxcomposer_fixed_windows memory) -- but still get the
    // Mdi/Floating toggle, per your direction ("even undockables").
    DockableContainer *voiceBankContainer =
        createManagedPanel(m_voiceBankPanel, "Voice Bank", "voiceBank", Qt::RightDockWidgetArea,
                            /*closable=*/false, /*dockable=*/false);
    createManagedPanel(m_playlistPanel, "Playlist / Blocks", "playlist", Qt::RightDockWidgetArea);
    createManagedPanel(m_bytesPanel, "Compiled Bytes", "bytes", Qt::BottomDockWidgetArea);
    DockableContainer *playbackContainer = createManagedPanel(m_playbackPanel, "Playback", "playback",
                                                                Qt::RightDockWidgetArea,
                                                                /*closable=*/false, /*dockable=*/false);

    // Fixed size, per your direction -- matches the layout screenshot you
    // shared (their content doesn't benefit from stretching, and a fixed
    // frame keeps the reference layout stable). Applied to the container
    // itself (header + content) so it holds regardless of which mode
    // (Mdi/Floating) it's currently in.
    voiceBankContainer->setFixedSize(880, 570);
    playbackContainer->setFixedSize(300, 190);

    // Default layout on first run only -- loadLayout() restores whatever
    // was last saved (via closeEvent()'s saveLayout()) if present, per your
    // direction to use the current on-screen arrangement as the "home"
    // layout going forward.
    loadLayout();

    // Transport toolbar, per your Qt Designer reference layout: icon
    // buttons (Qt's built-in standard icons -- no custom artwork needed for
    // these) for Play/Pause/Stop/Rewind/Record, then Tempo/Divide (bound to
    // the header's own smpsHeaderTempo entry) and a Time readout.
    // Pause/Rewind/Record/Time are visual placeholders only -- disabled,
    // not wired to anything real yet (Pause/Rewind have no backing state in
    // AudioEngine; Record is the deferred pitch-detection-based note-entry
    // idea, not WAV capture -- see plan notes) -- shown for layout parity
    // with your mockup, not faked functionality.
    auto *toolbar = addToolBar("Transport");
    toolbar->setObjectName("transportToolbar"); // required by saveState()/restoreState(), see loadLayout()
    toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPlay), "Play", this,
                        [this]() { playCompiledSong(false); });
    QAction *pauseAction = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaPause), "Pause");
    pauseAction->setEnabled(false);
    pauseAction->setToolTip("Not implemented yet");
    toolbar->addAction(style()->standardIcon(QStyle::SP_MediaStop), "Stop", this, &MainWindow::stopSong);
    QAction *rewindAction = toolbar->addAction(style()->standardIcon(QStyle::SP_MediaSeekBackward), "Rewind");
    rewindAction->setEnabled(false);
    rewindAction->setToolTip("Not implemented yet");
    QAction *recordAction = toolbar->addAction(QIcon::fromTheme("media-record"), "Record");
    recordAction->setEnabled(false);
    recordAction->setToolTip("Not implemented yet -- planned as pitch-detection-based note entry, not WAV capture");
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Tempo:", this));
    m_tempoSpin = new QSpinBox(this);
    m_tempoSpin->setRange(0, 255);
    toolbar->addWidget(m_tempoSpin);
    toolbar->addWidget(new QLabel("Divide:", this));
    m_divideSpin = new QSpinBox(this);
    m_divideSpin->setRange(0, 255);
    toolbar->addWidget(m_divideSpin);
    toolbar->addSeparator();
    // Waveform + VU meter, per your direction -- reflects AudioEngine's own
    // queued output, not a separate synthesis path.
    auto *waveformVu = new WaveformVUWidget(this);
    connect(&m_audio, &AudioEngine::samplesGenerated, waveformVu, &WaveformVUWidget::pushSamples);
    toolbar->addWidget(waveformVu);
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Time:", this));
    auto *timeField = new QLineEdit("--:--", this);
    timeField->setReadOnly(true);
    timeField->setMaximumWidth(60);
    timeField->setEnabled(false);
    timeField->setToolTip("Not implemented yet");
    toolbar->addWidget(timeField);
    connect(m_tempoSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::writeTransportToHeader);
    connect(m_divideSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::writeTransportToHeader);

    // Kept as members (not locals) so a Floating panel's own menu bar can
    // reuse these SAME QMenu/QAction objects -- see setPanelMode()'s
    // Floating case. A floating panel is a genuine separate top-level
    // window, so it has no other way to reach File/Edit/Window, and
    // WindowShortcut-context actions (Undo, Copy, etc.) wouldn't route to
    // it either without this.
    m_fileMenu = menuBar()->addMenu("&File");
    m_fileMenu->addAction("New Song", this, &MainWindow::newSong);
    m_fileMenu->addAction("New Voice Bank", this, &MainWindow::newVoiceBank);
    m_fileMenu->addAction("Open...", QKeySequence::Open, this, &MainWindow::openFile);
    m_fileMenu->addAction("Save", QKeySequence::Save, this, &MainWindow::saveFile);
    m_fileMenu->addAction("Save As...", QKeySequence::SaveAs, this, &MainWindow::saveFileAs);

    // Document-wide undo/redo (per your direction) -- covers every panel's
    // edits, since they all funnel into m_document.
    m_editMenu = menuBar()->addMenu("&Edit");
    m_undoAction = m_editMenu->addAction("Undo", QKeySequence::Undo, this, &MainWindow::undo);
    m_redoAction = m_editMenu->addAction("Redo", QKeySequence::Redo, this, &MainWindow::redo);
    updateUndoRedoActions();
    m_editMenu->addSeparator();
    // Piano roll clipboard ops (per your direction) -- Copy/Cut already work
    // via Ctrl+C/X directly on a focused grid; these route the same actions
    // through the menu, targeting whichever Piano Roll subwindow is active.
    m_editMenu->addAction("Copy", QKeySequence::Copy, this, [this]() {
        if (PianoRollPanel *panel = activePianoRollPanel())
            panel->copySelection();
    });
    m_editMenu->addAction("Cut", QKeySequence::Cut, this, [this]() {
        if (PianoRollPanel *panel = activePianoRollPanel())
            panel->cutSelection();
    });
    m_editMenu->addAction("Paste", QKeySequence::Paste, this, [this]() {
        if (PianoRollPanel *panel = activePianoRollPanel())
            panel->pasteAtPlayhead();
    });

    m_windowMenu = menuBar()->addMenu("&Window");
    m_windowMenu->addAction("Tile", m_mdiArea, &QMdiArea::tileSubWindows);
    m_windowMenu->addAction("Cascade", m_mdiArea, &QMdiArea::cascadeSubWindows);
    m_windowMenu->addAction("Close All", m_mdiArea, &QMdiArea::closeAllSubWindows);
    m_windowMenu->addSeparator();
    // Not spawned by default, per your direction -- only reachable here.
    m_windowMenu->addAction("Source (JSON) Editor", this, &MainWindow::openSourceEditor);

    connect(m_sourcePanel, &SourcePanel::applyRequested, this, &MainWindow::applySourceText);
    connect(m_voiceBankPanel, &VoiceBankPanel::voicesChanged, this, &MainWindow::onVoicesChanged);
    connect(m_voiceBankPanel, &VoiceBankPanel::previewRequested, this,
            [this](const QJsonObject &voice, int note) { m_audio.previewVoice(voice, note); });
    connect(m_voiceBankPanel, &VoiceBankPanel::previewStopRequested, this, [this]() { m_audio.stopPreview(); });
    connect(m_playlistPanel, &PlaylistPanel::playlistChanged, this, &MainWindow::onPlaylistChanged);
    connect(m_playlistPanel, &PlaylistPanel::blockDoubleClicked, this, &MainWindow::openPianoRollFor);
    connect(m_channelsPanel, &ChannelsPanel::headerChanged, this, &MainWindow::onHeaderChanged);
    connect(m_channelsPanel, &ChannelsPanel::channelMuteChanged, this,
            [this](int channelIndex, bool muted) { m_audio.setChannelMuted(channelIndex, muted); });
    connect(&m_audio, &AudioEngine::channelActivityUpdated, m_channelsPanel, &ChannelsPanel::setChannelActivity);
    connect(m_channelsPanel, &ChannelsPanel::blockNeeded, this, [this](const QString &name, const QJsonArray &events) {
        QJsonObject playlist = m_document.playlist();
        if (playlist.contains(name))
            return;
        playlist[name] = events;
        QStringList order = m_document.playlistOrder();
        order.append(name);
        m_document.setPlaylist(playlist, order);
        m_playlistPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
        broadcastAvailableBlocksToOpenPianoRolls();
        m_channelsPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    });
    connect(m_bytesPanel, &BytesPanel::compileRequested, this, &MainWindow::compileCurrentDocument);
    connect(m_playbackPanel, &PlaybackPanel::playRequested, this, &MainWindow::playCompiledSong);
    connect(m_playbackPanel, &PlaybackPanel::stopRequested, this, &MainWindow::stopSong);

    if (!m_audio.init())
        QMessageBox::warning(this, "Audio", "Could not initialize SDL audio -- playback will be silent.");

    newSong();
}

void MainWindow::newSong() {
    m_document = SongDocument::makeEmptySong();
    m_currentFilePath.clear();
    m_undoStack.clear(); // a whole new document -- the old history doesn't apply to it
    m_redoStack.clear();
    updateUndoRedoActions();
    refreshAllPanelsFromDocument();
}

void MainWindow::newVoiceBank() {
    m_document = SongDocument::makeEmptyVoiceBank();
    m_currentFilePath.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    updateUndoRedoActions();
    refreshAllPanelsFromDocument();
}

void MainWindow::openFile() {
    const QString path =
        QFileDialog::getOpenFileName(this, "Open ParadoxSMPS file", m_repoRoot + "/tools/paradoxsmps/converted",
                                      "ParadoxSMPS files (*.jsonc *.json)");
    if (path.isEmpty())
        return;
    QString error;
    if (!m_document.loadFromFile(path, &error)) {
        QMessageBox::warning(this, "Open failed", error);
        return;
    }
    m_currentFilePath = path;
    m_undoStack.clear();
    m_redoStack.clear();
    updateUndoRedoActions();
    refreshAllPanelsFromDocument();

    // DAW-only block ownership index (per your direction) -- rescanned
    // fresh on every open (not loaded back from a previous run) and
    // written out to <song>.metadata.json alongside the song file.
    m_blockMetadata.rebuild(m_document.playlist());
    QString metadataError;
    if (!m_blockMetadata.saveToFile(path, &metadataError))
        QMessageBox::warning(this, "Block metadata write failed", metadataError);
}

void MainWindow::saveFile() {
    if (m_currentFilePath.isEmpty()) {
        saveFileAs();
        return;
    }
    QString error;
    if (!m_document.saveToFile(m_currentFilePath, &error))
        QMessageBox::warning(this, "Save failed", error);
}

void MainWindow::saveFileAs() {
    const QString path = QFileDialog::getSaveFileName(this, "Save ParadoxSMPS file", m_repoRoot, "*.jsonc");
    if (path.isEmpty())
        return;
    m_currentFilePath = path;
    saveFile();
}

void MainWindow::applySourceText(const QString &jsonText) {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(SongDocument::stripJsonComments(jsonText).toUtf8(), &err);
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Parse error", err.errorString());
        return;
    }
    pushUndoSnapshot();
    m_document.root() = doc.object();
    // Re-derive playlist order from the just-applied text -- this is one of
    // only two points (the other being loadFromFile) where authored order
    // is actually recoverable, since QJsonObject itself doesn't preserve it
    // (see SongDocument.h's own comment).
    m_document.setPlaylist(m_document.playlist(), SongDocument::extractPlaylistKeyOrder(
                                                        SongDocument::stripJsonComments(jsonText)));
    // Refresh the structured panels (voice bank / playlist) from the newly
    // applied text, but don't re-write the Source panel's own text back at
    // itself -- that would fight the user's cursor position for no reason.
    m_voiceBankPanel->setVoices(m_document.voices());
    m_playlistPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    broadcastAvailableBlocksToOpenPianoRolls();
    m_channelsPanel->setHeader(m_document.header());
    m_channelsPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    m_playlistPanel->setHeader(m_document.header());
    syncTransportFromHeader();
    compileCurrentDocument();
}

void MainWindow::onVoicesChanged(const QJsonArray &voices) {
    pushUndoSnapshot();
    m_document.setVoices(voices);
    syncSourcePanelFromDocument();
}

void MainWindow::onPlaylistChanged(const QJsonObject &playlist, const QStringList &order) {
    pushUndoSnapshot();
    m_document.setPlaylist(playlist, order);
    broadcastAvailableBlocksToOpenPianoRolls();
    // Refresh any open Piano Roll whose block's content just changed via
    // the Playlist panel's own text editor, so the two stay in sync.
    for (auto it = m_pianoRollPanels.constBegin(); it != m_pianoRollPanels.constEnd(); ++it)
        if (playlist.contains(it.key())) {
            it.value()->setBlock(it.key(), playlist.value(it.key()).toArray());
            updateOwnerConflictUI(it.key(), it.value());
        }
    syncSourcePanelFromDocument();
}

void MainWindow::onHeaderChanged(const QJsonArray &header) {
    pushUndoSnapshot();
    m_document.setHeader(header);
    syncTransportFromHeader();
    syncSourcePanelFromDocument();
}

void MainWindow::writeTransportToHeader() {
    QJsonArray header = m_document.header();
    for (int i = 0; i < header.size(); i++) {
        QJsonObject entry = header.at(i).toObject();
        if (entry.contains("smpsHeaderTempo")) {
            pushUndoSnapshot();
            entry["smpsHeaderTempo"] = QJsonArray{intToHexField(m_divideSpin->value()), intToHexField(m_tempoSpin->value())};
            header[i] = entry;
            m_document.setHeader(header);
            m_channelsPanel->setHeader(header);
            syncSourcePanelFromDocument();
            broadcastTempoToOpenPianoRolls();
            return;
        }
    }
}

void MainWindow::broadcastTempoToOpenPianoRolls() {
    for (PianoRollPanel *panel : std::as_const(m_pianoRollPanels))
        panel->setTempo(m_divideSpin->value(), m_tempoSpin->value());
}

void MainWindow::syncTransportFromHeader() {
    const QJsonArray header = m_document.header();
    for (const QJsonValue &v : header) {
        const QJsonObject entry = v.toObject();
        if (entry.contains("smpsHeaderTempo")) {
            const QJsonArray args = entry.value("smpsHeaderTempo").toArray();
            const QSignalBlocker b1(m_tempoSpin), b2(m_divideSpin);
            if (args.size() > 0)
                m_divideSpin->setValue(hexFieldToInt(args.at(0)));
            if (args.size() > 1)
                m_tempoSpin->setValue(hexFieldToInt(args.at(1)));
            return;
        }
    }
}

void MainWindow::compileCurrentDocument() {
    // In-process compile via libparadoxsmps (NativeCompiler is just the Qt
    // boundary wrapper around it) -- fast enough to run on every edit, and
    // the same code path the real CMake build and jsonc2h_tool use, so there
    // is nothing left to cross-check against.
    const NativeCompiler::Result result = NativeCompiler::compile(m_document.root(), m_document.playlistOrder());
    m_bytesPanel->showResult(result.success, result.compiledBytes, QString(), result.errorMessage);
}

void MainWindow::playCompiledSong(bool isSfx) {
    const QByteArray bytes = m_bytesPanel->lastCompiledBytes();
    if (bytes.isEmpty()) {
        m_playbackPanel->setStatus("Nothing compiled yet -- use the Compiled Bytes tab first.");
        return;
    }
    m_audio.playSong(bytes, isSfx);
    m_playbackPanel->setStatus(QString("Playing %1 bytes...").arg(bytes.size()));
}

void MainWindow::stopSong() {
    m_audio.stopSong();
    m_playbackPanel->setStatus("Stopped.");
    for (PianoRollPanel *panel : std::as_const(m_pianoRollPanels))
        panel->resetPlayhead();
}

void MainWindow::onPianoRollEventsChanged(const QString &blockName, const QJsonArray &events) {
    QJsonObject playlist = m_document.playlist();
    if (!playlist.contains(blockName))
        return;
    pushUndoSnapshot();
    playlist[blockName] = events;
    m_document.setPlaylist(playlist, m_document.playlistOrder());
    m_playlistPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    m_channelsPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    syncSourcePanelFromDocument();
}

void MainWindow::updateOwnerConflictUI(const QString &name, PianoRollPanel *panel) {
    if (!m_blockMetadata.checkSharedConflict(name, m_document.header())) {
        panel->clearOwnerChoices();
        return;
    }
    const QStringList owners = m_blockMetadata.ownersOf(name);
    QStringList descriptions;
    for (const QString &owner : owners) {
        // Same "this owner's earliest call site into `name`" cutoff
        // checkSharedConflict itself resolves each owner's state against.
        double cutoff = -1;
        for (const BlockCallSite &site : m_blockMetadata.callSitesOf(name))
            if (site.ownerBlock == owner && (cutoff < 0 || site.tick < cutoff))
                cutoff = site.tick;
        const ResolvedBlockState state = m_blockMetadata.resolveStateAtTick(owner, cutoff, m_document.header());
        // Per your direction: "fTone5 and Voice 6" for an FM-vs-PSG
        // conflict, or "Noise" instead of a tone once a smpsPSGform ($E7)
        // flag has been crossed on that owner's path.
        QString description;
        if (state.kind == ChannelKind::FM)
            description = state.voiceOrTone >= 0 ? QString("Voice %1").arg(state.voiceOrTone) : "Voice ?";
        else if (state.kind == ChannelKind::PSG)
            description = state.noiseActive ? "Noise"
                           : state.voiceOrTone >= 0 ? QString("fTone%1").arg(state.voiceOrTone)
                                                     : "fTone ?";
        else if (state.kind == ChannelKind::DAC)
            description = "Drumkit"; // per your direction -- the rare FM/PSG-vs-DAC conflict case
        descriptions << description;
    }
    panel->setOwnerChoices(owners, descriptions);
}

PianoRollPanel *MainWindow::openPianoRollFor(const QString &name, const QJsonArray &events) {
    const QString key = "pianoRoll_" + name;
    if (PianoRollPanel *existing = m_pianoRollPanels.value(name)) {
        existing->setBlock(name, events);
        existing->setDacSampleNames(SongDocument::dacSampleNamesForBlock(m_document.header(), name));
        updateOwnerConflictUI(name, existing);
        raiseManagedPanel(key);
        return existing;
    }

    auto *panel = new PianoRollPanel(this);
    panel->setBlock(name, events);
    panel->setAvailableBlocks(m_document.playlistOrder());
    panel->setTempo(m_divideSpin->value(), m_tempoSpin->value());
    panel->setDacSampleNames(SongDocument::dacSampleNamesForBlock(m_document.header(), name));
    updateOwnerConflictUI(name, panel);

    // Closable, per your direction -- unlike the app's permanent panels, a
    // Piano Roll only exists once a block is opened with one, and should be
    // fully discardable again. There's no separate close button for this
    // (redundant with the host's own native "x", per your direction) --
    // setPanelMode() sets WA_DeleteOnClose on whatever host currently owns
    // this panel, so the native close control fully discards it instead of
    // just hiding it.
    DockableContainer *container =
        createManagedPanel(panel, "Piano Roll: " + name, key, Qt::RightDockWidgetArea, /*closable=*/true);
    // The Play/Fit/Cut/Length toolbar shares the container's own header row
    // (a general feature of that window class, per your direction) instead
    // of taking a separate row of its own.
    container->setToolBar(panel->toolbar());

    connect(panel, &PianoRollPanel::eventsChanged, this, &MainWindow::onPianoRollEventsChanged);
    // Both the sidebar's manual key clicks and stepPlayback()'s block
    // test-play land here, resolved identically against wherever the
    // playhead/cursor currently sits (per your direction: "the keys play
    // based on what the last note voice change was relative to the red
    // line cursor position" -- and the same for playback itself, "fixes
    // yellow and purple blocks", since resolveVoiceAtTick() chases the
    // smpsCall chain backward when this block hasn't set a voice yet).
    // A PSG home block (per your direction) previews through the PSG path
    // instead -- tone from the header's own default/whatever smpsPSGvoice
    // has run by the cursor, or periodic noise once an $F3 (smpsPSGform)
    // tag has been crossed.
    connect(panel, &PianoRollPanel::notePreviewRequested, this, [this, panel](int note) {
        const double cutoff = qMax(0.0, panel->currentPlayheadTick());
        const QString blockName = panel->blockName();

        // DAC-home block (per your direction): the piano-keys sidebar only
        // ever emits notePreviewRequested for rows it labeled with a real
        // sample (see PianoKeysWidget::mousePressEvent's own DAC-mode
        // guard), so any note here in that context IS meant to trigger the
        // actual sample, not a synthesized FM/PSG tone.
        if (!SongDocument::dacSampleNamesForBlock(m_document.header(), blockName).isEmpty()) {
            m_audio.previewDacSample(note);
            return;
        }

        // Shared-block conflict resolution (per your direction): if this
        // block's distinct owners genuinely disagree
        // (BlockMetadata::checkSharedConflict) and the toolbar dropdown has
        // a selection, resolve strictly along THAT owner's own path
        // (BlockMetadata::resolveStateAtTick, which correctly determines
        // channel kind via the header's home-block chain, unlike
        // SongDocument::resolveVoiceAtTick's older "just assume FM if
        // psgChannelInfoForBlock(blockName) doesn't match" fallback)
        // instead of the ordinary single-path resolution below.
        int overrideVoice = -1, overridePsgTone = -1;
        bool overridePsgNoise = false;
        bool haveOverride = false;
        const QString ownerContext = panel->selectedOwnerContext();
        if (!ownerContext.isEmpty() && m_blockMetadata.checkSharedConflict(blockName, m_document.header())) {
            double ownerCutoff = -1;
            for (const BlockCallSite &site : m_blockMetadata.callSitesOf(blockName))
                if (site.ownerBlock == ownerContext && (ownerCutoff < 0 || site.tick < ownerCutoff))
                    ownerCutoff = site.tick;
            const ResolvedBlockState state = m_blockMetadata.resolveStateAtTick(ownerContext, ownerCutoff, m_document.header());
            if (state.kind == ChannelKind::PSG) {
                overridePsgTone = state.voiceOrTone;
                overridePsgNoise = state.noiseActive;
                haveOverride = true;
            } else if (state.kind == ChannelKind::FM && state.voiceOrTone >= 0) {
                overrideVoice = state.voiceOrTone;
                haveOverride = true;
            }
        }

        int defaultTone = 0, psgAttenuation = 0;
        if ((haveOverride && overridePsgTone >= 0) ||
            (!haveOverride && SongDocument::psgChannelInfoForBlock(m_document.header(), blockName, &defaultTone, &psgAttenuation))) {
            SongDocument::psgChannelInfoForBlock(m_document.header(), blockName, &defaultTone, &psgAttenuation);
            int tone = defaultTone;
            bool noise = false;
            if (haveOverride) {
                tone = overridePsgTone;
                noise = overridePsgNoise;
            } else {
                m_document.resolvePsgStateAtTick(blockName, cutoff, defaultTone, &tone, &noise);
            }
            m_audio.previewPsgTone(note, tone, noise, psgAttenuation);
            return;
        }

        const int voiceIndex = haveOverride ? overrideVoice : m_document.resolveVoiceAtTick(blockName, cutoff);
        const QJsonArray voices = m_document.voices();
        if (voiceIndex < 0 || voiceIndex >= voices.size())
            return;
        const QJsonObject voice = voices.at(voiceIndex).toObject();
        const int attenuation = SongDocument::fmChannelAttenuationForBlock(m_document.header(), blockName);
        if (!voice.isEmpty())
            m_audio.previewVoice(voice, note, attenuation);
    });
    connect(panel, &PianoRollPanel::notePreviewStopRequested, this, [this]() { m_audio.stopPreview(); });
    connect(&m_audio, &AudioEngine::frameTicked, panel, &PianoRollPanel::advancePlayhead);
    // Fires once the container is actually destroyed -- which now only
    // happens via a closable panel's host being closed natively (see
    // ManagedPanel::closable), since setPanelMode()'s ordinary mode-switch
    // teardown always reparents the container onward rather than deleting
    // it. Cleans up both maps together since they track the same panel.
    connect(container, &QObject::destroyed, this, [this, key, name]() {
        m_managedPanels.remove(key);
        m_pianoRollPanels.remove(name);
    });

    m_pianoRollPanels.insert(name, panel);
    return panel;
}

void MainWindow::openSourceEditor() {
    syncSourcePanelFromDocument();
    if (!m_managedPanels.contains("source"))
        createManagedPanel(m_sourcePanel, "Source (JSON)", "source", Qt::RightDockWidgetArea);
    raiseManagedPanel("source");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    saveLayout();
    QMainWindow::closeEvent(event);
}

void MainWindow::saveLayout() {
    QSettings settings;
    settings.setValue("mainWindow/geometry", saveGeometry());
    // Also covers every managed panel currently in Docked mode (as long as
    // it's alive, i.e. has an objectName-tagged QDockWidget right now) --
    // QMainWindow's own state blob handles dock geometry/area/tabification
    // for those automatically.
    settings.setValue("mainWindow/state", saveState());

    // Which Mdi-mode panel (if any) is currently maximized -- tracked as
    // ONE single value rather than each panel saving its own maximized bit:
    // QMdiArea only allows one subwindow maximized at a time, so restoring
    // several independently-saved "I was maximized" flags in a loop would
    // un-maximize an earlier one mid-restore and leave ITS geometry
    // corrupted -- a real bug this sidesteps (it showed up as a random
    // window ending up maximized, overlapping the menu bar).
    QString maximizedKey;
    for (auto it = m_managedPanels.constBegin(); it != m_managedPanels.constEnd(); ++it) {
        const QString &key = it.key();
        if (key.startsWith("pianoRoll_"))
            continue; // dynamic windows aren't persisted, per your direction
        const ManagedPanel &mp = it.value();
        settings.beginGroup("panel_" + key);
        settings.setValue("mode", int(mp.mode));
        if (mp.mode == PanelMode::Mdi && mp.mdiHost) {
            settings.setValue("geometry", mp.mdiHost->normalGeometry());
            if (mp.mdiHost->isMaximized())
                maximizedKey = key;
        } else if (mp.mode == PanelMode::Floating) {
            settings.setValue("geometry", mp.container->normalGeometry());
        }
        settings.endGroup();
    }
    settings.setValue("mainWindow/maximizedPanel", maximizedKey);
}

void MainWindow::loadLayout() {
    QSettings settings;
    if (settings.contains("mainWindow/geometry"))
        restoreGeometry(settings.value("mainWindow/geometry").toByteArray());
    if (settings.contains("mainWindow/state"))
        restoreState(settings.value("mainWindow/state").toByteArray());

    bool anyRestored = false;
    for (auto it = m_managedPanels.begin(); it != m_managedPanels.end(); ++it) {
        const QString &key = it.key();
        if (key.startsWith("pianoRoll_"))
            continue;
        ManagedPanel &mp = *it;
        settings.beginGroup("panel_" + key);
        if (settings.contains("mode")) {
            auto mode = static_cast<PanelMode>(settings.value("mode").toInt());
            if (mode == PanelMode::Docked && !mp.dockable)
                mode = PanelMode::Mdi; // defensive -- see feedback_paradoxcomposer_fixed_windows memory
            setPanelMode(key, mode, /*forceRebuild=*/true);
            // Guards against a stale entry from before this system existed
            // (previously stored as a saveGeometry() byte blob, not a
            // QRect) -- reading that back as a QRect fails silently and
            // would otherwise collapse the window to 0x0.
            const QRect rect = settings.value("geometry").toRect();
            if (rect.isValid()) {
                if (mp.mode == PanelMode::Mdi && mp.mdiHost)
                    mp.mdiHost->setGeometry(rect);
                else if (mp.mode == PanelMode::Floating)
                    mp.container->setGeometry(rect);
            }
            anyRestored = true;
        }
        settings.endGroup();
    }
    if (!anyRestored) {
        m_mdiArea->tileSubWindows(); // first run / no saved layout yet -- fall back to the old default
        return;
    }
    const QString maximizedKey = settings.value("mainWindow/maximizedPanel").toString();
    if (!maximizedKey.isEmpty() && m_managedPanels.contains(maximizedKey)) {
        const ManagedPanel &mp = m_managedPanels.value(maximizedKey);
        if (mp.mode == PanelMode::Mdi && mp.mdiHost)
            mp.mdiHost->showMaximized();
    }
}

PianoRollPanel *MainWindow::activePianoRollPanel() const {
    QWidget *w = QApplication::focusWidget();
    while (w) {
        if (auto *panel = qobject_cast<PianoRollPanel *>(w))
            return panel;
        w = w->parentWidget();
    }
    return nullptr;
}

DockableContainer *MainWindow::createManagedPanel(QWidget *content, const QString &title, const QString &key,
                                                    Qt::DockWidgetArea dockArea, bool closable, bool dockable) {
    auto *container = new DockableContainer(content, title, dockable, this);
    ManagedPanel mp;
    mp.container = container;
    mp.dockArea = dockArea;
    mp.dockable = dockable;
    mp.closable = closable;
    m_managedPanels.insert(key, mp);

    connect(container, &DockableContainer::dockRequested, this, [this, key]() { setPanelMode(key, PanelMode::Docked); });
    connect(container, &DockableContainer::mdiRequested, this, [this, key]() { setPanelMode(key, PanelMode::Mdi); });
    connect(container, &DockableContainer::floatToggleRequested, this, [this, key]() {
        ManagedPanel &entry = m_managedPanels[key];
        if (entry.mode == PanelMode::Floating)
            setPanelMode(key, entry.preFloatMode); // per your direction: return to its original state, not always Mdi
        else {
            entry.preFloatMode = entry.mode;
            setPanelMode(key, PanelMode::Floating);
        }
    });

    setPanelMode(key, PanelMode::Mdi, /*forceRebuild=*/true); // builds the initial host (default state matches ManagedPanel::mode's default, so this needs forceRebuild)
    return container;
}

void MainWindow::setPanelMode(const QString &key, PanelMode mode, bool forceRebuild) {
    if (!m_managedPanels.contains(key))
        return;
    ManagedPanel &mp = m_managedPanels[key];
    if (mp.mode == mode && !forceRebuild)
        return;

    // Detach the container from whichever host currently owns it (there is
    // none yet on the very first call, from createManagedPanel), always
    // landing it on `this` (MainWindow) as a known-safe intermediate parent
    // first rather than reparenting straight from the old host's own
    // layout machinery into the new one -- doing it directly was found to
    // sometimes leave the container as a stray top-level window, or (worse)
    // not properly reparented at all, appearing to "lose its contents"
    // (Docked -> Mdi was the reported case, but the same teardown code path
    // is shared by every transition).
    mp.container->hide();
    if (mp.dockHost) {
        mp.dockHost->setWidget(nullptr);
        removeDockWidget(mp.dockHost);
        mp.dockHost->deleteLater();
        mp.dockHost = nullptr;
    } else if (mp.mdiHost) {
        m_mdiArea->removeSubWindow(mp.container);
        mp.mdiHost->deleteLater();
        mp.mdiHost = nullptr;
    }
    // setParent(this, Qt::Widget) in one call -- clears any leftover
    // Qt::Window flag from a prior Floating stint AND lands the container
    // as a plain embeddable child in the same step (setParent() alone
    // preserves whatever window flags were already set, which is exactly
    // what let a stray Qt::Window flag survive a transition in the old
    // code).
    mp.container->setParent(this, Qt::Widget);

    switch (mode) {
    case PanelMode::Docked: {
        auto *dock = new QDockWidget(mp.container->title(), this);
        dock->setObjectName(key + "Dock");
        dock->setWidget(mp.container);
        dock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable);
        // Per your direction: no separate close button on the container
        // itself (redundant with this dock's own native "x") -- for
        // closable panels (Piano Roll instances), that native close should
        // fully discard rather than just hide, so it goes through close()
        // with WA_DeleteOnClose instead of the default hide-only behavior.
        // The container's own destroyed() connection (see openPianoRollFor)
        // handles cleanup once this cascades to it.
        if (mp.closable)
            dock->setAttribute(Qt::WA_DeleteOnClose);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        addDockWidget(mp.dockArea, dock);
        mp.dockHost = dock;
        mp.container->show();
        dock->show();
        break;
    }
    case PanelMode::Mdi: {
        QMdiSubWindow *sub = m_mdiArea->addSubWindow(mp.container);
        sub->setWindowTitle(mp.container->title());
        if (mp.closable)
            sub->setAttribute(Qt::WA_DeleteOnClose); // see the Docked case's comment above
        mp.mdiHost = sub;
        // Explicitly showing the container itself, not just its new parent
        // subwindow -- the hide() up top marks it "explicitly hidden", and
        // Qt does NOT auto-reveal an explicitly-hidden child just because
        // its parent gets shown. Forgetting this left every Mdi-mode panel
        // rendering as a blank subwindow (title bar only, no header/gear
        // menu, no content) -- the actual bug you were seeing.
        mp.container->show();
        sub->show();
        break;
    }
    case PanelMode::Floating: {
        mp.container->setWindowFlag(Qt::Window, true);
        mp.container->setWindowTitle(mp.container->title());
        if (mp.closable)
            mp.container->setAttribute(Qt::WA_DeleteOnClose); // see the Docked case's comment above -- no separate host here, the container itself IS the window
        if (mp.container->size().isEmpty())
            mp.container->resize(mp.container->sizeHint());
        mp.container->show();
        mp.container->raise();
        mp.container->activateWindow();
        break;
    }
    }
    mp.mode = mode;
    mp.container->setMode(mode);
}

void MainWindow::raiseManagedPanel(const QString &key) {
    if (!m_managedPanels.contains(key))
        return;
    ManagedPanel &mp = m_managedPanels[key];
    switch (mp.mode) {
    case PanelMode::Mdi:
        if (mp.mdiHost && mp.mdiHost->isHidden()) {
            // Rebuilding the subwindow frame instead of reusing a
            // previously-hidden one, which was found to render blank on a
            // simple show() (a real Qt repaint/geometry quirk) -- the
            // container/content itself is untouched either way.
            setPanelMode(key, PanelMode::Mdi, /*forceRebuild=*/true);
        } else if (mp.mdiHost) {
            m_mdiArea->setActiveSubWindow(mp.mdiHost);
        }
        break;
    case PanelMode::Docked:
        if (mp.dockHost) {
            mp.dockHost->show();
            mp.dockHost->raise();
        }
        break;
    case PanelMode::Floating:
        mp.container->show();
        mp.container->raise();
        mp.container->activateWindow();
        break;
    }
}


void MainWindow::broadcastAvailableBlocksToOpenPianoRolls() {
    for (PianoRollPanel *panel : std::as_const(m_pianoRollPanels))
        panel->setAvailableBlocks(m_document.playlistOrder());
}

void MainWindow::refreshAllPanelsFromDocument() {
    syncSourcePanelFromDocument();
    m_voiceBankPanel->setVoices(m_document.voices());
    m_playlistPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    broadcastAvailableBlocksToOpenPianoRolls();
    // Refresh every already-open Piano Roll's own displayed block content too
    // (not just the block-name list broadcastAvailableBlocksToOpenPianoRolls
    // already handles) -- needed so undo/redo (and New/Open, which also call
    // this) correctly show restored/replaced content rather than stale data
    // left over from before the document was swapped out from under them.
    const QJsonObject playlist = m_document.playlist();
    for (auto it = m_pianoRollPanels.constBegin(); it != m_pianoRollPanels.constEnd(); ++it)
        if (playlist.contains(it.key())) {
            it.value()->setBlock(it.key(), playlist.value(it.key()).toArray());
            updateOwnerConflictUI(it.key(), it.value());
        }
    m_channelsPanel->setHeader(m_document.header());
    m_channelsPanel->setPlaylist(m_document.playlist(), m_document.playlistOrder());
    m_playlistPanel->setHeader(m_document.header());
    syncTransportFromHeader();
    broadcastTempoToOpenPianoRolls();
    compileCurrentDocument();
}

void MainWindow::pushUndoSnapshot() {
    m_undoStack.append(m_document.root());
    if (m_undoStack.size() > 200)
        m_undoStack.removeFirst();
    m_redoStack.clear();
    updateUndoRedoActions();
}

void MainWindow::undo() {
    if (m_undoStack.isEmpty())
        return;
    m_redoStack.append(m_document.root());
    m_document.root() = m_undoStack.takeLast();
    updateUndoRedoActions();
    showToast("Undo");
    // Deferred: triggered from the Edit menu's own dropdown, and
    // refreshAllPanelsFromDocument() repaints every panel synchronously --
    // doing that before the menu has finished closing left its popup as a
    // redraw artifact on screen. Letting the menu-close cycle run first
    // (a queued 0ms callback) fixes it.
    QTimer::singleShot(0, this, [this]() { refreshAllPanelsFromDocument(); });
}

void MainWindow::redo() {
    if (m_redoStack.isEmpty())
        return;
    m_undoStack.append(m_document.root());
    m_document.root() = m_redoStack.takeLast();
    updateUndoRedoActions();
    showToast("Redo");
    QTimer::singleShot(0, this, [this]() { refreshAllPanelsFromDocument(); });
}

void MainWindow::showToast(const QString &text) {
    if (!m_toastLabel) {
        // A genuine top-level window (Qt::ToolTip: frameless, always-on-top,
        // no taskbar entry, never steals focus), NOT a child QLabel painted
        // onto MainWindow's own surface -- a child widget shared the same
        // backing store as the MDI area/menu bar, and got left behind as a
        // redraw artifact when a menu-triggered action (Undo/Redo) repainted
        // everything underneath it before the menu had finished closing. A
        // separate native surface is composited independently, so it can't
        // be corrupted by unrelated repaints elsewhere in the window.
        m_toastLabel = new QLabel(this, Qt::ToolTip | Qt::FramelessWindowHint); // parented for lifetime only -- window flags still make it a real top-level surface
        m_toastLabel->setAttribute(Qt::WA_TranslucentBackground);
        m_toastLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_toastLabel->setAttribute(Qt::WA_ShowWithoutActivating);
        m_toastLabel->setStyleSheet("QLabel { background-color: rgba(40,40,40,230); color: white; "
                                     "padding: 6px 16px; border-radius: 8px; font-size: 12px; }");
        m_toastLabel->setGraphicsEffect(new QGraphicsOpacityEffect(m_toastLabel));
        m_toastLabel->hide();
    }
    auto *effect = static_cast<QGraphicsOpacityEffect *>(m_toastLabel->graphicsEffect());
    effect->setOpacity(1.0);
    m_toastLabel->setText(text);
    m_toastLabel->adjustSize();
    // Positioned relative to MainWindow's own on-screen geometry (this is
    // now a separate top-level window, so it needs global coordinates
    // rather than a local move() within a parent).
    const QPoint bottomCenter = mapToGlobal(QPoint(width() / 2, height()));
    m_toastLabel->move(bottomCenter.x() - m_toastLabel->width() / 2, bottomCenter.y() - m_toastLabel->height() - 40);
    m_toastLabel->show();
    m_toastLabel->raise();

    auto *fade = new QPropertyAnimation(effect, "opacity", m_toastLabel);
    fade->setDuration(400);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    connect(fade, &QPropertyAnimation::finished, m_toastLabel, &QLabel::hide);
    QTimer::singleShot(900, fade, [fade]() { fade->start(QAbstractAnimation::DeleteWhenStopped); });
}

void MainWindow::updateUndoRedoActions() {
    if (m_undoAction)
        m_undoAction->setEnabled(!m_undoStack.isEmpty());
    if (m_redoAction)
        m_redoAction->setEnabled(!m_redoStack.isEmpty());
}

void MainWindow::syncSourcePanelFromDocument() {
    const QJsonDocument doc(m_document.root());
    m_sourcePanel->setText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}
