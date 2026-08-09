#pragma once

#include <QByteArray>
#include <QWidget>

class QPlainTextEdit;
class QLabel;
class QPushButton;

// Stage 2/3 of the pipeline viewer: runs the current document through
// libparadoxsmps (NativeCompiler, a thin wrapper over the same C library
// jsonc2h_tool and the real CMake build link -- see NativeCompiler.h) and
// shows the resulting byte array as a hex dump, plus any compiler errors
// (e.g. unresolved jumpTo references) surfaced directly.
class BytesPanel : public QWidget {
    Q_OBJECT
public:
    explicit BytesPanel(QWidget *parent = nullptr);

    void showResult(bool success, const QByteArray &bytes, const QString &stderrOutput,
                     const QString &errorMessage);
    QByteArray lastCompiledBytes() const { return m_lastBytes; }

signals:
    void compileRequested();

private:
    QPushButton *m_compileButton;
    QPlainTextEdit *m_hexView;
    QLabel *m_statusLabel;
    QByteArray m_lastBytes;
};
