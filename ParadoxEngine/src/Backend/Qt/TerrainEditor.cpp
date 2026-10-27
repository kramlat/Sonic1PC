#include "TerrainEditor.h"

#include "../../DebugPeek.h"
#include "../../EditorLive.h"
extern "C" {
#include "../../Kosinski.h"
}

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QHash>
#include <QFile>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>

#include <cstdlib>
#include <functional>

extern "C" {
extern int screen_width, screen_height; // the picture's size (Video.c)
uint8_t *VDP_TileSpace(void); // (VDP.h: not included, it has C++ keywords in it) the tile space, VRAM_SIZE bytes
}

namespace TerrainEditor {

namespace {

// A page of tabs: scrolls when the window is too small for its tools
QScrollArea *Page(QWidget *page) {
	auto *area = new QScrollArea();
	area->setWidgetResizable(true);
	area->setFrameShape(QFrame::NoFrame);
	area->setWidget(page);
	return area;
}

// A note under a tool: it wraps to the window's width
QLabel *Note(const QString &text, QWidget *parent) {
	auto *label = new QLabel(text, parent);
	label->setWordWrap(true);
	return label;
}

void InvalidateThumbs();

constexpr int VRAM_SIZE = 0x10000; // the tile space (VDP.h)
constexpr int kTileZoom = 2, kBlockZoom = 2, kChunkThumb = 64;

struct State {
	bool dirty = false;
	int tile = 0, block = 0, chunk = 0;       // what is selected in each tab
	int palette_line = 1, colour = 1;         // the line the tiles are shown in and the colour the pencil paints
	int block_cell = 0;                       // which of a block's four tiles is being edited
	int sel_line = 1, sel_index = 1;          // the palette's selection
	quint64 change_count = 0;                 // counts the changes (what caches go by)
	bool block_night = false;                 // the Blocks tab in night mode: the tiles that are not high priority darkened
	int solid_view = 1;                       // the solids drawn over the chunk's blocks: 0 none, 1 path 1, 2 path 2
	QWidget *palette_pane = nullptr, *tile_grid = nullptr, *tile_canvas = nullptr, *block_grid = nullptr, *block_canvas = nullptr, *chunk_grid = nullptr, *chunk_canvas = nullptr, *solid_pane = nullptr;
	std::function<void()> refresh_all;
} s;

QColor Colour(uint16_t word) {
	return QColor(((word >> 1) & 7) * 255 / 7, ((word >> 5) & 7) * 255 / 7, ((word >> 9) & 7) * 255 / 7);
}

uint16_t Be16(const uint8_t *p) { return uint16_t((p[0] << 8) | p[1]); }
void PutBe16(uint8_t *p, uint16_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }

int TileCount() { size_t n; Editor_Art(&n); return int(n / 32); }
int BlockCount() { size_t n; Editor_Blocks(&n); return int(n / 8); }
int ChunkCount() { size_t n; Editor_Chunks(&n); return int(n / 128); }

// An index as the tools show it: 16 ($10) of 255 (the last one there is)
QString IndexText(int i, int count) {
	return QString("%1 ($%2) of %3").arg(i).arg(QString::number(i, 16).toUpper()).arg(qMax(0, count - 1));
}

void Changed(bool redraw) {
	InvalidateThumbs();
	s.change_count++;
	s.dirty = true;
	if (redraw)
		Editor_RedrawLevel();
	if (s.refresh_all)
		s.refresh_all();
}

// The chunk and plane pictures draw their tiles from the video memory, where the level's animated art (the cactus tops, waterfalls, ...) moves, not from the art as loaded
bool live_tiles = false;
struct LiveTiles {
	LiveTiles() { live_tiles = true; }
	~LiveTiles() { live_tiles = false; }
};

// A tile as a picture: palette line, flips and the transparent colour 0 shown as a check
QImage TileImage(int tile, int line, bool x_flip, bool y_flip) {
	QImage image(8, 8, QImage::Format_ARGB32);
	size_t bytes;
	const uint8_t *art = Editor_Art(&bytes);
	if (live_tiles && tile >= 0 && tile < VRAM_SIZE / 32) {
		art = VDP_TileSpace();
		bytes = VRAM_SIZE;
	}
	for (int y = 0; y < 8; y++) {
		for (int x = 0; x < 8; x++) {
			int px = x_flip ? 7 - x : x, py = y_flip ? 7 - y : y;
			int c = 0;
			if (art != nullptr && size_t(tile + 1) * 32 <= bytes) {
				const uint8_t b = art[tile * 32 + py * 4 + px / 2];
				c = (px & 1) ? (b & 15) : (b >> 4);
			}
			image.setPixelColor(x, y, c == 0 ? QColor(40, 40, 48) : Colour(Editor_GetColour(line, c)));
		}
	}
	return image;
}

bool priority_dim = false; // the planes' priority view: the tiles that are not high priority are darkened (the shadow of the VDP's shadow/highlight mode: the night mode Sega tested priority with)

void DrawTileWord(QPainter *p, uint16_t word, const QRect &at) {
	const QImage image = TileImage(word & 0x7FF, (word >> 13) & 3, word & 0x0800, word & 0x1000);
	p->drawImage(at, image);
	if (priority_dim && !(word & 0x8000))
		p->fillRect(at, QColor(0, 0, 40, 165));
}

void DrawBlock(QPainter *p, int block, const QRect &at, bool x_flip = false, bool y_flip = false) {
	size_t bytes;
	const uint8_t *blocks = Editor_Blocks(&bytes);
	if (blocks == nullptr || size_t(block + 1) * 8 > bytes)
		return;
	const int half = at.width() / 2;
	for (int ty = 0; ty < 2; ty++) {
		for (int tx = 0; tx < 2; tx++) {
			uint16_t word = Be16(blocks + block * 8 + (ty * 2 + tx) * 2);
			if (x_flip)
				word ^= 0x0800;
			if (y_flip)
				word ^= 0x1000;
			const int X = x_flip ? 1 - tx : tx, Y = y_flip ? 1 - ty : ty;
			DrawTileWord(p, word, QRect(at.x() + X * half, at.y() + Y * half, half, half));
		}
	}
}

void DrawChunk(QPainter *p, int chunk, const QRect &at) {
	LiveTiles live;
	size_t bytes;
	const uint8_t *chunks = Editor_Chunks(&bytes);
	if (chunks == nullptr || size_t(chunk + 1) * 128 > bytes)
		return;
	const int cell = at.width() / 8;
	for (int by = 0; by < 8; by++)
		for (int bx = 0; bx < 8; bx++) {
			const uint16_t word = Be16(chunks + chunk * 128 + (by * 8 + bx) * 2);
			DrawBlock(p, word & 0x3FF, QRect(at.x() + bx * cell, at.y() + by * cell, cell, cell), word & 0x0400, word & 0x0800);
		}
}

// Blocks that some chunk marks solid on a path while the block has no collision curve there (the level walks through them): what the Blocks tab outlines in orange, to be given a curve in the Solids tab
bool NeedsCurve(int block, int path) {
	static QVector<uint8_t> need[2];
	static quint64 stamp = ~0ull;
	size_t bytes;
	const uint8_t *chunks = Editor_Chunks(&bytes);
	const quint64 now = quint64(s.change_count);
	if (now != stamp) {
		stamp = now;
		for (int path_i = 0; path_i < 2; path_i++) {
			need[path_i].fill(0, 0x400);
			size_t sb;
			const uint8_t *solids = Editor_Solids(path_i, &sb);
			for (size_t i = 0; chunks != nullptr && solids != nullptr && i + 1 < bytes; i += 2) {
				const uint16_t w = Be16(chunks + i);
				const int b = w & 0x3FF;
				if (((w >> (12 + path_i * 2)) & 3) != 0 && b != 0 && size_t(b) < sb && solids[b] == 0)
					need[path_i][b] = 1;
			}
		}
	}
	return block >= 0 && block < 0x400 && need[path & 1][block];
}

// A grid of things in a scroll area: `count` cells of `cell` pixels, `columns` to a row; a click selects
class Grid : public QWidget {
public:
	Grid(int columns, int cell, std::function<int()> count, std::function<void(QPainter *, int, const QRect &)> draw, int *selected, std::function<void()> picked)
	    : columns_(columns), cell_(cell), count_(std::move(count)), draw_(std::move(draw)), selected_(selected), picked_(std::move(picked)) {}

