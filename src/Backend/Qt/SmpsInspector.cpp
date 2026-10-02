#include "SmpsInspector.h"

#include "../../DebugPeek.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <string.h>

extern "C" {
#include "../../Console.h"
#include "../SoundInspect.h"
}

namespace {

const int kFlashFrames = 6; // how long a struck note / hit drum stays lit, in ticks of the 60 Hz timer
const int kNoteCount = 96;   // 8 octaves, as Sound.c's note_index

// Row order and what each reads from the snapshot (Sound.h's channel layout: PSG 0-3, FM 4-9).
struct Row {
	const char *label;
	int channel;
	bool fm;
};
const Row kRows[] = {
	{"FM1", 4, true}, {"FM2", 5, true}, {"FM3", 6, true}, {"FM4", 7, true}, {"FM5", 8, true}, {"FM6", 9, true},
	{"PSG1", 0, false}, {"PSG2", 1, false}, {"PSG3", 2, false}, {"NOISE", 3, false},
};
const int kRowCount = int(sizeof(kRows) / sizeof(kRows[0]));

// The whole drum kit: row N is DAC note $81+N, as Sound.c's dac_notes[] (ParadoxComposer keeps a copy of this list).
const char *const kDrumNames[SOUND_INSPECT_DRUMS] = {
	"Kick", "Snare", "Timpani", "Scratch", "Clap", "Tom", "Bongo", "HiTimpani", "MidTimpani", "LowTimpani",
	"FloorTimpani", "MidTom", "LowTom", "FloorTom", "HiBongo", "MidBongo", "LowBongo", "S3Snare", "S3HiTom", "S3MidTom",
	"S3LowTom", "S3FloorTom", "S3Kick", "MuffledSnare", "Crash", "Ride", "LowMetalHit", "FloorMetalHit", "HighMetalHit",
	"HigherMetalHit", "MidMetalHit", "S3Clap", "ElectricHiTom", "ElectricMidTom", "ElectricLowTom", "ElectricFloorTom",
	"TightSnare", "MidPitchedSnare", "LooseSnare", "LooserSnare", "S3HiTimpani", "S3LowTimpani", "S3MidTimpani",
	"QuickLooseSnare", "Click", "PowerKick", "QuickGlassCrash", "GlassCrashSnare", "GlassCrash", "GlassCrashKick",
	"QuietGlassCrash", "OddSnareKick", "Claves", "DanceSnare", "LooseKick", "HandDrum", "PowerTom", "HiWoodBlock",
	"LowWoodBlock", "HiConga", "HiConga2", "GavelHitDrum", "GavelHitDrum2", "GunshotHitDrum", "GunshotHitDrum2",
	"HitDrum3A", "HitDrum3B", "HitDrum3C", "HitDrum3D", "HitDrum3E", "MetalCrashHit", "EchoClap", "LowEchoClap",
	"HipHopKick", "HipHopKickLow", "DanceKick", "HipHopKick2", "HipHopKick3", "DeepHit", "WoodBlock3", "Unused",
	"ReverseCymbal", "PsytranceKick", "PsytranceKickHi", "PsytranceKickLow", "PsytranceKickFloor", "PsytranceSnare",
	"PsytranceSnareHi", "PsytranceSnareLow", "PsytranceSnareFloor", "Cowbell", "Rimshot", "Cuica", "Guiro", "SEGA",
};

const QColor kLit(220, 60, 60), kWhiteKey(0xEE, 0xEE, 0xEE), kBlackKey(0xAA, 0xAA, 0xAA);

// The channel strips: a piano keyboard per tone channel, 96 squares for a noise track (its note picks the character,
// like the pieces of a drum kit).
class Strips : public QWidget {
public:
	explicit Strips(QWidget *parent) : QWidget(parent) {
		setMinimumSize(640, 260);
		memset(flash, 0, sizeof(flash));
		memset(note, 0, sizeof(note));
	}

