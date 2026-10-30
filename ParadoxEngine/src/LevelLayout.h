#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The level layout file: the Sonic 3 & Knuckles format (sonic3k.asm: Load_Level, sonic3k.constants.asm: Level_layout_header / Level_layout_main), so that a layout of that game loads as it is.
//
//   word  chunks in a foreground row      word  chunks in a background row
//   word  foreground rows                 word  background rows
//   $40 words: the rows' pointers, a foreground one then a background one for each row (0 where a plane has no such row)
//   the rows: one byte a chunk (0 is empty), every row of a plane as wide as the header says
//
// A row's pointer is $8000 plus its offset from the start of the file (the original's RAM address: the header and the pointers take $88 bytes, so the first row's is $8088). The file is not
// compressed. A level is as wide and as tall as its header says: nothing is padded to a fixed size, and the size comes with the level.
//
// In memory the layout stays the engine's fixed buffer (LevelData.h: level_layout, 16 rows of a $100 stride, the foreground in the first $80 bytes and the background in the second), zero
// where the file has no chunk, so everything that reads it is as before; level_layout_size says how much of it the file filled.

#define LEVEL_LAYOUT_FILE_HEADER 8
#define LEVEL_LAYOUT_FILE_ROWS 0x20      // the pointer table holds $40 words: a pair for each of 32 rows
#define LEVEL_LAYOUT_FILE_BASE 0x8000    // what a row's pointer is counted from

typedef struct {
    uint16_t fg_width, bg_width; // chunks in a row
    uint16_t fg_rows, bg_rows;
} LevelLayoutSize;

extern LevelLayoutSize level_layout_size;

// Reads a layout file into `interleaved` (rows * 0x100 bytes, cleared first; `rows` is how many rows it holds, widths are at most 0x80). Returns the sizes the file gave
// (clamped to what fits); false if the file is damaged (a row outside it).
bool LevelLayout_Unpack(const uint8_t *file, size_t file_size, uint8_t *interleaved, int rows, LevelLayoutSize *size);

// Writes a layout file from `interleaved`: the planes' sizes are the smallest that hold everything that is not 0. Returns a malloc'd buffer (the caller frees it), its size in *out_size.
uint8_t *LevelLayout_Pack(const uint8_t *interleaved, int rows, size_t *out_size, LevelLayoutSize *size);

// The level's layout: reads `file` into level_layout (and level_layout_size). `file_size` may be 0 when it is not known (the pointers are trusted then).
bool LevelLayout_Load(const uint8_t *file, size_t file_size);
// The same for a file that is Kosinski compressed: how the games keep them (the format is the Sonic 3 & Knuckles one, whose files are not compressed; uncompressed they would be
// three times the size of the interleaved, compressed layouts they replace, and Kosinski brings them back to it)
bool LevelLayout_LoadKosinski(const uint8_t *compressed);