	QSize sizeHint() const override { return QSize(columns_ * cell_ + 1, qMax(1, (count_() + columns_ - 1) / columns_) * cell_ + 1); }
	void Resize() { setFixedSize(sizeHint()); update(); }

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.fillRect(rect(), QColor(24, 24, 28));
		for (int i = 0; i < count_(); i++) {
			const QRect at((i % columns_) * cell_, (i / columns_) * cell_, cell_, cell_);
			if (!at.intersects(visibleRegion().boundingRect()))
				continue;
			draw_(&p, i, at);
			if (i == *selected_) {
				p.setPen(QPen(QColor(255, 160, 0), 2));
				p.setBrush(Qt::NoBrush);
				p.drawRect(at.adjusted(1, 1, -1, -1));
			}
		}
	}
	void mousePressEvent(QMouseEvent *e) override {
		const int i = (e->position().toPoint().y() / cell_) * columns_ + e->position().toPoint().x() / cell_;
		if (i >= 0 && i < count_()) {
			*selected_ = i;
			picked_();
			update();
		}
	}

private:
	int columns_, cell_;
	std::function<int()> count_;
	std::function<void(QPainter *, int, const QRect &)> draw_;
	int *selected_;
	std::function<void()> picked_;
};

QScrollArea *Scroll(QWidget *inner, QWidget *parent) {
	auto *area = new QScrollArea(parent);
	area->setWidget(inner);
	area->setWidgetResizable(false);
	return area;
}

// ---- The palette: four lines of sixteen colours and three sliders ----
class PalettePane : public QWidget {
public:
	PalettePane() {
		auto *layout = new QVBoxLayout(this);
		swatches = new QWidget(this);
		swatches->setFixedSize(16 * 26 + 1, 4 * 26 + 1);
		swatches->installEventFilter(this);
		layout->addWidget(swatches);
		auto *row = new QHBoxLayout();
		for (int c = 0; c < 3; c++) {
			spin[c] = new QSpinBox(this);
			spin[c]->setRange(0, 7);
			spin[c]->setPrefix(QString("RGB").mid(c, 1) + " ");
			row->addWidget(spin[c]);
			connect(spin[c], qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { Apply(); });
		}
		hex = new QLabel(this);
		row->addWidget(hex);
		row->addStretch();
		layout->addLayout(row);
		layout->addWidget(Note("Colour 0 of a line is the transparent one. A colour set here stays as set (palette cycles do not take it back) until the editor closes.", this));
		layout->addStretch();
		Load();
	}

	void Load() {
		const uint16_t w = Editor_GetBaseColour(s.sel_line, s.sel_index); // (the palette as it is: the placeholders of a palette cycle show as they are)
		loading = true;
		spin[0]->setValue((w >> 1) & 7);
		spin[1]->setValue((w >> 5) & 7);
		spin[2]->setValue((w >> 9) & 7);
		loading = false;
		hex->setText(QString("Line %1, colour %2: $%3").arg(s.sel_line).arg(s.sel_index).arg(w, 4, 16, QChar('0')).toUpper());
		swatches->update();
	}

protected:
	bool eventFilter(QObject *o, QEvent *e) override {
		if (o != swatches)
			return false;
		if (e->type() == QEvent::Paint) {
			QPainter p(swatches);
			for (int line = 0; line < 4; line++)
				for (int i = 0; i < 16; i++) {
					const QRect at(i * 26, line * 26, 26, 26);
					p.fillRect(at.adjusted(1, 1, -1, -1), Colour(Editor_GetBaseColour(line, i)));
					if (line == s.sel_line && i == s.sel_index) {
						p.setPen(QPen(Qt::white, 2));
						p.drawRect(at.adjusted(2, 2, -2, -2));
					}
				}
			return true;
		}
		if (e->type() == QEvent::MouseButtonPress) {
			const QPoint at = static_cast<QMouseEvent *>(e)->position().toPoint();
			s.sel_line = qBound(0, at.y() / 26, 3);
			s.sel_index = qBound(0, at.x() / 26, 15);
			s.palette_line = s.sel_line;
			s.colour = s.sel_index;
			Load();
			if (s.refresh_all)
				s.refresh_all();
			return true;
		}
		return false;
	}

private:
	void Apply() {
		if (loading)
			return;
		Editor_SetColour(s.sel_line, s.sel_index, uint16_t((spin[0]->value() << 1) | (spin[1]->value() << 5) | (spin[2]->value() << 9)));
		InvalidateThumbs();
		s.dirty = true;
		Load();
		if (s.refresh_all)
			s.refresh_all();
	}
	QWidget *swatches;
	QSpinBox *spin[3];
	QLabel *hex;
	bool loading = false;
};

// ---- The tile's pixels: click to paint, right click to pick the colour ----
class TileCanvas : public QWidget {
public:
	TileCanvas() { setFixedSize(8 * 28, 8 * 28); }

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		p.drawImage(rect(), TileImage(s.tile, s.palette_line, false, false).scaled(8, 8));
		p.setPen(QColor(0, 0, 0, 90));
		for (int i = 0; i <= 8; i++) {
			p.drawLine(i * 28, 0, i * 28, height());
			p.drawLine(0, i * 28, width(), i * 28);
		}
	}
	void mousePressEvent(QMouseEvent *e) override { Use(e); }
	void mouseMoveEvent(QMouseEvent *e) override { Use(e); }

private:
	void Use(QMouseEvent *e) {
		const int x = e->position().toPoint().x() / 28, y = e->position().toPoint().y() / 28;
		if (x < 0 || x > 7 || y < 0 || y > 7)
			return;
		size_t bytes;
		uint8_t *art = Editor_Art(&bytes);
		if (art == nullptr || size_t(s.tile + 1) * 32 > bytes)
			return;
		uint8_t &b = art[s.tile * 32 + y * 4 + x / 2];
		if (e->buttons() & Qt::RightButton) { // (pick)
			s.colour = (x & 1) ? (b & 15) : (b >> 4);
			if (s.refresh_all)
				s.refresh_all();
			return;
		}
		if (!(e->buttons() & Qt::LeftButton))
			return;
		b = (x & 1) ? uint8_t((b & 0xF0) | s.colour) : uint8_t((b & 0x0F) | (s.colour << 4));
		Editor_CommitArt(s.tile);
		Changed(false);
	}
};

