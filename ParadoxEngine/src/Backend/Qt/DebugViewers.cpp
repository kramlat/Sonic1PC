#include "DebugViewers.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <deque>
#include <functional>
#include <vector>
#include <math.h>

#include "../../DebugPeek.h"
#include "../../DebugLog.h"
#include "Settings.h"

// VDP memory views (Backend/VDP.h -- not included: it is a C-only header).
extern "C" {
const uint8_t *VDP_PeekVRAM(void);
uint16_t VDP_PeekCRAM(int pal, int index);
uint32_t VDP_PeekColour(int pal, int index);
int VDP_PeekSprites(VdpSpritePeek *out, int max);
extern bool Z80_PEEK_DISPLAY; // makes Game.c gather the snapshot each frame
}

namespace {

const int kTilesAcross = 16;
const int kTileCount = 0x10000 / 32; // 2048 tiles in VRAM

QRgb ColourOf(int pal, int index) {
	uint32_t c = VDP_PeekColour(pal, index); // 0xRRGGBBAA
	return qRgb((c >> 24) & 0xFF, (c >> 16) & 0xFF, (c >> 8) & 0xFF);
}

QFont MonoFont() {
	return QFontDatabase::systemFont(QFontDatabase::FixedFont);
}

// ---------------------------------------------------------------------------
// VDP viewer
// ---------------------------------------------------------------------------

// All 2048 VRAM tiles as one 128 x 1024 image (16 tiles across), drawn
// zoomed with nearest-neighbour inside a scroll area.
class TileView : public QWidget {
public:
	explicit TileView(QWidget *parent = nullptr)
	    : QWidget(parent), image(kTilesAcross * 8, (kTileCount / kTilesAcross) * 8, QImage::Format_Indexed8) {
		setMouseTracking(true);
		SetZoom(3);
	}

	std::function<void(int)> onHover; // tile index, or -1 when the mouse leaves
	std::function<void(int)> onSelect;

	int palette = 0;
	bool grid = true;
	int selected = -1;

	void SetZoom(int z) {
		zoom = z;
		setFixedSize(image.width() * zoom, image.height() * zoom);
		update();
	}

	// Re-reads VRAM and the palette.
	void Refresh() {
		QList<QRgb> table;
		for (int i = 0; i < 16; i++)
			table.append(ColourOf(palette, i));
		image.setColorTable(table);

		const uint8_t *vram = VDP_PeekVRAM();
		for (int t = 0; t < kTileCount; t++) {
			int x0 = (t % kTilesAcross) * 8, y0 = (t / kTilesAcross) * 8;
			for (int row = 0; row < 8; row++) {
				uchar *line = image.scanLine(y0 + row) + x0;
				const uint8_t *src = vram + t * 32 + row * 4;
				for (int col = 0; col < 8; col++)
					line[col] = (col & 1) ? (src[col / 2] & 0xF) : (src[col / 2] >> 4);
			}
		}
		update();
	}

	// 8x8 palette indices of a tile, for the inspector.
	static void TilePixels(int tile, uint8_t out[64]) {
		const uint8_t *src = VDP_PeekVRAM() + tile * 32;
		for (int i = 0; i < 64; i++)
			out[i] = (i & 1) ? (src[i / 2] & 0xF) : (src[i / 2] >> 4);
	}

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setRenderHint(QPainter::SmoothPixmapTransform, false);
		p.drawImage(rect(), image);
		int cell = 8 * zoom;
		if (grid && zoom >= 3) {
			p.setPen(QColor(255, 255, 255, 40));
			for (int x = 0; x <= width(); x += cell)
				p.drawLine(x, 0, x, height());
			for (int y = 0; y <= height(); y += cell)
				p.drawLine(0, y, width(), y);
		}
		if (selected >= 0) {
			p.setPen(QPen(QColor(255, 220, 80), 2));
			p.drawRect((selected % kTilesAcross) * cell, (selected / kTilesAcross) * cell, cell - 1, cell - 1);
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		if (onHover)
			onHover(TileAt(e->pos()));
	}
	void leaveEvent(QEvent *) override {
		if (onHover)
			onHover(-1);
	}
	void mousePressEvent(QMouseEvent *e) override {
		int t = TileAt(e->pos());
		if (t >= 0) {
			selected = t;
			update();
			if (onSelect)
				onSelect(t);
		}
	}

private:
	int TileAt(const QPoint &pos) const {
		int cell = 8 * zoom;
		int tx = pos.x() / cell, ty = pos.y() / cell;
		if (tx < 0 || tx >= kTilesAcross || ty < 0)
			return -1;
		int t = ty * kTilesAcross + tx;
		return t < kTileCount ? t : -1;
	}

	QImage image;
	int zoom = 3;
};

// A raw 9-bit CRAM word -> colour (same levels as the VDP's own conversion).
QRgb CramColour(uint16_t cv) {
	static const uint8_t level[] = {0, 52, 87, 116, 144, 172, 206, 255};
	return qRgb(level[(cv & 0x00E) >> 1], level[(cv & 0x0E0) >> 5], level[(cv & 0xE00) >> 9]);
}

// Where a palette comes from: the VDP's live CRAM, or one of the game's own
// palette copies (Peek_GamePalette: PEEK_PAL_*), e.g. the water palette.
const int kSourceVdp = -1;

uint16_t RawCram(int source, int pal, int index) {
	return source == kSourceVdp ? VDP_PeekCRAM(pal, index) : Peek_GamePalette(source, pal, index);
}

// The 4 x 16 palette as swatches.
class PaletteView : public QWidget {
public:
	explicit PaletteView(int source_, QWidget *parent = nullptr) : QWidget(parent), source(source_) {
		setMouseTracking(true);
		setFixedSize(kCell * 16 + 1, kCell * 4 + 1);
	}

