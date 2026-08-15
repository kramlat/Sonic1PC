#pragma once

#include <QWidget>

class QPushButton;
class QCheckBox;
class QLabel;

// Stage 4 of the pipeline viewer: play/stop the currently-compiled bytes
// through the real sequencer (Sound.c, via Sound_DebugPlayRawSong), for
// authentic multi-channel FM+PSG playback of what BytesPanel just compiled.
class PlaybackPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaybackPanel(QWidget *parent = nullptr);

    void setStatus(const QString &text);

signals:
    void playRequested(bool isSfx);
    void stopRequested();
    void ladderEffectToggled(bool enabled);

private:
    QPushButton *m_playButton;
    QPushButton *m_stopButton;
    QCheckBox *m_sfxCheck;
    QCheckBox *m_ladderCheck;
    QLabel *m_statusLabel;
};