void FlipTile(bool horizontal) {
	size_t bytes;
	uint8_t *art = Editor_Art(&bytes);
	if (art == nullptr || size_t(s.tile + 1) * 32 > bytes)
		return;
	uint8_t px[64];
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x++) {
			const uint8_t b = art[s.tile * 32 + y * 4 + x / 2];
			px[y * 8 + x] = (x & 1) ? (b & 15) : (b >> 4);
		}
	for (int y = 0; y < 8; y++)
		for (int x = 0; x < 8; x += 2) {
			const int sx0 = horizontal ? 7 - x : x, sx1 = horizontal ? 6 - x : x + 1, sy = horizontal ? y : 7 - y;
			art[s.tile * 32 + y * 4 + x / 2] = uint8_t((px[sy * 8 + sx0] << 4) | px[sy * 8 + sx1]);
		}
	Editor_CommitArt(s.tile);
	Changed(false);
}

// The colours of the line the tiles are shown in, under the tile: the pencil's colour is the selected one
class Strip : public QWidget {
public:
	Strip() { setFixedSize(16 * 20 + 1, 22); }

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		for (int i = 0; i < 16; i++) {
			const QRect at(i * 20, 0, 20, 22);
			p.fillRect(at.adjusted(1, 1, -1, -1), i == 0 ? QColor(40, 40, 48) : Colour(Editor_GetColour(s.palette_line, i)));
			if (i == s.colour) {
				p.setPen(QPen(Qt::white, 2));
				p.drawRect(at.adjusted(2, 2, -2, -2));
			}
		}
	}
	void mousePressEvent(QMouseEvent *e) override {
		s.colour = qBound(0, e->position().toPoint().x() / 20, 15);
		if (s.refresh_all)
			s.refresh_all();
	}
};

// ---- A block: four tile words ----
class BlockCanvas : public QWidget {
public:
	BlockCanvas() { setFixedSize(2 * 72, 2 * 72); }

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		size_t bytes;
		const uint8_t *blocks = Editor_Blocks(&bytes);
		if (blocks == nullptr || size_t(s.block + 1) * 8 > bytes)
			return;
		for (int c = 0; c < 4; c++) {
			const QRect at((c % 2) * 72, (c / 2) * 72, 72, 72);
			priority_dim = s.block_night;
			DrawTileWord(&p, Be16(blocks + s.block * 8 + c * 2), at);
			priority_dim = false;
			p.setPen(QPen(c == s.block_cell ? QColor(255, 160, 0) : QColor(0, 0, 0, 120), 2));
			p.setBrush(Qt::NoBrush);
			p.drawRect(at.adjusted(1, 1, -1, -1));
		}
	}
	void mousePressEvent(QMouseEvent *e) override {
		s.block_cell = qBound(0, e->position().toPoint().y() / 72, 1) * 2 + qBound(0, e->position().toPoint().x() / 72, 1);
		if (s.refresh_all)
			s.refresh_all();
	}
};

// The solidity of a chunk's block over its picture, for the path being shown (1 or 2): the colour follows the shape of the block's collision curve on that path (its flips as the chunk has them), semi-transparent:
// top solid is clear white, left and right solid yellow with the bottom edge in red, all solid black. A block whose curve is empty has no shape, so nothing is drawn.
void DrawSolidOverlay(QPainter *p, uint16_t word, int path, const QRect &at) {
	const int bits = path == 2 ? (word >> 14) & 3 : (word >> 12) & 3;
	if (bits == 0)
		return;
	size_t bytes;
	const uint8_t *solids = Editor_Solids(path == 2 ? 1 : 0, &bytes);
	const uint8_t *heights = Editor_HeightMap();
	const int block = word & 0x3FF;
	if (solids == nullptr || heights == nullptr || size_t(block) >= bytes)
		return;
	const int curve = solids[block];
	if (curve == 0)
		return;
	const bool x_flip = word & 0x0400, y_flip = word & 0x0800;
	const double cw = at.width() / 16.0, ch = at.height() / 16.0;
	QPainterPath shape;
	for (int x = 0; x < 16; x++) {
		int h = heights[curve * 16 + (x_flip ? 15 - x : x)];
		h = h <= 16 ? h : (h >= 0xF0 ? -(256 - h) : 16); // (a negative height hangs from the top)
		if (h == 0)
			continue;
		const bool from_bottom = (h > 0) != y_flip;
		const int rows = h > 0 ? h : -h;
		shape.addRect(QRectF(at.x() + x * cw, from_bottom ? at.y() + at.height() - rows * ch : at.y(), cw, rows * ch));
	}
	p->setPen(Qt::NoPen);
	if (bits == 1) { // top only
		p->fillPath(shape, QColor(255, 255, 255, 130));
	} else if (bits == 2) { // left, right and bottom
		p->fillPath(shape, QColor(255, 230, 0, 130));
		QPainterPath bottom;
		bottom.addRect(QRectF(at.x(), y_flip ? at.y() : at.y() + at.height() * 0.75, at.width(), at.height() * 0.25));
		p->fillPath(shape.intersected(bottom), QColor(255, 40, 40, 170));
	} else { // all
		p->fillPath(shape, QColor(0, 0, 0, 150));
	}
}

// The chunk: eight by eight blocks. Left click places the selected block, right click picks the one under it
class ChunkCanvas : public QWidget {
public:
	std::function<uint16_t()> flags; // the flips and solidity of what a click places (the word's bits 10-15)
	std::function<void(uint16_t)> picked_flags;

