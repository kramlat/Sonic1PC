#include "WaveformVUWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QTimer>

WaveformVUWidget::WaveformVUWidget(QWidget *parent) : QWidget(parent) {
    m_decayTimer = new QTimer(this);
    m_decayTimer->setInterval(1000 / 30);
    connect(m_decayTimer, &QTimer::timeout, this, [this]() {
        m_peakL *= 0.85f;
        m_peakR *= 0.85f;
        update();
    });
    m_decayTimer->start();
}

void WaveformVUWidget::pushSamples(const QVector<qint16> &interleavedStereo) {
    if (interleavedStereo.isEmpty())
        return;

    m_waveform.clear();
    float peakL = 0.0f, peakR = 0.0f;
    for (int i = 0; i + 1 < interleavedStereo.size(); i += 2) {
        m_waveform.append(interleavedStereo[i]);
        peakL = qMax(peakL, qAbs(interleavedStereo[i]) / 32768.0f);
        peakR = qMax(peakR, qAbs(interleavedStereo[i + 1]) / 32768.0f);
    }
    m_peakL = qMax(m_peakL, peakL);
    m_peakR = qMax(m_peakR, peakR);
}

void WaveformVUWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 24));

    constexpr int kVuWidth = 10;
    const QRect scopeRect(0, 0, width() - kVuWidth * 2 - 6, height());
    const QRect vuLRect(scopeRect.right() + 4, 0, kVuWidth, height());
    const QRect vuRRect(vuLRect.right() + 2, 0, kVuWidth, height());

    // Waveform scope line.
    p.setPen(QColor(90, 200, 120));
    if (m_waveform.size() > 1) {
        const double xStep = double(scopeRect.width()) / (m_waveform.size() - 1);
        QPainterPath path;
        auto yFor = [&](int i) {
            const double norm = m_waveform[i] / 32768.0;
            return scopeRect.center().y() - norm * (scopeRect.height() / 2 - 2);
        };
        path.moveTo(scopeRect.left(), yFor(0));
        for (int i = 1; i < m_waveform.size(); i++)
            path.lineTo(scopeRect.left() + i * xStep, yFor(i));
        p.drawPath(path);
    } else {
        p.drawLine(scopeRect.left(), scopeRect.center().y(), scopeRect.right(), scopeRect.center().y());
    }
    p.setPen(QColor(60, 60, 66));
    p.drawRect(scopeRect.adjusted(0, 0, -1, -1));

    // VU bars (L/R), green->yellow->red gradient by level.
    auto drawVu = [&](const QRect &r, float level) {
        p.fillRect(r, QColor(10, 10, 12));
        const int filledH = int(r.height() * qBound(0.0f, level, 1.0f));
        const QRect filled(r.left(), r.bottom() - filledH, r.width(), filledH);
        QColor color = level > 0.85f ? QColor(220, 60, 60) : (level > 0.6f ? QColor(220, 200, 60) : QColor(80, 200, 100));
        p.fillRect(filled, color);
        p.setPen(QColor(60, 60, 66));
        p.drawRect(r.adjusted(0, 0, -1, -1));
    };
    drawVu(vuLRect, m_peakL);
    drawVu(vuRRect, m_peakR);
}
