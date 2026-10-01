#include "QtHost.h"
#include "DebugViewers.h"
#include "QtAudio.h"
#include "DemoTools.h"
#include "ConsoleDrawer.h"
#include "Settings.h"
#include "../../DebugPeek.h"
#include "../../DebugLog.h"

#include <QAction>
#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSlider>
#include <QStatusBar>
#include <QGuiApplication>
#include <QWidgetAction>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>
#include <QSurfaceFormat>
#include <QTabWidget>

#include <SDL_scancode.h>

#include <deque>
#include <functional>
#include <vector>
#include <stdlib.h>
#include <string.h>

// Sound engine channel mutes (Sound.h is a C-only header; see SOUND_MUTE_* there for the numbering).
extern "C" {
bool Demo_PlaybackActive(void);
extern int32_t cli_start_level;
void Sound_SetChannelMuted(int channel, bool muted);
bool Sound_IsChannelMuted(int channel);
}

namespace {

// Qt key -> the SDL scancode Input.c already uses for it (only the keys the
// game and its debug tools read).
int ScancodeFor(int key) {
	if (key >= Qt::Key_A && key <= Qt::Key_Z)
		return SDL_SCANCODE_A + (key - Qt::Key_A);
	if (key >= Qt::Key_1 && key <= Qt::Key_9)
		return SDL_SCANCODE_1 + (key - Qt::Key_1);
	switch (key) {
		case Qt::Key_Return:
		case Qt::Key_Enter:        return SDL_SCANCODE_RETURN;
		case Qt::Key_Backspace:    return SDL_SCANCODE_BACKSPACE;
		case Qt::Key_Tab:          return SDL_SCANCODE_TAB;
		case Qt::Key_Left:         return SDL_SCANCODE_LEFT;
		case Qt::Key_Right:        return SDL_SCANCODE_RIGHT;
		case Qt::Key_Up:           return SDL_SCANCODE_UP;
		case Qt::Key_Down:         return SDL_SCANCODE_DOWN;
		case Qt::Key_Alt:          return SDL_SCANCODE_LALT;
		case Qt::Key_Slash:        return SDL_SCANCODE_SLASH;
		case Qt::Key_Comma:        return SDL_SCANCODE_COMMA;
		case Qt::Key_Period:       return SDL_SCANCODE_PERIOD;
		case Qt::Key_BracketLeft:  return SDL_SCANCODE_LEFTBRACKET;
		case Qt::Key_BracketRight: return SDL_SCANCODE_RIGHTBRACKET;
		case Qt::Key_QuoteLeft:    return SDL_SCANCODE_GRAVE;
		case Qt::Key_F11:          return SDL_SCANCODE_F11;
		default:                   return SDL_SCANCODE_UNKNOWN;
	}
}

uint8_t g_keys[SDL_NUM_SCANCODES];
std::deque<QtHost_KeyEvent> g_key_events;
bool g_should_quit = false;

// The drawing widget: keeps the latest frame and draws it, aspect-correct and
// centred (black bars), as a texture.
class GameView : public QOpenGLWidget {
public:
	explicit GameView(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
		setFocusPolicy(Qt::StrongFocus);
		QSurfaceFormat fmt;
		fmt.setSwapInterval(0); // the game paces itself (Render.c's frame clock)
		setFormat(fmt);
	}

	void setFrame(const QImage &image) { frame = image; }
	bool saveFrame(const QString &path) const { return frame.save(path); }

	std::function<void()> on_console_key; // the backtick key: opens/closes the console drawer
	std::function<void()> on_resized;

	QSize sizeHint() const override { return frame.size(); }

protected:
	void paintGL() override {
		QPainter p(this);
		p.fillRect(rect(), Qt::black);
		if (frame.isNull())
			return;
		double scale = qMin(double(width()) / frame.width(), double(height()) / frame.height());
		double w = frame.width() * scale, h = frame.height() * scale;
		QRectF dest((width() - w) / 2, (height() - h) / 2, w, h);
		p.setRenderHint(QPainter::SmoothPixmapTransform, false); // crisp pixels, like the SDL window
		p.drawImage(dest, frame);
	}

