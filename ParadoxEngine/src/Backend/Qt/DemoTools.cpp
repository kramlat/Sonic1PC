#include "GameInterface.h"
#include "DemoTools.h"

#include "Settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QRegularExpression>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStatusBar>
#include <QFileInfo>
#include <stdlib.h>

extern "C" {
#include "../../Demo.h" // the recorder/player request API (a C header)
}

namespace DemoTools {

namespace {

const int kZoneCount = 7;
const char *const kZoneNames[kZoneCount] = {"Green Hill", "Labyrinth", "Marble", "Star Light", "Spring Yard", "Scrap Brain", "Ending"};
const int kZoneLabyrinth = 1, kZoneScrapBrain = 5, kZoneEnding = 6;

// Acts a zone has. Labyrinth has a 4th: Scrap Brain Act 3 (SBZ3) lives in the Labyrinth slot (LZ act 4).
int ActCount(int zone) {
	if (zone == kZoneEnding)
		return 2; // the two endings: 00 = bad (levels $600), 01 = good ($601)
	return zone == kZoneLabyrinth ? 4 : 3;
}

QString ActLabel(int zone, int act) {
	if (zone == kZoneLabyrinth && act == 3)
		return "Act 4 (Scrap Brain Act 3)";
	if (zone == kZoneScrapBrain && act == 2)
		return "Final Zone";
	if (zone == kZoneEnding)
		return act == 0 ? "Bad ending (00)" : "Good ending (01)"; // without / with all six emeralds
	return QString("Act %1").arg(act + 1);
}

QString LevelName(int zone, int act) {
	if (zone == kZoneLabyrinth && act == 3)
		return "Scrap Brain act 3";
	if (zone == kZoneScrapBrain && act == 2)
		return "Final Zone";
	if (zone == kZoneEnding)
		return act == 0 ? "bad ending" : "good ending";
	return QString("%1 act %2").arg(kZoneNames[qBound(0, zone, kZoneCount - 1)]).arg(act + 1);
}

// What the last recording was made from (for the status bar).
struct RecordInfo {
	DemoRecordRequest request;
	bool valid = false;
} g_last;

// A recording's file name carries its level, so a demo file stays a plain file and Play Demo can pick the
// level from the name: "Zone 3 Act 2 2026.10.01-021855.bin", or with a start override
// "Zone 3 Act 2 x1290 y0460 2026.10.01-021855.bin" (act is 1-3 in the name).
struct NameInfo {
	bool known = false;
	int zone = 0, act = 0, start_x = -1, start_y = -1;
};

NameInfo ParseDemoName(const QString &file) {
	NameInfo info;
	static const QRegularExpression re(R"(^Zone (\d+) Act (\d+)(?: x(\d+) y(\d+))? )");
	QRegularExpressionMatch m = re.match(QFileInfo(file).fileName());
	if (m.hasMatch()) {
		info.known = true;
		info.zone = qBound(0, m.captured(1).toInt(), kZoneCount - 1);
		info.act = qBound(0, m.captured(2).toInt() - 1, 3);
		if (!m.captured(3).isEmpty()) {
			info.start_x = m.captured(3).toInt();
			info.start_y = m.captured(4).toInt();
		}
	}
	return info;
}

// Zone / act / start position pickers shared by the two dialogs.
struct LevelPicker {
	QComboBox *zone = nullptr, *act = nullptr;
	QCheckBox *use_start = nullptr;
	QSpinBox *start_x = nullptr, *start_y = nullptr;

	void AddTo(QFormLayout *form) {
		zone = new QComboBox;
		for (int i = 0; i < kZoneCount; i++)
			zone->addItem(QString("%1 (%2)").arg(kZoneNames[i]).arg(i), i);
		act = new QComboBox;
		FillActs(0);
		form->addRow("Zone:", zone);
		form->addRow("Act:", act);
		// The act list depends on the zone (Labyrinth also offers Scrap Brain Act 3).
		QObject::connect(zone, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int) {
			int keep = act->currentIndex();
			FillActs(Zone());
			act->setCurrentIndex(qMin(keep, act->count() - 1));
		});

		use_start = new QCheckBox(QString("Override ") + game_info.player_name + "'s start position");
		start_x = new QSpinBox;
		start_x->setRange(0, 0x7FFF);
		start_y = new QSpinBox;
		start_y->setRange(0, 0x7FFF);
		auto *row = new QHBoxLayout;
		row->addWidget(new QLabel("X"));
		row->addWidget(start_x);
		row->addWidget(new QLabel("Y"));
		row->addWidget(start_y);
		form->addRow(use_start);
		form->addRow("", row);
		start_x->setEnabled(false);
		start_y->setEnabled(false);
		QObject::connect(use_start, &QCheckBox::toggled, [this](bool on) {
			start_x->setEnabled(on);
			start_y->setEnabled(on);
		});
	}

