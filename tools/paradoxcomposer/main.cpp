#include "MainWindow.h"

#include <QApplication>
#include <QDir>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    // Needed for QSettings' default constructor (MainWindow::saveLayout()/
    // loadLayout()) to resolve a sensible config file location.
    QApplication::setOrganizationName("ParadoxSMPS");
    QApplication::setApplicationName("ParadoxComposer");

    // Repo root: this binary is built to <build>/tests/ (or similar), and
    // the source tree layout (tools/paradoxsmps/json_to_header.py) is only
    // known relative to the CMake source dir, baked in at configure time --
    // see CMakeLists.txt's PARADOXCOMPOSER_REPO_ROOT compile definition.
#ifdef PARADOXCOMPOSER_REPO_ROOT
    const QString repoRoot = PARADOXCOMPOSER_REPO_ROOT;
#else
    const QString repoRoot = QDir::currentPath();
#endif

    MainWindow window(repoRoot);
    window.show();
    return app.exec();
}
