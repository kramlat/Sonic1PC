#include "NativeCompiler.h"

#include "../../libparadoxsmps/compiler.h"
#include "../../libparadoxsmps/json.h"

#include <QJsonArray>
#include <QJsonDocument>

namespace NativeCompiler {
namespace {

// Serializes song/playlistOrder to the same compact JSON text
// SongDocument::toJsonText() produces (playlist blocks written out in
// playlistOrder, not QJsonObject's own -- order-losing -- iteration), then
// hands it to libparadoxsmps's own parser, which preserves object key order
// by construction (see json.h). This is the only place order needs
// re-threading at all -- once inside libparadoxsmps, PJValue carries it
// natively, unlike QJsonObject.
QString toJsonText(const QJsonObject &song, const QStringList &playlistOrder) {
    const QString headerText = QString::fromUtf8(QJsonDocument(song.value("header").toArray()).toJson(QJsonDocument::Compact));
    const QString voicesText = QString::fromUtf8(QJsonDocument(song.value("voices").toArray()).toJson(QJsonDocument::Compact));

    const QJsonObject pl = song.value("SMPSplaylist").toObject();
    QStringList order = playlistOrder;
    for (auto it = pl.constBegin(); it != pl.constEnd(); ++it)
        if (!order.contains(it.key()))
            order.append(it.key());

    QStringList blockEntries;
    for (const QString &name : order) {
        if (!pl.contains(name))
            continue;
        const QString eventsText =
            QString::fromUtf8(QJsonDocument(pl.value(name).toArray()).toJson(QJsonDocument::Compact));
        blockEntries << QString("\"%1\":%2").arg(name, eventsText);
    }

    return QString("{\"header\":%1,\"voices\":%2,\"SMPSplaylist\":{%3}}")
        .arg(headerText, voicesText, blockEntries.join(","));
}

} // namespace

Result compile(const QJsonObject &song, const QStringList &playlistOrder) {
    Result result;

    const QByteArray text = toJsonText(song, playlistOrder).toUtf8();
    char parseErr[256] = {0};
    PJValue *parsed = pj_parse(text.constData(), parseErr, sizeof(parseErr));
    if (!parsed) {
        result.errorMessage = QString::fromUtf8(parseErr);
        return result;
    }

    PSCompileResult native = ps_compile_song(parsed);
    pj_free(parsed);

    if (!native.success) {
        result.errorMessage = QString::fromUtf8(native.error_message);
        ps_compile_result_free(&native);
        return result;
    }

    result.success = true;
    result.compiledBytes = QByteArray(reinterpret_cast<const char *>(native.bytes), static_cast<int>(native.byte_count));
    ps_compile_result_free(&native);
    return result;
}

int noteValue(const QString &name) { return ps_note_value(name.toUtf8().constData()); }

} // namespace NativeCompiler
