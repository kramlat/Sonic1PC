#include "DockableContainer.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

DockableContainer::DockableContainer(QWidget *content, const QString &title, bool dockable, QWidget *parent)
    : QWidget(parent), m_title(title), m_dockable(dockable) {
    setWindowTitle(title);

    // No title label here -- whatever host is currently showing this
    // container (QDockWidget's own bar, QMdiSubWindow's own bar, or the OS
    // window chrome when Floating) already displays the title, so a
    // duplicate one in our own header was pure redundancy across every
    // mode. The freed-up space is now a general toolbar dock area (see
    // setToolBar()) instead.
    auto *header = new QWidget(this);
    header->setStyleSheet("background-color: palette(mid);");
    m_headerLayout = new QHBoxLayout(header);
    m_headerLayout->setContentsMargins(6, 2, 4, 2);
    m_headerLayout->addStretch(1);

    // Single options button + checkable menu (per your FL Studio reference
    // screenshot) instead of separate always-visible toggle buttons.
    m_optionsButton = new QToolButton(header);
    m_optionsButton->setText("⚙"); // gear glyph
    m_optionsButton->setToolTip("Window options");
    m_optionsButton->setAutoRaise(true);
    m_optionsButton->setPopupMode(QToolButton::InstantPopup);

    auto *menu = new QMenu(m_optionsButton);
    if (m_dockable) {
        m_dockedAction = menu->addAction("Docked");
        m_dockedAction->setCheckable(true);
        connect(m_dockedAction, &QAction::triggered, this, [this](bool checked) {
            if (checked)
                emit dockRequested();
            else
                emit mdiRequested();
        });
    }
    m_detachedAction = menu->addAction("Detached");
    m_detachedAction->setCheckable(true);
    connect(m_detachedAction, &QAction::triggered, this, &DockableContainer::floatToggleRequested);
    m_optionsButton->setMenu(menu);
    m_headerLayout->addWidget(m_optionsButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(content, 1);

    setMode(PanelMode::Mdi);
}

void DockableContainer::setMode(PanelMode mode) {
    m_mode = mode;
    if (m_dockedAction) {
        const QSignalBlocker blocker(m_dockedAction); // setChecked() must not re-trigger triggered()
        m_dockedAction->setChecked(mode == PanelMode::Docked);
    }
    const QSignalBlocker blocker(m_detachedAction);
    m_detachedAction->setChecked(mode == PanelMode::Floating);
}

void DockableContainer::setToolBar(QToolBar *bar) {
    m_headerLayout->insertWidget(0, bar, 1);
}
