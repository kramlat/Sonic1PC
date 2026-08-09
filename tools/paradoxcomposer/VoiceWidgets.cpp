#include "VoiceWidgets.h"

#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QtMath>

#include <cmath>

extern "C" {
#include "../../fmcore/fm_voice.h" // real routing table -- single source of truth, not hand-transcribed
}

// ---------------------------------------------------------------------------
// AlgorithmDiagramWidget
// ---------------------------------------------------------------------------

AlgorithmDiagramWidget::AlgorithmDiagramWidget(QWidget *parent) : QWidget(parent) {
    setFixedSize(96, 96); // fixed, not just a minimum -- prevents layout reflow/jumpiness as siblings resize
}

void AlgorithmDiagramWidget::setAlgorithm(int algorithm) {
    m_algorithm = qBound(0, algorithm, FM_VOICE_ALGORITHM_COUNT - 1);
    update();
}

void AlgorithmDiagramWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    // 5 nodes: operators 1-4 in a 2x2 grid, OUT centered below.
    const int w = width(), h = height();
    const QPointF nodePos[5] = {
        QPointF(w * 0.28, h * 0.22), QPointF(w * 0.72, h * 0.22), // op1, op2
        QPointF(w * 0.28, h * 0.52), QPointF(w * 0.72, h * 0.52), // op3, op4
        QPointF(w * 0.5, h * 0.85),                               // OUT
    };
    const QString labels[5] = {"1", "2", "3", "4", "OUT"};
    const qreal r = qMin(w, h) * 0.09;

    p.setPen(QPen(palette().text().color(), 1.4));
    for (int from = 0; from < FM_VOICE_OP_COUNT; from++) {
        for (int to = 0; to <= FM_VOICE_OP_COUNT; to++) {
            if (!FM_VOICE_ALGORITHM_CONNECT[m_algorithm][from][to])
                continue;
            const QPointF a = nodePos[from], b = nodePos[to];
            if (a == b)
                continue; // no real self-loops in this table, but guard anyway
            QLineF line(a, b);
            line.setLength(line.length() - r * 1.6);
            const QPointF start = b - QPointF(line.dx(), line.dy());
            p.drawLine(start, b);
            // simple arrowhead at `b`
            const qreal angle = std::atan2(-line.dy(), line.dx());
            const qreal arrowSize = 5.0;
            QPointF p1 = b - QPointF(std::cos(angle - M_PI / 7) * arrowSize, -std::sin(angle - M_PI / 7) * arrowSize);
            QPointF p2 = b - QPointF(std::cos(angle + M_PI / 7) * arrowSize, -std::sin(angle + M_PI / 7) * arrowSize);
            p.drawLine(b, p1);
            p.drawLine(b, p2);
        }
    }

    for (int i = 0; i < 5; i++) {
        const bool isOut = (i == 4);
        p.setBrush(isOut ? palette().highlight() : palette().button());
        p.setPen(QPen(palette().text().color(), 1.2));
        p.drawEllipse(nodePos[i], r, r);
        p.drawText(QRectF(nodePos[i] - QPointF(r, r), nodePos[i] + QPointF(r, r)), Qt::AlignCenter, labels[i]);
    }
}

// ---------------------------------------------------------------------------
// EnvelopeViewWidget
// ---------------------------------------------------------------------------

EnvelopeViewWidget::EnvelopeViewWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(200, 80);
}

void EnvelopeViewWidget::setEnvelope(int ar, int d1r, int d1l, int d2r, int rr, int tl) {
    m_ar = ar;
    m_d1r = d1r;
    m_d1l = d1l;
    m_d2r = d2r;
    m_rr = rr;
    m_tl = tl;
    update();
}

void EnvelopeViewWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().base());

    const int w = width(), h = height();
    const qreal margin = 8.0;
    const qreal top = margin, bottom = h - margin;
    const qreal left = margin, right = w - margin;
    const qreal usableW = right - left;

    // Peak amplitude from Total Level (0 = loudest, 127 = silent -- inverted
    // polarity vs everything else here, matching the real field).
    const qreal peak = 1.0 - (qBound(0, m_tl, 127) / 127.0);
    const qreal sustain = peak * (1.0 - (qBound(0, m_d1l, 15) / 15.0));

    // Segment widths: faster rate (higher AR/D1R/D2R/RR) = shorter segment.
    // Purely qualitative -- not modeling the real exponential/rate-scaled
    // curve, just conveying relative shape.
    auto durationFraction = [](int rate, qreal minFrac, qreal maxFrac) {
        const qreal t = qBound(0, rate, 31) / 31.0; // 0=slowest, 1=fastest
        return maxFrac - t * (maxFrac - minFrac);
    };
    const qreal attackW = usableW * durationFraction(m_ar, 0.04, 0.22);
    const qreal decay1W = usableW * durationFraction(m_d1r, 0.04, 0.20);
    const qreal decay2W = usableW * durationFraction(m_d2r, 0.10, 0.30); // sustain fade, held key
    const qreal releaseW = usableW * durationFraction(m_rr, 0.06, 0.24);

    const qreal x0 = left;
    const qreal x1 = x0 + attackW;               // end of attack (peak)
    const qreal x2 = x1 + decay1W;                // end of decay1 (sustain level)
    const qreal x3 = qMin(x2 + decay2W, right - releaseW); // key-off point (end of held sustain fade)
    const qreal x4 = qMin(x3 + releaseW, right);  // end of release (silence)

    auto y = [&](qreal level) { return bottom - level * (bottom - top); };

    QPainterPath path;
    path.moveTo(x0, y(0));
    path.lineTo(x1, y(peak));
    path.lineTo(x2, y(sustain));
    path.lineTo(x3, y(sustain * 0.85)); // gentle fade during decay2/sustain-hold
    path.lineTo(x4, y(0));
    path.lineTo(right, y(0));

    p.setPen(QPen(palette().text().color(), 1.0, Qt::DashLine));
    p.drawLine(QPointF(left, bottom), QPointF(right, bottom));

    p.setPen(QPen(palette().highlight().color(), 2.0));
    p.drawPath(path);

    p.setPen(palette().text().color());
    p.drawText(QRectF(x1 - 10, top, 24, 12), Qt::AlignCenter, "A");
    p.drawText(QRectF(x2 - 10, top, 24, 12), Qt::AlignCenter, "D");
    p.drawText(QRectF(x3 - 10, top, 24, 12), Qt::AlignCenter, "S");
    p.drawText(QRectF(x4 - 10, top, 24, 12), Qt::AlignCenter, "R");
}

