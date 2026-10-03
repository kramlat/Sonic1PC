#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

// Thin C++ wrapper around libparadoxsmps (json.c + compiler.c) -- the SAME
// static library both the real CMake build (jsonc2h_tool) and this DAW link
// directly. There is exactly one compiler implementation in the project now;
// this file just converts at the Qt boundary (QJsonObject/QStringList in,
// QByteArray out) rather than reimplementing compile_song() a second time in
// C++, which is what this file used to do -- and the reason it could
// silently drift from the real compiler (json_to_header.py at the time).
namespace NativeCompiler {

struct Result {
    bool success = false;
    QByteArray compiledBytes;
    QString errorMessage; // mirrors the Python compiler's own exception text
    int driverVersion = 1; // from the song's own "driverVersion" field (PSCompileResult::driver_version) -- see SMPS driver-version-3 plan
};

// playlistOrder: SongDocument::playlistOrder() -- MUST be passed explicitly
// rather than derived from `song["SMPSplaylist"]`'s own key iteration,
// since QJsonObject does not preserve source key order (verified
// empirically; see SongDocument.h's own comment on this). Block order
// affects compiled addresses, so this is a correctness requirement, not a
// style preference.
Result compile(const QJsonObject &song, const QStringList &playlistOrder);

// Note-name -> raw byte value (nRst=0x80, nC0=0x81, chromatic upward --
// matches json_to_header.py's note_value() exactly). Exposed publicly so
// the piano roll editor's pitch layout uses the SAME mapping the compiler
// does, rather than a second hand-written copy that could drift.
int noteValue(const QString &name);

} // namespace NativeCompiler
