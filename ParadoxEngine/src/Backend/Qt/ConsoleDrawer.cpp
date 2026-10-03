#include "ConsoleDrawer.h"

extern "C" {
#include "../../Console.h"
}
#include "../../DebugPeek.h"

#include <QEasingCurve>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPropertyAnimation>
#include <QVBoxLayout>

ConsoleDrawer::ConsoleDrawer(QWidget *view_) : QWidget(view_), view(view_) {
	setAutoFillBackground(false);
	setAttribute(Qt::WA_StyledBackground, false);

	const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);

	output = new QPlainTextEdit(this);
	output->setReadOnly(true);
	output->setFrameShape(QFrame::NoFrame);
	output->setMaximumBlockCount(1000);
	output->setFont(mono);
	output->setFocusPolicy(Qt::NoFocus);

	QLabel *prompt = new QLabel(">", this);
	prompt->setFont(mono);

	input = new QLineEdit(this);
	input->setFont(mono);
	input->setFrame(false);
	input->installEventFilter(this); // backtick, Escape and the history keys
	input->setPlaceholderText("type 'help'");

	// Transparent widgets over the painted translucent panel, light text, a green prompt.
	setStyleSheet("QPlainTextEdit, QLineEdit { background: transparent; color: #d8e0e6; selection-background-color: #3b5b7a; }"
	              "QLabel { color: #7dff7d; background: transparent; }");

	QHBoxLayout *line = new QHBoxLayout;
	line->setContentsMargins(0, 0, 0, 0);
	line->setSpacing(6);
	line->addWidget(prompt);
	line->addWidget(input, 1);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(10, 6, 10, 8);
	layout->setSpacing(4);
	layout->addWidget(output, 1);
	layout->addLayout(line);

	slide = new QPropertyAnimation(this, "pos", this);
	slide->setDuration(180);
	slide->setEasingCurve(QEasingCurve::OutCubic);

	hide();
}

int ConsoleDrawer::DrawerHeight() const {
	return qMax(160, view->height() * 45 / 100);
}

void ConsoleDrawer::Reposition() {
	resize(view->width(), DrawerHeight());
	if (open && slide->state() != QAbstractAnimation::Running)
		move(0, 0);
	else if (!open)
		move(0, -height());
}

void ConsoleDrawer::paintEvent(QPaintEvent *) {
	QPainter p(this);
	p.fillRect(rect(), QColor(8, 10, 16, 225));
	p.fillRect(QRect(0, height() - 2, width(), 2), QColor(125, 255, 125, 200)); // the drawer's lower edge
}

void ConsoleDrawer::Slide(bool down) {
	slide->stop();
	slide->disconnect();
	slide->setStartValue(pos());
	slide->setEndValue(QPoint(0, down ? 0 : -height()));
	if (!down)
		QObject::connect(slide, &QPropertyAnimation::finished, this, [this] {
			if (!open)
				hide();
		});
	slide->start();
}

void ConsoleDrawer::Toggle() {
	if (!open && Peek_DebugToolsAvailable() == 0)
		return; // only while debugging is available
	open = !open;
	Console_Toggle(); // the game side: freezes gameplay and music while it's open
	if (open) {
		resize(view->width(), DrawerHeight());
		if (!isVisible())
			move(0, -height());
		show();
		raise();
		Slide(true);
		input->setFocus();
	} else {
		Slide(false);
		view->setFocus();
	}
}

void ConsoleDrawer::Sync() {
	// Output the game side logged since last frame (its ring only keeps the newest few lines).
	uint32_t total = Console_LineCount();
	uint32_t fresh = total - shown_lines;
	if (fresh > CONSOLE_LOG_LINES)
		fresh = CONSOLE_LOG_LINES;
	for (int i = (int)fresh - 1; i >= 0; i--)
		if (const char *text = Console_GetLogLine(i))
			output->appendPlainText(QString::fromLocal8Bit(text));
	shown_lines = total;

	if (open && Peek_DebugToolsAvailable() == 0)
		Toggle(); // debug mode was turned off underneath it
}

void ConsoleDrawer::Submit() {
	Console_SetInput(input->text().toLocal8Bit().constData());
	Console_HandleKey(ConsoleKey_Enter); // runs it; the echo and any output arrive through Sync()
	input->clear();
}

bool ConsoleDrawer::eventFilter(QObject *watched, QEvent *e) {
	if (watched == input && e->type() == QEvent::KeyPress) {
		QKeyEvent *k = static_cast<QKeyEvent *>(e);
		switch (k->key()) {
		case Qt::Key_QuoteLeft:
		case Qt::Key_Escape:
			Toggle();
			return true;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			Submit();
			return true;
		case Qt::Key_Up:
		case Qt::Key_Down:
			Console_HandleKey(k->key() == Qt::Key_Up ? ConsoleKey_Up : ConsoleKey_Down);
			input->setText(QString::fromLocal8Bit(Console_GetInputLine()));
			return true;
		default:
			break;
		}
	}
	return QWidget::eventFilter(watched, e);
}
