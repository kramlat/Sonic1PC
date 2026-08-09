#pragma once

#include <QVector>
#include <QWidget>

// Toolbar waveform + VU meter, per your direction -- placed just after the
// tempo editor. Shows the exact interleaved-stereo buffer AudioEngine just
// queued to the audio device each tick (AudioEngine::samplesGenerated), so
// it reflects real playback output, not a separate synthesis path.
class WaveformVUWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaveformVUWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(180, 32); }

public slots:
    void pushSamples(const QVector<qint16> &interleavedStereo);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<qint16> m_waveform; // most recent buffer's left channel, for the scope line
    float m_peakL = 0.0f, m_peakR = 0.0f; // 0-1, decays each repaint via m_decayTimer

    class QTimer *m_decayTimer;
};
