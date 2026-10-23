#include "LevelEditor.h"

#include "GameInterface.h"
#include "../../DebugPeek.h"
#include "../../EditorLive.h"

#include <cstdio>
#include <cstdlib>

#include <QCheckBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace LevelEditor {

namespace {

// (the first five columns of the objects' table edit as text, the last three are check boxes)
enum ObjectColumn { ColX, ColY, ColId, ColName, ColSubtype, ColXFlip, ColYFlip, ColRemember, ObjectColumnCount };
enum RingColumn { RingColX, RingColY, RingColCount, RingColVertical, RingColumnCount };
constexpr int kPickRadius = 12;

struct Window {
	QDialog *dialog = nullptr;
	QLabel *level_label = nullptr, *message = nullptr, *folder_label = nullptr;
	QPushButton *edit_button = nullptr;
	QTableWidget *objects = nullptr, *rings = nullptr;
	QTabWidget *tabs = nullptr;
	QLineEdit *filter = nullptr;
	QListWidget *id_list = nullptr;
	QSpinBox *subtype = nullptr, *ring_count = nullptr;
	QCheckBox *x_flip = nullptr, *y_flip = nullptr, *remember = nullptr, *vertical = nullptr;
	QRadioButton *place_objects = nullptr, *place_rings = nullptr;
	bool filling = false; // the tables are being rebuilt: their edits are not the user's
	int shown_zone = -1, shown_act = -1;
	bool shown_in_level = false;
	int drag_object = -1, drag_ring = -1;
	int drag_dx = 0, drag_dy = 0;
	bool moved = false;
} w;

QString HexText(int value, int digits) {
	return QString("%1").arg(value, digits, 16, QChar('0')).toUpper();
}

int ParseHex(const QString &text, bool *ok) {
	return text.trimmed().remove("0x", Qt::CaseInsensitive).remove('$').toInt(ok, 16);
}

QString OutputFolder() {
	QSettings settings("ParadoxEngine", game_info.app_id);
	QString dir = settings.value("editor/output").toString();
	if (dir.isEmpty())
		dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/editor-out";
	return dir;
}

QString ObjectLabel(int id) {
	const char *name = Peek_ObjectName(id);
	return name ? QString::fromUtf8(name) : QString();
}

QString LevelTitle(const EditorPeek &peek) {
	QString zone = QString("Zone $%1").arg(HexText(peek.zone, 2));
	if (game_zone_list != nullptr)
		for (int i = 0; i < game_zone_list->count; i++)
			if (game_zone_list->zones[i].id == peek.zone)
				zone = QString::fromUtf8(game_zone_list->zones[i].name);
	return QString("%1, act %2").arg(zone).arg(peek.act + 1);
}

void SetMessage(const QString &text) {
	if (getenv("SONIC_QT_EDITOR"))
		fprintf(stderr, "LEVELEDITOR: %s\n", text.toUtf8().constData());
	if (w.message)
		w.message->setText(text);
}

void RebuildTables() {
	w.filling = true;
	const EditorLevel &level = editor_session.level;
	w.objects->setRowCount(editor_open ? level.object_count : 0);
	for (int i = 0; editor_open && i < level.object_count; i++) {
		const EditorObject &o = level.objects[i];
		auto text = [&](int col, const QString &value, bool editable) {
			auto *item = new QTableWidgetItem(value);
			if (!editable)
				item->setFlags(item->flags() & ~Qt::ItemIsEditable);
			w.objects->setItem(i, col, item);
		};
		text(ColX, HexText(o.x, 4), true);
		text(ColY, HexText(o.y, 3), true);
		text(ColId, HexText(o.id, 2), true);
		text(ColName, ObjectLabel(o.id), false);
		text(ColSubtype, HexText(o.subtype, 2), true);
		const bool flags[3] = { o.x_flip, o.y_flip, o.remember };
		for (int c = 0; c < 3; c++) {
			auto *item = new QTableWidgetItem();
			item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
			item->setCheckState(flags[c] ? Qt::Checked : Qt::Unchecked);
			w.objects->setItem(i, ColXFlip + c, item);
		}
	}
	w.rings->setRowCount(editor_open ? level.ring_count : 0);
	for (int i = 0; editor_open && i < level.ring_count; i++) {
		const EditorRing &r = level.rings[i];
		w.rings->setItem(i, RingColX, new QTableWidgetItem(HexText(r.x, 4)));
		w.rings->setItem(i, RingColY, new QTableWidgetItem(HexText(r.y, 3)));
		w.rings->setItem(i, RingColCount, new QTableWidgetItem(QString::number(r.count)));
		auto *item = new QTableWidgetItem();
		item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		item->setCheckState(r.vertical ? Qt::Checked : Qt::Unchecked);
		w.rings->setItem(i, RingColVertical, item);
	}
	w.filling = false;
}

void Changed(const QString &what) { // the model changed: the game loads the level's objects again from it
	Editor_Apply();
	RebuildTables();
	SetMessage(what);
}

void ObjectEdited(int row, int column) {
	if (w.filling || !editor_open || row >= editor_session.level.object_count)
		return;
	EditorObject o = editor_session.level.objects[row];
	bool ok = true;
	switch (column) {
		case ColX: o.x = (uint16_t)ParseHex(w.objects->item(row, column)->text(), &ok); break;
		case ColY: o.y = (uint16_t)(ParseHex(w.objects->item(row, column)->text(), &ok) & 0xFFF); break;
		case ColId: o.id = (uint8_t)ParseHex(w.objects->item(row, column)->text(), &ok); break;
		case ColSubtype: o.subtype = (uint8_t)ParseHex(w.objects->item(row, column)->text(), &ok); break;
		case ColXFlip: o.x_flip = w.objects->item(row, column)->checkState() == Qt::Checked; break;
		case ColYFlip: o.y_flip = w.objects->item(row, column)->checkState() == Qt::Checked; break;
		case ColRemember: o.remember = w.objects->item(row, column)->checkState() == Qt::Checked; break;
		default: return;
	}
	if (!ok) {
		RebuildTables(); // (not a number: back to what it was)
		return;
	}
	EditorLevel_SetObject(&editor_session.level, row, &o);
	Changed("Object changed.");
}

void RingEdited(int row, int column) {
	if (w.filling || !editor_open || row >= editor_session.level.ring_count)
		return;
	EditorRing r = editor_session.level.rings[row];
	bool ok = true;
	switch (column) {
		case RingColX: r.x = (uint16_t)ParseHex(w.rings->item(row, column)->text(), &ok); break;
		case RingColY: r.y = (uint16_t)(ParseHex(w.rings->item(row, column)->text(), &ok) & 0xFFF); break;
		case RingColCount: r.count = (uint8_t)qBound(1, w.rings->item(row, column)->text().toInt(&ok), 8); break;
		case RingColVertical: r.vertical = w.rings->item(row, column)->checkState() == Qt::Checked; break;
		default: return;
	}
	if (!ok) {
		RebuildTables();
		return;
	}
	EditorLevel_SetRing(&editor_session.level, row, &r);
	Changed("Ring changed.");
}

int PickedId() {
	QListWidgetItem *item = w.id_list->currentItem();
	return item ? item->data(Qt::UserRole).toInt() : 0;
}

void FilterIds() {
	const QString needle = w.filter->text().trimmed().toLower();
	for (int i = 0; i < w.id_list->count(); i++) {
		QListWidgetItem *item = w.id_list->item(i);
		item->setHidden(!needle.isEmpty() && !item->text().toLower().contains(needle));
	}
}

void StartEditing() {
	EditorPeek peek;
	Peek_EditorState(&peek);
	if (!peek.in_level) {
		SetMessage("Start a level first: the editor works on the level that is running.");
		return;
	}
	Editor_Open(peek.zone, peek.act, peek.objects, peek.rings);
	Editor_SetCursor(peek.player_x, peek.player_y); // (the cursor starts where the player was)
	Editor_Apply();
	RebuildTables();
	SetMessage("Editing. Left click places, drag moves, right click deletes.");
}

void StopEditing() {
	if (editor_open && editor_session.level.dirty &&
	    QMessageBox::question(w.dialog, "Level Editor", "The changes have not been saved. Stop editing and drop them?") != QMessageBox::Yes)
		return;
	Editor_Close();
	RebuildTables();
	SetMessage("Not editing.");
}

// The files of the level being edited, in the output folder: <folder>/slotNN/<S2Objects>/<SSZ1> (as they are in res/), or objects_aN.bin when the game gives no names
bool Save() {
	if (!editor_open)
		return false;
	const QString slot = QString("%1/slot%2").arg(OutputFolder(), HexText(editor_session.zone, 2));
	const char *level_name = Peek_LevelName(editor_session.zone, editor_session.act);
	auto path = [&](const char *folder, const char *fallback) {
		if (folder != nullptr && level_name != nullptr)
			return QString("%1/%2/%3").arg(slot, folder, level_name);
		return QString("%1/%2_act%3.bin").arg(slot, fallback).arg(editor_session.act + 1);
	};
	static uint8_t buffer[EDITOR_MAX_OBJECTS * 6 + 6];
	bool ok = true;
	struct Kind { QString path; size_t size; };
	Kind kinds[2];
	kinds[0] = { path(Peek_LevelObjectsFolder(), "objects"), EditorLevel_WriteObjects(&editor_session.level, buffer, sizeof(buffer)) };
	QByteArray objects(reinterpret_cast<const char *>(buffer), (qsizetype)kinds[0].size);
	static uint8_t ring_buffer[EDITOR_MAX_RINGS * 4 + 4];
	kinds[1] = { path(Peek_LevelRingsFolder(), "rings"), EditorLevel_WriteRings(&editor_session.level, ring_buffer, sizeof(ring_buffer)) };
	QByteArray rings(reinterpret_cast<const char *>(ring_buffer), (qsizetype)kinds[1].size);
	const QByteArray *data[2] = { &objects, &rings };
	for (int i = 0; i < 2; i++) {
		QDir().mkpath(QFileInfo(kinds[i].path).absolutePath());
		QFile file(kinds[i].path);
		ok = ok && file.open(QIODevice::WriteOnly) && file.write(*data[i]) == data[i]->size();
	}
	if (ok) {
		editor_session.level.dirty = false;
		SetMessage(QString("Saved to %1 (copy its files into res/ to keep them).").arg(slot));
	} else {
		SetMessage("Could not save: check the output folder.");
	}
	if (EditorLevel_RememberCount(&editor_session.level) > 255)
		SetMessage(w.message->text() + " More than 255 objects remember their state: the extra ones will not.");
	return ok;
}

void Build(QMainWindow *parent) {
	w.dialog = new QDialog(parent, Qt::Tool);
	w.dialog->setWindowTitle("Level Editor");
	w.dialog->resize(560, 700);
	auto *layout = new QVBoxLayout(w.dialog);

	w.level_label = new QLabel(w.dialog);
	layout->addWidget(w.level_label);
	w.edit_button = new QPushButton("Start editing", w.dialog);
	layout->addWidget(w.edit_button);
	QObject::connect(w.edit_button, &QPushButton::clicked, w.dialog, [] { editor_open ? StopEditing() : StartEditing(); });

	auto *tabs = w.tabs = new QTabWidget(w.dialog);
	layout->addWidget(tabs, 3);
	w.objects = new QTableWidget(0, ObjectColumnCount, tabs);
	w.objects->setHorizontalHeaderLabels({ "X", "Y", "Id", "Object", "Subtype", "X flip", "Y flip", "Remember" });
	w.objects->horizontalHeader()->setStretchLastSection(true);
	w.objects->setSelectionBehavior(QAbstractItemView::SelectRows);
	QObject::connect(w.objects, &QTableWidget::cellChanged, w.dialog, ObjectEdited);
	tabs->addTab(w.objects, "Objects");
	w.rings = new QTableWidget(0, RingColumnCount, tabs);
	w.rings->setHorizontalHeaderLabels({ "X", "Y", "Rings", "Vertical" });
	w.rings->horizontalHeader()->setStretchLastSection(true);
	w.rings->setSelectionBehavior(QAbstractItemView::SelectRows);
	QObject::connect(w.rings, &QTableWidget::cellChanged, w.dialog, RingEdited);
	tabs->addTab(w.rings, "Rings");
	// Selecting an entry takes the cursor there (the players are not in the level while it is edited: the cursor is, and the camera follows it)
	QObject::connect(w.objects, &QTableWidget::itemSelectionChanged, w.dialog, [] {
		const int row = w.objects->currentRow();
		if (!w.filling && editor_open && row >= 0 && row < editor_session.level.object_count)
			Editor_SetCursor(editor_session.level.objects[row].x, editor_session.level.objects[row].y);
	});
	QObject::connect(w.rings, &QTableWidget::itemSelectionChanged, w.dialog, [] {
		const int row = w.rings->currentRow();
		if (!w.filling && editor_open && row >= 0 && row < editor_session.level.ring_count)
			Editor_SetCursor(editor_session.level.rings[row].x, editor_session.level.rings[row].y);
	});
	QObject::connect(tabs, &QTabWidget::currentChanged, w.dialog, [](int index) { // (and the tab says what the clicks place)
		w.place_objects->setChecked(index == 0);
		w.place_rings->setChecked(index == 1);
	});

	auto *remove_row = new QHBoxLayout();
	auto *remove = new QPushButton("Delete selected", w.dialog);
	remove_row->addWidget(remove);
	remove_row->addStretch();
	layout->addLayout(remove_row);
	QObject::connect(remove, &QPushButton::clicked, w.dialog, [tabs] {
		if (!editor_open)
			return;
		QTableWidget *table = tabs->currentIndex() == 0 ? w.objects : w.rings;
		const int row = table->currentRow();
		if (row < 0)
			return;
		if (tabs->currentIndex() == 0)
			EditorLevel_RemoveObject(&editor_session.level, row);
		else
			EditorLevel_RemoveRing(&editor_session.level, row);
		Changed("Deleted.");
	});

	// What a click places
	auto *place_row = new QHBoxLayout();
	w.place_objects = new QRadioButton("Place objects", w.dialog);
	w.place_objects->setChecked(true);
	auto *place_rings = w.place_rings = new QRadioButton("Place rings", w.dialog);
	place_row->addWidget(w.place_objects);
	place_row->addWidget(place_rings);
	place_row->addStretch();
	layout->addLayout(place_row);

	w.filter = new QLineEdit(w.dialog);
	w.filter->setPlaceholderText("Find an object by name or id");
	layout->addWidget(w.filter);
	QObject::connect(w.filter, &QLineEdit::textChanged, w.dialog, [](const QString &) { FilterIds(); });
	w.id_list = new QListWidget(w.dialog);
	for (int id = 3; id < 0x100; id++) { // (ids 1 and 2 are the players, which the game makes itself)
		const QString name = ObjectLabel(id);
		auto *item = new QListWidgetItem(QString("%1  %2").arg(HexText(id, 2), name.isEmpty() ? QString("(unnamed)") : name), w.id_list);
		item->setData(Qt::UserRole, id);
		if (name.isEmpty())
			item->setForeground(Qt::gray);
	}
	w.id_list->setCurrentRow(0x26 - 3); // (a monitor, which every game has: a harmless first choice)
	layout->addWidget(w.id_list, 2);

	auto *options = new QHBoxLayout();
	w.subtype = new QSpinBox(w.dialog);
	w.subtype->setRange(0, 255);
	w.subtype->setDisplayIntegerBase(16);
	w.subtype->setPrefix("Subtype $");
	w.x_flip = new QCheckBox("X flip", w.dialog);
	w.y_flip = new QCheckBox("Y flip", w.dialog);
	w.remember = new QCheckBox("Remember", w.dialog);
	w.ring_count = new QSpinBox(w.dialog);
	w.ring_count->setRange(1, 8);
	w.ring_count->setPrefix("Rings ");
	w.ring_count->setValue(5);
	w.vertical = new QCheckBox("Vertical", w.dialog);
	for (QWidget *widget : std::initializer_list<QWidget *>{ w.subtype, w.x_flip, w.y_flip, w.remember, w.ring_count, w.vertical })
		options->addWidget(widget);
	layout->addLayout(options);

	auto *save_row = new QHBoxLayout();
	auto *save = new QPushButton("Save", w.dialog);
	auto *choose = new QPushButton("Output folder...", w.dialog);
	auto *open = new QPushButton("Open folder", w.dialog);
	save_row->addWidget(save);
	save_row->addWidget(choose);
	save_row->addWidget(open);
	layout->addLayout(save_row);
	w.folder_label = new QLabel(w.dialog);
	w.folder_label->setWordWrap(true);
	layout->addWidget(w.folder_label);
	w.message = new QLabel("Not editing.", w.dialog);
	w.message->setWordWrap(true);
	layout->addWidget(w.message);
	w.folder_label->setText("Saves go to: " + OutputFolder());

	QObject::connect(save, &QPushButton::clicked, w.dialog, [] { Save(); });
	QObject::connect(choose, &QPushButton::clicked, w.dialog, [] {
		const QString dir = QFileDialog::getExistingDirectory(w.dialog, "Level editor output folder", OutputFolder());
		if (dir.isEmpty())
			return;
		QSettings("ParadoxEngine", game_info.app_id).setValue("editor/output", dir);
		w.folder_label->setText("Saves go to: " + dir);
	});
	QObject::connect(open, &QPushButton::clicked, w.dialog, [] {
		QDir().mkpath(OutputFolder());
		QDesktopServices::openUrl(QUrl::fromLocalFile(OutputFolder()));
	});
	QObject::connect(w.dialog, &QDialog::finished, w.dialog, [](int) {
		if (editor_open) { // closing the window ends the editing (what was not saved is dropped)
			Editor_Close();
			RebuildTables();
		}
	});
}

EditorObject NewObject(int x, int y) {
	EditorObject o = {};
	o.x = (uint16_t)qMax(0, x);
	o.y = (uint16_t)qBound(0, y, 0xFFF);
	o.id = (uint8_t)PickedId();
	o.subtype = (uint8_t)w.subtype->value();
	o.x_flip = w.x_flip->isChecked();
	o.y_flip = w.y_flip->isChecked();
	o.remember = w.remember->isChecked();
	return o;
}

} // namespace