	void keyPressEvent(QKeyEvent *e) override {
		if (e->key() == Qt::Key_QuoteLeft) {
			if (!e->isAutoRepeat() && on_console_key)
				on_console_key();
			return;
		}
		int sc = ScancodeFor(e->key());
		if (sc == SDL_SCANCODE_UNKNOWN)
			return;
		g_keys[sc] = 1;
		QtHost_KeyEvent ev = {};
		ev.scancode = sc;
		ev.repeat = e->isAutoRepeat();
		strncpy(ev.text, e->text().toUtf8().constData(), sizeof(ev.text) - 1);
		g_key_events.push_back(ev);
	}

	void keyReleaseEvent(QKeyEvent *e) override {
		if (e->isAutoRepeat())
			return; // X11 sends release+press pairs while a key repeats; the key is still held
		int sc = ScancodeFor(e->key());
		if (sc != SDL_SCANCODE_UNKNOWN)
			g_keys[sc] = 0;
	}

	void resizeEvent(QResizeEvent *e) override {
		QOpenGLWidget::resizeEvent(e);
		if (on_resized)
			on_resized();
	}

	void focusOutEvent(QFocusEvent *e) override {
		memset(g_keys, 0, sizeof(g_keys)); // don't leave keys stuck down when focus moves away
		QOpenGLWidget::focusOutEvent(e);
	}

private:
	QImage frame;
};

class MainWindow : public QMainWindow {
public:
	MainWindow() {
		view = new GameView(this);
		setCentralWidget(view);
		menuBar()->setNativeMenuBar(false); // keep the menu bar inside the window (no KDE/macOS global menu export)
		statusBar()->setSizeGripEnabled(false); // messages from the Tools menu (demo recording) appear here
		console = new ConsoleDrawer(view);
		view->on_console_key = [this] { console->Toggle(); };
		view->on_resized = [this] { console->Reposition(); };
		tool_label = new QLabel(this); // right-hand side: which tools are running
		statusBar()->addPermanentWidget(tool_label);
		launch_injected = cli_start_level >= 0; // started straight into a level (--zone), before the game clears it
		BuildMenus();
	}

	GameView *view;
	ConsoleDrawer *console = nullptr;

	// Window geometry is only remembered once the saved state has been restored (see QtHost_Init).
	void StartTrackingGeometry() { tracking = true; }

	// Lists the tools in use on the right of the status bar, so nothing runs unnoticed.
	void UpdateToolStatus() {
		console->Sync();
		QStringList in_use;
		if (DemoTools::Recording())
			in_use << "Demo recording";
		if (Demo_PlaybackActive())
			in_use << "Demo playback";
		if (DebugViewers::IsLogging())
			in_use << "Logging";
		for (const QString &viewer : DebugViewers::OpenViewers())
			in_use << viewer;
		if (console->IsOpen())
			in_use << "Console";
		if (launch_injected)
			in_use << "Level injection";
		QString text = in_use.isEmpty() ? QString() : "Tools in use: " + in_use.join(" · ");
		if (tool_label->text() != text)
			tool_label->setText(text);
	}

	// Shows or hides the debug tools menu to match the game's debug mode.
	void SyncDebugMenu() {
		bool available = Peek_DebugToolsAvailable() != 0;
		if (debug_menu->isVisible() != available)
			debug_menu->setVisible(available);
		if (tools_menu->isVisible() != available)
			tools_menu->setVisible(available);
		if (available) {
			bool recording = DemoTools::Recording();
			record_action->setEnabled(!recording);
			stop_record_action->setEnabled(recording);
			play_action->setEnabled(!recording);
			// keep the menu item in step with the log (it can also be started/stopped from the Log window)
			const char *wanted = DebugViewers::IsLogging() ? "Stop Logging" : "Start Logging";
			if (log_toggle->text() != wanted)
				log_toggle->setText(wanted);
		}
	}

	void ToggleFullscreen() {
		bool to_fullscreen = !isFullScreen();
		menuBar()->setVisible(!to_fullscreen);
		statusBar()->setVisible(!to_fullscreen);
		Settings::Get().fullscreen = to_fullscreen;
		Settings::SaveSoon();
		view->setCursor(to_fullscreen ? Qt::BlankCursor : Qt::ArrowCursor);
		if (to_fullscreen)
			showFullScreen();
		else
			showNormal();
		view->setFocus();
	}

protected:
	void closeEvent(QCloseEvent *e) override {
		g_should_quit = true;
		Settings::Save(); // final write: audio state and video state
		QMainWindow::closeEvent(e);
	}

