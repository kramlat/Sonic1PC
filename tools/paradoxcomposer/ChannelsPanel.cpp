#include "ChannelsPanel.h"
#include "NativeCompiler.h"
#include "PianoRollWidgets.h"
#include "SongDocument.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDial>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLayoutItem>
#include <QPainter>
#include <QPair>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

// Rotated channel-name label ("FM1", "PSG2", ...), matching the reference
// mockup's vertical strip label -- Qt has no built-in vertical-text label,
// so this is a small custom paint.
class VerticalLabel : public QWidget {
public:
    explicit VerticalLabel(const QString &text, QWidget *parent = nullptr) : QWidget(parent), m_text(text) {
        setFixedWidth(18);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.translate(0, height());
        p.rotate(-90);
        p.setPen(palette().text().color());
        p.drawText(QRect(0, 0, height(), width()), Qt::AlignCenter, m_text);
    }

private:
    QString m_text;
};

} // namespace

// One or a pair of small vertical VU bars (per your direction: a pair for
// FM/DAC -- real hardware's own L/R pan bits -- one for PSG, which has no
// pan control in this engine). Driven externally via setLevel() from
// AudioEngine::channelActivityUpdated, forwarded through MainWindow.
class ChannelVUWidget : public QWidget {
public:
    explicit ChannelVUWidget(bool pair, QWidget *parent = nullptr) : QWidget(parent), m_pair(pair) {
        setFixedWidth(pair ? 16 : 8);
    }
    void setLevel(float a, float b = 0.0f) {
        m_levelA = a;
        m_levelB = b;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        auto drawBar = [&](const QRect &r, float level) {
            p.fillRect(r, QColor(10, 10, 12));
            const int filledH = int(r.height() * qBound(0.0f, level, 1.0f));
            const QRect filled(r.left(), r.bottom() - filledH, r.width(), filledH);
            p.fillRect(filled, QColor(80, 200, 100));
            p.setPen(QColor(50, 50, 55));
            p.drawRect(r.adjusted(0, 0, -1, -1));
        };
        drawBar(QRect(0, 0, 6, height()), m_levelA);
        if (m_pair)
            drawBar(QRect(9, 0, 6, height()), m_levelB);
    }

private:
    bool m_pair;
    float m_levelA = 0.0f, m_levelB = 0.0f;
};

ChannelsPanel::ChannelsPanel(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);

    auto makeGroup = [this, mainLayout](const QString &title, QVBoxLayout *&stripsLayoutOut, bool isFmGroup) {
        auto *groupBox = new QGroupBox(title, this);
        auto *groupLayout = new QVBoxLayout(groupBox);

        // Single column (per your direction), each group's own vertical
        // scroll area so a long channel list doesn't push the other group
        // (or the rest of the panel) off-window.
        auto *scroll = new QScrollArea(groupBox);
        auto *container = new QWidget(scroll);
        stripsLayoutOut = new QVBoxLayout(container);
        scroll->setWidget(container);
        scroll->setWidgetResizable(true);
        scroll->setFixedHeight(320);
        groupLayout->addWidget(scroll);

        auto *buttonRow = new QHBoxLayout();
        auto *addButton = new QPushButton("+", groupBox);
        auto *removeButton = new QPushButton("-", groupBox);
        addButton->setFixedWidth(28);
        removeButton->setFixedWidth(28);
        connect(addButton, &QPushButton::clicked, this, [this, isFmGroup]() { addChannel(isFmGroup); });
        connect(removeButton, &QPushButton::clicked, this, [this, isFmGroup]() { removeChannel(isFmGroup); });
        buttonRow->addWidget(addButton);
        buttonRow->addWidget(removeButton);
        buttonRow->addStretch();
        groupLayout->addLayout(buttonRow);

        mainLayout->addWidget(groupBox);
    };

    makeGroup("FM + DAC", m_fmStripsLayout, true);
    makeGroup("PSG", m_psgStripsLayout, false);
}

void ChannelsPanel::setHeader(const QJsonArray &header) {
    m_header = header;
    rebuild();
}

void ChannelsPanel::setPlaylist(const QJsonObject &playlist, const QStringList &order) {
    m_playlist = playlist;
    m_playlistOrder = order;
    rebuild();
}

QPair<int, int> ChannelsPanel::chanCounts() const {
    for (const QJsonValue &v : m_header) {
        const QJsonObject entry = v.toObject();
        if (entry.contains("smpsHeaderChan")) {
            const QJsonArray args = entry.value("smpsHeaderChan").toArray();
            const int fm = args.size() > 0 ? hexFieldToInt(args.at(0)) : 0;
            const int psg = args.size() > 1 ? hexFieldToInt(args.at(1)) : 0;
            return {fm, psg};
        }
    }
    return {0, 0};
}

