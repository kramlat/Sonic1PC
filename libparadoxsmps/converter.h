#pragma once

// ParadoxSMPS asm-to-json converter, C port of tools/paradoxsmps/asm_to_json.py's
// convert_file(), function-for-function -- see that file's own module
// docstring for the schema design this implements. Migration tool, not part
// of the CMake build or the DAW's hot path; ported anyway so there is
// exactly one implementation of every ParadoxSMPS transform in the project,
// same reasoning as compiler.c replacing json_to_header.py.

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int success;
    PJValue *result; // owned; {"header":.., "voices":.., "SMPSplaylist":..}; NULL on failure
    char error_message[256];
} PSConvertResult;

// `text`: the whole .asm file's contents, already read by the caller (same
// convention as compiler.h's ps_compile_song taking an already-parsed
// PJValue rather than a path). Warnings about unresolved smpsLoop/smpsJump
// targets are printed directly to stderr, matching asm_to_json.py's own
// behavior -- not collected into the result.
PSConvertResult ps_convert_asm(const char *text);
void ps_convert_result_free(PSConvertResult *result);

#ifdef __cplusplus
}
#endif
