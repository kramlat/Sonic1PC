#include "GameInterface.h"
#include "HelpWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace {

// Where the handbook's HTML may be: next to a source tree's bin/ folder (bin/Sonic, bin/Debug/Sonic), or installed.
QString FindHtml() {
	const QString app = QCoreApplication::applicationDirPath();
	const QStringList candidates = {
		app + "/../doc/html/index.html",
		app + "/../../doc/html/index.html",
		app + "/../../../doc/html/index.html",
		app + "/../share/doc/" + game_info.app_id + "/html/index.html",
		app + "/doc/html/index.html",
	};
	for (const QString &c : candidates)
		if (QFileInfo::exists(c))
			return QFileInfo(c).canonicalFilePath();
	return QString();
}

class HelpBrowserWindow : public QWidget {
public:
	explicit HelpBrowserWindow(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle(QString(game_info.app_name) + " Handbook");
		auto *root = new QVBoxLayout(this);
		auto *bar = new QHBoxLayout;
		auto *back = new QPushButton("Back");
		auto *forward = new QPushButton("Forward");
		auto *home = new QPushButton("Contents");
		bar->addWidget(back);
		bar->addWidget(forward);
		bar->addWidget(home);
		bar->addStretch();
		root->addLayout(bar);
		browser = new QTextBrowser;
		browser->setOpenExternalLinks(false);
		root->addWidget(browser);
		connect(back, &QPushButton::clicked, browser, &QTextBrowser::backward);
		connect(forward, &QPushButton::clicked, browser, &QTextBrowser::forward);
		connect(home, &QPushButton::clicked, browser, &QTextBrowser::home);
		resize(940, 720);
	}

	void Open(const QString &html_path) {
		browser->setSource(QUrl::fromLocalFile(html_path));
		show();
		raise();
		activateWindow();
	}

private:
	QTextBrowser *browser = nullptr;
};

HelpBrowserWindow *g_window = nullptr;

} // namespace

namespace HelpWindow {

void ShowHandbook(QWidget *parent) {
	// KDE Help Center, when it and the installed handbook are there.
	const QString khelpcenter = QStandardPaths::findExecutable("khelpcenter");
	const QString installed = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QString("doc/HTML/en/") + game_info.app_id + "/index.docbook");
	if (!khelpcenter.isEmpty() && !installed.isEmpty() && QProcess::startDetached(khelpcenter, { QString("help:/") + game_info.app_id }))
		return;

	const QString html = FindHtml();
	if (html.isEmpty()) {
		QMessageBox::information(parent, "Handbook",
		                         "The handbook is not installed. It is built from doc/index.docbook (the HTML version needs xsltproc).");
		return;
	}
	if (g_window == nullptr)
		g_window = new HelpBrowserWindow(parent);
	g_window->Open(html);
}

} // namespace HelpWindow
