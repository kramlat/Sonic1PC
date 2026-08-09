#include "PlaybackPanel.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PlaybackPanel::PlaybackPanel(QWidget *parent) : QWidget(parent) {
    m_sfxCheck = new QCheckBox("This is an SFX, not a music track", this);
    m_playButton = new QPushButton("Play compiled song", this);
    m_stopButton = new QPushButton("Stop", this);
    m_statusLabel = new QLabel("Idle", this);

    connect(m_playButton, &QPushButton::clicked, this, [this]() { emit playRequested(m_sfxCheck->isChecked()); });
    connect(m_stopButton, &QPushButton::clicked, this, &PlaybackPanel::stopRequested);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_sfxCheck);
    layout->addWidget(m_playButton);
    layout->addWidget(m_stopButton);
    layout->addWidget(m_statusLabel);
    layout->addStretch();
}

void PlaybackPanel::setStatus(const QString &text) { m_statusLabel->setText(text); }
