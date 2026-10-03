#pragma once

#include <QMainWindow>
#include <QMap>
#include <QVector>

#include "AudioEngine.h"
#include "BlockMetadata.h"
#include "DockableContainer.h"
#include "SongDocument.h"

class SourcePanel;
class BytesPanel;
class VoiceBankPanel;
class PlaylistPanel;
class PlaybackPanel;
class ChannelsPanel;
class PianoRollPanel;
class QAction;
class QDockWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &repoRoot, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void newSong();
    void newVoiceBank();
    void openFile();
    void saveFile();
    void saveFileAs();

    void applySourceText(const QString &jsonText);
    void onVoicesChanged(const QJsonArray &voices);
    void onPlaylistChanged(const QJsonObject &playlist, const QStringList &order);
    void onHeaderChanged(const QJsonArray &header);
    void writeTransportToHeader();

    void compileCurrentDocument();
    void playCompiledSong(bool isSfx);
    void stopSong();
    void onPianoRollEventsChanged(const QString &blockName, const QJsonArray &events);

public slots:
    // Brief, non-modal feedback bubble over the MDI area (currently used for
    // Undo/Redo) -- public so other panels (Playlist, Voice Bank, etc.) can
    // wire their own signals straight to it, per your direction, rather than
    // each one needing its own toast implementation.
    void showToast(const QString &text);

private slots:
    // Document-wide undo/redo (per your direction: "a history for undo and
    // redo", not just one level) -- covers every panel's edits (header,
    // voices, playlist, any open piano roll's own block events, transport
    // tempo), since they all funnel into m_document through the handlers
    // above. Each one calls pushUndoSnapshot() before mutating.
    void undo();
    void redo();

