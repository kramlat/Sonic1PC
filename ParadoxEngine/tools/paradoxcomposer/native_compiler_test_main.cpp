// Standalone correctness check for NativeCompiler: compiles a .jsonc file
// and writes the raw compiled bytes to stdout, so they can be byte-diffed
// against tools/paradoxsmps/json_to_header.py's own output on the same
// file (the actual source of truth). Not part of the ParadoxComposer GUI --
// a throwaway verification tool, same spirit as paradoxsmps_verify.
#include "NativeCompiler.h"
#include "SongDocument.h"

#include <QCoreApplication>
#include <QFile>
#include <cstdio>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (argc != 2) {
        fprintf(stderr, "Usage: native_compiler_test <input.jsonc>\n");
        return 1;
    }
    SongDocument doc;
    QString error;
    if (!doc.loadFromFile(argv[1], &error)) {
        fprintf(stderr, "load failed: %s\n", qPrintable(error));
        return 1;
    }
    const NativeCompiler::Result result = NativeCompiler::compile(doc.root(), doc.playlistOrder());
    if (!result.success) {
        fprintf(stderr, "compile failed: %s\n", qPrintable(result.errorMessage));
        return 1;
    }
    fwrite(result.compiledBytes.constData(), 1, result.compiledBytes.size(), stdout);
    fprintf(stderr, "%d bytes\n", result.compiledBytes.size());
    return 0;
}
