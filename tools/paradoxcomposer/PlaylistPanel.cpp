#include "PlaylistPanel.h"
#include "PianoRollWidgets.h"

#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <functional>

namespace {

// One block's card in the horizontal row -- color-coded (per your
// direction: red = a channel's home block, purple = contains a loop,
// yellow = normal) with a mini piano-roll thumbnail of its own notes.
// Plain callbacks instead of Qt signals so this doesn't need Q_OBJECT/moc
// for a class defined entirely in this .cpp file.
class BlockCard : public QFrame {
public:
    explicit BlockCard(const QString &name, QWidget *parent = nullptr) : QFrame(parent), m_name(name) {
        setFixedSize(150, 110);
        setFrameShape(QFrame::StyledPanel);
        setCursor(Qt::PointingHandCursor);
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(3, 3, 3, 3);
        auto *nameLabel = new QLabel(name, this);
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet("font-weight: bold;");
        m_thumbnail = new NoteThumbnailWidget(this);
        layout->addWidget(nameLabel);
        layout->addWidget(m_thumbnail, 1);
    }

    void setEvents(const QJsonArray &events) { m_thumbnail->setEvents(events); }

    // classification: "home" (red), "loop" (purple), "normal" (yellow).
    void setClassification(const QString &classification, bool selected) {
        const QString accent = classification == "home"   ? "#c0392b"
                                : classification == "loop" ? "#8e44ad"
                                                            : "#d4ac0d"; // normal
        const int borderWidth = selected ? 4 : 2;
        setStyleSheet(QString("QFrame { border: %1px solid %2; border-radius: 5px; background: #1e1e22; }")
                          .arg(borderWidth)
                          .arg(accent));
    }

    std::function<void(const QString &)> onClicked;
    std::function<void(const QString &)> onDoubleClicked;

protected:
    void mousePressEvent(QMouseEvent *) override {
        if (onClicked)
            onClicked(m_name);
    }
    void mouseDoubleClickEvent(QMouseEvent *) override {
        if (onDoubleClicked)
            onDoubleClicked(m_name);
    }

private:
    QString m_name;
    NoteThumbnailWidget *m_thumbnail;
};

} // namespace

PlaylistPanel::PlaylistPanel(QWidget *parent) : QWidget(parent) {
    auto *mainLayout = new QVBoxLayout(this);

    auto *buttonRow = new QHBoxLayout();
    auto *addButton = new QPushButton("+ Block", this);
    auto *renameButton = new QPushButton("Rename", this);
    auto *removeButton = new QPushButton("- Block", this);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, "New block", "Block name:", QLineEdit::Normal, QString(), &ok);
        if (!ok || name.isEmpty() || m_playlist.contains(name))
            return;
        m_playlist[name] = QJsonArray{};
        m_order.append(name);
        rebuildCardRow();
        emit playlistChanged(m_playlist, m_order);
    });
    connect(renameButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedBlockName.isEmpty())
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, "Rename block", "Block name:", QLineEdit::Normal,
                                                     m_selectedBlockName, &ok);
        if (!ok || name.isEmpty() || name == m_selectedBlockName || m_playlist.contains(name))
            return;
        writeEditorIntoSelectedBlock();
        m_playlist[name] = m_playlist.value(m_selectedBlockName);
        m_playlist.remove(m_selectedBlockName);
        const int pos = m_order.indexOf(m_selectedBlockName);
        if (pos >= 0)
            m_order[pos] = name; // rename in place -- preserves compiled block order
        else
            m_order.append(name);
        QMessageBox::information(this, "Renamed",
                                  "Block renamed. Note: any \"jumpTo\"/\"start\" references to the old name "
                                  "elsewhere in the document must be updated by hand in the Source panel.");
        m_selectedBlockName = name;
        rebuildCardRow();
        emit playlistChanged(m_playlist, m_order);
    });
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        if (m_selectedBlockName.isEmpty())
            return;
        m_playlist.remove(m_selectedBlockName);
        m_order.removeAll(m_selectedBlockName);
        m_selectedBlockName.clear();
        rebuildCardRow();
        emit playlistChanged(m_playlist, m_order);
    });
    buttonRow->addWidget(addButton);
    buttonRow->addWidget(renameButton);
    buttonRow->addWidget(removeButton);
    buttonRow->addStretch();
    mainLayout->addLayout(buttonRow);

    // Horizontal row of block cards (per your Qt Designer reference),
    // single-click selects, double-click opens a Piano Roll window.
    auto *scroll = new QScrollArea(this);
    auto *cardContainer = new QWidget(scroll);
    m_cardRow = new QHBoxLayout(cardContainer);
    scroll->setWidget(cardContainer);
    scroll->setWidgetResizable(true);
    scroll->setFixedHeight(140);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(scroll);

    m_eventsEditor = new QPlainTextEdit(this);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_eventsEditor->setFont(mono);

    m_status = new QLabel(this);
    auto *applyButton = new QPushButton("Apply block edits", this);
    connect(applyButton, &QPushButton::clicked, this, [this]() { writeEditorIntoSelectedBlock(); });

    mainLayout->addWidget(new QLabel("Selected block's event array (JSON):", this));
    mainLayout->addWidget(m_eventsEditor, 1);
    auto *applyRow = new QHBoxLayout();
    applyRow->addWidget(applyButton);
    applyRow->addWidget(m_status, 1);
    mainLayout->addLayout(applyRow);
}

