#pragma once

// ParadoxSMPS JSON-to-bytes compiler, C port of
// tools/paradoxsmps/json_to_header.py's compile_song() (itself already
// verified byte-exact against every real song/SFX). This is the ONE
// canonical compiler implementation -- both the real CMake build and
// ParadoxComposer link this same library directly, so there is no second
// hand-ported copy that can drift out of sync (the exact problem this
// replaces -- see project context on why ParadoxComposer's own earlier
// C++/Qt port was retired).

#include "json.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int success;
    uint8_t *bytes; // owned -- free with ps_compile_result_free()
    size_t byte_count;
    char error_message[256];
} PSCompileResult;

// `song`: the parsed document (header/voices/SMPSplaylist, see
// project_paradoxsmps_schema). Playlist block order is read directly from
// the SMPSplaylist object's own insertion order (this library's PJValue
// preserves it exactly, unlike Qt's QJsonObject -- see json.h's own
// comment), so there is no separate order parameter to pass or forget,
// unlike the retired Qt/C++ port.
PSCompileResult ps_compile_song(const PJValue *song);
void ps_compile_result_free(PSCompileResult *result);

// Note-name -> raw byte value (nRst=0x80, nC0=0x81, chromatic upward).
// Returns -1 for an unrecognized name instead of failing -- used by the
// DAW's piano roll for note-name/row display, where a bad name should just
// mean "can't place this row", not abort a whole compile.
int ps_note_value(const char *name);

#ifdef __cplusplus
}
#endif