	void Feed(const SoundInspectData &d) {
		data = d;
		for (int r = 0; r < kRowCount; r++) {
			int ch = kRows[r].channel;
			if (flash[r] > 0)
				flash[r]--;
			if (d.keyed[ch]) {
				flash[r] = kFlashFrames;
				note[r] = d.note[ch];
			}
		}
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.fillRect(rect(), QColor(0, 0, 0));

		QVector<int> visible;
		for (int r = 0; r < kRowCount; r++)
			if (Visible(r))
				visible << r;
		if (visible.isEmpty()) {
			p.setPen(QColor(150, 150, 160));
			p.drawText(rect(), Qt::AlignCenter, "No song playing");
			return;
		}

		const int label_w = 64, margin = 8;
		const int row_h = (height() - 2 * margin) / visible.size();
		const int strip_x = label_w + margin, strip_w = width() - strip_x - margin;
		QFont label_font = font();
		label_font.setBold(true);
		p.setFont(label_font);
		for (int vi = 0; vi < visible.size(); vi++) {
			const int r = visible[vi], y = margin + vi * row_h;
			p.fillRect(QRect(margin, y + 2, label_w - margin, row_h - 4), QColor(60, 60, 70));
			p.setPen(QColor(230, 230, 235));
			p.drawText(QRect(margin, y + 2, label_w - margin, row_h - 4), Qt::AlignCenter, kRows[r].label);
			const int lit = flash[r] > 0 ? note[r] : -1;
			if (data.noise[kRows[r].channel])
				PaintSquares(p, strip_x, y, strip_w, row_h, lit);
			else
				PaintPiano(p, strip_x, y, strip_w, row_h, lit);
		}
	}

private:
	// Rows the song's header actually allocates (Sound.c's LoadMusic): the DAC takes the first FM block.
	bool Visible(int r) const {
		const int ch = kRows[r].channel;
		if (data.music_id == 0)
			return false;
		if (kRows[r].fm) {
			int real_fm = data.fm_count > 0 ? data.fm_count - 1 : 0;
			return (ch - 4) < real_fm;
		}
		if (ch == 3)
			return data.psg_count == 4;
		return ch < data.psg_count;
	}

	static void PaintPiano(QPainter &p, int x, int y, int w, int h, int lit) {
		static const int white_chromatic[7] = {0, 2, 4, 5, 7, 9, 11};
		static const int black_chromatic[5] = {1, 3, 6, 8, 10};
		static const int black_before_white[5] = {0, 1, 3, 4, 5};
		const int white_count = kNoteCount / 12 * 7;
		const int white_w = w / white_count;
		const int black_w = white_w * 6 / 10, black_h = h * 6 / 10;
		for (int k = 0; k < white_count; k++) {
			int n = k / 7 * 12 + white_chromatic[k % 7];
			p.fillRect(QRect(x + k * white_w + 1, y + 2, white_w - 1, h - 4), n == lit ? kLit : kWhiteKey);
		}
		for (int k = 0; k < kNoteCount / 12 * 5; k++) {
			int n = k / 5 * 12 + black_chromatic[k % 5];
			int boundary = k / 5 * 7 + black_before_white[k % 5];
			int cx = x + (boundary + 1) * white_w;
			p.fillRect(QRect(cx - black_w / 2, y + 2, black_w, black_h), n == lit ? kLit : kBlackKey);
		}
	}

	static void PaintSquares(QPainter &p, int x, int y, int w, int h, int lit) {
		const int sq_w = w / kNoteCount;
		for (int k = 0; k < kNoteCount; k++)
			p.fillRect(QRect(x + k * sq_w + 1, y + 4, sq_w - 2, h - 8), k == lit ? kLit : kWhiteKey);
	}

	SoundInspectData data{};
	int flash[kRowCount];
	int note[kRowCount];
};

// The drum bar: every drum of the kit as a labelled cell that lights when the song hits it; click one to hear it.
class DrumBar : public QWidget {
public:
	explicit DrumBar(QWidget *parent) : QWidget(parent) {
		setMinimumHeight(kRowsOfCells * 28);
		setMouseTracking(false);
		memset(flash, 0, sizeof(flash));
	}

	void Feed(const SoundInspectData &d) {
		for (int i = 0; i < SOUND_INSPECT_DRUMS; i++) {
			if (flash[i] > 0)
				flash[i]--;
			if (d.drums[i >> 5] & (1u << (i & 31)))
				flash[i] = kFlashFrames;
		}
		update();
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.fillRect(rect(), QColor(0, 0, 0));
		QFont f = font();
		f.setPointSizeF(qMax(6.5, f.pointSizeF() - 2));
		p.setFont(f);
		const QFontMetrics fm(f);
		for (int i = 0; i < SOUND_INSPECT_DRUMS; i++) {
			QRect cell = Cell(i).adjusted(1, 1, -1, -1);
			bool lit = flash[i] > 0;
			p.fillRect(cell, lit ? kLit : kWhiteKey);
			p.setPen(lit ? QColor(255, 255, 255) : QColor(40, 40, 45));
			p.drawText(cell.adjusted(3, 0, -3, 0), Qt::AlignCenter, fm.elidedText(kDrumNames[i], Qt::ElideRight, cell.width() - 6));
		}
	}