	ChunkCanvas() { setFixedSize(8 * 40, 8 * 40); }

protected:
	void paintEvent(QPaintEvent *) override {
		QPainter p(this);
		DrawChunk(&p, s.chunk, rect());
		p.setPen(QColor(0, 0, 0, 90));
		for (int i = 0; i <= 8; i++) {
			p.drawLine(i * 40, 0, i * 40, height());
			p.drawLine(0, i * 40, width(), i * 40);
		}
		// the solids of the path being shown, over the blocks
		size_t bytes;
		const uint8_t *chunks = Editor_Chunks(&bytes);
		if (s.solid_view == 0 || chunks == nullptr || size_t(s.chunk + 1) * 128 > bytes)
			return;
		for (int i = 0; i < 64; i++)
			DrawSolidOverlay(&p, Be16(chunks + s.chunk * 128 + i * 2), s.solid_view, QRect((i % 8) * 40, (i / 8) * 40, 40, 40));
	}
	void mousePressEvent(QMouseEvent *e) override { Use(e); }
	void mouseMoveEvent(QMouseEvent *e) override { Use(e); }

private:
	void Use(QMouseEvent *e) {
		const int x = e->position().toPoint().x() / 40, y = e->position().toPoint().y() / 40;
		if (x < 0 || x > 7 || y < 0 || y > 7)
			return;
		size_t bytes;
		uint8_t *chunks = Editor_Chunks(&bytes);
		if (chunks == nullptr || size_t(s.chunk + 1) * 128 > bytes)
			return;
		uint8_t *at = chunks + s.chunk * 128 + (y * 8 + x) * 2;
		if (e->buttons() & Qt::RightButton) {
			const uint16_t w = Be16(at);
			s.block = w & 0x3FF;
			if (picked_flags)
				picked_flags(w & 0xFC00);
			if (s.refresh_all)
				s.refresh_all();
			return;
		}
		if (!(e->buttons() & Qt::LeftButton))
			return;
		const uint16_t word = uint16_t((s.block & 0x3FF) | (flags ? flags() : 0));
		if (Be16(at) == word)
			return;
		PutBe16(at, word);
		Changed(true);
	}
};

// ---- The solids: the collision curve each block has on each path, and the curve ----
class SolidPane : public QWidget {
public:
	SolidPane() {
		auto *layout = new QVBoxLayout(this);
		info = new QLabel(this);
		layout->addWidget(info);
		auto *row = new QHBoxLayout();
		for (int path = 0; path < 2; path++) {
			spin[path] = new QSpinBox(this);
			spin[path]->setRange(0, 255);
			spin[path]->setPrefix(path == 0 ? "Path A curve " : "Path B curve ");
			row->addWidget(spin[path]);
			connect(spin[path], qOverload<int>(&QSpinBox::valueChanged), this, [this, path](int v) {
				if (loading)
					return;
				size_t bytes;
				uint8_t *solids = Editor_Solids(path, &bytes);
				if (solids != nullptr && size_t(s.block) < bytes) {
					solids[s.block] = uint8_t(v);
					Changed(false);
				}
			});
		}
		same = new QPushButton("Copy path A to path B", this);
		connect(same, &QPushButton::clicked, this, [this] {
			size_t b0, b1;
			uint8_t *p0 = Editor_Solids(0, &b0), *p1 = Editor_Solids(1, &b1);
			if (p0 && p1 && size_t(s.block) < b0 && size_t(s.block) < b1) {
				p1[s.block] = p0[s.block];
				Changed(false);
			}
		});
		row->addWidget(same);
		row->addStretch();
		layout->addLayout(row);
		view = new QWidget(this);
		view->setFixedSize(2 * (16 * 8 + 24), 16 * 8 + 20);
		view->installEventFilter(this);
		layout->addWidget(view);
		layout->addWidget(Note("Pick a block in the Blocks tab. The block is drawn at 8 times its size with its collision curve over it, a pixel of the block for each 8 here: the curve's heights are how many pixels of each column are solid (from the bottom up; a curve of 0 is empty). Red outlines in the Blocks tab mark blocks that have a curve, orange ones blocks that a chunk marks solid but that have none (the level walks through them).", this));
		layout->addStretch();
	}

	void Load() {
		size_t b0, b1;
		const uint8_t *p0 = Editor_Solids(0, &b0), *p1 = Editor_Solids(1, &b1);
		loading = true;
		spin[0]->setEnabled(p0 && size_t(s.block) < b0);
		spin[1]->setEnabled(p1 && size_t(s.block) < b1);
		spin[0]->setValue(p0 && size_t(s.block) < b0 ? p0[s.block] : 0);
		spin[1]->setValue(p1 && size_t(s.block) < b1 ? p1[s.block] : 0);
		loading = false;
		info->setText(QString("Block %1").arg(s.block));
		view->update();
	}

protected:
	bool eventFilter(QObject *o, QEvent *e) override {
		if (o != view || e->type() != QEvent::Paint)
			return false;
		QPainter p(view);
		const uint8_t *heights = Editor_HeightMap();
		const uint8_t *angles = Editor_CurveAngles();
		for (int path = 0; path < 2; path++) {
			const int curve = spin[path]->value();
			const int x0 = path * (16 * 8 + 24);
			// the block as it is, a pixel of it for each 8 on the screen, and the curve laid over it (path 1 red, path 2 blue): the curve's height in a column is how many pixels of the block it fills
			p.fillRect(QRect(x0, 0, 16 * 8, 16 * 8), QColor(24, 24, 28));
			DrawBlock(&p, s.block, QRect(x0, 0, 16 * 8, 16 * 8));
			if (heights != nullptr)
				for (int x = 0; x < 16; x++) {
					int h = heights[curve * 16 + x];
					h = h <= 16 ? h : (h >= 0xF0 ? -(256 - h) : 16); // (a negative height hangs from the top)
					p.fillRect(QRect(x0 + x * 8, h >= 0 ? (16 - h) * 8 : 0, 8, h >= 0 ? h * 8 : -h * 8), path == 0 ? QColor(255, 40, 40, 130) : QColor(60, 120, 255, 130));
				}
			p.setPen(QColor(0, 0, 0, 60));
			for (int i = 0; i <= 16; i++) {
				p.drawLine(x0 + i * 8, 0, x0 + i * 8, 16 * 8);
				p.drawLine(x0, i * 8, x0 + 16 * 8, i * 8);
			}
			p.setPen(view->palette().color(QPalette::WindowText));
			p.drawText(QPoint(x0, 16 * 8 + 14), QString("%1: curve %2, angle $%3").arg(path == 0 ? "A" : "B").arg(curve).arg(QString::number(angles ? angles[curve] : 0, 16).toUpper(), 2, QChar('0')));
		}
		return true;
	}

private:
	QLabel *info;
	QSpinBox *spin[2];
	QPushButton *same;
	QWidget *view;
	bool loading = false;
};

// ---- The planes: the level's layout of chunks, foreground (plane A) and background (plane B) ----
QHash<int, QImage> thumb_cache; // chunk pictures by size and chunk, drawn once until something changes

void InvalidateThumbs() { thumb_cache.clear(); }