	const int source;
	std::function<void(int, int, int)> onHover; // source, pal, index; pal = -1 on leave

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.setFont(MonoFont());
		for (int pal = 0; pal < 4; pal++) {
			for (int i = 0; i < 16; i++) {
				QRect r(i * kCell, pal * kCell, kCell, kCell);
				QColor c(CramColour(RawCram(source, pal, i)));
				p.fillRect(r, c);
				p.setPen(c.lightness() > 128 ? Qt::black : Qt::white);
				p.drawText(r, Qt::AlignCenter, QString::number(i, 16).toUpper());
				p.setPen(QColor(0, 0, 0, 90));
				p.drawRect(r);
			}
		}
	}
	void mouseMoveEvent(QMouseEvent *e) override {
		int pal = e->pos().y() / kCell, idx = e->pos().x() / kCell;
		bool inside = pal < 4 && idx < 16;
		if (onHover)
			onHover(source, inside ? pal : -1, inside ? idx : -1);
	}
	void leaveEvent(QEvent *) override {
		if (onHover)
			onHover(source, -1, -1);
	}

private:
	static const int kCell = 36;
};

class VdpViewer : public QWidget {
public:
	explicit VdpViewer(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle("VDP Viewer");
		auto *root = new QVBoxLayout(this);

		// Controls row
		auto *controls = new QHBoxLayout;
		controls->addWidget(new QLabel("Palette:"));
		palette_box = new QComboBox;
		for (int i = 0; i < 4; i++)
			palette_box->addItem(QString("Palette %1").arg(i));
		controls->addWidget(palette_box);
		controls->addWidget(new QLabel("Zoom:"));
		zoom_box = new QComboBox;
		for (int z = 1; z <= 6; z++)
			zoom_box->addItem(QString("%1x").arg(z), z);
		zoom_box->setCurrentIndex(2);
		controls->addWidget(zoom_box);
		grid_box = new QCheckBox("Grid");
		grid_box->setChecked(true);
		controls->addWidget(grid_box);
		live_box = new QCheckBox("Live");
		live_box->setChecked(true);
		controls->addWidget(live_box);
		refresh_button = new QPushButton("Refresh");
		controls->addWidget(refresh_button);
		controls->addStretch();
		root->addLayout(controls);

		auto *tabs = new QTabWidget;
		root->addWidget(tabs, 1);

		// Tiles tab: scrolling tile sheet + inspector
		auto *tiles_page = new QWidget;
		auto *tiles_layout = new QHBoxLayout(tiles_page);
		tiles = new TileView;
		auto *scroll = new QScrollArea;
		scroll->setWidget(tiles);
		scroll->setMinimumWidth(128 * 3 + 40);
		tiles_layout->addWidget(scroll, 1);

		auto *side = new QVBoxLayout;
		inspector_image = new QLabel;
		inspector_image->setFixedSize(8 * 12, 8 * 12);
		inspector_image->setFrameStyle(QFrame::Box);
		side->addWidget(inspector_image);
		inspector_text = new QLabel("Click a tile");
		inspector_text->setFont(MonoFont());
		inspector_text->setTextInteractionFlags(Qt::TextSelectableByMouse);
		side->addWidget(inspector_text);
		side->addStretch();
		tiles_layout->addLayout(side);
		tabs->addTab(tiles_page, "Tiles");

		// Palettes tab: the live VDP CRAM, then the game's own water and dry palette copies
		auto *pal_page = new QWidget;
		auto *pal_layout = new QVBoxLayout(pal_page);
		struct { const char *title; int source; } kinds[] = {
			{"VDP CRAM (live -- what is on screen)", kSourceVdp},
			{"Water palette (game)", PEEK_PAL_WET},
			{"Dry palette (game)", PEEK_PAL_DRY},
		};
		for (auto &k : kinds) {
			pal_layout->addWidget(new QLabel(k.title));
			auto *view = new PaletteView(k.source);
			view->onHover = [this](int src, int pal, int idx) { HoverPalette(src, pal, idx); };
			palette_views.push_back(view);
			pal_layout->addWidget(view);
		}
		palette_text = new QLabel("Hover a colour");
		palette_text->setFont(MonoFont());
		pal_layout->addWidget(palette_text);
		pal_layout->addStretch();
		tabs->addTab(pal_page, "Palettes");

		// Sprites tab: the VDP sprite table, in link order, with a preview of the selected one
		auto *spr_page = new QWidget;
		auto *spr_layout = new QHBoxLayout(spr_page);
		sprite_table = new QTableWidget(0, 9);
		sprite_table->setHorizontalHeaderLabels({"#", "X", "Y", "Size", "Tile", "Pal", "Pri", "Flip", "Link"});
		sprite_table->verticalHeader()->hide();
		sprite_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
		sprite_table->setSelectionBehavior(QAbstractItemView::SelectRows);
		sprite_table->setSelectionMode(QAbstractItemView::SingleSelection);
		sprite_table->setFont(MonoFont());
		sprite_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
		sprite_table->horizontalHeader()->setStretchLastSection(true);
		spr_layout->addWidget(sprite_table, 1);
		auto *spr_side = new QVBoxLayout;
		sprite_preview = new QLabel;
		sprite_preview->setFixedSize(32 * 6 + 2, 32 * 6 + 2);
		sprite_preview->setFrameStyle(QFrame::Box);
		sprite_preview->setAlignment(Qt::AlignCenter);
		spr_side->addWidget(sprite_preview);
		sprite_count = new QLabel(" ");
		sprite_count->setFont(MonoFont());
		spr_side->addWidget(sprite_count);
		spr_side->addStretch();
		spr_layout->addLayout(spr_side);
		connect(sprite_table, &QTableWidget::itemSelectionChanged, this, [this] { UpdateSpritePreview(); });
		tabs->addTab(spr_page, "Sprites");

		status = new QLabel(" ");
		status->setFont(MonoFont());
		root->addWidget(status);

		// Wiring
		connect(palette_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
			tiles->palette = i;
			Update();
		});
		connect(zoom_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		        [this](int) { tiles->SetZoom(zoom_box->currentData().toInt()); });
		connect(grid_box, &QCheckBox::toggled, this, [this](bool on) {
			tiles->grid = on;
			tiles->update();
		});
		connect(refresh_button, &QPushButton::clicked, this, [this] { Update(); });
		connect(live_box, &QCheckBox::toggled, this, [this](bool on) { refresh_button->setEnabled(!on); });
		refresh_button->setEnabled(false);