	void FillActs(int z) {
		act->clear();
		for (int i = 0; i < ActCount(z); i++)
			act->addItem(ActLabel(z, i), i);
	}

	void Set(int z, int a, int x, int y) {
		zone->setCurrentIndex(qBound(0, z, kZoneCount - 1)); // fills the act list for that zone
		act->setCurrentIndex(qBound(0, a, ActCount(Zone()) - 1));
		use_start->setChecked(x >= 0 && y >= 0);
		start_x->setValue(qMax(0, x));
		start_y->setValue(qMax(0, y));
	}
	int Zone() const { return zone->currentData().toInt(); }
	int Act() const { return act->currentData().toInt(); }
	int StartX() const { return use_start->isChecked() ? start_x->value() : -1; }
	int StartY() const { return use_start->isChecked() ? start_y->value() : -1; }
};

QString DefaultDemoName(bool special, int zone, int act, int stage, int start_x, int start_y) {
	QString stamp = QDateTime::currentDateTime().toString("yyyy.MM.dd-hhmmss");
	if (special)
		return QString("Special Stage %1 %2.bin").arg(stage).arg(stamp);
	QString start = (start_x >= 0 && start_y >= 0) ? QString(" x%1 y%2").arg(start_x).arg(start_y, 4, 10, QChar('0')) : QString();
	return QString("Zone %1 Act %2%3 %4.bin").arg(zone).arg(act + 1).arg(start).arg(stamp);
}

} // namespace

bool Recording() {
	return Demo_RecordingActive();
}