void Show(QMainWindow *window) {
	if (w.dialog == nullptr)
		Build(window);
	w.dialog->show();
	w.dialog->raise();
	w.dialog->activateWindow();
	if (!editor_open)
		StartEditing();
}

bool SaveNow() {
	return Save();
}

bool IsOpen() {
	return w.dialog != nullptr && w.dialog->isVisible();
}

bool Editing() {
	return editor_open && IsOpen();
}

bool Mouse(int kind, int button, int x, int y) {
	if (!Editing())
		return false;
	EditorPeek peek;
	Peek_EditorState(&peek);
	const int wx = peek.camera_x + x, wy = peek.camera_y + y;
	EditorLevel &level = editor_session.level;
	const bool objects = w.place_objects->isChecked();
	if (kind == 0 && button == Qt::LeftButton) {
		w.drag_object = objects ? EditorLevel_FindObject(&level, wx, wy, kPickRadius) : -1;
		w.drag_ring = objects ? -1 : EditorLevel_FindRing(&level, wx, wy, kPickRadius);
		w.moved = false;
		if (w.drag_object >= 0) {
			w.drag_dx = level.objects[w.drag_object].x - wx;
			w.drag_dy = level.objects[w.drag_object].y - wy;
			w.objects->selectRow(w.drag_object);
		} else if (w.drag_ring >= 0) {
			w.drag_dx = level.rings[w.drag_ring].x - wx;
			w.drag_dy = level.rings[w.drag_ring].y - wy;
			w.rings->selectRow(w.drag_ring);
		} else if (objects) {
			const EditorObject o = NewObject(wx, wy);
			const int at = EditorLevel_AddObject(&level, &o);
			Changed(at < 0 ? "The object list is full." : QString("Placed object $%1 at $%2, $%3.").arg(HexText(o.id, 2), HexText(o.x, 4), HexText(o.y, 3)));
			if (at >= 0)
				w.objects->selectRow(at);
		} else {
			EditorRing r = {};
			r.x = (uint16_t)qMax(0, wx);
			r.y = (uint16_t)qBound(0, wy, 0xFFF);
			r.count = (uint8_t)w.ring_count->value();
			r.vertical = w.vertical->isChecked();
			const int at = EditorLevel_AddRing(&level, &r);
			Changed(at < 0 ? "The ring list is full." : QString("Placed %1 rings at $%2, $%3.").arg(r.count).arg(HexText(r.x, 4), HexText(r.y, 3)));
		}
		return true;
	}
	if (kind == 1 && (w.drag_object >= 0 || w.drag_ring >= 0)) {
		if (w.drag_object >= 0) {
			EditorObject o = level.objects[w.drag_object];
			o.x = (uint16_t)qMax(0, wx + w.drag_dx);
			o.y = (uint16_t)qBound(0, wy + w.drag_dy, 0xFFF);
			w.drag_object = EditorLevel_SetObject(&level, w.drag_object, &o);
		} else {
			EditorRing r = level.rings[w.drag_ring];
			r.x = (uint16_t)qMax(0, wx + w.drag_dx);
			r.y = (uint16_t)qBound(0, wy + w.drag_dy, 0xFFF);
			w.drag_ring = EditorLevel_SetRing(&level, w.drag_ring, &r);
		}
		w.moved = true;
		return true;
	}
	if (kind == 2 && button == Qt::LeftButton && (w.drag_object >= 0 || w.drag_ring >= 0)) {
		const bool moved = w.moved;
		w.drag_object = w.drag_ring = -1;
		if (moved)
			Changed("Moved.");
		return true;
	}
	if (kind == 0 && button == Qt::RightButton) {
		const int found = objects ? EditorLevel_FindObject(&level, wx, wy, kPickRadius) : EditorLevel_FindRing(&level, wx, wy, kPickRadius);
		if (found >= 0) {
			objects ? EditorLevel_RemoveObject(&level, found) : EditorLevel_RemoveRing(&level, found);
			Changed("Deleted.");
		}
		return true;
	}
	return false;
}