void ChannelsPanel::setChanCounts(int fmCount, int psgCount) {
    for (int i = 0; i < m_header.size(); i++) {
        QJsonObject entry = m_header.at(i).toObject();
        if (entry.contains("smpsHeaderChan")) {
            entry["smpsHeaderChan"] = QJsonArray{intToHexField(fmCount), intToHexField(psgCount)};
            m_header[i] = entry;
            return;
        }
    }
}

QString ChannelsPanel::freshBlockName(const QString &hint) const {
    QString candidate = hint;
    int n = 1;
    while (candidate.isEmpty() || m_playlistOrder.contains(candidate))
        candidate = QString("%1%2").arg(hint).arg(n++);
    return candidate;
}

void ChannelsPanel::rebuild() {
    QLayoutItem *item;
    while ((item = m_fmStripsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    while ((item = m_psgStripsLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    m_labelToChannelIndex.clear();
    m_labelToVU.clear();

    QVector<ChannelEntry> fmEntries, psgEntries;
    int fmCounter = 0, psgCounter = 0;
    // Runtime channel indices match Sound_DebugSetChannelMuted's own
    // layout: PSG 0..SOUND_CHANNELS_PSG-1 first, then FM/DAC starting at
    // SOUND_CHANNEL_FM_BASE (== SOUND_CHANNELS_PSG == 4) -- see Sound.h.
    // DAC always occupies FM slot 0 if present (LoadMusic's own layout
    // rule), so position-in-fmEntries IS the FM-relative slot directly.
    int fmSlot = 0, psgSlot = 0;
    for (int i = 0; i < m_header.size(); i++) {
        const QJsonObject entry = m_header.at(i).toObject();
        if (entry.contains("smpsHeaderDAC"))
            fmEntries.append({"smpsHeaderDAC", i, "DAC", 4 + fmSlot++});
        else if (entry.contains("smpsHeaderFM"))
            fmEntries.append({"smpsHeaderFM", i, QString("FM%1").arg(++fmCounter), 4 + fmSlot++});
        else if (entry.contains("smpsHeaderPSG"))
            psgEntries.append({"smpsHeaderPSG", i, QString("PSG%1").arg(++psgCounter), psgSlot++});
    }
    for (const ChannelEntry &e : fmEntries)
        m_fmStripsLayout->addWidget(buildStrip(e));
    m_fmStripsLayout->addStretch();
    for (const ChannelEntry &e : psgEntries)
        m_psgStripsLayout->addWidget(buildStrip(e));
    m_psgStripsLayout->addStretch();

    applyMuteSolo(); // re-push mute state for the just-rebuilt channel set
}

QWidget *ChannelsPanel::buildStrip(const ChannelEntry &entry) {
    const QJsonObject headerEntry = m_header.at(entry.headerIndex).toObject();
    QJsonArray args = headerEntry.value(entry.macro).toArray();
    while (args.size() < 3)
        args.append(intToHexField(0));
    const QString loc = args.at(0).toString();
    const int rawDisplacement = hexFieldToInt(args.at(1));
    const int vol = hexFieldToInt(args.at(2));

    auto *strip = new QFrame(this);
    strip->setFrameShape(QFrame::StyledPanel);
    strip->setFixedWidth(230);
    // Color-coded by channel type, per your direction: FM red, PSG green, DAC blue.
    const QString accent = entry.macro == "smpsHeaderFM"    ? "#c0392b"
                            : entry.macro == "smpsHeaderPSG" ? "#27ae60"
                                                              : "#2980b9"; // smpsHeaderDAC
    strip->setStyleSheet(QString("QFrame { border: 2px solid %1; border-radius: 4px; }").arg(accent));
    auto *stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(4, 4, 4, 4);

    stripLayout->addWidget(new VerticalLabel(entry.label, strip));

    auto *dispDial = new QDial(strip);
    dispDial->setRange(-128, 127); // pitch displacement -- signed byte (matches real values like $F4 = -12)
    dispDial->setFixedSize(36, 36);
    dispDial->setValue(rawDisplacement > 127 ? rawDisplacement - 256 : rawDisplacement);
    auto *dispLabel = new QLabel("Displacement", strip);
    dispLabel->setAlignment(Qt::AlignCenter);
    const int headerIndex = entry.headerIndex;
    connect(dispDial, &QDial::valueChanged, this,
            [this, headerIndex](int v) { writeArg(headerIndex, 1, intToHexField(v & 0xFF)); });
    auto *dispBox = new QVBoxLayout();
    dispBox->addWidget(dispDial, 0, Qt::AlignHCenter);
    dispBox->addWidget(dispLabel);

    auto *volDial = new QDial(strip);
    volDial->setRange(0, 127);
    volDial->setFixedSize(36, 36);
    volDial->setValue(qBound(0, vol, 127));
    auto *volLabel = new QLabel("Vol", strip);
    volLabel->setAlignment(Qt::AlignCenter);
    connect(volDial, &QDial::valueChanged, this,
            [this, headerIndex](int v) { writeArg(headerIndex, 2, intToHexField(v)); });
    auto *volBox = new QVBoxLayout();
    volBox->addWidget(volDial, 0, Qt::AlignHCenter);
    volBox->addWidget(volLabel);

    auto *knobsRow = new QHBoxLayout();
    knobsRow->addLayout(dispBox);
    knobsRow->addLayout(volBox);

    auto *rightCol = new QVBoxLayout();
    rightCol->addLayout(knobsRow);

    auto *selectHomeButton = new QPushButton("Select Home", strip);
    selectHomeButton->setToolTip(loc.isEmpty() ? "(none)" : loc);
    connect(selectHomeButton, &QPushButton::clicked, this, [this, headerIndex]() {
        if (m_playlistOrder.isEmpty())
            return;
        bool ok = false;
        const QString chosen = QInputDialog::getItem(this, "Select home block", "Block:", m_playlistOrder, 0, false, &ok);
        if (ok)
            writeArg(headerIndex, 0, chosen);
    });
    rightCol->addWidget(selectHomeButton);

    // PSG tone dropdown, per your direction -- the real full valid set is
    // exactly fTone_01..fTone_09 (smps.h's own enum), not just whatever
    // happens to appear in this project's own corpus.
    if (entry.macro == "smpsHeaderPSG") {
        while (args.size() < 4)
            args.append(intToHexField(0));
        const QString currentTone = args.at(3).toString();
        auto *toneCombo = new QComboBox(strip);
        for (int i = 1; i <= 9; i++)
            toneCombo->addItem(QString("fTone_%1").arg(i, 2, 10, QChar('0')));
        const int currentIndex = toneCombo->findText(currentTone);
        toneCombo->setCurrentIndex(currentIndex >= 0 ? currentIndex : 0);
        connect(toneCombo, &QComboBox::currentTextChanged, this,
                [this, headerIndex](const QString &text) { writeArg(headerIndex, 3, text); });
        rightCol->addWidget(toneCombo);
    }

    // Mute/Solo checkboxes, per your direction.
    const QString label = entry.label;
    auto *muteCheck = new QCheckBox("M", strip);
    muteCheck->setChecked(m_mutedLabels.contains(label));
    connect(muteCheck, &QCheckBox::toggled, this, [this, label](bool checked) {
        if (checked)
            m_mutedLabels.insert(label);
        else
            m_mutedLabels.remove(label);
        applyMuteSolo();
    });
    auto *soloCheck = new QCheckBox("S", strip);
    soloCheck->setChecked(m_soloedLabels.contains(label));
    connect(soloCheck, &QCheckBox::toggled, this, [this, label](bool checked) {
        if (checked)
            m_soloedLabels.insert(label);
        else
            m_soloedLabels.remove(label);
        applyMuteSolo();
    });
    auto *muteSoloRow = new QHBoxLayout();
    muteSoloRow->addWidget(muteCheck);
    muteSoloRow->addWidget(soloCheck);
    rightCol->addLayout(muteSoloRow);

    stripLayout->addLayout(rightCol);

    auto *thumb = new NoteThumbnailWidget(strip);
    thumb->setEvents(m_playlist.value(loc).toArray());
    stripLayout->addWidget(thumb, 1);

    auto *vu = new ChannelVUWidget(entry.macro != "smpsHeaderPSG", strip);
    stripLayout->addWidget(vu);
    m_labelToChannelIndex[label] = entry.runtimeChannelIndex;
    m_labelToVU[label] = vu;

    return strip;
}

void ChannelsPanel::writeArg(int headerIndex, int argIndex, const QJsonValue &value) {
    if (headerIndex < 0 || headerIndex >= m_header.size())
        return;
    QJsonObject entry = m_header.at(headerIndex).toObject();
    if (entry.isEmpty())
        return;
    const QString macro = entry.keys().first(); // single-key object -- order-safe regardless of QJsonObject's own quirks
    QJsonArray args = entry.value(macro).toArray();
    while (args.size() <= argIndex)
        args.append(intToHexField(0));
    args[argIndex] = value;
    entry[macro] = args;
    m_header[headerIndex] = entry;
    emit headerChanged(m_header);
    rebuild(); // refresh thumbnail/tooltip if "Select Home" just changed the loc
}

void ChannelsPanel::addChannel(bool isFmGroup) {
    const QPair<int, int> counts = chanCounts();
    const int fmCount = counts.first, psgCount = counts.second;

    if (isFmGroup) {
        if (fmCount >= 7) // DAC + 6 FM max -- this engine's own independent-software-DAC cap (see Sound.c's own comment)
            return;
        bool hasDac = false, hasFm = false;
        int insertPos = m_header.size();
        for (int i = 0; i < m_header.size(); i++) {
            const QJsonObject e = m_header.at(i).toObject();
            if (e.contains("smpsHeaderDAC"))
                hasDac = true;
            if (e.contains("smpsHeaderFM"))
                hasFm = true;
            if (e.contains("smpsHeaderPSG") && insertPos == m_header.size())
                insertPos = i;
        }
        QString macro;
        QJsonArray args;
        QString name;
        if (!hasDac && !hasFm) {
            name = freshBlockName("NewDAC");
            macro = "smpsHeaderDAC";
            args = QJsonArray{name};
        } else {
            name = freshBlockName("NewFM");
            macro = "smpsHeaderFM";
            args = QJsonArray{name, intToHexField(0), intToHexField(0)};
        }
        QJsonObject newEntry;
        newEntry[macro] = args;
        m_header.insert(insertPos, newEntry);
        setChanCounts(fmCount + 1, psgCount);
        m_playlistOrder.append(name); // keep local copy consistent until MainWindow's next full refresh
        emit blockNeeded(name, QJsonArray{"smpsStop"});
        emit headerChanged(m_header);
        rebuild();
    } else {
        if (psgCount >= 3) // real PSG hardware only has 3 tone channels
            return;
        int insertPos = m_header.size();
        for (int i = m_header.size() - 1; i >= 0; i--)
            if (m_header.at(i).toObject().contains("smpsHeaderPSG")) {
                insertPos = i + 1;
                break;
            }
        const QString name = freshBlockName("NewPSG");
        QJsonObject newEntry;
        newEntry["smpsHeaderPSG"] = QJsonArray{name, intToHexField(0), intToHexField(0), intToHexField(0)};
        m_header.insert(insertPos, newEntry);
        setChanCounts(fmCount, psgCount + 1);
        m_playlistOrder.append(name);
        emit blockNeeded(name, QJsonArray{"smpsStop"});
        emit headerChanged(m_header);
        rebuild();
    }
}

void ChannelsPanel::applyMuteSolo() {
    // Effective mute = explicitly muted, OR (something else is soloed AND
    // this channel isn't one of the soloed ones).
    const bool anySoloed = !m_soloedLabels.isEmpty();
    for (auto it = m_labelToChannelIndex.constBegin(); it != m_labelToChannelIndex.constEnd(); ++it) {
        const QString &label = it.key();
        const bool effectiveMuted = m_mutedLabels.contains(label) || (anySoloed && !m_soloedLabels.contains(label));
        emit channelMuteChanged(it.value(), effectiveMuted);
    }
}

void ChannelsPanel::setChannelActivity(const QVector<float> &fmLevelsL, const QVector<float> &fmLevelsR,
                                        const QVector<float> &psgLevels) {
    for (auto it = m_labelToChannelIndex.constBegin(); it != m_labelToChannelIndex.constEnd(); ++it) {
        ChannelVUWidget *vu = m_labelToVU.value(it.key());
        if (!vu)
            continue;
        const int idx = it.value();
        if (idx >= 4 && idx - 4 < fmLevelsL.size())
            vu->setLevel(fmLevelsL[idx - 4], fmLevelsR[idx - 4]);
        else if (idx >= 0 && idx < psgLevels.size())
            vu->setLevel(psgLevels[idx]);
    }
}

void ChannelsPanel::removeChannel(bool isFmGroup) {
    const QPair<int, int> counts = chanCounts();
    const int fmCount = counts.first, psgCount = counts.second;

    if (isFmGroup) {
        int lastFm = -1, dacIdx = -1;
        for (int i = 0; i < m_header.size(); i++) {
            const QJsonObject e = m_header.at(i).toObject();
            if (e.contains("smpsHeaderFM"))
                lastFm = i;
            if (e.contains("smpsHeaderDAC"))
                dacIdx = i;
        }
        const int removeIdx = lastFm >= 0 ? lastFm : dacIdx;
        if (removeIdx < 0)
            return;
        m_header.removeAt(removeIdx);
        setChanCounts(qMax(0, fmCount - 1), psgCount);
        emit headerChanged(m_header);
        rebuild();
    } else {
        int lastPsg = -1;
        for (int i = 0; i < m_header.size(); i++)
            if (m_header.at(i).toObject().contains("smpsHeaderPSG"))
                lastPsg = i;
        if (lastPsg < 0)
            return;
        m_header.removeAt(lastPsg);
        setChanCounts(fmCount, qMax(0, psgCount - 1));
        emit headerChanged(m_header);
        rebuild();
    }
}