// The picture of a chunk, with what the plane's view adds to it: mode 0 none, 1 priority (the tiles that are not high priority darkened), 2 and 3 the solids of path A and B over the blocks
const QImage &ChunkThumb(int chunk, int size, int mode = 0) {
	const int key = mode * 1000000 + size * 1000 + chunk;
	auto found = thumb_cache.find(key);
	if (found == thumb_cache.end()) {
		QImage image(size, size, QImage::Format_ARGB32);
		image.fill(QColor(24, 24, 28));
		if (chunk != 0) {
			QImage full(128, 128, QImage::Format_ARGB32);
			full.fill(QColor(24, 24, 28));
			QPainter fp(&full);
			priority_dim = mode == 1;
			DrawChunk(&fp, chunk, QRect(0, 0, 128, 128));
			priority_dim = false;
			if (mode >= 2) {
				size_t chunk_bytes;
				const uint8_t *chunks = Editor_Chunks(&chunk_bytes);
				if (chunks != nullptr && size_t(chunk + 1) * 128 <= chunk_bytes)
					for (int i = 0; i < 64; i++)
						DrawSolidOverlay(&fp, Be16(chunks + chunk * 128 + i * 2), mode - 1, QRect((i % 8) * 16, (i / 8) * 16, 16, 16));
			}
			fp.end();
			QPainter p(&image);
			p.drawImage(QRect(0, 0, size, size), full);
		}
		found = thumb_cache.insert(key, image);
	}
	return found.value();
}

QCheckBox *guides_box = nullptr; // Plane A's "Objects and rings": what is drawn over the chunks to place them by

class LayerCanvas : public QWidget {
public:
	LayerCanvas(int plane, QLabel *where) : plane_(plane), where_(where) {
		setMouseTracking(true);
		SetZoom(128);
	}
	void SetMode(int mode) { // what is shown on the chunks: 0 nothing, 1 priority, 2 solids of path A, 3 of path B
		mode_ = mode;
		update();
	}
	void SetZoom(int zoom) {
		zoom_ = zoom;
		setFixedSize(128 * zoom_ + 1, 16 * zoom_ + 1);
		update();
	}

protected:
	void paintEvent(QPaintEvent *e) override {
		QPainter p(this);
		size_t bytes;
		const uint8_t *layout = Editor_Layout(&bytes);
		const QRect area = e->rect();
		for (int row = qMax(0, area.top() / zoom_); row <= qMin(15, area.bottom() / zoom_); row++)
			for (int col = qMax(0, area.left() / zoom_); col <= qMin(127, area.right() / zoom_); col++) {
				const int chunk = layout[row * 256 + plane_ * 128 + col];
				p.drawImage(col * zoom_, row * zoom_, ChunkThumb(chunk, zoom_, mode_));
			}
		p.setPen(QColor(0, 0, 0, 70));
		for (int c = area.left() / zoom_; c <= qMin(128, area.right() / zoom_ + 1); c++)
			p.drawLine(c * zoom_, 0, c * zoom_, height());
		for (int r = 0; r <= 16; r++)
			p.drawLine(0, r * zoom_, width(), r * zoom_);
		if (plane_ == 0 && guides_box != nullptr && guides_box->isChecked())
			DrawGuides(&p);
		// the camera (plane A) and the cursor
		EditorPeek peek;
		Peek_EditorState(&peek);
		int cx, cy;
		Editor_GetCursor(&cx, &cy);
		if (plane_ == 0) {
			p.setPen(QPen(QColor(0, 255, 255), 2));
			p.setBrush(Qt::NoBrush);
			p.drawRect(QRectF(peek.camera_x * zoom_ / 128.0, peek.camera_y * zoom_ / 128.0, screen_width * zoom_ / 128.0, screen_height * zoom_ / 128.0));
		}
		p.setPen(QPen(QColor(255, 255, 80), 2));
		const QPointF at(cx * zoom_ / 128.0, cy * zoom_ / 128.0);
		p.drawLine(QPointF(at.x() - 6, at.y()), QPointF(at.x() + 6, at.y()));
		p.drawLine(QPointF(at.x(), at.y() - 6), QPointF(at.x(), at.y() + 6));
	}
	void mousePressEvent(QMouseEvent *e) override { Use(e); }
	void mouseMoveEvent(QMouseEvent *e) override { Use(e); }

private:
	// The level's objects (a box and their id) and rings, where they are, so that chunks can be placed by them
	void DrawGuides(QPainter *p) {
		const EditorLevel &level = editor_session.level;
		const double k = zoom_ / 128.0;
		const double half = qMax(3.0, 8 * k);
		QFont font = p->font();
		font.setPixelSize(qMax(8, int(12 * k)));
		p->setFont(font);
		for (int i = 0; i < level.object_count; i++) {
			const EditorObject &o = level.objects[i];
			const QPointF at(o.x * k, o.y * k);
			p->setPen(QPen(QColor(0, 255, 255), 1));
			p->setBrush(QColor(0, 255, 255, 50));
			p->drawRect(QRectF(at.x() - half, at.y() - half, half * 2, half * 2));
			p->drawText(QPointF(at.x() - half, at.y() - half - 2), QString::number(o.id, 16).toUpper().rightJustified(2, '0'));
		}
		p->setBrush(QColor(255, 220, 0, 70));
		p->setPen(QPen(QColor(255, 220, 0), 1));
		for (int i = 0; i < level.ring_count; i++) {
			const EditorRing &r = level.rings[i];
			for (int n = 0; n < r.count; n++)
				p->drawEllipse(QPointF((r.vertical ? r.x : r.x + n * 0x18) * k, (r.vertical ? r.y + n * 0x18 : r.y) * k), half * 0.8, half * 0.8);
		}
	}
	void Use(QMouseEvent *e) {
		const int col = e->position().toPoint().x() / zoom_, row = e->position().toPoint().y() / zoom_;
		if (col < 0 || col > 127 || row < 0 || row > 15)
			return;
		size_t bytes;
		uint8_t *layout = Editor_Layout(&bytes);
		uint8_t &at = layout[row * 256 + plane_ * 128 + col];
		where_->setText(QString("Plane %1  column %2  row %3  x $%4  y $%5  chunk %6").arg(plane_ == 0 ? "A" : "B").arg(col).arg(row).arg(QString::number(col * 128, 16).toUpper().rightJustified(4, '0')).arg(QString::number(row * 128, 16).toUpper().rightJustified(3, '0')).arg(IndexText(at, ChunkCount())));
		if (e->buttons() & Qt::RightButton) { // picks the chunk
			s.chunk = at;
			if (s.refresh_all)
				s.refresh_all();
			return;
		}
		if (!(e->buttons() & Qt::LeftButton))
			return;
		if (e->modifiers() & Qt::ControlModifier) { // takes the cursor (and so the camera) there
			Editor_SetCursor(col * 128 + 64, row * 128 + 64);
			update();
			return;
		}
		if (at == s.chunk)
			return;
		at = uint8_t(s.chunk);
		InvalidateThumbs();
		s.dirty = true;
		Editor_RedrawLevel();
		update();
	}
	int plane_;
	QLabel *where_;
	int zoom_ = 128;
	int mode_ = 0;
};

} // namespace