void PlaylistPanel::setHeader(const QJsonArray &header) {
    m_header = header;
    rebuildCardRow();
}

void PlaylistPanel::setPlaylist(const QJsonObject &playlist, const QStringList &order) {
    m_playlist = playlist;
    m_order.clear();
    for (const QString &name : order)
        if (playlist.contains(name))
            m_order.append(name);
    for (auto it = playlist.constBegin(); it != playlist.constEnd(); ++it)
        if (!m_order.contains(it.key()))
            m_order.append(it.key());
    m_selectedBlockName.clear();
    rebuildCardRow();
}

QString PlaylistPanel::classifyBlock(const QString &name) const {
    for (const QJsonValue &v : m_header) {
        const QJsonObject entry = v.toObject();
        for (auto it = entry.constBegin(); it != entry.constEnd(); ++it) {
            if (it.key() != "smpsHeaderDAC" && it.key() != "smpsHeaderFM" && it.key() != "smpsHeaderPSG")
                continue;
            const QJsonArray args = it.value().toArray();
            if (!args.isEmpty() && args.at(0).toString() == name)
                return "home";
        }
    }
    const QJsonArray events = m_playlist.value(name).toArray();
    for (const QJsonValue &ev : events) {
        if (!ev.isObject())
            continue;
        const QJsonObject obj = ev.toObject();
        if (obj.contains("smpsLoop") || (obj.contains("jumpTo") && obj.contains("smpsLoopArgs")))
            return "loop";
    }
    return "normal";
}

void PlaylistPanel::rebuildCardRow() {
    QLayoutItem *item;
    while ((item = m_cardRow->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    for (const QString &name : m_order) {
        auto *card = new BlockCard(name, this);
        card->setEvents(m_playlist.value(name).toArray());
        card->setClassification(classifyBlock(name), name == m_selectedBlockName);
        card->onClicked = [this](const QString &n) { selectBlock(n); };
        card->onDoubleClicked = [this](const QString &n) { emit blockDoubleClicked(n, m_playlist.value(n).toArray()); };
        m_cardRow->addWidget(card);
    }
    m_cardRow->addStretch();
}

void PlaylistPanel::selectBlock(const QString &name) {
    if (!m_loading && !m_selectedBlockName.isEmpty())
        writeEditorIntoSelectedBlock();
    m_selectedBlockName = name;
    loadSelectedBlockIntoEditor();
    rebuildCardRow(); // refresh selection highlight
}

void PlaylistPanel::loadSelectedBlockIntoEditor() {
    m_loading = true;
    if (m_selectedBlockName.isEmpty() || !m_playlist.contains(m_selectedBlockName)) {
        m_eventsEditor->setPlainText(QString());
        m_loading = false;
        return;
    }
    const QJsonArray events = m_playlist.value(m_selectedBlockName).toArray();
    const QJsonDocument doc(events);
    m_eventsEditor->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    m_status->clear();
    m_loading = false;
}

void PlaylistPanel::writeEditorIntoSelectedBlock() {
    if (m_selectedBlockName.isEmpty())
        return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(m_eventsEditor->toPlainText().toUtf8(), &err);
    if (!doc.isArray()) {
        m_status->setText("Parse error: " + err.errorString());
        return;
    }
    m_playlist[m_selectedBlockName] = doc.array();
    m_status->setText("Applied.");
    emit playlistChanged(m_playlist, m_order);
}
