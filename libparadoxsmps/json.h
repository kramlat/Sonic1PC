#pragma once

// Minimal, self-contained JSON(C) value model/parser/serializer -- written
// from scratch rather than vendoring a third-party library, since the
// schema's actual JSON usage is small and well-understood (objects/arrays/
// strings/numbers/bool/null, plus // and /* */ comments), and because
// object key INSERTION ORDER must be preserved exactly (Qt's QJsonObject
// does NOT do this -- verified empirically in ParadoxComposer's own
// SongDocument.h, a real bug this project already hit once; this parser is
// written so that mistake can't happen again by construction: pj_object is
// a linear ordered list, never a hash map).
//
// This is the shared foundation both the converter (asm_to_json) and
// compiler (json_to_header) C ports build on, so the DAW and the real CMake
// build link the exact same code -- see project context: "a C based
// converter and compiler can be hooled into by the daw directly and
// guarantee the same code quality consistently".

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PJ_NULL, PJ_BOOL, PJ_NUMBER, PJ_STRING, PJ_ARRAY, PJ_OBJECT } PJType;

typedef struct PJValue PJValue;

// --- Construction (all take ownership of any PJValue* passed in) ---
PJValue *pj_new_null(void);
PJValue *pj_new_bool(bool b);
PJValue *pj_new_number(double n);
PJValue *pj_new_string(const char *s); // copies s
PJValue *pj_new_array(void);
PJValue *pj_new_object(void);
void pj_free(PJValue *v);

// --- Array ---
void pj_array_append(PJValue *arr, PJValue *item); // arr takes ownership of item
size_t pj_array_size(const PJValue *arr);           // 0 if arr is NULL or not an array
PJValue *pj_array_get(const PJValue *arr, size_t index); // NULL if out of range

// --- Object (insertion order preserved; pj_object_set on an existing key
// updates its value in place, does not move it to the end) ---
void pj_object_set(PJValue *obj, const char *key, PJValue *value); // obj takes ownership of value
bool pj_object_has(const PJValue *obj, const char *key);
PJValue *pj_object_get(const PJValue *obj, const char *key); // NULL if missing
void pj_object_remove(PJValue *obj, const char *key);
size_t pj_object_size(const PJValue *obj);
const char *pj_object_key_at(const PJValue *obj, size_t index); // iteration in insertion order
PJValue *pj_object_value_at(const PJValue *obj, size_t index);
// The first key in the object, e.g. for the schema's own single-key header
// entries ({"smpsHeaderFM": [...]}) -- NULL if empty.
const char *pj_object_first_key(const PJValue *obj);

// --- Accessors with fallback (never NULL-crash on the wrong type) ---
PJType pj_type(const PJValue *v); // PJ_NULL if v itself is NULL
const char *pj_get_string(const PJValue *v, const char *fallback);
double pj_get_number(const PJValue *v, double fallback);
bool pj_get_bool(const PJValue *v, bool fallback);

// --- Parse/serialize ---
// Strips // and /* */ comments (respecting quoted strings) before parsing,
// same JSONC dialect as tools/paradoxsmps/json_to_header.py's own
// strip_jsonc_comments(). Returns NULL on a real parse error, writing a
// message into errbuf (if non-NULL).
PJValue *pj_parse(const char *text, char *errbuf, size_t errbuf_size);
// Caller frees the returned buffer (free()). pretty=true indents with 2
// spaces per level (for human-authored .jsonc files); false emits compact
// single-line JSON (for machine-to-machine use, e.g. feeding a compiler).
char *pj_serialize(const PJValue *v, bool pretty);

#ifdef __cplusplus
}
#endif