void DrawOverlay(QPainter *p, const QRectF &dest, double scale) {
	if (!Editing())
		return;
	EditorPeek peek;
	Peek_EditorState(&peek);
	const EditorLevel &level = editor_session.level;
	auto to_widget = [&](int x, int y) { return QPointF(dest.left() + (x - peek.camera_x) * scale, dest.top() + (y - peek.camera_y) * scale); };
	const double half = 6 * scale;
	const int selected = w.objects->currentRow();
	QFont font = p->font();
	font.setPixelSize(qMax(8, int(8 * scale)));
	p->setFont(font);
	for (int i = 0; i < level.object_count; i++) {
		const EditorObject &o = level.objects[i];
		const QPointF at = to_widget(o.x, o.y);
		if (!dest.adjusted(-20, -20, 20, 20).contains(at))
			continue;
		p->setPen(QPen(i == selected ? QColor(255, 160, 0) : QColor(0, 255, 255), 1));
		p->setBrush(Qt::NoBrush);
		p->drawRect(QRectF(at.x() - half, at.y() - half, half * 2, half * 2));
		p->drawText(QPointF(at.x() - half, at.y() - half - 2), HexText(o.id, 2));
	}
	// The cursor: a ring when the rings are what is being placed, else a cross, where the camera is looking
	int cx, cy;
	Editor_GetCursor(&cx, &cy);
	const QPointF cursor = to_widget(cx, cy);
	p->setBrush(Qt::NoBrush);
	if (w.place_rings->isChecked()) {
		p->setPen(QPen(QColor(255, 255, 80), 2));
		p->drawEllipse(cursor, half * 1.3, half * 1.3);
		p->setPen(QPen(QColor(255, 160, 0), 2));
		p->drawEllipse(cursor, half * 0.7, half * 0.7);
	} else {
		p->setPen(QPen(QColor(255, 255, 80), 2));
		p->drawLine(QPointF(cursor.x() - half * 1.5, cursor.y()), QPointF(cursor.x() + half * 1.5, cursor.y()));
		p->drawLine(QPointF(cursor.x(), cursor.y() - half * 1.5), QPointF(cursor.x(), cursor.y() + half * 1.5));
	}
	p->setPen(QPen(QColor(255, 220, 0), 1));
	for (int i = 0; i < level.ring_count; i++) {
		const EditorRing &r = level.rings[i];
		for (int n = 0; n < r.count; n++) {
			const QPointF at = to_widget(r.vertical ? r.x : r.x + n * 0x18, r.vertical ? r.y + n * 0x18 : r.y);
			if (dest.adjusted(-20, -20, 20, 20).contains(at))
				p->drawEllipse(at, half * 0.8, half * 0.8);
		}
	}
}

void Poll() {
	if (!IsOpen())
		return;
	EditorPeek peek;
	Peek_EditorState(&peek);
	if (editor_open && (!peek.in_level || peek.zone != editor_session.zone || peek.act != editor_session.act)) {
		Editor_Close(); // another level started: the editing of the last one ends
		RebuildTables();
		SetMessage("The level changed; editing stopped.");
	}
	if (peek.in_level != w.shown_in_level || peek.zone != w.shown_zone || peek.act != w.shown_act || w.edit_button->text().isEmpty()) {
		w.shown_in_level = peek.in_level;
		w.shown_zone = peek.zone;
		w.shown_act = peek.act;
		w.level_label->setText(peek.in_level ? "Level: " + LevelTitle(peek) : "No level is running.");
	}
	const QString button = editor_open ? "Stop editing" : "Start editing";
	if (w.edit_button->text() != button)
		w.edit_button->setText(button);
	w.edit_button->setEnabled(editor_open || peek.in_level);
}

} // namespace LevelEditor
