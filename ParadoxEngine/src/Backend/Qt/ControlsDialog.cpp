#include "GameInterface.h"
#include "ControlsDialog.h"
#include "QtHost.h"
#include "Settings.h"
#include "../Controls.h"

#include <QComboBox>
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
		BuildButtonsPage(tabs, 0);
		if (Players() > 1)
			BuildButtonsPage(tabs, 1);
		BuildGamepadTab(tabs);
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::RestoreDefaults);
		root->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
		connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
			Controls_Reset();
			Controls_ChoosePad(0, -1); // (every gamepad back to automatic)
			Controls_ChoosePad(1, -1);
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
		for (int pl = 0; pl < Players(); pl++) // a key drives one button only, of either player
			for (int i = 0; i < CTL_COUNT; i++)
				for (int s = 0; s < 2; s++)
					if (*Controls_KeySlot(pl, i, s) == sc)
						*Controls_KeySlot(pl, i, s) = 0;
		*Controls_KeySlot(cap_player, cap_ctl, cap_slot) = sc;
		Finish();
	}

private:
	// A game with a two-player mode (Sonic 2 and later) has a second set of controls
	static int Players() { return game_info.split_screen ? 2 : 1; }

	// The Buttons tab of a player: his keys and gamepad buttons
	void BuildButtonsPage(QTabWidget *tabs, int player) {
		auto *page = new QWidget;
		auto *page_layout = new QVBoxLayout(page);
		tabs->addTab(page, player == 0 ? (Players() > 1 ? "Player 1" : "Buttons") : "Player 2");
		QString who = player == 0 ? QString(game_info.player_name) : QString("the second player");
		page_layout->addWidget(new QLabel(QString("Click a button, then press the key (or gamepad button) to use. Escape cancels.\n"
		                           "The D-pad and left stick always move on a gamepad. F11 and the backtick key are reserved.")
		                           + (Players() > 1 ? QString("\nThese are %1's. The second player's gamepad is the second one connected.").arg(who) : QString())));
		auto *grid = new QGridLayout;
		page_layout->addLayout(grid);
		grid->addWidget(new QLabel("<b>Mega Drive pad</b>"), 0, 0);
		grid->addWidget(new QLabel("<b>Keyboard</b>"), 0, 1);
		grid->addWidget(new QLabel("<b>Alternate key</b>"), 0, 2);
		grid->addWidget(new QLabel("<b>Gamepad</b>"), 0, 3);
		for (int i = 0; i < CTL_COUNT; i++) {
			grid->addWidget(new QLabel(kLabels[i]), i + 1, 0);
			for (int slot = 0; slot < 2; slot++) {
				key_buttons[player][i][slot] = new QPushButton;
				grid->addWidget(key_buttons[player][i][slot], i + 1, slot + 1);
				connect(key_buttons[player][i][slot], &QPushButton::clicked, this, [this, player, i, slot] { Capture(player, i, slot); });
			}
			if (i >= CTL_A) {
				pad_buttons[player][i] = new QPushButton;
				grid->addWidget(pad_buttons[player][i], i + 1, 3);
				connect(pad_buttons[player][i], &QPushButton::clicked, this, [this, player, i] { Capture(player, i, 2); });
			} else {
				grid->addWidget(new QLabel("D-pad / left stick"), i + 1, 3);
			}
		}
	}

	// The Gamepad tab: which gamepad is which player's, and how far each stick must move to count, with a live view of the sticks to set it by.
	// Sticks: 0 the first player's left stick, 1 his right stick (the debug tools'), 2 the second player's left stick (a game with a two-player mode).
	void BuildGamepadTab(QTabWidget *tabs) {
		auto *page = new QWidget;
		auto *layout = new QVBoxLayout(page);

		if (Players() > 1) {
			auto *who = new QGridLayout;
			for (int pl = 0; pl < 2; pl++) {
				pad_choice[pl] = new QComboBox;
				who->addWidget(new QLabel(pl == 0 ? "Player 1's gamepad:" : "Player 2's gamepad:"), pl, 0);
				who->addWidget(pad_choice[pl], pl, 1);
				connect(pad_choice[pl], QOverload<int>::of(&QComboBox::activated), this, [this, pl](int index) {
					Controls_ChoosePad(pl, pad_choice[pl]->itemData(index).toInt()); // (-1 is automatic)
					Settings::SaveSoon();
					FillPadChoices();
				});
			}
			who->setColumnStretch(1, 1);
			layout->addLayout(who);
			FillPadChoices();
		}

		layout->addWidget(new QLabel(QString("A worn stick that drifts can make %1 creep along on their own. Raise a dead zone until the dot\n"
		                             "stays inside the shaded square while the stick rests: the stick then has to move further than\n"
		                             "that to count. (This does what Steam Input's dead zone setting would, without needing it.)").arg(game_info.player_name)));
		auto *grid = new QGridLayout;
		layout->addLayout(grid);
		const QString names[3] = {Players() > 1 ? QString("Player 1's left stick (moves %1)").arg(game_info.player_name)
		                                       : QString("Left stick (moves %1)").arg(game_info.player_name),
		                          "Right stick (debug tools only)", "Player 2's left stick"};
		for (int s = 0; s < Sticks(); s++) {
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
			connect(sliders[s], &QSlider::valueChanged, this, [this, s](int v) {
				*Value(s) = v;
				value_labels[s]->setText(QString("%1%").arg(v));
				Settings::SaveSoon();
			});
		}
		for (int pl = 0; pl < Players(); pl++) { // each player's dead zone has a shape of its own
			auto *shape = new QHBoxLayout;
			shape->addWidget(new QLabel(Players() > 1 ? QString("Player %1's dead zone shape:").arg(pl + 1) : QString("Dead zone shape:")));
			box_radio[pl] = new QRadioButton("Box");
			ring_radio[pl] = new QRadioButton("Ring");
			box_radio[pl]->setToolTip("Each direction is judged on its own: the stick has to pass the line on that axis.");
			ring_radio[pl]->setToolTip("The stick's overall push is judged: drift that sits a little off to one side stays ignored, whichever way.");
			shape->addWidget(box_radio[pl]);
			shape->addWidget(ring_radio[pl]);
			shape->addStretch();
			layout->addLayout(shape);
			connect(ring_radio[pl], &QRadioButton::toggled, this, [pl](bool on) {
				(pl ? Controls_DeadzoneRing2 : Controls_DeadzoneRing) = on ? 1 : 0;
				Settings::SaveSoon();
			});
		}
		no_pad_label = new QLabel("No gamepad found: connect one to see its sticks here.");
		layout->addWidget(no_pad_label);
		layout->addStretch();
		tabs->addTab(page, "Gamepad");
		SyncSliders();
	}

	int Sticks() const { return Players() > 1 ? 3 : 2; }
	static int *Value(int stick) {
		static int *values[3] = {&Controls_DeadzoneLeft, &Controls_DeadzoneRight, &Controls_DeadzoneLeft2};
		return values[stick];
	}
	static bool Ring(int stick) { return (stick == 2 ? Controls_DeadzoneRing2 : Controls_DeadzoneRing) != 0; }

	// The gamepads that are connected, for each player to pick from ("Automatic" is the first one that is not taken)
	void FillPadChoices() {
		int count = Controls_PadDeviceCount();
		for (int pl = 0; pl < 2; pl++) {
			if (pad_choice[pl] == nullptr)
				continue;
			QSignalBlocker block(pad_choice[pl]);
			pad_choice[pl]->clear();
			QString now = Controls_PadAssigned(pl) >= 0 ? QString(Controls_PadDeviceName(Controls_PadAssigned(pl))) : QString("none connected");
			pad_choice[pl]->addItem(Controls_PadGuid[pl][0] ? QString("(chosen pad is not connected)") : QString("Automatic (%1)").arg(now), -1);
			for (int d = 0; d < count; d++)
				pad_choice[pl]->addItem(QString("%1: %2").arg(d + 1).arg(Controls_PadDeviceName(d)), d);
			int assigned = Controls_PadGuid[pl][0] ? Controls_PadAssigned(pl) : -1;
			pad_choice[pl]->setCurrentIndex(assigned >= 0 ? pad_choice[pl]->findData(assigned) : 0);
		}
		known_pads = count;
	}

	void SyncSliders() {
		for (int pl = 0; pl < Players(); pl++) {
			QSignalBlocker block_box(box_radio[pl]), block_ring(ring_radio[pl]);
			box_radio[pl]->setChecked(!(pl ? Controls_DeadzoneRing2 : Controls_DeadzoneRing));
			ring_radio[pl]->setChecked((pl ? Controls_DeadzoneRing2 : Controls_DeadzoneRing) != 0);
		}
		for (int s = 0; s < Sticks(); s++) {
			QSignalBlocker block(sliders[s]);
			sliders[s]->setValue(*Value(s));
			value_labels[s]->setText(QString("%1%").arg(*Value(s)));
		}
		if (pad_choice[0] != nullptr)
			FillPadChoices();
	}

	// The stick's position as a percentage of its travel, and whether that is inside the dead zone (so it does not count).
	void SetReadout(int s, int x, int y, int dead, bool on) {
		if (!on) {
			pos_labels[s]->setText("-");
			return;
		}
		int px = qRound(x * 100.0 / 32767), py = qRound(y * 100.0 / 32767);
		bool counts = StickCounts(x, y, dead, Ring(s));
		pos_labels[s]->setText(QString("Now: across %1%, down %2%\n%3")
		                           .arg(px).arg(py).arg(counts ? "counts as a push" : "at rest (inside the dead zone)"));
	}

	void PollAxes() {
		if (pad_choice[0] != nullptr && Controls_PadDeviceCount() != known_pads)
			FillPadChoices(); // a gamepad came or went
		int lx = 0, ly = 0, rx = 0, ry = 0;
		bool on = Controls_PollAxes(0, &lx, &ly, &rx, &ry);
		views[0]->SetValues(lx, ly, Controls_DeadzoneLeft, Controls_DeadzoneRing != 0, on);
		views[1]->SetValues(rx, ry, Controls_DeadzoneRight, Controls_DeadzoneRing != 0, on);
		SetReadout(0, lx, ly, Controls_DeadzoneLeft, on);
		SetReadout(1, rx, ry, Controls_DeadzoneRight, on);
		bool any = on;
		if (Players() > 1) {
			int l2x = 0, l2y = 0, r2x = 0, r2y = 0;
			bool on2 = Controls_PollAxes(1, &l2x, &l2y, &r2x, &r2y);
			views[2]->SetValues(l2x, l2y, Controls_DeadzoneLeft2, Controls_DeadzoneRing2 != 0, on2);
			SetReadout(2, l2x, l2y, Controls_DeadzoneLeft2, on2);
			any = any || on2;
		}
		no_pad_label->setVisible(!any);
	}

	void Capture(int player, int ctl, int slot) {
		EndCapture();
		cap_player = player;
		cap_ctl = ctl;
		cap_slot = slot;
		QPushButton *b = slot > 1 ? pad_buttons[player][ctl] : key_buttons[player][ctl][slot];
		b->setText(slot > 1 ? "Press a pad button..." : "Press a key...");
		grabKeyboard();
		if (slot > 1) {
			pad_wait_release = true;
			pad_timer->start();
		}
	}

	void PollPad() {
		int b = Controls_PollPadButton(cap_player);
		if (pad_wait_release) {
			pad_wait_release = b >= 0;
			return;
		}
		if (b < 0)
			return;
		*Controls_PadSlot(cap_player, cap_ctl) = b;
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
		for (int pl = 0; pl < Players(); pl++)
			for (int i = 0; i < CTL_COUNT; i++) {
				for (int s = 0; s < 2; s++) {
					const char *n = Controls_KeyName(*Controls_KeySlot(pl, i, s));
					key_buttons[pl][i][s]->setText(*n ? n : "-");
				}
				if (pad_buttons[pl][i]) {
					const char *n = Controls_PadButtonName(*Controls_PadSlot(pl, i));
					pad_buttons[pl][i]->setText(*n ? QString(n).left(1).toUpper() + QString(n).mid(1) : "-");
				}
			}
	}

	QPushButton *key_buttons[2][CTL_COUNT][2] = {};
	QPushButton *pad_buttons[2][CTL_COUNT] = {};
	QTimer *pad_timer;
	QTimer *axis_timer = nullptr;
	StickView *views[3] = {};
	QSlider *sliders[3] = {};
	QLabel *value_labels[3] = {};
	QLabel *pos_labels[3] = {};
	QRadioButton *box_radio[2] = {}, *ring_radio[2] = {};
	QComboBox *pad_choice[2] = {};
	int known_pads = -1;
	QLabel *no_pad_label = nullptr;
	int cap_player = 0, cap_ctl = -1, cap_slot = 0;
	bool pad_wait_release = false;
};

} // namespace

namespace ControlsDialog {

void Show(QWidget *parent) {
	Dialog dialog(parent);
	dialog.exec();
}

} // namespace ControlsDialog