	void mousePressEvent(QMouseEvent *e) override {
		for (int i = 0; i < SOUND_INSPECT_DRUMS; i++)
			if (Cell(i).contains(e->pos())) {
				Sound_InspectPreviewDrum(i);
				flash[i] = kFlashFrames;
				update();
				return;
			}
	}

private:
	static const int kColumns = 12;
	static const int kRowsOfCells = (SOUND_INSPECT_DRUMS + kColumns - 1) / kColumns;

	QRect Cell(int i) const {
		const int w = width() / kColumns, h = height() / kRowsOfCells;
		return QRect((i % kColumns) * w, (i / kColumns) * h, w, h);
	}

	int flash[SOUND_INSPECT_DRUMS];
};

class InspectorWindow : public QWidget {
public:
	explicit InspectorWindow(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle("SMPS Inspector");
		auto *root = new QVBoxLayout(this);

		auto *controls = new QHBoxLayout;
		auto *prev = new QPushButton("<");
		auto *next = new QPushButton(">");
		prev->setFixedWidth(32);
		next->setFixedWidth(32);
		sounds = new QComboBox;
		sounds->setMinimumContentsLength(30);
		for (int i = 0; i < Sound_InspectEntryCount(); i++) {
			const SoundInspectEntry *e = Sound_InspectEntryAt(i);
			sounds->addItem(QString("%1  %2%3").arg(e->id, 2, 16, QChar('0')).toUpper().arg(e->name).arg(e->is_music ? "" : "  (effect)"), e->id);
		}
		auto *play = new QPushButton("Play");
		auto *stop = new QPushButton("Stop");
		json = new QCheckBox("JSON engine");
		json->setToolTip("Play through the JSON tree-walking engine (what the level select's sound test uses) instead of the byte driver");
		controls->addWidget(prev);
		controls->addWidget(sounds, 1);
		controls->addWidget(next);
		controls->addWidget(play);
		controls->addWidget(stop);
		controls->addWidget(json);
		root->addLayout(controls);

		strips = new Strips(this);
		root->addWidget(strips, 3);
		root->addWidget(new QLabel("Drum kit (click to hear one)"));
		drums = new DrumBar(this);
		root->addWidget(drums, 2);

		connect(play, &QPushButton::clicked, this, [this] { PlayCurrent(); });
		connect(stop, &QPushButton::clicked, this, [] { Sound_InspectStop(); });
		connect(prev, &QPushButton::clicked, this, [this] { Step(-1); });
		connect(next, &QPushButton::clicked, this, [this] { Step(+1); });
		connect(sounds, QOverload<int>::of(&QComboBox::activated), this, [this](int) { PlayCurrent(); });

		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] { Tick(); });
		resize(980, 640);
	}

protected:
	void showEvent(QShowEvent *e) override {
		QWidget::showEvent(e);
		Console_SetToolPause(true); // freeze the game, keep the sound engine ticking
		timer->start(16);
	}
	void hideEvent(QHideEvent *e) override {
		QWidget::hideEvent(e);
		Console_SetToolPause(false);
		timer->stop();
	}
	void keyPressEvent(QKeyEvent *e) override {
		if (e->key() == Qt::Key_Left)
			Step(-1);
		else if (e->key() == Qt::Key_Right)
			Step(+1);
		else
			QWidget::keyPressEvent(e);
	}

private:
	void PlayCurrent() {
		int index = sounds->currentIndex();
		if (index >= 0)
			Sound_InspectPlay(sounds->itemData(index).toInt(), json->isChecked() ? 1 : 0);
	}
	void Step(int delta) {
		int n = sounds->count();
		if (n == 0)
			return;
		sounds->setCurrentIndex((sounds->currentIndex() + delta + n) % n);
		PlayCurrent();
	}
	void Tick() {
		if (Peek_DebugToolsAvailable() == 0) { // debug mode was turned off underneath it
			hide();
			return;
		}
		SoundInspectData d;
		Sound_InspectTake(&d);
		strips->Feed(d);
		drums->Feed(d);
	}

	QComboBox *sounds = nullptr;
	QCheckBox *json = nullptr;
	Strips *strips = nullptr;
	DrumBar *drums = nullptr;
	QTimer *timer = nullptr;
};

InspectorWindow *g_window = nullptr;

} // namespace

namespace SmpsInspector {

void Show(QWidget *parent) {
	if (g_window == nullptr)
		g_window = new InspectorWindow(parent);
	g_window->show();
	g_window->raise();
	g_window->activateWindow();
}

bool IsOpen() { return g_window != nullptr && g_window->isVisible(); }

void Play(int sound_id, bool json_engine) { Sound_InspectPlay(sound_id, json_engine ? 1 : 0); }

} // namespace SmpsInspector