// ---------------------------------------------------------------------------
// HorizontalPianoWidget
// ---------------------------------------------------------------------------

namespace {
constexpr int kPianoRangeStart = 24; // 4 octaves centered on a typical melodic register
constexpr int kPianoRangeCount = 48;

bool isBlackSemitone(int semitoneInOctave) {
    static const QSet<int> blackSemitones{1, 3, 6, 8, 10};
    return blackSemitones.contains(semitoneInOctave);
}
} // namespace

HorizontalPianoWidget::HorizontalPianoWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(56);
}

QVector<HorizontalPianoWidget::KeyRect> HorizontalPianoWidget::layoutKeys() const {
    QVector<KeyRect> keys;
    int whiteCount = 0;
    for (int row = kPianoRangeStart; row < kPianoRangeStart + kPianoRangeCount; row++)
        if (!isBlackSemitone(((row % 12) + 12) % 12))
            ++whiteCount;
    if (whiteCount == 0)
        return keys;
    const double whiteW = double(width()) / whiteCount;

    int whiteIndex = 0;
    QVector<int> whiteIndexForRow(kPianoRangeCount, -1);
    for (int i = 0; i < kPianoRangeCount; i++) {
        const int row = kPianoRangeStart + i;
        const int semi = ((row % 12) + 12) % 12;
        if (!isBlackSemitone(semi)) {
            whiteIndexForRow[i] = whiteIndex;
            keys.append({row, QRect(int(whiteIndex * whiteW), 0, int(whiteW) + 1, height()), false});
            ++whiteIndex;
        }
    }
    for (int i = 0; i < kPianoRangeCount; i++) {
        const int row = kPianoRangeStart + i;
        const int semi = ((row % 12) + 12) % 12;
        if (isBlackSemitone(semi)) {
            // Sits between the white key to its left and right -- use the
            // previous row's white slot (already assigned) offset by
            // roughly one white-key width, centered at the boundary.
            int leftWhiteIdx = -1;
            for (int j = i - 1; j >= 0; j--)
                if (whiteIndexForRow[j] >= 0) {
                    leftWhiteIdx = whiteIndexForRow[j];
                    break;
                }
            if (leftWhiteIdx < 0)
                continue;
            const double centerX = (leftWhiteIdx + 1) * whiteW;
            const double blackW = whiteW * 0.6;
            keys.append(
                {row, QRect(int(centerX - blackW / 2), 0, int(blackW), int(height() * 0.6)), true});
        }
    }
    return keys;
}

void HorizontalPianoWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    const QVector<KeyRect> keys = layoutKeys();
    for (const KeyRect &k : keys) {
        if (k.isBlack)
            continue;
        p.fillRect(k.rect, k.row == m_pressedNote ? QColor(90, 170, 255) : Qt::white);
        p.setPen(QColor(120, 120, 120));
        p.drawRect(k.rect);
    }
    for (const KeyRect &k : keys) {
        if (!k.isBlack)
            continue;
        p.fillRect(k.rect, k.row == m_pressedNote ? QColor(90, 170, 255) : QColor(30, 30, 34));
        p.setPen(Qt::black);
        p.drawRect(k.rect);
    }
}

int HorizontalPianoWidget::noteAt(const QPoint &pos) const {
    const QVector<KeyRect> keys = layoutKeys();
    for (const KeyRect &k : keys) // black keys drawn on top -- test them first
        if (k.isBlack && k.rect.contains(pos))
            return k.row;
    for (const KeyRect &k : keys)
        if (!k.isBlack && k.rect.contains(pos))
            return k.row;
    return -1;
}

void HorizontalPianoWidget::mousePressEvent(QMouseEvent *event) {
    m_pressedNote = noteAt(event->pos());
    if (m_pressedNote >= 0)
        emit notePressed(m_pressedNote);
    update();
}

void HorizontalPianoWidget::mouseReleaseEvent(QMouseEvent *) {
    if (m_pressedNote >= 0)
        emit noteReleased();
    m_pressedNote = -1;
    update();
}