bool Dirty() { return s.dirty; }

void Reset() {
	s.dirty = false;
	s.tile = s.block = s.chunk = 0;
	if (s.refresh_all)
		s.refresh_all();
}

void AddTabs(QTabWidget *tabs) {
	if (s.refresh_all == nullptr) {
		static QVector<QWidget *> animated; // what shows the palette's colours: redrawn as a palette cycle moves them
		auto *parent = tabs;
		(void)parent;
		// Palette
		auto *palette = new PalettePane();
		tabs->addTab(Page(palette), "Palette");

		// Tiles
		auto *tile_page = new QWidget();
		auto *tile_layout = new QHBoxLayout(tile_page);
		auto *tile_grid = new Grid(16, 8 * kTileZoom + 1, [] { return TileCount(); },
		                           [](QPainter *p, int i, const QRect &at) { p->drawImage(at.adjusted(0, 0, -1, -1), TileImage(i, s.palette_line, false, false)); }, &s.tile, [] { if (s.refresh_all) s.refresh_all(); });
		animated << tile_grid;
		tile_layout->addWidget(Scroll(tile_grid, tile_page), 1);
		auto *tile_side = new QVBoxLayout();
		auto *tile_label = new QLabel(tile_page);
		auto *line_box = new QComboBox(tile_page);
		for (int i = 0; i < 4; i++)
			line_box->addItem(QString("Palette line %1").arg(i));
		line_box->setCurrentIndex(s.palette_line);
		auto *tile_canvas = new TileCanvas();
		animated << tile_canvas;
		auto *strip = new Strip();
		animated << strip;
		auto *flip_row = new QHBoxLayout();
		auto *flip_h = new QPushButton("Flip left-right", tile_page);
		auto *flip_v = new QPushButton("Flip up-down", tile_page);
		flip_row->addWidget(flip_h);
		flip_row->addWidget(flip_v);
		tile_side->addWidget(tile_label);
		tile_side->addWidget(line_box);
		tile_side->addWidget(tile_canvas);
		tile_side->addWidget(strip);
		tile_side->addLayout(flip_row);
		tile_side->addWidget(Note("Left click paints with the colour picked below, right click picks the colour under the pointer.", tile_page));
		tile_side->addStretch();
		tile_layout->addLayout(tile_side);
		tabs->addTab(Page(tile_page), "Tiles");
		QObject::connect(line_box, qOverload<int>(&QComboBox::currentIndexChanged), tile_page, [](int line) { s.palette_line = line; if (s.refresh_all) s.refresh_all(); });
		QObject::connect(flip_h, &QPushButton::clicked, tile_page, [] { FlipTile(true); });
		QObject::connect(flip_v, &QPushButton::clicked, tile_page, [] { FlipTile(false); });


		// Blocks
		auto *block_page = new QWidget();
		auto *block_layout = new QHBoxLayout(block_page);
		auto *block_grid = new Grid(16, 16 * kBlockZoom + 1, [] { return BlockCount(); },
		                            [](QPainter *p, int i, const QRect &at) {
			                            priority_dim = s.block_night;
			                            DrawBlock(p, i, at.adjusted(0, 0, -1, -1));
			                            priority_dim = false;
			                            size_t bytes;
			                            const uint8_t *solids = Editor_Solids(0, &bytes);
			                            if (solids != nullptr && size_t(i) < bytes && solids[i] != 0) {
				                            p->setPen(QPen(QColor(255, 60, 60), 1));
				                            p->setBrush(Qt::NoBrush);
				                            p->drawRect(at.adjusted(1, 1, -2, -2));
			                            } else if (NeedsCurve(i, 0) || NeedsCurve(i, 1)) { // (marked solid by a chunk but with no curve)
				                            p->setPen(QPen(QColor(255, 150, 0), 2));
				                            p->setBrush(Qt::NoBrush);
				                            p->drawRect(at.adjusted(1, 1, -2, -2));
			                            }
		                            },
		                            &s.block, [] { if (s.refresh_all) s.refresh_all(); });
		animated << block_grid;
		block_layout->addWidget(Scroll(block_grid, block_page), 1);
		auto *block_side = new QVBoxLayout();
		auto *block_label = new QLabel(block_page);
		auto *block_canvas = new BlockCanvas();
		animated << block_canvas;
		block_side->addWidget(block_label);
		block_side->addWidget(block_canvas);
		auto *tile_spin = new QSpinBox(block_page);
		tile_spin->setRange(0, 0x7FF);
		tile_spin->setPrefix("Tile ");
		auto *line_spin = new QSpinBox(block_page);
		line_spin->setRange(0, 3);
		line_spin->setPrefix("Palette line ");
		auto *x_flip = new QCheckBox("X flip", block_page);
		auto *y_flip = new QCheckBox("Y flip", block_page);
		auto *priority = new QCheckBox("High priority", block_page);
		auto *use_tile = new QPushButton("Use the tile selected in Tiles", block_page);
		auto *night = new QCheckBox("Night mode (darken the tiles that are not high priority)", block_page);
		s.block_night = s.block_night || getenv("SONIC_QT_BLOCKNIGHT") != nullptr; // (the env var: for the automated checks)
		night->setChecked(s.block_night);
		QObject::connect(night, &QCheckBox::toggled, block_page, [](bool on) { s.block_night = on; if (s.refresh_all) s.refresh_all(); });
		for (QWidget *widget : std::initializer_list<QWidget *>{ night, tile_spin, line_spin, x_flip, y_flip, priority, use_tile })
			block_side->addWidget(widget);
		block_side->addStretch();
		block_layout->addLayout(block_side);
		tabs->addTab(Page(block_page), "Blocks");

		// Chunks
		auto *chunk_page = new QWidget();
		auto *chunk_layout = new QHBoxLayout(chunk_page);
		auto *chunk_grid = new Grid(4, kChunkThumb + 1, [] { return ChunkCount(); },
		                            [](QPainter *p, int i, const QRect &at) { DrawChunk(p, i, QRect(at.x(), at.y(), kChunkThumb, kChunkThumb)); }, &s.chunk, [] { if (s.refresh_all) s.refresh_all(); });
		animated << chunk_grid;
		chunk_layout->addWidget(Scroll(chunk_grid, chunk_page), 0);
		auto *chunk_side = new QVBoxLayout();
		auto *chunk_label = new QLabel(chunk_page);
		auto *chunk_canvas = new ChunkCanvas();
		animated << chunk_canvas;
		auto *cx = new QCheckBox("X flip", chunk_page);
		auto *cy = new QCheckBox("Y flip", chunk_page);
		auto *s1t = new QCheckBox("Path A top solid", chunk_page);
		auto *s1l = new QCheckBox("Path A sides and bottom solid", chunk_page);
		auto *s2t = new QCheckBox("Path B top solid", chunk_page);
		auto *s2l = new QCheckBox("Path B sides and bottom solid", chunk_page);
		auto *solid_box = new QComboBox(chunk_page);
		solid_box->addItems({ "Solids shown: none", "Solids shown: path A (primary)", "Solids shown: path B (secondary)" });
		solid_box->setCurrentIndex(s.solid_view);
		QObject::connect(solid_box, qOverload<int>(&QComboBox::currentIndexChanged), chunk_page, [](int v) { s.solid_view = v; if (s.refresh_all) s.refresh_all(); });
		chunk_side->addWidget(chunk_label);
		chunk_side->addWidget(solid_box);
		chunk_side->addWidget(chunk_canvas);
		for (QCheckBox *box : { cx, cy, s1t, s1l, s2t, s2l })
			chunk_side->addWidget(box);
		chunk_side->addWidget(Note("Left click places the block selected in Blocks (with these flags), right click picks the block and flags under the pointer. The solids of the path shown are laid over the blocks: top solid is clear white, left and right solid yellow (with the bottom in red), all solid black.", chunk_page));
		chunk_side->addStretch();
		chunk_layout->addLayout(chunk_side, 1);
		tabs->addTab(Page(chunk_page), "Chunks");
		chunk_canvas->flags = [=] {
			return uint16_t((cx->isChecked() ? 0x0400 : 0) | (cy->isChecked() ? 0x0800 : 0) | (s1t->isChecked() ? 0x1000 : 0) | (s1l->isChecked() ? 0x2000 : 0) | (s2t->isChecked() ? 0x4000 : 0) | (s2l->isChecked() ? 0x8000 : 0));
		};
		chunk_canvas->picked_flags = [=](uint16_t f) {
			cx->setChecked(f & 0x0400); cy->setChecked(f & 0x0800); s1t->setChecked(f & 0x1000); s1l->setChecked(f & 0x2000); s2t->setChecked(f & 0x4000); s2l->setChecked(f & 0x8000);
		};

		// Solids
		auto *solids = new SolidPane();
		animated << solids;
		tabs->addTab(Page(solids), "Solids");

		// The planes: the foreground (A) and the background (B), the same tool on the two halves of the layout
		LayerCanvas *canvases[2];
		QLabel *chunk_picks[2];
		for (int plane = 0; plane < 2; plane++) {
			auto *page = new QWidget();
			auto *box = new QVBoxLayout(page);
			auto *where = Note(QString("Plane %1: left click paints with the chunk picked below, right click picks the chunk under the pointer, Ctrl and left click takes the camera there.").arg(plane == 0 ? "A, the foreground" : "B, the background"), page);
			box->addWidget(where);
			chunk_picks[plane] = new QLabel(page);
			auto *picker = new Grid(256, 68, [] { return ChunkCount(); },
			                        [](QPainter *p, int i, const QRect &at) { p->drawImage(at.x(), at.y(), ChunkThumb(i, 64)); }, &s.chunk, [] { if (s.refresh_all) s.refresh_all(); });
			auto *picker_area = Scroll(picker, page);
			picker_area->setFixedHeight(68 + 20);
			box->addWidget(chunk_picks[plane]);
			box->addWidget(picker_area);
			auto *zoom = new QComboBox(page);
			for (int z : { 16, 32, 48, 64, 96, 128 }) // (128 is the chunk's own size)
				zoom->addItem(QString("%1 pixels a chunk").arg(z), z);
			zoom->setCurrentIndex(getenv("SONIC_QT_PLANEZOOM") ? atoi(getenv("SONIC_QT_PLANEZOOM")) : 5); // (the chunks at their own size: 128 pixels)
			box->addWidget(zoom);
			QComboBox *view_box = nullptr;
			if (plane == 0) {
				view_box = new QComboBox(page);
				view_box->addItems({ "View: normal", "View: priority (shadow/highlight night mode)", "View: solids path A (primary)", "View: solids path B (secondary)" });
				box->addWidget(view_box);
				if (const char *forced = getenv("SONIC_QT_PLANEVIEW")) // (for the automated checks: the view the plane starts in)
					view_box->setCurrentIndex(atoi(forced));
				guides_box = new QCheckBox("Show objects and rings (as guides to place chunks by)", page);
				QObject::connect(guides_box, &QCheckBox::toggled, page, [] { if (s.refresh_all) s.refresh_all(); });
				box->addWidget(guides_box);
			}
			canvases[plane] = new LayerCanvas(plane, where);
			if (view_box != nullptr)
				QObject::connect(view_box, qOverload<int>(&QComboBox::currentIndexChanged), page, [=](int mode) { canvases[0]->SetMode(mode); });
			animated << canvases[plane] << picker;
			auto *canvas_area = Scroll(canvases[plane], page);
			box->addWidget(canvas_area, 1);
			QObject::connect(zoom, qOverload<int>(&QComboBox::currentIndexChanged), page, [=](int) {
				canvases[plane]->SetZoom(zoom->currentData().toInt());
				InvalidateThumbs();
			});
			canvases[plane]->SetZoom(zoom->currentData().toInt());
			tabs->addTab(page, plane == 0 ? "Plane A" : "Plane B");
			// opening the tab shows where the cursor (and so the camera) is
			QObject::connect(tabs, &QTabWidget::currentChanged, page, [=](int) {
				if (tabs->currentWidget() != page)
					return;
				int cx, cy;
				Editor_GetCursor(&cx, &cy);
				const double z = zoom->currentData().toInt() / 128.0;
				canvas_area->ensureVisible(int(cx * z), int(cy * z), canvas_area->viewport()->width() / 2, canvas_area->viewport()->height() / 2);
			});
		}

		// The block's own fields write the word
		auto write_cell = [=] {
			size_t bytes;
			uint8_t *blocks = Editor_Blocks(&bytes);
			if (blocks == nullptr || size_t(s.block + 1) * 8 > bytes)
				return;
			const uint16_t word = uint16_t((tile_spin->value() & 0x7FF) | (x_flip->isChecked() ? 0x0800 : 0) | (y_flip->isChecked() ? 0x1000 : 0) | (line_spin->value() << 13) | (priority->isChecked() ? 0x8000 : 0));
			PutBe16(blocks + s.block * 8 + s.block_cell * 2, word);
			Changed(true);
		};
		static bool loading_cell = false;
		auto field_changed = [=](int) { if (!loading_cell) write_cell(); };
		QObject::connect(tile_spin, qOverload<int>(&QSpinBox::valueChanged), block_page, field_changed);
		QObject::connect(line_spin, qOverload<int>(&QSpinBox::valueChanged), block_page, field_changed);
		for (QCheckBox *box : { x_flip, y_flip, priority })
			QObject::connect(box, &QCheckBox::toggled, block_page, [=](bool) { if (!loading_cell) write_cell(); });
		QObject::connect(use_tile, &QPushButton::clicked, block_page, [=] { tile_spin->setValue(s.tile); });

		// The palette cycles on the screen: the pictures follow it, redrawn when the colours they show have moved (the Palette tab shows the palette itself, with its placeholders as they are)
		auto *timer = new QTimer(tabs);
		QObject::connect(timer, &QTimer::timeout, tabs, [tabs] {
			if (!tabs->isVisible())
				return;
			quint64 signature = 1469598103934665603ull;
			for (int line = 0; line < 4; line++)
				for (int i = 0; i < 16; i++)
					signature = (signature ^ Editor_GetColour(line, i)) * 1099511628211ull;
			// the level's animated art moves in the tile space: the first $800 tiles (the level's art and what animates over it)
			const quint64 *tiles = reinterpret_cast<const quint64 *>(VDP_TileSpace());
			for (int i = 0; i < 0x800 * 32 / 8; i++)
				signature = (signature ^ tiles[i]) * 1099511628211ull;
			if (guides_box != nullptr && guides_box->isChecked()) { // the objects and rings drawn over Plane A follow the editor's lists
				const EditorLevel &lv = editor_session.level;
				for (int i = 0; i < lv.object_count; i++)
					signature = (signature ^ (quint64(lv.objects[i].x) << 16 ^ lv.objects[i].y ^ quint64(lv.objects[i].id) << 32)) * 1099511628211ull;
				for (int i = 0; i < lv.ring_count; i++)
					signature = (signature ^ (quint64(lv.rings[i].x) << 16 ^ lv.rings[i].y ^ quint64(lv.rings[i].count) << 32 ^ lv.rings[i].vertical)) * 1099511628211ull;
			}
			static quint64 last = 0;
			if (signature == last)
				return;
			last = signature;
			InvalidateThumbs();
			for (QWidget *widget : animated)
				if (widget->isVisible())
					widget->update();
		});
		timer->start(40);

		s.refresh_all = [=] {
			tile_grid->Resize();
			block_grid->Resize();
			chunk_grid->Resize();
			tile_canvas->update();
			strip->update();
			block_canvas->update();
			chunk_canvas->update();
			palette->Load();
			solids->Load();
			for (int plane = 0; plane < 2; plane++) {
				canvases[plane]->update();
				chunk_picks[plane]->setText("Painting with chunk " + IndexText(s.chunk, ChunkCount()));
			}
			tile_label->setText("Tile " + IndexText(s.tile, TileCount()));
			block_label->setText(QString("Block %1, tile %2 of 4").arg(IndexText(s.block, BlockCount())).arg(s.block_cell + 1));
			chunk_label->setText(QString("Chunk %1 (drawn with block %2)").arg(IndexText(s.chunk, ChunkCount())).arg(IndexText(s.block, BlockCount())));
			line_box->blockSignals(true);
			line_box->setCurrentIndex(s.palette_line);
			line_box->blockSignals(false);
			size_t bytes;
			const uint8_t *blocks = Editor_Blocks(&bytes);
			if (blocks != nullptr && size_t(s.block + 1) * 8 <= bytes) {
				const uint16_t word = Be16(blocks + s.block * 8 + s.block_cell * 2);
				loading_cell = true;
				tile_spin->setValue(word & 0x7FF);
				line_spin->setValue((word >> 13) & 3);
				x_flip->setChecked(word & 0x0800);
				y_flip->setChecked(word & 0x1000);
				priority->setChecked(word & 0x8000);
				loading_cell = false;
			}
		};
	}
	s.refresh_all();
}

