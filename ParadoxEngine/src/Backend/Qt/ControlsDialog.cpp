#include "GameInterface.h"
#include "ControlsDialog.h"
#include "QtHost.h"
#include "Settings.h"
#include "../Controls.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const char *const kLabels[CTL_COUNT] = {"Up", "Down", "Left", "Right", "A", "B", "C", "Start"};

// Whether a stick at (x, y) is pushed far enough to count with this dead zone, as the game decides it (Input.c).
bool StickCounts(int x, int y, int percent, bool ring) {
	double dead = percent * 32767.0 / 100;
	if (ring)
		return (double)x * x + (double)y * y > dead * dead;
	return qAbs(x) > dead || qAbs(y) > dead;
}

// Shows where a stick is and what its dead zone covers: inside the shaded square (or circle, for a ring dead zone) the stick counts as
// being at rest.
class StickView : public QWidget {
public:
	StickView() { setMinimumSize(110, 110); }
	void SetValues(int x, int y, int dead_percent, bool ring_shape, bool connected) {
		sx = x;
		sy = y;
		dead = dead_percent;
		ring = ring_shape;
		on = connected;
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		int side = qMin(width(), height()) - 6;
		QRectF box((width() - side) / 2.0, (height() - side) / 2.0, side, side);
		p.setPen(palette().color(QPalette::Mid));
		p.setBrush(palette().color(QPalette::Base));
		p.drawEllipse(box);
		double r = side / 2.0 * dead / 100.0; // the dead zone, as a share of the travel
		QRectF zone(box.center().x() - r, box.center().y() - r, 2 * r, 2 * r);
		p.setBrush(QColor(200, 70, 70, 90));
		p.setPen(Qt::NoPen);
		if (ring)
			p.drawEllipse(zone);
		else
			p.drawRect(zone);
		p.setPen(palette().color(QPalette::Mid));
		p.drawLine(QPointF(box.left(), box.center().y()), QPointF(box.right(), box.center().y()));
		p.drawLine(QPointF(box.center().x(), box.top()), QPointF(box.center().x(), box.bottom()));
		if (!on)
			return;
		QPointF dot(box.center().x() + sx / 32768.0 * side / 2.0, box.center().y() + sy / 32768.0 * side / 2.0);
		bool counts = StickCounts(sx, sy, dead, ring);
		p.setBrush(counts ? QColor(40, 160, 70) : QColor(120, 120, 120));
		p.setPen(Qt::NoPen);
		p.drawEllipse(dot, 5, 5);
	}

private:
	int sx = 0, sy = 0, dead = 0;
	bool ring = false, on = false;
};

class Dialog : public QDialog {
public:
	explicit Dialog(QWidget *parent) : QDialog(parent) {
		setWindowTitle(QString("Configure ") + game_info.game_title);
		auto *root = new QVBoxLayout(this);
		auto *tabs = new QTabWidget;
		root->addWidget(tabs);
		auto *page = new QWidget;
		auto *page_layout = new QVBoxLayout(page);
		tabs->addTab(page, "Buttons");
		page_layout->addWidget(new QLabel("Click a button, then press the key (or gamepad button) to use. Escape cancels.\n"
		                           "The D-pad and left stick always move on a gamepad. F11 and the backtick key are reserved."));
		auto *grid = new QGridLayout;
		page_layout->addLayout(grid);
		grid->addWidget(new QLabel("<b>Mega Drive pad</b>"), 0, 0);
		grid->addWidget(new QLabel("<b>Keyboard</b>"), 0, 1);
		grid->addWidget(new QLabel("<b>Alternate key</b>"), 0, 2);
		grid->addWidget(new QLabel("<b>Gamepad</b>"), 0, 3);
		for (int i = 0; i < CTL_COUNT; i++) {
			grid->addWidget(new QLabel(kLabels[i]), i + 1, 0);
			for (int slot = 0; slot < 2; slot++) {
				key_buttons[i][slot] = new QPushButton;
				grid->addWidget(key_buttons[i][slot], i + 1, slot + 1);
				connect(key_buttons[i][slot], &QPushButton::clicked, this, [this, i, slot] { Capture(i, slot); });
			}
			if (i >= CTL_A) {
				pad_buttons[i] = new QPushButton;
				grid->addWidget(pad_buttons[i], i + 1, 3);
				connect(pad_buttons[i], &QPushButton::clicked, this, [this, i] { Capture(i, 2); });
			} else {
				grid->addWidget(new QLabel("D-pad / left stick"), i + 1, 3);
			}
		}
		BuildGamepadTab(tabs);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::RestoreDefaults);
		root->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
		connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
			Controls_Reset();
			EndCapture();
			Refresh();
			SyncSliders();
			Settings::SaveSoon();
		});
		pad_timer = new QTimer(this);
		pad_timer->setInterval(30);
		connect(pad_timer, &QTimer::timeout, this, [this] { PollPad(); });
		axis_timer = new QTimer(this); // the live stick views
		axis_timer->setInterval(30);
		connect(axis_timer, &QTimer::timeout, this, [this] { PollAxes(); });
		axis_timer->start();
		Refresh();
	}