void RecordDialog(QWidget *parent) {
	if (Demo_RecordingActive()) {
		QMessageBox::information(parent, "Record Demo", "A recording is already running. Stop it from Tools > Stop Recording first.");
		return;
	}

	QDialog dialog(parent);
	dialog.setWindowTitle("Record Demo");
	auto *form = new QFormLayout(&dialog);

	auto *level_radio = new QRadioButton("Zone level");
	auto *special_radio = new QRadioButton("Special stage");
	level_radio->setChecked(true);
	auto *kind = new QHBoxLayout;
	kind->addWidget(level_radio);
	kind->addWidget(special_radio);
	form->addRow("Record:", kind);

	LevelPicker picker;
	picker.AddTo(form);
	picker.Set(Settings::Get().demo_zone, Settings::Get().demo_act, -1, -1);

	auto *stage = new QSpinBox;
	stage->setRange(1, 6);
	stage->setEnabled(false);
	form->addRow("Special stage:", stage);
	QObject::connect(special_radio, &QRadioButton::toggled, [&](bool on) {
		stage->setEnabled(on);
		picker.zone->setEnabled(!on);
		picker.act->setEnabled(!on);
	});

	auto *limit = new QCheckBox("Stop automatically after");
	auto *frames = new QSpinBox;
	frames->setRange(1, 65535);
	frames->setValue(3600);
	frames->setSuffix(" frames");
	frames->setEnabled(false);
	QObject::connect(limit, &QCheckBox::toggled, frames, &QWidget::setEnabled);
	auto *limit_row = new QHBoxLayout;
	limit_row->addWidget(limit);
	limit_row->addWidget(frames);
	form->addRow("", limit_row);

	auto *folder = new QLineEdit(Settings::DemoDir());
	auto *browse = new QPushButton("Browse...");
	QObject::connect(browse, &QPushButton::clicked, [&] {
		QString dir = QFileDialog::getExistingDirectory(&dialog, "Recordings folder", folder->text());
		if (!dir.isEmpty())
			folder->setText(dir);
	});
	auto *folder_row = new QHBoxLayout;
	folder_row->addWidget(folder, 1);
	folder_row->addWidget(browse);
	form->addRow("Save to:", folder_row);

	form->addRow(new QLabel("The level restarts and recording begins. Stop it from Tools > Stop Recording\n(Start still pauses the game)."));
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons->button(QDialogButtonBox::Ok)->setText("Start Recording");
	form->addRow(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

	if (dialog.exec() != QDialog::Accepted)
		return;

	DemoRecordRequest r = {};
	r.special = special_radio->isChecked();
	r.zone = picker.Zone();
	r.act = picker.Act();
	r.special_stage = stage->value() - 1;
	r.start_x = r.special ? -1 : picker.StartX();
	r.start_y = r.special ? -1 : picker.StartY();
	r.frames = limit->isChecked() ? frames->value() : -1;

	QString dir = folder->text().isEmpty() ? Settings::DemoDir() : folder->text();
	Settings::Get().demo_dir = dir;
	Settings::Get().demo_zone = r.zone;
	Settings::Get().demo_act = r.act;
	Settings::SaveSoon();
	QString path = dir + "/" + DefaultDemoName(r.special, r.zone, r.act, r.special_stage + 1, r.start_x, r.start_y);
	QByteArray bytes = path.toLocal8Bit();
	snprintf(r.path, sizeof(r.path), "%s", bytes.constData());

	g_last.request = r;
	g_last.valid = true;
	Demo_RequestRecording(&r);
}

void StopRecording() {
	Demo_StopRecording(); // Poll() notices it ended, writes the metadata and reports
}

void PlayDialog(QWidget *parent) {
	QString file = QFileDialog::getOpenFileName(parent, "Play demo", Settings::DemoDir(), "Demo files (*.bin);;All files (*)");
	if (file.isEmpty())
		return;
	QFile f(file);
	if (!f.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(parent, "Play Demo", "Could not read " + file);
		return;
	}
	QByteArray data = f.readAll();
	f.close();

	// The file has no level in it: take it from the file name the recorder gave it, if it still has one.
	NameInfo name = ParseDemoName(file);
	int zone = name.known ? name.zone : Settings::Get().demo_zone;
	int act = name.known ? name.act : Settings::Get().demo_act;
	int sx = name.start_x, sy = name.start_y;
	bool known = name.known;
	if (QFileInfo(file).fileName().startsWith("Special Stage")) {
		QMessageBox::information(parent, "Play Demo", "Special stage demos can't be played back yet.");
		return;
	}

	QDialog dialog(parent);
	dialog.setWindowTitle("Play Demo");
	auto *form = new QFormLayout(&dialog);
	form->addRow(new QLabel(QString("<b>%1</b> (%2 bytes)").arg(QFileInfo(file).fileName()).arg(data.size())));
	form->addRow(new QLabel(known ? "Level taken from the file name." : "The file name doesn't say which level this is: choose the level it was recorded in."));
	LevelPicker picker;
	picker.AddTo(form);
	picker.Set(zone, act, sx, sy);
	form->addRow(new QLabel("The demo plays as attract mode does, then returns to the Sega screen."));
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttons->button(QDialogButtonBox::Ok)->setText("Play");
	form->addRow(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted)
		return;

	DemoPlayRequest p = {picker.Zone(), picker.Act(), picker.StartX(), picker.StartY()};
	if (!Demo_RequestPlayback(&p, reinterpret_cast<const uint8_t *>(data.constData()), (size_t)data.size()))
		QMessageBox::warning(parent, "Play Demo", "That file isn't a usable demo (too small or too large).");
}

// Developer hooks (automated checks), filing the same requests the dialogs do:
//   SONIC_QT_RECORD=<zone>,<act>,<frames>,<file>   record (act 0-2) and stop after <frames>
//   SONIC_QT_PLAY=<file>,<zone>,<act>              play a demo file
static void RunEnvironmentHooks(int frame) {
	if (frame != 120)
		return; // let the game boot first
	if (const char *rec = getenv("SONIC_QT_RECORD")) {
		QStringList a = QString(rec).split(',');
		if (a.size() >= 4) {
			DemoRecordRequest r = {};
			r.zone = a[0].toInt();
			r.act = a[1].toInt();
			r.start_x = r.start_y = -1;
			r.frames = a[2].toInt();
			snprintf(r.path, sizeof(r.path), "%s", a[3].toLocal8Bit().constData());
			g_last.request = r;
			g_last.valid = true;
			Demo_RequestRecording(&r);
		}
	}
	if (const char *play = getenv("SONIC_QT_PLAY")) {
		QStringList a = QString(play).split(',');
		QFile f(a[0]);
		if (a.size() >= 3 && f.open(QIODevice::ReadOnly)) {
			QByteArray data = f.readAll();
			DemoPlayRequest p = {a[1].toInt(), a[2].toInt(), -1, -1};
			Demo_RequestPlayback(&p, reinterpret_cast<const uint8_t *>(data.constData()), (size_t)data.size());
		}
	}
}

void Poll(QMainWindow *window) {
	static int frame = 0;
	RunEnvironmentHooks(++frame);
	static bool was_recording = false;
	static int last_shown = -1;
	bool now = Demo_RecordingActive();

	if (now) {
		int frames = Demo_RecordedFrames();
		if (!was_recording || frames / 15 != last_shown) { // refresh ~4 times a second
			last_shown = frames / 15;
			QString where = g_last.valid ? (g_last.request.special ? QString("special stage %1").arg(g_last.request.special_stage + 1)
			                                                       : LevelName(g_last.request.zone, g_last.request.act))
			                             : QString("demo");
			window->statusBar()->showMessage(QString("● REC  %1  %2 frames (%3 s)  -  Tools > Stop Recording to save")
			                                     .arg(where).arg(frames).arg(frames / 60.0, 0, 'f', 1));
		}
	} else if (was_recording) {
		QString path = QString::fromLocal8Bit(Demo_LastSavedPath());
		if (!path.isEmpty()) {
			window->statusBar()->showMessage(QString("Demo saved: %1  (%2 frames)").arg(path).arg(Demo_LastSavedFrames()), 15000);
		} else {
			window->statusBar()->showMessage("Could not save the demo.", 15000);
		}
	}
	was_recording = now;
}

} // namespace DemoTools
