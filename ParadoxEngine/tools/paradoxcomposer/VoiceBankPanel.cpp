#include "VoiceBankPanel.h"
#include "SongDocument.h"
#include "VoiceWidgets.h"

#include <QCheckBox>
#include <QDial>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
struct OpFieldSpec {
    const char *field;
    const char *title;
    int lo, hi;
};
// Order matches VOICE_ORDER in asm_to_json.py / OP_WRITE_ORDER semantics --
// natural op1..op4 field order, not the physical write-order permutation
// (that's the compiler's own concern, never the schema's).
const OpFieldSpec kOpFields[10] = {
    {"smpsVcDetune", "Detune (DT)", 0, 7},          {"smpsVcCoarseFreq", "Multiple (MUL)", 0, 15},
    {"smpsVcRateScale", "Rate Scale (RS)", 0, 3},   {"smpsVcAttackRate", "Attack (AR)", 0, 31},
    {"smpsVcAmpMod", "Amp Mod (AM)", 0, 1},         {"smpsVcDecayRate1", "Decay 1 (D1R)", 0, 31},
    {"smpsVcDecayRate2", "Decay 2 (D2R)", 0, 31},   {"smpsVcDecayLevel", "Decay Lvl (D1L)", 0, 15},
    {"smpsVcReleaseRate", "Release (RR)", 0, 15},   {"smpsVcTotalLevel", "Total Level (TL)", 0, 127},
};
} // namespace