	// The windowed size and position are remembered (not while fullscreen: that keeps the windowed one).
	void resizeEvent(QResizeEvent *e) override {
		QMainWindow::resizeEvent(e);
		RememberGeometry();
	}
	void moveEvent(QMoveEvent *e) override {
		QMainWindow::moveEvent(e);
		RememberGeometry();
	}

private:
	void RememberGeometry() {
		if (!tracking || isFullScreen() || isMaximized() || !isVisible())
			return;
		Settings::Data &d = Settings::Get();
		d.has_window_geometry = true;
		d.window_width = width();
		d.window_height = height();
		d.window_x = x();
		d.window_y = y();
		Settings::SaveSoon();
	}
	bool tracking = false;

	// One row of the Audio menu: a mute check box and a volume slider for one output (music or effects).
	void AddOutput(QMenu *menu, const char *label, bool (*enabled)(void), void (*set_enabled)(bool), int (*volume)(void),
	               void (*set_volume)(int)) {
		QAction *on = menu->addAction(label);
		on->setCheckable(true);
		on->setChecked(enabled());
		connect(on, &QAction::toggled, this, [set_enabled](bool checked) {
			set_enabled(checked);
			Settings::SaveSoon();
		});

		auto *row = new QWidget;
		auto *layout = new QHBoxLayout(row);
		layout->setContentsMargins(24, 0, 12, 0);
		layout->addWidget(new QLabel("Volume"));
		auto *slider = new QSlider(Qt::Horizontal);
		slider->setRange(0, 100);
		slider->setValue(volume());
		slider->setMinimumWidth(140);
		layout->addWidget(slider);
		connect(slider, &QSlider::valueChanged, this, [set_volume](int v) {
			set_volume(v);
			Settings::SaveSoon();
		});
		auto *action = new QWidgetAction(menu);
		action->setDefaultWidget(row);
		menu->addAction(action);
	}

	// Audio menu: whole outputs (music / sound effects), then the individual sound chip channels.
	void BuildAudioMenu() {
		QMenu *audio = menuBar()->addMenu("Audio");
		AddOutput(audio, "Music", QtAudio_MusicEnabled, QtAudio_SetMusicEnabled, QtAudio_MusicVolume, QtAudio_SetMusicVolume);
		audio->addSeparator();
		AddOutput(audio, "Sound Effects", QtAudio_SfxEnabled, QtAudio_SetSfxEnabled, QtAudio_SfxVolume, QtAudio_SetSfxVolume);
		audio->addSeparator();

		// Checked = audible. Numbering matches SOUND_MUTE_* in Sound.h.
		QMenu *channels = audio->addMenu("Channels");
		const char *const *names = Settings::kChannelNames;
		for (int i = 0; i < 11; i++) {
			if (i == 6 || i == 10)
				channels->addSeparator();
			QAction *a = channels->addAction(names[i]);
			a->setCheckable(true);
			a->setChecked(!Sound_IsChannelMuted(i));
			connect(a, &QAction::toggled, this, [i](bool audible) {
				Sound_SetChannelMuted(i, !audible);
				Settings::SaveSoon();
			});
			channel_actions.push_back(a);
		}
		channels->addSeparator();
		QAction *all = channels->addAction("Unmute All");
		connect(all, &QAction::triggered, this, [this] {
			for (QAction *a : channel_actions)
				a->setChecked(true); // each toggle unmutes its channel
		});
	}

	std::vector<QAction *> channel_actions;
	QLabel *tool_label = nullptr;
	bool launch_injected = false;
	QAction *debug_menu = nullptr;
	QAction *tools_menu = nullptr;
	QAction *record_action = nullptr, *stop_record_action = nullptr, *play_action = nullptr;
	QAction *log_toggle = nullptr;