		tiles->onHover = [this](int t) {
			status->setText(t < 0 ? " " : QString("Tile $%1   VRAM $%2").arg(t, 3, 16, QChar('0')).arg(t * 32, 4, 16, QChar('0')).toUpper());
		};
		tiles->onSelect = [this](int t) { Inspect(t); };
		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] {
			if (live_box->isChecked())
				Update();
		});
		timer->start(100);

		resize(128 * 3 + 260, 640);
		Update();
	}

private:
	void Update() {
		if (!isVisible())
			return;
		tiles->Refresh();
		for (PaletteView *v : palette_views)
			v->update();
		UpdateSprites();
		if (tiles->selected >= 0)
			Inspect(tiles->selected);
	}

	void HoverPalette(int source, int pal, int idx) {
		if (pal < 0) {
			palette_text->setText("Hover a colour");
			return;
		}
		static const char *const names[] = {"dry", "water", "dry target", "water target"};
		QString where = source == kSourceVdp ? "VDP CRAM" : QString("game %1").arg(names[source]);
		uint16_t cram = RawCram(source, pal, idx);
		QColor c(CramColour(cram));
		palette_text->setText(QString("%1: palette %2, colour %3   CRAM $%4   RGB (%5, %6, %7)   #%8")
		                          .arg(where).arg(pal).arg(idx, 0, 16).arg(cram, 4, 16, QChar('0')).arg(c.red()).arg(c.green()).arg(c.blue())
		                          .arg(c.name().mid(1).toUpper()));
	}

	void UpdateSprites() {
		VdpSpritePeek list[80];
		int n = VDP_PeekSprites(list, 80);
		sprites.assign(list, list + n);
		int sel = sprite_table->currentRow();
		sprite_table->setRowCount(n);
		for (int r = 0; r < n; r++) {
			const VdpSpritePeek &e = list[r];
			QStringList cols = {
				QString::number(e.index), QString::number(e.x), QString::number(e.y),
				QString("%1x%2").arg(e.width).arg(e.height), QString("$%1").arg(e.pattern, 3, 16, QChar('0')).toUpper(),
				QString::number(e.palette), e.priority ? "hi" : "lo",
				QString("%1%2").arg(e.x_flip ? "X" : "-").arg(e.y_flip ? "Y" : "-"), e.link ? QString::number(e.link) : "end"};
			for (int c = 0; c < cols.size(); c++) {
				QTableWidgetItem *item = sprite_table->item(r, c);
				if (item == nullptr) {
					item = new QTableWidgetItem;
					sprite_table->setItem(r, c, item);
				}
				item->setText(cols[c]);
			}
		}
		if (sel >= 0 && sel < n)
			sprite_table->selectRow(sel);
		sprite_count->setText(QString("%1 sprites on the chain (max 80)").arg(n));
		UpdateSpritePreview();
	}

	// The selected sprite drawn from VRAM (tiles run down each column first).
	void UpdateSpritePreview() {
		int row = sprite_table->currentRow();
		if (row < 0 || row >= (int)sprites.size()) {
			sprite_preview->clear();
			return;
		}
		const VdpSpritePeek &e = sprites[row];
		QImage img(e.width * 8, e.height * 8, QImage::Format_ARGB32);
		img.fill(Qt::transparent);
		for (int col = 0; col < e.width; col++) {
			for (int trow = 0; trow < e.height; trow++) {
				int tile = (e.pattern + col * e.height + trow) & 0x7FF;
				uint8_t px[64];
				TileView::TilePixels(tile, px);
				for (int y = 0; y < 8; y++) {
					for (int x = 0; x < 8; x++) {
						int v = px[y * 8 + x];
						int dx = e.x_flip ? (e.width * 8 - 1) - (col * 8 + x) : col * 8 + x;
						int dy = e.y_flip ? (e.height * 8 - 1) - (trow * 8 + y) : trow * 8 + y;
						if (v)
							img.setPixel(dx, dy, ColourOf(e.palette, v));
					}
				}
			}
		}
		sprite_preview->setPixmap(QPixmap::fromImage(img).scaled(sprite_preview->width() - 2, sprite_preview->height() - 2,
		                                                       Qt::KeepAspectRatio, Qt::FastTransformation));
	}

	// Big view of one tile plus its 8 rows of palette indices.
	void Inspect(int t) {
		uint8_t px[64];
		TileView::TilePixels(t, px);
		QImage img(8, 8, QImage::Format_RGB32);
		QString rows;
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				img.setPixel(x, y, ColourOf(tiles->palette, px[y * 8 + x]));
				rows += QString::number(px[y * 8 + x], 16).toUpper();
			}
			rows += '\n';
		}
		inspector_image->setPixmap(QPixmap::fromImage(img).scaled(8 * 12, 8 * 12, Qt::KeepAspectRatio, Qt::FastTransformation));
		inspector_text->setText(QString("Tile $%1\nVRAM $%2\n\n%3").arg(t, 3, 16, QChar('0')).arg(t * 32, 4, 16, QChar('0')).arg(rows).toUpper());
	}

protected:
	void showEvent(QShowEvent *e) override {
		QWidget::showEvent(e);
		Update();
	}

private:
	QComboBox *palette_box, *zoom_box;
	QCheckBox *grid_box, *live_box;
	QPushButton *refresh_button;
	TileView *tiles;
	std::vector<PaletteView *> palette_views;
	QTableWidget *sprite_table;
	QLabel *sprite_preview, *sprite_count;
	std::vector<VdpSpritePeek> sprites;
	QLabel *inspector_image, *inspector_text, *palette_text, *status;
	QTimer *timer;
};