protected:
	void keyPressEvent(QKeyEvent *e) override {
		if (cap_ctl < 0) {
			QDialog::keyPressEvent(e);
			return;
		}
		if (e->isAutoRepeat())
			return;
		if (e->key() == Qt::Key_Escape) {
			EndCapture();
			Refresh();
			return;
		}
		if (cap_slot > 1)
			return; // waiting for a gamepad button
		int sc = QtHost_ScancodeFor(e->key());
		if (sc == 0 || e->key() == Qt::Key_F11 || e->key() == Qt::Key_QuoteLeft)
			return;
		for (int i = 0; i < CTL_COUNT; i++) // a key drives one button only
			for (int s = 0; s < 2; s++)
				if (Controls_Key[i][s] == sc)
					Controls_Key[i][s] = 0;
		Controls_Key[cap_ctl][cap_slot] = sc;
		Finish();
	}

private:
	// The Gamepad tab: how far each stick must move to count, with a live view of the sticks to set it by.
	void BuildGamepadTab(QTabWidget *tabs) {
		auto *page = new QWidget;
		auto *layout = new QVBoxLayout(page);
		layout->addWidget(new QLabel(QString("A worn stick that drifts can make %1 creep along on their own. Raise a dead zone until the dot\n"
		                             "stays inside the shaded square while the stick rests: the stick then has to move further than\n"
		                             "that to count. (This does what Steam Input's dead zone setting would, without needing it.)").arg(game_info.player_name)));
		auto *grid = new QGridLayout;
		layout->addLayout(grid);
		const QString names[2] = {QString("Left stick (moves %1)").arg(game_info.player_name), "Right stick (debug tools only)"};
		int *values[2] = {&Controls_DeadzoneLeft, &Controls_DeadzoneRight};
		for (int s = 0; s < 2; s++) {
			views[s] = new StickView;
			sliders[s] = new QSlider(Qt::Horizontal);
			sliders[s]->setRange(0, CONTROLS_DEADZONE_MAX);
			value_labels[s] = new QLabel;
			grid->addWidget(new QLabel(QString("<b>%1</b>").arg(names[s])), 0, s * 2, 1, 2);
			grid->addWidget(views[s], 1, s * 2, 1, 2);
			grid->addWidget(sliders[s], 2, s * 2);
			grid->addWidget(value_labels[s], 2, s * 2 + 1);
			pos_labels[s] = new QLabel;
			pos_labels[s]->setAlignment(Qt::AlignCenter);
			grid->addWidget(pos_labels[s], 3, s * 2, 1, 2);
			connect(sliders[s], &QSlider::valueChanged, this, [this, s, values](int v) {
				*values[s] = v;
				value_labels[s]->setText(QString("%1%").arg(v));
				Settings::SaveSoon();
			});
		}
		auto *shape = new QHBoxLayout;
		shape->addWidget(new QLabel("Dead zone shape:"));
		box_radio = new QRadioButton("Box");
		ring_radio = new QRadioButton("Ring");
		box_radio->setToolTip("Each direction is judged on its own: the stick has to pass the line on that axis.");
		ring_radio->setToolTip("The stick's overall push is judged: drift that sits a little off to one side stays ignored, whichever way.");
		shape->addWidget(box_radio);
		shape->addWidget(ring_radio);
		shape->addStretch();
		layout->addLayout(shape);
		connect(ring_radio, &QRadioButton::toggled, this, [](bool on) {
			Controls_DeadzoneRing = on ? 1 : 0;
			Settings::SaveSoon();
		});
		no_pad_label = new QLabel("No gamepad found: connect one to see its sticks here.");
		layout->addWidget(no_pad_label);
		layout->addStretch();
		tabs->addTab(page, "Gamepad");
		SyncSliders();
	}

	void SyncSliders() {
		QSignalBlocker block_box(box_radio), block_ring(ring_radio);
		box_radio->setChecked(!Controls_DeadzoneRing);
		ring_radio->setChecked(Controls_DeadzoneRing != 0);
		int *values[2] = {&Controls_DeadzoneLeft, &Controls_DeadzoneRight};
		for (int s = 0; s < 2; s++) {
			QSignalBlocker block(sliders[s]);
			sliders[s]->setValue(*values[s]);
			value_labels[s]->setText(QString("%1%").arg(*values[s]));
		}
	}

	// The stick's position as a percentage of its travel, and whether that is inside the dead zone (so it does not count).
	void SetReadout(int s, int x, int y, int dead, bool on) {
		if (!on) {
			pos_labels[s]->setText("-");
			return;
		}
		int px = qRound(x * 100.0 / 32767), py = qRound(y * 100.0 / 32767);
		bool counts = StickCounts(x, y, dead, Controls_DeadzoneRing != 0);
		pos_labels[s]->setText(QString("Now: across %1%, down %2%\n%3")
		                           .arg(px).arg(py).arg(counts ? "counts as a push" : "at rest (inside the dead zone)"));
	}

	void PollAxes() {
		int lx = 0, ly = 0, rx = 0, ry = 0;
		bool on = Controls_PollAxes(&lx, &ly, &rx, &ry);
		views[0]->SetValues(lx, ly, Controls_DeadzoneLeft, Controls_DeadzoneRing != 0, on);
		views[1]->SetValues(rx, ry, Controls_DeadzoneRight, Controls_DeadzoneRing != 0, on);
		SetReadout(0, lx, ly, Controls_DeadzoneLeft, on);
		SetReadout(1, rx, ry, Controls_DeadzoneRight, on);
		no_pad_label->setVisible(!on);
	}

	void Capture(int ctl, int slot) {
		EndCapture();
		cap_ctl = ctl;
		cap_slot = slot;
		QPushButton *b = slot > 1 ? pad_buttons[ctl] : key_buttons[ctl][slot];
		b->setText(slot > 1 ? "Press a pad button..." : "Press a key...");
		grabKeyboard();
		if (slot > 1) {
			pad_wait_release = true;
			pad_timer->start();
		}
	}

	void PollPad() {
		int b = Controls_PollPadButton();
		if (pad_wait_release) {
			pad_wait_release = b >= 0;
			return;
		}
		if (b < 0)
			return;
		Controls_Pad[cap_ctl] = b;
		Finish();
	}

	void Finish() {
		EndCapture();
		Refresh();
		Settings::SaveSoon();
	}

	void EndCapture() {
		cap_ctl = -1;
		pad_timer->stop();
		releaseKeyboard();
	}

	void Refresh() {
		for (int i = 0; i < CTL_COUNT; i++) {
			for (int s = 0; s < 2; s++) {
				const char *n = Controls_KeyName(Controls_Key[i][s]);
				key_buttons[i][s]->setText(*n ? n : "-");
			}
			if (pad_buttons[i]) {
				const char *n = Controls_PadButtonName(Controls_Pad[i]);
				pad_buttons[i]->setText(*n ? QString(n).left(1).toUpper() + QString(n).mid(1) : "-");
			}
		}
	}

	QPushButton *key_buttons[CTL_COUNT][2] = {};
	QPushButton *pad_buttons[CTL_COUNT] = {};
	QTimer *pad_timer;
	QTimer *axis_timer = nullptr;
	StickView *views[2] = {};
	QSlider *sliders[2] = {};
	QLabel *value_labels[2] = {};
	QLabel *pos_labels[2] = {};
	QRadioButton *box_radio = nullptr, *ring_radio = nullptr;
	QLabel *no_pad_label = nullptr;
	int cap_ctl = -1, cap_slot = 0;
	bool pad_wait_release = false;
};

} // namespace

namespace ControlsDialog {

void Show(QWidget *parent) {
	Dialog dialog(parent);
	dialog.exec();
}

} // namespace ControlsDialog
