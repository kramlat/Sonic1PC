#pragma once

#include <QVector>
#include <QWidget>

// Small routing-diagram icon for the currently selected algorithm: draws
// boxes 1-4 (operators) and OUT, with arrows per fm_voice.h's own
// connect[from][to] table (FM_VOICE_ALGORITHM_CONNECT) -- read directly
// from fmcore via SonicCore (already linked), not hand-transcribed, so it
// can never drift out of sync with the real routing data.
class AlgorithmDiagramWidget : public QWidget {
    Q_OBJECT
public:
    explicit AlgorithmDiagramWidget(QWidget *parent = nullptr);
    void setAlgorithm(int algorithm);
    QSize sizeHint() const override { return QSize(96, 96); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_algorithm = 0;
};

// Schematic ADSR envelope shape for one operator -- qualitative, not a
// cycle-accurate render of the real envelope generator (rate-scaling/
// exponential curve timing is complex; this is a visual reference showing
// relative attack/decay/sustain/release shape from the operator's own
// AR/D1R/D1L/D2R/RR/TL knob values, redrawn live as they change).
class EnvelopeViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit EnvelopeViewWidget(QWidget *parent = nullptr);
    // ar/d1r/d2r/rr: 0 (slowest) - 31 (fastest), matching the real fields'
    // own ranges. d1l: 0 (loudest sustain) - 15 (quietest). tl: 0 (loudest)
    // - 127 (silent), matching the real Total Level field's own polarity.
    void setEnvelope(int ar, int d1r, int d1l, int d2r, int rr, int tl);
    QSize sizeHint() const override { return QSize(300, 120); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_ar = 31, m_d1r = 0, m_d1l = 0, m_d2r = 0, m_rr = 0, m_tl = 0;
};

// Horizontal on-screen keyboard for auditioning the voice being edited --
// same note_index convention as AudioEngine::previewVoice (0-95), a fixed
// 4-octave range centered on a typical melodic register. Placed at the
// bottom of the voice editor per your direction.
class HorizontalPianoWidget : public QWidget {
    Q_OBJECT
public:
    explicit HorizontalPianoWidget(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(400, 56); }

signals:
    void notePressed(int noteIndex);
    void noteReleased();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    struct KeyRect {
        int row;
        QRect rect;
        bool isBlack;
    };
    QVector<KeyRect> layoutKeys() const;
    int noteAt(const QPoint &pos) const;

    int m_pressedNote = -1;
};