// ---------------------------------------------------------------------------
// Sound viewer (YM2612 + SN76489)
// ---------------------------------------------------------------------------

Z80PeekData g_snapshot = {};
bool g_snapshot_valid = false;

const char *const kAlgorithmRouting[8] = {
	"1 > 2 > 3 > 4",
	"(1 + 2) > 3 > 4",
	"(1 + (2 > 3)) > 4",
	"((1 > 2) + 3) > 4",
	"(1 > 2) + (3 > 4)",
	"1 > (2 + 3 + 4)",
	"(1 > 2) + 3 + 4",
	"1 + 2 + 3 + 4",
};

class SoundViewer : public QWidget {
public:
	explicit SoundViewer(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle("Sound Viewer (YM2612 / SN76489)");
		auto *root = new QVBoxLayout(this);

		auto *controls = new QHBoxLayout;
		freeze_box = new QCheckBox("Freeze");
		controls->addWidget(freeze_box);
		controls->addStretch();
		root->addLayout(controls);

		root->addWidget(new QLabel("YM2612 (FM)"));
		fm = MakeTable({"Ch", "Key", "Alg", "FB", "Pan", "Block", "F-Num", "Freq (Hz)", "TL1", "TL2", "TL3", "TL4", "Routing"}, 6);
		root->addWidget(fm);

		root->addWidget(new QLabel("SN76489 (PSG)"));
		psg = MakeTable({"Channel", "Period / Rate", "Freq (Hz)", "Atten", "Level", "Mode"}, 4);
		root->addWidget(psg);

		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] { Update(); });
		timer->start(50);
		resize(900, 420);
	}

	void Update() {
		if (!isVisible() || freeze_box->isChecked() || !g_snapshot_valid)
			return;
		const Z80PeekData &d = g_snapshot;

		for (int n = 0; n < 6; n++) {
			int port = n / 3, ch = n % 3;
			bool key = (d.fm_keyon & (1 << n)) != 0;
			uint8_t algfb = d.fm_alg_fb[port][ch];
			int alg = algfb & 7, fb = (algfb >> 3) & 7;
			uint8_t pan = d.fm_pan[port][ch];
			int block = (d.fm_freq[port][ch] >> 11) & 7, fnum = d.fm_freq[port][ch] & 0x7FF;
			// f = fnum * (clock / 144) / 2^(21 - block), clock = 7670454 (NTSC)
			double hz = fnum * (7670454.0 / 144.0) / double(1 << (21 - block));

			Set(fm, n, 0, QString("FM%1").arg(n + 1));
			Set(fm, n, 1, key ? "ON" : "-", key ? QColor(80, 200, 100) : QColor());
			Set(fm, n, 2, QString::number(alg));
			Set(fm, n, 3, QString::number(fb));
			Set(fm, n, 4, QString("%1%2").arg(pan & 0x80 ? "L" : "-").arg(pan & 0x40 ? "R" : "-"));
			Set(fm, n, 5, QString::number(block));
			Set(fm, n, 6, QString("$%1").arg(fnum, 3, 16, QChar('0')).toUpper());
			Set(fm, n, 7, fnum ? QString::number(hz, 'f', 1) : "-");
			for (int op = 0; op < 4; op++) {
				int tl = d.fm_tl[port][ch][op] & 0x7F;
				Set(fm, n, 8 + op, QString("$%1").arg(tl, 2, 16, QChar('0')).toUpper(), tl < 100 ? QColor(220, 190, 60) : QColor());
			}
			Set(fm, n, 12, kAlgorithmRouting[alg]);
		}

		for (int c = 0; c < 3; c++) {
			int period = d.psg_tone_period[c], atten = d.psg_tone_atten[c];
			double hz = period ? 3579545.0 / (32.0 * period) : 0.0;
			bool audible = atten < 15;
			Set(psg, c, 0, QString("Tone %1").arg(c + 1));
			Set(psg, c, 1, QString("$%1").arg(period, 3, 16, QChar('0')).toUpper());
			Set(psg, c, 2, period ? QString::number(hz, 'f', 1) : "-");
			Set(psg, c, 3, QString("$%1").arg(atten, 1, 16).toUpper(), audible ? QColor(220, 190, 60) : QColor());
			Set(psg, c, 4, atten >= 15 ? "off" : QString("-%1 dB").arg(atten * 2));
			Set(psg, c, 5, "square");
		}
		static const char *const kNoiseRates[4] = {"N/512", "N/1024", "N/2048", "tone 3"};
		int rate = d.psg_noise_shift_rate & 3, natten = d.psg_noise_atten;
		Set(psg, 3, 0, "Noise");
		Set(psg, 3, 1, kNoiseRates[rate]);
		Set(psg, 3, 2, "-");
		Set(psg, 3, 3, QString("$%1").arg(natten, 1, 16).toUpper(), natten < 15 ? QColor(220, 190, 60) : QColor());
		Set(psg, 3, 4, natten >= 15 ? "off" : QString("-%1 dB").arg(natten * 2));
		Set(psg, 3, 5, d.psg_noise_fb_white ? "white" : "periodic");
	}

protected:
	void showEvent(QShowEvent *e) override {
		QWidget::showEvent(e);
		Z80_PEEK_DISPLAY = true; // makes Game.c gather the snapshot every frame
	}
	void hideEvent(QHideEvent *e) override {
		Z80_PEEK_DISPLAY = false;
		QWidget::hideEvent(e);
	}