	// The menu bar. Add new menus/entries here. Don't use '&' mnemonics: Alt is
	// a game key (debug Z80 peek), and an Alt press must not pull focus away.
	void BuildMenus() {
		QMenu *file = menuBar()->addMenu("File");
		// F11 itself is handled by the game (Input.c), so no QAction shortcut here.
		QAction *fullscreen = file->addAction("Fullscreen\tF11");
		connect(fullscreen, &QAction::triggered, this, [this] { ToggleFullscreen(); });
		file->addSeparator();
		QAction *quit = file->addAction("Quit");
		connect(quit, &QAction::triggered, this, [this] { g_should_quit = true; close(); });

		// Debug tools. The menu only appears while debug mode is active (a debug build, or the
		// C,C,C,C + Up,Down,Left,Right code in any build) -- see SyncDebugMenu.
		QMenu *view_menu = menuBar()->addMenu("View");
		debug_menu = view_menu->menuAction();
		debug_menu->setVisible(false);
		connect(view_menu->addAction("VDP Viewer"), &QAction::triggered, this, [this] { DebugViewers::ShowVdpViewer(this); });
		connect(view_menu->addAction("Sound Viewer"), &QAction::triggered, this, [this] { DebugViewers::ShowSoundViewer(this); });
		connect(view_menu->addAction("Console"), &QAction::triggered, this, [this] { console->Toggle(); });
		connect(view_menu->addAction("Variables"), &QAction::triggered, this, [this] { DebugViewers::ShowVariableViewer(this); });
		QMenu *log_menu = view_menu->addMenu("Logging");
		log_toggle = log_menu->addAction("Start Logging");
		connect(log_toggle, &QAction::triggered, this, [this] { DebugViewers::ToggleLogging(this); });
		connect(log_menu->addAction("Log File..."), &QAction::triggered, this, [this] { DebugViewers::ChooseLogFile(this); });
		connect(log_menu->addAction("Show Log"), &QAction::triggered, this, [this] { DebugViewers::ShowLogViewer(this); });
		connect(view_menu->addAction("Object RAM"), &QAction::triggered, this, [this] { DebugViewers::ShowObjectViewer(this); });

		// Tools: demo recording and playback. Like View, only while debug mode is active (see SyncDebugMenu).
		QMenu *tools = menuBar()->addMenu("Tools");
		tools_menu = tools->menuAction();
		tools_menu->setVisible(false);
		record_action = tools->addAction("Record Demo...");
		connect(record_action, &QAction::triggered, this, [this] { DemoTools::RecordDialog(this); });
		stop_record_action = tools->addAction("Stop Recording");
		stop_record_action->setEnabled(false);
		connect(stop_record_action, &QAction::triggered, this, [] { DemoTools::StopRecording(); });
		tools->addSeparator();
		play_action = tools->addAction("Play Demo...");
		connect(play_action, &QAction::triggered, this, [this] { DemoTools::PlayDialog(this); });

		BuildAudioMenu();

		QMenu *help = menuBar()->addMenu("Help");
		QAction *about = help->addAction("About");
		connect(about, &QAction::triggered, this, [this] {
			QMessageBox::about(this, "About", "Sonic the Hedgehog (PC recompilation)");
			view->setFocus();
		});
	}
};

QApplication *g_app = nullptr;
MainWindow *g_window = nullptr;

} // namespace

