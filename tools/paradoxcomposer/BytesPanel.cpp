#include "BytesPanel.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

BytesPanel::BytesPanel(QWidget *parent) : QWidget(parent) {
    // Compiling runs in-process via libparadoxsmps (no subprocess), so this
    // button is just a manual re-trigger for convenience -- MainWindow
    // actually recompiles automatically on every document change.
    m_compileButton = new QPushButton("Recompile now", this);
    connect(m_compileButton, &QPushButton::clicked, this, &BytesPanel::compileRequested);

    m_statusLabel = new QLabel("Not compiled yet", this);

    m_hexView = new QPlainTextEdit(this);
    m_hexView->setReadOnly(true);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_hexView->setFont(mono);

    auto *layout = new QVBoxLayout(this);
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_compileButton);
    layout->addLayout(buttonRow);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_hexView);
}

void BytesPanel::showResult(bool success, const QByteArray &bytes, const QString &stderrOutput,
                             const QString &errorMessage) {
    m_lastBytes = bytes;

    if (!errorMessage.isEmpty()) {
        m_statusLabel->setText("Launch failed: " + errorMessage);
        m_hexView->setPlainText(QString());
        return;
    }

    if (!success) {
        m_statusLabel->setText("Compile FAILED");
        m_hexView->setPlainText(stderrOutput);
        return;
    }

    m_statusLabel->setText(QString("Compiled OK -- %1 bytes").arg(bytes.size()));

    QString hex;
    for (int i = 0; i < bytes.size(); i += 16) {
        hex += QString("%1  ").arg(i, 4, 16, QChar('0')).toUpper();
        for (int j = i; j < qMin(i + 16, bytes.size()); j++)
            hex += QString("%1 ").arg((unsigned char)bytes[j], 2, 16, QChar('0')).toUpper();
        hex += "\n";
    }
    if (!stderrOutput.isEmpty())
        hex += "\n--- compiler stderr ---\n" + stderrOutput;
    m_hexView->setPlainText(hex);
}