private:
	QTableWidget *MakeTable(const QStringList &headers, int rows) {
		auto *t = new QTableWidget(rows, headers.size());
		t->setHorizontalHeaderLabels(headers);
		t->verticalHeader()->hide();
		t->setEditTriggers(QAbstractItemView::NoEditTriggers);
		t->setSelectionMode(QAbstractItemView::NoSelection);
		t->setFont(MonoFont());
		t->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
		t->horizontalHeader()->setStretchLastSection(true);
		t->setMinimumHeight(t->horizontalHeader()->sizeHint().height() + rows * 30 + 12);
		return t;
	}

	static void Set(QTableWidget *t, int row, int col, const QString &text, const QColor &bg = QColor()) {
		QTableWidgetItem *item = t->item(row, col);
		if (item == nullptr) {
			item = new QTableWidgetItem;
			t->setItem(row, col, item);
		}
		item->setText(text);
		item->setBackground(bg.isValid() ? QBrush(bg) : QBrush());
		item->setForeground(bg.isValid() ? QBrush(Qt::black) : QBrush());
	}

	QCheckBox *freeze_box;
	QTableWidget *fm, *psg;
	QTimer *timer;
};

// ---------------------------------------------------------------------------
// Object RAM viewer: every object slot, plus the raw bytes of the selected one
// ---------------------------------------------------------------------------

class ObjectViewer : public QWidget {
public:
	explicit ObjectViewer(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle("Object RAM");
		auto *root = new QVBoxLayout(this);

		auto *controls = new QHBoxLayout;
		empty_box = new QCheckBox("Show empty slots");
		controls->addWidget(empty_box);
		freeze_box = new QCheckBox("Freeze");
		controls->addWidget(freeze_box);
		count_label = new QLabel(" ");
		count_label->setFont(MonoFont());
		controls->addWidget(count_label);
		controls->addStretch();
		root->addLayout(controls);

		auto *split = new QSplitter(Qt::Vertical);
		table = new QTableWidget(0, 13);
		table->setHorizontalHeaderLabels({"Slot", "Type", "Rout", "Sec", "X", "Y", "XSpd", "YSpd", "Frame", "Anim", "Render", "Status", "Subtype"});
		table->verticalHeader()->hide();
		table->setEditTriggers(QAbstractItemView::NoEditTriggers);
		table->setSelectionBehavior(QAbstractItemView::SelectRows);
		table->setSelectionMode(QAbstractItemView::SingleSelection);
		table->setFont(MonoFont());
		table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
		split->addWidget(table);
		dump = new QPlainTextEdit;
		dump->setReadOnly(true);
		dump->setFont(MonoFont());
		dump->setLineWrapMode(QPlainTextEdit::NoWrap);
		split->addWidget(dump);
		split->setStretchFactor(0, 3);
		root->addWidget(split, 1);

		connect(table, &QTableWidget::itemSelectionChanged, this, [this] { UpdateDump(); });
		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] { Update(); });
		timer->start(100);
		resize(900, 640);
	}

protected:
	void showEvent(QShowEvent *e) override {
		QWidget::showEvent(e);
		Update();
	}

private:
	int SelectedSlot() const {
		int row = table->currentRow();
		return row >= 0 && row < (int)slot_ids.size() ? slot_ids[row] : -1;
	}

	void Update() {
		if (!isVisible() || freeze_box->isChecked())
			return;
		int selected = SelectedSlot();
		slot_ids.clear();
		int used = 0;
		for (int i = 0; i < Peek_ObjectCount(); i++) {
			ObjectPeek o;
			Peek_GetObject(i, &o);
			if (o.type != 0)
				used++;
			if (o.type != 0 || empty_box->isChecked())
				slot_ids.push_back(i);
		}
		count_label->setText(QString("%1 / %2 slots in use").arg(used).arg(Peek_ObjectCount()));

		table->setRowCount((int)slot_ids.size());
		for (int r = 0; r < (int)slot_ids.size(); r++) {
			ObjectPeek o;
			Peek_GetObject(slot_ids[r], &o);
			auto hex = [](int v, int w) { return QString("$%1").arg(v & ((1 << (w * 4)) - 1), w, 16, QChar('0')).toUpper(); };
			QStringList cols = {QString::number(slot_ids[r]), hex(o.type, 2), hex(o.routine, 2), hex(o.routine_sec, 2),
			                    hex(o.x, 4), hex(o.y, 4), hex(o.xsp, 4), hex(o.ysp, 4), hex(o.frame, 2), hex(o.anim, 2),
			                    hex(o.render, 2), hex(o.status, 2), hex(o.subtype, 2)};
			for (int c = 0; c < cols.size(); c++) {
				QTableWidgetItem *item = table->item(r, c);
				if (item == nullptr) {
					item = new QTableWidgetItem;
					table->setItem(r, c, item);
				}
				item->setText(cols[c]);
			}
		}
		for (int r = 0; r < (int)slot_ids.size(); r++)
			if (slot_ids[r] == selected)
				table->selectRow(r);
		UpdateDump();
	}

	// Hex dump of the selected slot's whole Object struct (this port's layout, not the
	// original's $40-byte one -- the last 24 bytes are the object's scratch area).
	void UpdateDump() {
		int slot = SelectedSlot();
		if (slot < 0) {
			dump->setPlainText("Select an object to see its raw bytes.");
			return;
		}
		int size;
		const uint8_t *bytes = Peek_ObjectBytes(slot, &size);
		QString text = QString("Slot %1  (%2 bytes)\n\n").arg(slot).arg(size);
		for (int i = 0; i < size; i += 16) {
			text += QString("%1:").arg(i, 3, 16, QChar('0')).toUpper();
			QString ascii;
			for (int j = 0; j < 16 && i + j < size; j++) {
				text += QString(" %1").arg(bytes[i + j], 2, 16, QChar('0')).toUpper();
				ascii += (bytes[i + j] >= 32 && bytes[i + j] < 127) ? QChar(bytes[i + j]) : QChar('.');
			}
			text += "  " + ascii + "\n";
		}
		int scroll = dump->verticalScrollBar()->value();
		dump->setPlainText(text);
		dump->verticalScrollBar()->setValue(scroll);
	}

	QCheckBox *empty_box, *freeze_box;
	QLabel *count_label;
	QTableWidget *table;
	QPlainTextEdit *dump;
	QTimer *timer;
	std::vector<int> slot_ids;
};

