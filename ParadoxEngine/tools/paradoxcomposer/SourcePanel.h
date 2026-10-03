#pragma once

#include <QWidget>

class QPlainTextEdit;
class QLabel;
class SongDocument;

// Shows/edits the loaded document as its own literal JSON text -- the
// schema's JSONC *is* already the human-readable form (no separate
// tree/disassembly view invented on top of it), so this panel is
// deliberately just a text editor over that text, with an "Apply" step that
// reparses it back into the SongDocument.
class SourcePanel : public QWidget {
    Q_OBJECT
public:
    explicit SourcePanel(QWidget *parent = nullptr);

    void setText(const QString &jsonText);
    QString text() const;

signals:
    void applyRequested(const QString &jsonText);

private:
    QPlainTextEdit *m_editor;
    QLabel *m_status;
};
