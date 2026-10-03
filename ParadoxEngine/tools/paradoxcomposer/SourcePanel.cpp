#include "SourcePanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

SourcePanel::SourcePanel(QWidget *parent) : QWidget(parent) {
    m_editor = new QPlainTextEdit(this);
    m_editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_editor->setFont(mono);

    m_status = new QLabel(this);

    auto *applyButton = new QPushButton("Apply", this);
    connect(applyButton, &QPushButton::clicked, this, [this]() { emit applyRequested(m_editor->toPlainText()); });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_editor);
    auto *row = new QWidget(this);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->addWidget(applyButton);
    rowLayout->addWidget(m_status, 1);
    layout->addWidget(row);
}

void SourcePanel::setText(const QString &jsonText) { m_editor->setPlainText(jsonText); }

QString SourcePanel::text() const { return m_editor->toPlainText(); }
