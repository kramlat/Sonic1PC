#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QWidget>

class QHBoxLayout;
class QPlainTextEdit;
class QLabel;

// Structural editor for SMPSplaylist blocks, per your Qt Designer reference:
// a horizontal row of color-coded rectangles (red = a channel's home block,
// purple = contains a loop, yellow = normal), each showing a mini piano-roll
// thumbnail of its own notes. Single-click selects (loads into the JSON
// text editor below, for direct edits/add/rename/delete); double-click
// spawns a full Piano Roll editor window for that block (MainWindow's
// openPianoRollFor(), one window per block name, not auto-created --
// per your direction, "do not spawn the editor until one opens a block
// with it").
//
// Tracks block order explicitly (m_order), separate from QJsonObject's own
// iteration -- QJsonObject does not preserve source key order (see
// SongDocument.h's own comment), and block order affects compiled
// addresses, so the card row and every mutation here must go through
// m_order, never m_playlist's own iterator.
class PlaylistPanel : public QWidget {
    Q_OBJECT
public:
    explicit PlaylistPanel(QWidget *parent = nullptr);

    void setPlaylist(const QJsonObject &playlist, const QStringList &order);
    QJsonObject playlist() const { return m_playlist; }
    QStringList order() const { return m_order; }
    // Needed for the "home block" (red) classification -- call whenever
    // SongDocument's header changes.
    void setHeader(const QJsonArray &header);

signals:
    void playlistChanged(const QJsonObject &playlist, const QStringList &order);
    // Opens/raises a full Piano Roll editor window for this block.
    void blockDoubleClicked(const QString &name, const QJsonArray &events);

private:
    void rebuildCardRow();
    void loadSelectedBlockIntoEditor();
    void writeEditorIntoSelectedBlock();
    void selectBlock(const QString &name);
    // "home" (red, this block is some channel's loc), "loop" (purple,
    // contains a loop construct), or "normal" (yellow) -- home takes
    // priority over loop if a block happens to be both.
    QString classifyBlock(const QString &name) const;

    QHBoxLayout *m_cardRow;
    QPlainTextEdit *m_eventsEditor;
    QLabel *m_status;

    QJsonArray m_header;
    QJsonObject m_playlist;
    QStringList m_order;
    QString m_selectedBlockName;
    bool m_loading = false;
};