VoiceBankPanel::VoiceBankPanel(QWidget *parent) : QWidget(parent) {
    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_loading && m_selectedIndex >= 0)
            writeEditorIntoSelectedVoice();
        m_selectedIndex = row;
        loadSelectedVoiceIntoEditor();
    });

    auto *addButton = new QPushButton("+ Voice", this);
    auto *removeButton = new QPushButton("- Voice", this);
    auto *importButton = new QPushButton("Import from bank...", this);
    auto *exportButton = new QPushButton("Save to bank...", this);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        QJsonObject v;
        v["operators"] = QJsonArray{QJsonObject(), QJsonObject(), QJsonObject(), QJsonObject()};
        m_voices.append(v);
        rebuildVoiceList();
        m_list->setCurrentRow(m_voices.size() - 1);
        emit voicesChanged(m_voices);
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedIndex < 0 || m_selectedIndex >= m_voices.size())
            return;
        m_voices.removeAt(m_selectedIndex);
        m_selectedIndex = -1;
        rebuildVoiceList();
        emit voicesChanged(m_voices);
    });
    connect(importButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, "Import voice from bank", QString(),
                                                            "ParadoxSMPS voice bank (*.jsonc *.json)");
        if (path.isEmpty())
            return;
        SongDocument bank;
        QString error;
        if (!bank.loadFromFile(path, &error) || !bank.isVoiceBank()) {
            QMessageBox::warning(this, "Import failed", error.isEmpty() ? "Not a voice bank file" : error);
            return;
        }
        const QJsonArray bankVoices = bank.voices();
        QStringList names;
        for (const QJsonValue &v : bankVoices)
            names << v.toObject().value("name").toString("(unnamed)");
        bool ok = false;
        const QString choice = QInputDialog::getItem(this, "Import voice", "Choose a voice:", names, 0, false, &ok);
        if (!ok)
            return;
        const int idx = names.indexOf(choice);
        if (idx < 0)
            return;
        m_voices.append(bankVoices.at(idx));
        rebuildVoiceList();
        m_list->setCurrentRow(m_voices.size() - 1);
        emit voicesChanged(m_voices);
    });
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedIndex < 0 || m_selectedIndex >= m_voices.size())
            return;
        writeEditorIntoSelectedVoice();
        const QString path = QFileDialog::getSaveFileName(this, "Save voice to bank", QString(),
                                                            "ParadoxSMPS voice bank (*.jsonc)");
        if (path.isEmpty())
            return;
        SongDocument bank;
        QString error;
        if (bank.loadFromFile(path, &error) && bank.isVoiceBank()) {
            QJsonArray existing = bank.voices();
            existing.append(m_voices.at(m_selectedIndex));
            bank.setVoices(existing);
        } else {
            bank = SongDocument::makeEmptyVoiceBank();
            bank.setVoices(QJsonArray{m_voices.at(m_selectedIndex)});
        }
        if (!bank.saveToFile(path, &error))
            QMessageBox::warning(this, "Save failed", error);
    });

    auto *listPanel = new QWidget(this);
    auto *listLayout = new QVBoxLayout(listPanel);
    listLayout->addWidget(m_list);
    auto *listButtons = new QHBoxLayout();
    listButtons->addWidget(addButton);
    listButtons->addWidget(removeButton);
    listLayout->addLayout(listButtons);
    listLayout->addWidget(importButton);
    listLayout->addWidget(exportButton);
    listPanel->setMaximumWidth(220);

    // Editor: name, algorithm/feedback, then 4 operator groups of knobs.
    auto *editorContent = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(editorContent);

    m_nameEdit = new QLineEdit(editorContent);
    m_nameEdit->setPlaceholderText("Voice name (editor-only, ignored by the compiler)");
    editorLayout->addWidget(m_nameEdit);

    // Top row, per your Qt Designer reference: algorithm-diagram icon, then
    // ALG/FB knobs, then unused-bit checkbox.
    auto *topRow = new QHBoxLayout();
    m_algorithmDiagram = new AlgorithmDiagramWidget(editorContent);
    topRow->addWidget(m_algorithmDiagram);

    m_algorithmDial = new QDial(editorContent);
    m_algorithmDial->setRange(0, 15);
    m_algorithmDial->setNotchesVisible(true);
    m_algorithmDial->setFixedSize(64, 64); // QDial defaults to expanding, which reflows/jumps as sibling widgets resize
    m_algorithmLabel = new QLabel("Algorithm: 0", editorContent);
    connect(m_algorithmDial, &QDial::valueChanged, this, [this](int v) {
        m_algorithmLabel->setText(QString("Algorithm: %1%2").arg(v).arg(v >= 8 ? " (custom ParadoxFM)" : ""));
        updateAlgorithmDiagram();
        updateEnvelopeView(); // TL mask depends on algorithm
    });
    auto *algBox = new QVBoxLayout();
    algBox->addWidget(m_algorithmLabel);
    algBox->addWidget(m_algorithmDial);

    m_feedbackDial = new QDial(editorContent);
    m_feedbackDial->setRange(0, 7);
    m_feedbackDial->setNotchesVisible(true);
    m_feedbackDial->setFixedSize(64, 64);
    m_feedbackLabel = new QLabel("Feedback: 0", editorContent);
    connect(m_feedbackDial, &QDial::valueChanged, this,
            [this](int v) { m_feedbackLabel->setText(QString("Feedback: %1").arg(v)); });
    auto *fbBox = new QVBoxLayout();
    fbBox->addWidget(m_feedbackLabel);
    fbBox->addWidget(m_feedbackDial);

    m_unusedBitsCheck = new QCheckBox("Unused bit", editorContent);

    topRow->addLayout(algBox);
    topRow->addLayout(fbBox);
    topRow->addWidget(m_unusedBitsCheck);
    topRow->addStretch();
    editorLayout->addLayout(topRow);

    // OP1-OP4 tabs (per your Qt Designer reference, replacing four stacked
    // group boxes) -- each tab holds that operator's 10 knobs, and switching
    // tabs updates the envelope viewer below to match.
    m_opTabs = new QTabWidget(editorContent);
    static const char *opTitles[4] = {"OP1", "OP2", "OP3", "OP4"};
    for (int op = 0; op < 4; op++) {
        auto *page = new QWidget(m_opTabs);
        auto *grid = new QGridLayout(page);
        for (int i = 0; i < 10; i++) {
            Knob k = makeKnob(page, kOpFields[i].title, kOpFields[i].field, kOpFields[i].lo, kOpFields[i].hi);
            int col = i % 5, row = (i / 5) * 2;
            grid->addWidget(k.valueLabel, row, col);
            grid->addWidget(k.dial, row + 1, col);
            connect(k.dial, &QDial::valueChanged, this, [this](int) { updateEnvelopeView(); });
            m_opKnobs[op].append(k);
        }
        m_opTabs->addTab(page, opTitles[op]);
    }
    connect(m_opTabs, &QTabWidget::currentChanged, this, [this](int) { updateEnvelopeView(); });
    editorLayout->addWidget(m_opTabs);

    // Envelope viewer for whichever operator tab is currently active, per
    // your Qt Designer reference layout.
    m_envelopeView = new EnvelopeViewWidget(editorContent);
    editorLayout->addWidget(m_envelopeView);

    // On-screen keyboard for auditioning the voice -- click/hold a key to
    // preview it, same model as a synth plugin's own keyboard (e.g. FL
    // Studio's Genny: click a key to hear the current preset). Always
    // previews at channel volume attenuation 0 (full carrier level).
    m_piano = new HorizontalPianoWidget(editorContent);
    connect(m_piano, &HorizontalPianoWidget::notePressed, this, [this](int note) {
        if (m_selectedIndex < 0 || m_selectedIndex >= m_voices.size())
            return;
        writeEditorIntoSelectedVoice();
        emit previewRequested(m_voices.at(m_selectedIndex).toObject(), note);
    });
    connect(m_piano, &HorizontalPianoWidget::noteReleased, this, [this]() { emit previewStopRequested(); });
    editorLayout->addWidget(m_piano);

    auto *scroll = new QScrollArea(this);
    scroll->setWidget(editorContent);
    scroll->setWidgetResizable(true);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->addWidget(listPanel);
    mainLayout->addWidget(scroll, 1);
}