// ---------------------------------------------------------------------------
// Variables viewer: the game's globals (DebugVars.c), with filter, change
// highlighting and editing
// ---------------------------------------------------------------------------

// Tells the viewer when a cell editor is open, so live updates don't overwrite what is being typed.
class EditTrackingDelegate : public QStyledItemDelegate {
public:
	EditTrackingDelegate(bool *flag, QObject *parent) : QStyledItemDelegate(parent), editing(flag) {}
	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
		*editing = true;
		return QStyledItemDelegate::createEditor(parent, option, index);
	}
	void destroyEditor(QWidget *editor, const QModelIndex &index) const override {
		*editing = false;
		QStyledItemDelegate::destroyEditor(editor, index);
	}

private:
	bool *editing;
};

class VariableViewer : public QWidget {
public:
	explicit VariableViewer(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle("Variables");
		auto *root = new QVBoxLayout(this);

		auto *controls = new QHBoxLayout;
		filter_edit = new QLineEdit;
		filter_edit->setPlaceholderText("Filter by name...");
		filter_edit->setClearButtonEnabled(true);
		controls->addWidget(filter_edit, 1);
		group_box = new QComboBox;
		group_box->addItem("All groups");
		std::vector<QString> groups;
		for (int i = 0; i < Peek_VarCount(); i++) {
			VarPeek v;
			Peek_GetVar(i, &v);
			if (std::find(groups.begin(), groups.end(), QString(v.group)) == groups.end())
				groups.push_back(v.group);
		}
		std::sort(groups.begin(), groups.end());
		for (const QString &g : groups)
			group_box->addItem(g);
		controls->addWidget(group_box);
		freeze_box = new QCheckBox("Freeze");
		controls->addWidget(freeze_box);
		count_label = new QLabel(" ");
		count_label->setFont(MonoFont());
		controls->addWidget(count_label);
		root->addLayout(controls);

		table = new QTableWidget(0, 5);
		table->setHorizontalHeaderLabels({"Name", "Group", "Type", "Value", "Hex"});
		table->verticalHeader()->hide();
		table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
		table->setSelectionBehavior(QAbstractItemView::SelectRows);
		table->setFont(MonoFont());
		table->setItemDelegate(new EditTrackingDelegate(&cell_editing, table));
		table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive); // not ResizeToContents: that re-measures every refresh
		const int widths[] = {240, 120, 80, 130};
		for (int c = 0; c < 4; c++)
			table->setColumnWidth(c, widths[c]);
		table->horizontalHeader()->setStretchLastSection(true);
		table->verticalHeader()->setDefaultSectionSize(22);
		root->addWidget(table, 1);
		hint = new QLabel("Double-click a Value or Hex cell to change a variable (decimal, $hex or 0xhex). Changes flash yellow.");
		root->addWidget(hint);

		connect(filter_edit, &QLineEdit::textChanged, this, [this] { Rebuild(); });
		connect(group_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { Rebuild(); });
		connect(table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) { Edited(item); });

		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] { Update(); });
		timer->start(100);
		resize(760, 640);
		Rebuild();
	}

protected:
	void showEvent(QShowEvent *e) override {
		QWidget::showEvent(e);
		Update();
	}