extern "C" {

int QtHost_Init(const char *title, int width, int height, const uint8_t *icon_rgb16) {
	static int argc = 1;
	static char arg0[] = "Sonic";
	static char *argv[] = { arg0, nullptr };
	g_app = new QApplication(argc, argv);

	Settings::Load(); // before the window: the menus read the live audio state it applies
	g_window = new MainWindow();
	g_window->setWindowTitle(title);
	if (icon_rgb16 != nullptr) {
		QImage icon(icon_rgb16, 16, 16, 16 * 3, QImage::Format_RGB888);
		g_window->setWindowIcon(QPixmap::fromImage(icon.copy()));
	}
	g_window->view->setMinimumSize(width / 2, height / 2);
	g_window->view->setFrame(QImage(width, height, QImage::Format_RGBX8888));
	g_window->resize(width, height); // the client area is the view; the menu bar sits above it
	const Settings::Data &saved = Settings::Get();
	if (saved.has_window_geometry) {
		g_window->resize(saved.window_width, saved.window_height);
		// Wayland compositors place windows themselves, so a saved position only applies elsewhere.
		if (!QGuiApplication::platformName().startsWith("wayland") && !QGuiApplication::platformName().startsWith("offscreen"))
			g_window->move(saved.window_x, saved.window_y);
	}
	g_window->show();
	g_window->view->setFocus();
	g_app->processEvents();
	g_window->StartTrackingGeometry();
	if (saved.fullscreen)
		g_window->ToggleFullscreen();

	// Developer hooks (used by automated checks, harmless otherwise):
	//   SONIC_QT_OPEN=vdp,sound,objects,console   open those debug viewers (or the console drawer) at startup
	//   SONIC_QT_LOG=<file>               start logging to a file once debug mode is active
	//   SONIC_QT_GRAB=<prefix>            after ~4 s, save every window as <prefix>-<n>.png
	if (const char *open = getenv("SONIC_QT_OPEN")) {
		QString list = QString(",%1,").arg(open);
		if (list.contains(",vdp,"))
			DebugViewers::ShowVdpViewer(g_window);
		if (list.contains(",sound,"))
			DebugViewers::ShowSoundViewer(g_window);
		if (list.contains(",vars,"))
			DebugViewers::ShowVariableViewer(g_window);
		if (list.contains(",log,"))
			DebugViewers::ShowLogViewer(g_window);
		if (list.contains(",console,"))
			g_window->console->Toggle(); // only opens while debugging is available
		if (list.contains(",objects,"))
			DebugViewers::ShowObjectViewer(g_window);
	}
	return 0;
}

void QtHost_Quit(void) {
	Settings::Save();
	delete g_window;
	g_window = nullptr;
	delete g_app;
	g_app = nullptr;
}

void QtHost_PumpEvents(void) {
	if (g_app != nullptr) {
		g_window->SyncDebugMenu();
		g_window->UpdateToolStatus();
		DemoTools::Poll(g_window);
		g_app->processEvents();

		static int frames = 0;
		++frames;
		// SONIC_QT_LOG=<file> starts logging to a file as soon as debug mode is active (the title screen sets it)
		static bool log_requested = getenv("SONIC_QT_LOG") != nullptr;
		if (log_requested && Debug_LogStart(getenv("SONIC_QT_LOG")))
			log_requested = false;
		// SONIC_QT_FRAME=<prefix> saves the game picture itself (not the window) as <prefix>-<frame>.png -- the OpenGL
		// view doesn't render offscreen, so this is how a headless run sees the game. By default every 120 frames from
		// frame 120 to 1200; SONIC_QT_FRAME_RANGE=start,end,step picks other frames (e.g. 200,230,1 for a close look).
		static const char *frame_prefix = getenv("SONIC_QT_FRAME");
		if (frame_prefix != nullptr) {
			static int start = 120, end = 1200, step = 120;
			static bool parsed = false;
			if (!parsed) {
				parsed = true;
				if (const char *range = getenv("SONIC_QT_FRAME_RANGE"))
					sscanf(range, "%d,%d,%d", &start, &end, &step);
			}
			if (step > 0 && frames >= start && frames <= end && (frames - start) % step == 0)
				g_window->view->saveFrame(QString("%1-%2.png").arg(frame_prefix).arg(frames));
		}
		if (frames == 240) {
			if (const char *prefix = getenv("SONIC_QT_GRAB")) {
				int n = 0;
				for (QWidget *w : QApplication::topLevelWidgets())
					if (w->isVisible()) {
						w->grab().save(QString("%1-%2.png").arg(prefix).arg(n));
						for (QTabWidget *tabs : w->findChildren<QTabWidget *>()) // every tab, not just the visible one
							for (int t = 0; t < tabs->count(); t++) {
								tabs->setCurrentIndex(t);
								QApplication::processEvents();
								w->grab().save(QString("%1-%2-tab%3.png").arg(prefix).arg(n).arg(t));
							}
						n++;
					}
			}
		}
	}
}

bool QtHost_ShouldQuit(void) {
	return g_should_quit;
}

void QtHost_Present(const void *pixels, int pitch) {
	if (g_window == nullptr)
		return;
	// Copy now: the caller's buffer is reused for the next frame.
	QImage frame(static_cast<const uint8_t *>(pixels), g_window->view->sizeHint().width(),
	             g_window->view->sizeHint().height(), pitch, QImage::Format_RGBX8888);
	g_window->view->setFrame(frame.copy());
	g_window->view->repaint();
}

void QtHost_ToggleFullscreen(void) {
	if (g_window != nullptr)
		g_window->ToggleFullscreen();
}

void QtHost_SetZ80Peek(bool active, const Z80PeekData *data) {
	DebugViewers::SetSoundSnapshot(active, data);
}

const uint8_t *QtHost_KeyState(void) {
	return g_keys;
}

bool QtHost_PollKeyEvent(QtHost_KeyEvent *ev) {
	if (g_key_events.empty())
		return false;
	*ev = g_key_events.front();
	g_key_events.pop_front();
	return true;
}

} // extern "C"