VoiceBankPanel::Knob VoiceBankPanel::makeKnob(QWidget *parent, const QString &title, const QString &field, int lo,
                                               int hi) {
    Knob k;
    k.field = field;
    k.dial = new QDial(parent);
    k.dial->setRange(lo, hi);
    k.dial->setFixedSize(48, 48);
    k.valueLabel = new QLabel(QString("%1: %2").arg(title).arg(lo), parent);
    connect(k.dial, &QDial::valueChanged, this,
            [this, label = k.valueLabel, title](int v) { label->setText(QString("%1: %2").arg(title).arg(v)); });
    return k;
}

void VoiceBankPanel::setVoices(const QJsonArray &voices) {
    m_voices = voices;
    m_selectedIndex = -1;
    rebuildVoiceList();
}

void VoiceBankPanel::rebuildVoiceList() {
    m_loading = true;
    m_list->clear();
    for (int i = 0; i < m_voices.size(); i++) {
        const QJsonObject v = m_voices.at(i).toObject();
        const QString name = v.value("name").toString();
        m_list->addItem(QString("%1: %2").arg(i).arg(name.isEmpty() ? "(unnamed)" : name));
    }
    m_loading = false;
}

void VoiceBankPanel::loadSelectedVoiceIntoEditor() {
    m_loading = true;
    if (m_selectedIndex < 0 || m_selectedIndex >= m_voices.size()) {
        m_loading = false;
        return;
    }
    const QJsonObject v = m_voices.at(m_selectedIndex).toObject();
    m_nameEdit->setText(v.value("name").toString());
    m_algorithmDial->setValue(hexFieldToInt(v.value("smpsVcAlgorithm")));
    m_feedbackDial->setValue(hexFieldToInt(v.value("smpsVcFeedback")));
    m_unusedBitsCheck->setChecked(hexFieldToInt(v.value("smpsVcUnusedBits")) != 0);

    const QJsonArray ops = v.value("operators").toArray();
    for (int op = 0; op < 4; op++) {
        const QJsonObject o = (op < ops.size()) ? ops.at(op).toObject() : QJsonObject();
        for (Knob &k : m_opKnobs[op])
            k.dial->setValue(hexFieldToInt(o.value(k.field)));
    }
    m_loading = false;
    updateAlgorithmDiagram();
    updateEnvelopeView();
}

void VoiceBankPanel::updateAlgorithmDiagram() { m_algorithmDiagram->setAlgorithm(m_algorithmDial->value()); }

void VoiceBankPanel::updateEnvelopeView() {
    const int op = m_opTabs->currentIndex();
    if (op < 0 || op >= 4 || m_opKnobs[op].size() < 10)
        return;
    // Field order matches kOpFields: DT,MUL,RS,AR,AM,D1R,D2R,D1L,RR,TL.
    const int ar = m_opKnobs[op][3].dial->value();
    const int d1r = m_opKnobs[op][5].dial->value();
    const int d2r = m_opKnobs[op][6].dial->value();
    const int d1l = m_opKnobs[op][7].dial->value();
    const int rr = m_opKnobs[op][8].dial->value();
    const int tl = m_opKnobs[op][9].dial->value();
    m_envelopeView->setEnvelope(ar, d1r, d1l, d2r, rr, tl);
}

void VoiceBankPanel::writeEditorIntoSelectedVoice() {
    if (m_selectedIndex < 0 || m_selectedIndex >= m_voices.size())
        return;
    QJsonObject v = m_voices.at(m_selectedIndex).toObject();
    v["name"] = m_nameEdit->text();
    v["smpsVcAlgorithm"] = intToHexField(m_algorithmDial->value());
    v["smpsVcFeedback"] = intToHexField(m_feedbackDial->value());
    v["smpsVcUnusedBits"] = intToHexField(m_unusedBitsCheck->isChecked() ? 1 : 0);

    QJsonArray ops;
    for (int op = 0; op < 4; op++) {
        QJsonObject o;
        for (Knob &k : m_opKnobs[op])
            o[k.field] = intToHexField(k.dial->value());
        ops.append(o);
    }
    v["operators"] = ops;

    m_voices[m_selectedIndex] = v;
    emit voicesChanged(m_voices);
}
