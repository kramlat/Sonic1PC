#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QListWidget;
class QLineEdit;
class QDial;
class QLabel;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QTabWidget;
class AlgorithmDiagramWidget;
class EnvelopeViewWidget;
class HorizontalPianoWidget;

// Voice bank editor: list of voices in the current document, each editable
// via QDial knobs per operator param (per your direction -- knobs, not
// sliders) plus an optional human-readable "name" field (metadata the real
// compiler already ignores -- see json_to_header.py's emit_voice(), which
// only ever reads known keys via .get()). Also supports importing/exporting
// single voices against a standalone voice bank file
// ({"paradoxVoiceBank": true, "voices": [...]}), independent of any song.
class VoiceBankPanel : public QWidget {
    Q_OBJECT
public:
    explicit VoiceBankPanel(QWidget *parent = nullptr);

    void setVoices(const QJsonArray &voices);
    QJsonArray voices() const { return m_voices; }
    // The voice currently selected in this panel -- used by the piano
    // roll's key-press preview (plays through whichever voice the user is
    // actively looking at here), empty object if none selected.
    QJsonObject currentVoice() const {
        return (m_selectedIndex >= 0 && m_selectedIndex < m_voices.size()) ? m_voices.at(m_selectedIndex).toObject()
                                                                             : QJsonObject();
    }

signals:
    void voicesChanged(const QJsonArray &voices);
    // Emitted on piano key-press for live preview -- always at channel
    // volume attenuation 0 (full carrier level), matching a synth plugin's
    // own "play this preset" keyboard, not real in-song channel volume.
    void previewRequested(const QJsonObject &voice, int noteIndex);
    void previewStopRequested();

private:
    struct Knob {
        QDial *dial;
        QLabel *valueLabel;
        QString field; // e.g. "smpsVcDetune" -- key inside the operator object
    };

    void rebuildVoiceList();
    void loadSelectedVoiceIntoEditor();
    void writeEditorIntoSelectedVoice();
    Knob makeKnob(QWidget *parent, const QString &title, const QString &field, int lo, int hi);
    void refreshKnobLabel(Knob &k);

    void updateAlgorithmDiagram();
    void updateEnvelopeView();

    QListWidget *m_list;
    QLineEdit *m_nameEdit;
    AlgorithmDiagramWidget *m_algorithmDiagram;
    QDial *m_algorithmDial;
    QLabel *m_algorithmLabel;
    QDial *m_feedbackDial;
    QLabel *m_feedbackLabel;
    QCheckBox *m_unusedBitsCheck;
    QTabWidget *m_opTabs; // OP1-OP4 tabs, per your Qt Designer reference layout
    QVector<Knob> m_opKnobs[4]; // 10 knobs per operator
    EnvelopeViewWidget *m_envelopeView; // reflects the currently-active operator tab
    HorizontalPianoWidget *m_piano;

    QJsonArray m_voices;
    int m_selectedIndex = -1;
    bool m_loading = false; // guards against feedback loops while populating widgets
};
