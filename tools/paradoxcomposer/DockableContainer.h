#pragma once

#include <QWidget>

class QAction;
class QMenu;
class QToolBar;
class QToolButton;

// Where a DockableContainer currently lives -- FL Studio-style, per your
// direction: Mdi is the default "floating" state (a window contained inside
// the main app's own MDI canvas, exactly like Piano Roll/Playlist windows
// always worked), Floating is a REAL separate desktop window (explicit
// opt-in via the float button), and Docked attaches it to a main-window
// edge like the Channels panel.
enum class PanelMode { Docked, Mdi, Floating };

// A slim, persistent header bar wrapped around one panel's real content --
// this widget itself never gets destroyed as its panel moves between
// containers (QDockWidget, QMdiSubWindow, or a true top-level window); only
// the container around it changes, in MainWindow::setPanelMode(). The
// header's options button opens a single menu with checkable "Docked" /
// "Detached" entries (per your FL Studio reference screenshot). There is no
// separate close button here -- whichever host currently wraps this
// container already provides a native close control (its own title bar's
// "x"), and MainWindow wires that to actually discard closable panels (see
// setPanelMode's ManagedPanel::closable handling) instead of duplicating it
// here.
class DockableContainer : public QWidget {
    Q_OBJECT
public:
    // dockable: false for Voice Bank / Playback (per your direction, those
    // two must stay fixed-size and can never attach to a main-window edge)
    // -- omits the "Docked" menu entry, but "Detached" still applies ("even
    // undockables" get true-floating on/off, per your direction).
    explicit DockableContainer(QWidget *content, const QString &title, bool dockable = true,
                                QWidget *parent = nullptr);

    QString title() const { return m_title; }
    // Updates the options menu's checked entries to match where this
    // container currently lives -- called by MainWindow right after it
    // finishes moving this panel into its new host.
    void setMode(PanelMode mode);

    // Places `bar` into this container's own header row (a general feature
    // of this window class, per your direction, not specific to any one
    // panel) instead of the panel needing a separate toolbar row of its
    // own underneath -- e.g. Piano Roll's Play/Fit/Cut/Length toolbar.
    // Ownership is unaffected; this just reparents `bar` into the header.
    void setToolBar(QToolBar *bar);

signals:
    void dockRequested();         // "attach me to a main-window edge"
    void mdiRequested();          // "back to the default contained/MDI window"
    void floatToggleRequested();  // "pop out to / return from a real desktop window"

private:
    QString m_title;
    PanelMode m_mode = PanelMode::Mdi;
    bool m_dockable = true;
    class QHBoxLayout *m_headerLayout;
    QToolButton *m_optionsButton;
    QAction *m_dockedAction = nullptr;   // checkable, only present when dockable
    QAction *m_detachedAction;           // checkable
};
