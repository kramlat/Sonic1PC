#pragma once

// Quake-style console drawer: slides down over the top of the game view, with a scrollback and a prompt in the
// system's fixed-width font. The commands and variables are Console.c's; this is only the window. It exists
// only while debugging is available (the same check that shows the Tools and View menus).

#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPropertyAnimation;

class ConsoleDrawer : public QWidget {
public:
	explicit ConsoleDrawer(QWidget *view); // `view` is the game view the drawer slides over

	void Toggle();         // slide open/closed (no-op unless debugging is available)
	bool IsOpen() const { return open; }
	void Sync();           // once per frame: appends new output, closes if debugging went away
	void Reposition();     // the view was resized

protected:
	void paintEvent(QPaintEvent *e) override;
	bool eventFilter(QObject *watched, QEvent *e) override;

private:
	void Submit();
	void Slide(bool down);
	int DrawerHeight() const;

	QWidget *view;
	QPlainTextEdit *output;
	QLineEdit *input;
	QPropertyAnimation *slide;
	bool open = false;
	uint32_t shown_lines = 0;
};