private:
	struct Row {
		int var;
		int element;   // element index; -1 for a whole large array (read-only summary row)
		int64_t last;  // value at the previous update, for change highlighting
		int flash;     // updates left to keep the highlight
		bool lit = false; // highlight currently applied to the row
	};

	static const int kMaxFlattened = 32; // arrays up to this long get one row per element

	static QString TypeName(const VarPeek &v) {
		static const char *const names[] = {"u8", "s8", "u16", "s16", "u32", "s32", "bool", "16.16"};
		QString t = names[v.kind];
		if (v.count > 1)
			t += QString("[%1]").arg(v.count);
		return t;
	}

	static QString Hex(int64_t value, int size) {
		uint64_t mask = size >= 8 ? ~0ull : ((1ull << (size * 8)) - 1);
		return QString("$%1").arg((uint64_t)value & mask, size * 2, 16, QChar('0')).toUpper();
	}

	// FIXED values show their pixel position (high word, signed); everything else its numeric value.
	static QString ValueText(const VarPeek &v, int64_t raw) {
		if (v.kind == VAR_FIXED)
			return QString::number((int16_t)(raw >> 16));
		return QString::number(raw);
	}

	void Rebuild() {
		updating = true;
		rows.clear();
		QString filter = filter_edit->text().trimmed();
		QString group = group_box->currentIndex() > 0 ? group_box->currentText() : QString();
		for (int i = 0; i < Peek_VarCount(); i++) {
			VarPeek v;
			Peek_GetVar(i, &v);
			if (!group.isEmpty() && group != v.group)
				continue;
			if (!filter.isEmpty() && !QString(v.name).contains(filter, Qt::CaseInsensitive))
				continue;
			if (v.count == 1)
				rows.push_back({i, 0, 0, 0, false});
			else if (v.count <= kMaxFlattened)
				for (int e = 0; e < v.count; e++)
					rows.push_back({i, e, 0, 0, false});
			else
				rows.push_back({i, -1, 0, 0, false});
		}
		table->setRowCount((int)rows.size());
		count_label->setText(QString("%1 shown").arg(rows.size()));
		for (int r = 0; r < (int)rows.size(); r++) {
			VarPeek v;
			Peek_GetVar(rows[r].var, &v);
			QString name = v.name;
			if (rows[r].element >= 0 && v.count > 1)
				name += QString("[%1]").arg(rows[r].element);
			else if (rows[r].element < 0)
				name += QString("[%1]").arg(v.count);
			SetCell(r, 0, name, false);
			SetCell(r, 1, v.group, false);
			SetCell(r, 2, rows[r].element < 0 ? TypeName(v) : (v.count > 1 ? TypeName(v).section('[', 0, 0) : TypeName(v)), false);
			SetCell(r, 3, "", rows[r].element >= 0);
			SetCell(r, 4, "", rows[r].element >= 0);
			rows[r].last = INT64_MIN; // first update fills the cells without flashing
		}
		updating = false;
		RefreshValues(true);
	}

	void SetCell(int r, int c, const QString &text, bool editable) {
		QTableWidgetItem *item = table->item(r, c);
		if (item == nullptr) {
			item = new QTableWidgetItem;
			table->setItem(r, c, item);
		}
		Qt::ItemFlags f = item->flags();
		item->setFlags(editable ? (f | Qt::ItemIsEditable) : (f & ~Qt::ItemIsEditable));
		if (item->text() != text)
			item->setText(text);
	}

	void Update() {
		if (!isVisible() || freeze_box->isChecked())
			return;
		RefreshValues(false);
	}

	void RefreshValues(bool first) {
		if (cell_editing)
			return; // don't overwrite a cell being typed into
		updating = true;
		for (int r = 0; r < (int)rows.size(); r++) {
			Row &row = rows[r];
			VarPeek v;
			Peek_GetVar(row.var, &v);
			QString value, hex;
			int64_t raw = 0;
			if (row.element < 0) { // large array: first bytes as a hex strip
				for (int e = 0; e < 8 && e < v.count; e++)
					hex += Hex(Peek_VarRead(row.var, e), v.elem_size) + " ";
				value = "...";
				hex = hex.trimmed() + " ...";
				raw = 0;
			} else {
				raw = Peek_VarRead(row.var, row.element);
				value = ValueText(v, raw);
				hex = Hex(raw, v.elem_size);
				if (!first && row.last != INT64_MIN && raw != row.last)
					row.flash = 8;
			}
			SetCell(r, 3, value, row.element >= 0);
			SetCell(r, 4, hex, row.element >= 0);
			bool lit = row.flash > 0;
			if (lit != row.lit) {
				QBrush bg = lit ? QBrush(QColor(255, 235, 120)) : QBrush();
				for (int c = 0; c < 5; c++) {
					table->item(r, c)->setBackground(bg);
					table->item(r, c)->setForeground(lit ? QBrush(Qt::black) : QBrush());
				}
				row.lit = lit;
			}
			if (row.flash > 0)
				row.flash--;
			row.last = raw;
		}
		updating = false;
	}

	static bool ParseNumber(QString text, int64_t *out) {
		text = text.trimmed();
		int base = 10;
		if (text.startsWith('$')) {
			text.remove(0, 1);
			base = 16;
		} else if (text.startsWith("0x", Qt::CaseInsensitive)) {
			text.remove(0, 2);
			base = 16;
		}
		bool ok = false;
		qlonglong v = text.toLongLong(&ok, base);
		if (ok)
			*out = v;
		return ok;
	}

	void Edited(QTableWidgetItem *item) {
		if (updating)
			return;
		int r = item->row(), c = item->column();
		if (r < 0 || r >= (int)rows.size() || (c != 3 && c != 4) || rows[r].element < 0)
			return;
		VarPeek v;
		Peek_GetVar(rows[r].var, &v);
		int64_t number;
		if (ParseNumber(item->text(), &number)) {
			if (c == 3 && v.kind == VAR_FIXED)
				number = (int64_t)(int32_t)((uint32_t)(uint16_t)number << 16); // pixel position, fraction 0
			Peek_VarWrite(rows[r].var, rows[r].element, number);
			rows[r].flash = 8;
		}
		RefreshValues(true); // re-read: restores the cell if the text didn't parse
	}

	QLineEdit *filter_edit;
	QComboBox *group_box;
	QCheckBox *freeze_box;
	QLabel *count_label, *hint;
	QTableWidget *table;
	QTimer *timer;
	std::vector<Row> rows;
	bool updating = false;
	bool cell_editing = false;
};

// ---------------------------------------------------------------------------
// Log window + logging control (DebugLog.c)
// ---------------------------------------------------------------------------

QString &g_log_file = Settings::Get().log_file; // chosen log file (remembered in the settings); empty = in memory only

class LogViewer : public QWidget {
public:
	explicit LogViewer(QWidget *parent) : QWidget(parent, Qt::Window) {
		setWindowTitle("Log");
		auto *root = new QVBoxLayout(this);

		auto *controls = new QHBoxLayout;
		toggle_button = new QPushButton;
		controls->addWidget(toggle_button);
		auto *file_button = new QPushButton("Log File...");
		controls->addWidget(file_button);
		auto *clear_button = new QPushButton("Clear");
		controls->addWidget(clear_button);
		auto *save_button = new QPushButton("Save As...");
		controls->addWidget(save_button);
		scroll_box = new QCheckBox("Auto-scroll");
		scroll_box->setChecked(true);
		controls->addWidget(scroll_box);
		filter_edit = new QLineEdit;
		filter_edit->setPlaceholderText("Filter (category or text)...");
		filter_edit->setClearButtonEnabled(true);
		controls->addWidget(filter_edit, 1);
		root->addLayout(controls);

		text = new QPlainTextEdit;
		text->setReadOnly(true);
		text->setFont(MonoFont());
		text->setLineWrapMode(QPlainTextEdit::NoWrap);
		text->setMaximumBlockCount(kMaxLines);
		root->addWidget(text, 1);

		status = new QLabel(" ");
		status->setFont(MonoFont());
		root->addWidget(status);

		connect(toggle_button, &QPushButton::clicked, this, [this] { DebugViewers::ToggleLogging(this); });
		connect(file_button, &QPushButton::clicked, this, [this] { DebugViewers::ChooseLogFile(this); });
		connect(clear_button, &QPushButton::clicked, this, [this] {
			lines.clear();
			text->clear();
			Debug_LogClear();
		});
		connect(save_button, &QPushButton::clicked, this, [this] { Save(); });
		connect(filter_edit, &QLineEdit::textChanged, this, [this] { Refilter(); });

		timer = new QTimer(this);
		connect(timer, &QTimer::timeout, this, [this] { Pull(); });
		timer->start(100);
		resize(900, 520);
		Pull();
	}

protected:
	void showEvent(QShowEvent *e) override {
		QWidget::showEvent(e);
		Pull();
	}

private:
	static const int kMaxLines = 50000;