private:
    // Snapshots the document's CURRENT state onto the undo stack and clears
    // the redo stack (a fresh edit invalidates any previously-undone
    // future) -- call before applying a new change, from every
    // document-mutating handler above.
    void pushUndoSnapshot();
    void updateUndoRedoActions(); // enables/disables the Edit menu's Undo/Redo entries to match stack state

    void refreshAllPanelsFromDocument();
    void syncSourcePanelFromDocument();
    void syncTransportFromHeader();
    // Spawns a new Piano Roll editor for `name`, or raises/refreshes an
    // already-open one for that same block -- per your direction, no piano
    // roll exists until a block is actually opened with one (via
    // PlaylistPanel's double-click).
    PianoRollPanel *openPianoRollFor(const QString &name, const QJsonArray &events);
    // Checks `name` for a genuine shared-owner conflict (BlockMetadata::
    // checkSharedConflict) and shows/populates or hides `panel`'s
    // conflict-resolution dropdown to match -- called after every setBlock()
    // on an open Piano Roll (new or refreshed), since which block it's
    // showing (and therefore its conflict status) can change independently
    // of the panel itself.
    void updateOwnerConflictUI(const QString &name, PianoRollPanel *panel);
    // The Piano Roll panel that currently has keyboard focus (walks up the
    // focus chain, since a Piano Roll can now live in any of the three
    // container modes below), or nullptr if none does -- backs the Edit
    // menu's Copy/Cut/Paste actions.
    PianoRollPanel *activePianoRollPanel() const;
    void broadcastAvailableBlocksToOpenPianoRolls();
    // Pushes the document's own smpsHeaderTempo (duration_mult/main_tempo)
    // to every open piano roll panel's playhead speed (see
    // PianoRollPanel::setTempo) -- called after any edit that could change
    // it (writeTransportToHeader) and once per newly-opened panel.
    void broadcastTempoToOpenPianoRolls();
    // Opens/raises the Source (JSON) editor -- per your direction, not
    // spawned by default, only reachable from the Window menu.
    void openSourceEditor();

    // FL Studio-style dockable/floating panel management (per your
    // direction): every panel below except Voice Bank/Playback (which must
    // stay fixed-size, see feedback_paradoxcomposer_fixed_windows memory)
    // is wrapped in a DockableContainer that can live in one of three
    // container types -- a QDockWidget attached to a main-window edge, an
    // QMdiSubWindow (the default "floating" state, contained inside the
    // app's own MDI canvas), or a real top-level desktop window (explicit
    // opt-in via the container's own Float button). Even the two
    // undockable panels still get the Mdi/Floating toggle -- only the Dock
    // option is withheld from them.
    struct ManagedPanel {
        DockableContainer *container = nullptr;
        QDockWidget *dockHost = nullptr;
        class QMdiSubWindow *mdiHost = nullptr;
        Qt::DockWidgetArea dockArea = Qt::RightDockWidgetArea;
        bool dockable = true;
        // Piano Roll instances only -- per your direction, there's no
        // separate close button on the container anymore (redundant with
        // whichever host's own native close control), so for these,
        // setPanelMode() sets WA_DeleteOnClose on the host itself (or the
        // container, when Floating) so the native "x" fully discards the
        // panel instead of just hiding it.
        bool closable = false;
        PanelMode mode = PanelMode::Mdi;
        // Whatever mode this panel was in right before it was last sent to
        // Floating -- per your direction, toggling true floating back off
        // returns the window to its original state inside the app, not
        // unconditionally to Mdi.
        PanelMode preFloatMode = PanelMode::Mdi;
    };
    // Wraps `content` in a new DockableContainer, registers it under `key`,
    // and places it in its default Mdi (contained-floating) container.
    DockableContainer *createManagedPanel(QWidget *content, const QString &title, const QString &key,
                                           Qt::DockWidgetArea dockArea, bool closable = false, bool dockable = true);
    // Moves an already-registered panel between Docked/Mdi/Floating,
    // tearing down whichever container currently hosts it and building the
    // new one -- the panel's own content widget is never destroyed, only
    // reparented. forceRebuild rebuilds the host even if already in `mode`
    // (used for the very first placement, and to work around a real Qt
    // quirk where reusing a previously-hidden QMdiSubWindow renders blank).
    void setPanelMode(const QString &key, PanelMode mode, bool forceRebuild = false);
    // Brings an already-open managed panel to the front regardless of which
    // container mode it's currently in.
    void raiseManagedPanel(const QString &key);
    QMap<QString, ManagedPanel> m_managedPanels;

    // Persists/restores the main window's dock/toolbar state plus every
    // named MDI subwindow's geometry via QSettings -- per your direction,
    // "use this as the home layout": saveLayout() runs on close, capturing
    // whatever arrangement is currently on screen as what loadLayout()
    // restores on the next launch. Dynamic windows (Piano Roll instances,
    // which have no fixed objectName) are intentionally not persisted.
    void saveLayout();
    void loadLayout();

    QString m_repoRoot;
    QString m_currentFilePath;
    SongDocument m_document;
    AudioEngine m_audio;
    // DAW-only block ownership index (per your direction) -- rebuilt on
    // every openFile(), see BlockMetadata's own header comment for why
    // this exists and what it does/doesn't persist.
    BlockMetadata m_blockMetadata;

    // Undo/redo history (per your direction) -- full document snapshots,
    // not a diff/command log; SongDocument's own root object is small
    // enough (a handful of songs/voices/blocks) that this is simple and
    // robust rather than needing per-edit command objects. Capped so a long
    // editing session doesn't grow this unboundedly.
    QVector<QJsonObject> m_undoStack;
    QVector<QJsonObject> m_redoStack;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    class QLabel *m_toastLabel = nullptr;

    // Kept so a Floating panel's own menu bar (see DockableContainer's
    // setFloatingMenus()) can reuse these exact same QMenu/QAction objects
    // -- a Floating panel is a separate top-level window with no other way
    // to reach the app's own File/Edit/Window menu.
    class QMenu *m_fileMenu = nullptr;
    class QMenu *m_editMenu = nullptr;
    class QMenu *m_windowMenu = nullptr;

    SourcePanel *m_sourcePanel;
    BytesPanel *m_bytesPanel;
    VoiceBankPanel *m_voiceBankPanel;
    PlaylistPanel *m_playlistPanel;
    PlaybackPanel *m_playbackPanel;
    ChannelsPanel *m_channelsPanel;
    QMap<QString, PianoRollPanel *> m_pianoRollPanels; // block name -> its open editor window, if any

    class QSpinBox *m_tempoSpin;
    class QSpinBox *m_divideSpin;
    class QMdiArea *m_mdiArea;
};
