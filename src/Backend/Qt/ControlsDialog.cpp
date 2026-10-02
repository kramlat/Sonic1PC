#include "ControlsDialog.h"
#include "QtHost.h"
#include "Settings.h"
#include "../Controls.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const char *const kLabels[CTL_COUNT] = {"Up", "Down", "Left", "Right", "A", "B", "C", "Start"};

class Dialog : public QDialog {
public:
	explicit Dialog(QWidget *parent) : QDialog(parent) {
		setWindowTitle("Configure Sonic the Hedgehog");
		auto *root = new QVBoxLayout(this);
		root->addWidget(new QLabel("Click a button, then press the key (or gamepad button) to use. Escape cancels.\n"
		                           "The D-pad and left stick always move on a gamepad. F11 and the backtick key are reserved."));
		auto *grid = new QGridLayout;
		root->addLayout(grid);
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
		auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::RestoreDefaults);
		root->addWidget(buttons);
		connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
		connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
			Controls_Reset();
			EndCapture();
			Refresh();
			Settings::SaveSoon();
		});
		pad_timer = new QTimer(this);
		pad_timer->setInterval(30);
		connect(pad_timer, &QTimer::timeout, this, [this] { PollPad(); });
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