	void Pull() {
		toggle_button->setText(debug_log_active ? "Stop Logging" : "Start Logging");
		const char *file = Debug_LogFile();
		QString where = file ? QString("writing to %1").arg(file) : (g_log_file.isEmpty() ? "memory only" : QString("file: %1 (used when logging starts)").arg(g_log_file));
		status->setText(QString("%1  |  %2  |  %3 lines").arg(debug_log_active ? "LOGGING" : "stopped").arg(where).arg(lines.size()));
		if (!isVisible())
			return;

		DebugLogEntry batch[256];
		int n;
		QStringList added;
		while ((n = Debug_LogRead(&next_seq, batch, 256)) > 0) {
			for (int i = 0; i < n; i++) {
				QString line = QString("[%1] %2  %3").arg(batch[i].frame, 6).arg(batch[i].category, -7).arg(batch[i].text);
				lines.push_back(line);
				if (Matches(line))
					added << line;
			}
		}
		while ((int)lines.size() > kMaxLines)
			lines.pop_front();
		if (!added.isEmpty()) {
			for (const QString &l : added)
				text->appendPlainText(l);
			if (scroll_box->isChecked())
				text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
		}
	}

	bool Matches(const QString &line) const {
		QString f = filter_edit->text().trimmed();
		return f.isEmpty() || line.contains(f, Qt::CaseInsensitive);
	}

	void Refilter() {
		text->clear();
		QStringList shown;
		for (const QString &l : lines)
			if (Matches(l))
				shown << l;
		text->setPlainText(shown.join('\n'));
		if (scroll_box->isChecked())
			text->verticalScrollBar()->setValue(text->verticalScrollBar()->maximum());
	}

	void Save() {
		QString path = QFileDialog::getSaveFileName(this, "Save log", "sonic-log.txt", "Text files (*.txt *.log);;All files (*)");
		if (path.isEmpty())
			return;
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QMessageBox::warning(this, "Save log", "Could not write " + path);
			return;
		}
		for (const QString &l : lines)
			f.write((l + "\n").toUtf8());
	}

	QPushButton *toggle_button;
	QCheckBox *scroll_box;
	QLineEdit *filter_edit;
	QPlainTextEdit *text;
	QLabel *status;
	QTimer *timer;
	std::deque<QString> lines;
	uint32_t next_seq = 1;
};

LogViewer *g_log_viewer = nullptr;

VdpViewer *g_vdp_viewer = nullptr;
SoundViewer *g_sound_viewer = nullptr;
ObjectViewer *g_object_viewer = nullptr;
VariableViewer *g_variable_viewer = nullptr;

void Raise(QWidget *w) {
	w->show();
	w->raise();
	w->activateWindow();
}

} // namespace

namespace DebugViewers {

void ShowVdpViewer(QWidget *parent) {
	if (g_vdp_viewer == nullptr)
		g_vdp_viewer = new VdpViewer(parent);
	Raise(g_vdp_viewer);
}

void ShowSoundViewer(QWidget *parent) {
	if (g_sound_viewer == nullptr)
		g_sound_viewer = new SoundViewer(parent);
	Raise(g_sound_viewer);
}

void ShowObjectViewer(QWidget *parent) {
	if (g_object_viewer == nullptr)
		g_object_viewer = new ObjectViewer(parent);
	Raise(g_object_viewer);
}

void ShowVariableViewer(QWidget *parent) {
	if (g_variable_viewer == nullptr)
		g_variable_viewer = new VariableViewer(parent);
	Raise(g_variable_viewer);
}

void ShowLogViewer(QWidget *parent) {
	if (g_log_viewer == nullptr)
		g_log_viewer = new LogViewer(parent);
	Raise(g_log_viewer);
}

QStringList OpenViewers() {
	QStringList open;
	if (g_vdp_viewer != nullptr && g_vdp_viewer->isVisible())
		open << "VDP viewer";
	if (g_sound_viewer != nullptr && g_sound_viewer->isVisible())
		open << "sound viewer";
	if (g_variable_viewer != nullptr && g_variable_viewer->isVisible())
		open << "variables";
	if (g_object_viewer != nullptr && g_object_viewer->isVisible())
		open << "object viewer";
	if (g_log_viewer != nullptr && g_log_viewer->isVisible())
		open << "log viewer";
	return open;
}

bool IsLogging() {
	return debug_log_active;
}

void ToggleLogging(QWidget *parent) {
	if (debug_log_active) {
		Debug_LogStop();
		return;
	}
	QByteArray path = g_log_file.toLocal8Bit();
	if (!Debug_LogStart(path.isEmpty() ? nullptr : path.constData()))
		QMessageBox::warning(parent, "Logging", "Could not start logging (debug mode must be active, and the log file must be writable).");
	else if (g_log_viewer == nullptr)
		ShowLogViewer(parent); // show what is being logged the first time
}

void ChooseLogFile(QWidget *parent) {
	QString path = QFileDialog::getSaveFileName(parent, "Log to file", g_log_file.isEmpty() ? "sonic-log.txt" : g_log_file,
	                                            "Text files (*.txt *.log);;All files (*)");
	if (!path.isEmpty())
		g_log_file = path; // applies the next time logging starts
		Settings::SaveSoon();
}

void SetSoundSnapshot(bool active, const Z80PeekData *data) {
	if (active && data != nullptr) {
		g_snapshot = *data;
		g_snapshot_valid = true;
	}
}

} // namespace DebugViewers