void SelectForTest(int block, int chunk) {
	s.block = block;
	s.chunk = chunk;
	if (s.refresh_all)
		s.refresh_all();
}

void Refresh() {
	if (s.refresh_all)
		s.refresh_all();
}

bool Save(const QString &slot, int zone, int act, QString *message) {
	size_t bytes;
	struct Item { int kind; QString suffix; const uint8_t *data; size_t size; };
	QList<Item> items;
	const uint8_t *art = Editor_Art(&bytes);
	items << Item{ 0, "", art, bytes };
	const uint8_t *blocks = Editor_Blocks(&bytes);
	items << Item{ 1, "", blocks, bytes };
	const uint8_t *chunks = Editor_Chunks(&bytes);
	items << Item{ 2, "", chunks, bytes };
	for (int path = 0; path < 2; path++) {
		const uint8_t *solids = Editor_Solids(path, &bytes);
		items << Item{ 3, QString::number(path + 1), solids, bytes };
	}
	bool ok = true;
	const char *code = Peek_ZoneCode(zone);
	static const char *const fallback[5] = { "art", "blocks", "chunks", "solids", "palette" };
	for (const Item &item : items) {
		if (item.data == nullptr || item.size == 0)
			continue;
		size_t packed_size;
		uint8_t *packed = KosEnc(item.data, item.size, &packed_size);
		const char *folder = Peek_TerrainFolder(item.kind);
		const QString path = folder && code ? QString("%1/%2/%3%4").arg(slot, folder, code, item.suffix) : QString("%1/%2%3.kos").arg(slot, fallback[item.kind], item.suffix);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		ok = ok && packed != nullptr && file.open(QIODevice::WriteOnly) && file.write(reinterpret_cast<const char *>(packed), qint64(packed_size)) == qint64(packed_size);
		free(packed);
	}
	// the level's layout (both planes, the act being edited)
	{
		const uint8_t *layout = Editor_Layout(&bytes);
		const char *folder = Peek_LevelLayoutFolder();
		const char *name = Peek_LevelName(zone, act);
		size_t packed_size;
		uint8_t *packed = KosEnc(layout, bytes, &packed_size);
		const QString path = folder && name ? QString("%1/%2/%3").arg(slot, folder, name) : QString("%1/layout_act%2.kos").arg(slot).arg(act + 1);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		ok = ok && packed != nullptr && file.open(QIODevice::WriteOnly) && file.write(reinterpret_cast<const char *>(packed), qint64(packed_size)) == qint64(packed_size);
		free(packed);
	}
	// the palette: lines 1 to 3, as the game's resource has them (words, high byte first)
	if (Peek_TerrainFolder(4) != nullptr && code != nullptr) {
		QByteArray palette;
		for (int line = 1; line < 4; line++)
			for (int i = 0; i < 16; i++) {
				const uint16_t w = Editor_GetBaseColour(line, i); // (the palette as loaded: a cycle's colours on the screen are not the file's)
				palette.append(char(w >> 8));
				palette.append(char(w & 0xFF));
			}
		const QString path = QString("%1/%2/%3").arg(slot, Peek_TerrainFolder(4), code);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		ok = ok && file.open(QIODevice::WriteOnly) && file.write(palette) == palette.size();
	}
	if (ok)
		s.dirty = false;
	if (message)
		*message = ok ? "terrain saved" : "could not save the terrain";
	return ok;
}

} // namespace TerrainEditor
