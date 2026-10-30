#include "LevelLayout.h"

#include <stdlib.h>
#include <string.h>

LevelLayoutSize level_layout_size;

static uint16_t Word(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

bool LevelLayout_Unpack(const uint8_t *file, size_t file_size, uint8_t *interleaved, int rows, LevelLayoutSize *size) {
    memset(interleaved, 0, (size_t)rows * 0x100);
    LevelLayoutSize s = { Word(file), Word(file + 2), Word(file + 4), Word(file + 6) };
    bool ok = true;
    for (int plane = 0; plane < 2; plane++) {
        const uint16_t width = plane ? s.bg_width : s.fg_width, count = plane ? s.bg_rows : s.fg_rows;
        for (int row = 0; row < count && row < LEVEL_LAYOUT_FILE_ROWS; row++) {
            const uint16_t pointer = Word(file + LEVEL_LAYOUT_FILE_HEADER + row * 4 + plane * 2);
            if (pointer == 0)
                continue;
            const size_t at = (size_t)pointer - LEVEL_LAYOUT_FILE_BASE;
            if (pointer < LEVEL_LAYOUT_FILE_BASE || (file_size != 0 && at + width > file_size)) {
                ok = false;
                continue;
            }
            if (row >= rows)
                continue; // (more rows than the buffer holds)
            memcpy(interleaved + (size_t)row * 0x100 + (size_t)plane * 0x80, file + at, width > 0x80 ? 0x80 : width);
        }
    }
    if (s.fg_width > 0x80) s.fg_width = 0x80;
    if (s.bg_width > 0x80) s.bg_width = 0x80;
    if (s.fg_rows > rows) s.fg_rows = (uint16_t)rows;
    if (s.bg_rows > rows) s.bg_rows = (uint16_t)rows;
    if (size)
        *size = s;
    return ok;
}

uint8_t *LevelLayout_Pack(const uint8_t *interleaved, int rows, size_t *out_size, LevelLayoutSize *size) {
    LevelLayoutSize s = { 0, 0, 0, 0 };
    for (int plane = 0; plane < 2; plane++)
        for (int row = 0; row < rows; row++)
            for (int col = 0; col < 0x80; col++)
                if (interleaved[row * 0x100 + plane * 0x80 + col] != 0) {
                    uint16_t *width = plane ? &s.bg_width : &s.fg_width, *count = plane ? &s.bg_rows : &s.fg_rows;
                    if (col + 1 > *width) *width = (uint16_t)(col + 1);
                    if (row + 1 > *count) *count = (uint16_t)(row + 1);
                }
    const size_t table = LEVEL_LAYOUT_FILE_HEADER + 2 * LEVEL_LAYOUT_FILE_ROWS * 2;
    const size_t total = table + (size_t)s.fg_width * s.fg_rows + (size_t)s.bg_width * s.bg_rows;
    uint8_t *out = calloc(total ? total : 1, 1);
    out[0] = (uint8_t)(s.fg_width >> 8); out[1] = (uint8_t)s.fg_width;
    out[2] = (uint8_t)(s.bg_width >> 8); out[3] = (uint8_t)s.bg_width;
    out[4] = (uint8_t)(s.fg_rows >> 8);  out[5] = (uint8_t)s.fg_rows;
    out[6] = (uint8_t)(s.bg_rows >> 8);  out[7] = (uint8_t)s.bg_rows;
    size_t at = table;
    for (int plane = 0; plane < 2; plane++) // (all the foreground rows, then all the background rows: the order of the original's files)
        for (int row = 0; row < rows && row < LEVEL_LAYOUT_FILE_ROWS; row++) {
            const uint16_t width = plane ? s.bg_width : s.fg_width, count = plane ? s.bg_rows : s.fg_rows;
            if (row >= count || width == 0)
                continue;
            const uint16_t pointer = (uint16_t)(LEVEL_LAYOUT_FILE_BASE + at);
            out[LEVEL_LAYOUT_FILE_HEADER + row * 4 + plane * 2] = (uint8_t)(pointer >> 8);
            out[LEVEL_LAYOUT_FILE_HEADER + row * 4 + plane * 2 + 1] = (uint8_t)pointer;
            memcpy(out + at, interleaved + row * 0x100 + plane * 0x80, width);
            at += width;
        }
    if (out_size)
        *out_size = at;
    if (size)
        *size = s;
    return out;
}

#ifndef LEVELLAYOUT_NO_LEVEL // (the layout converter is built without the level's buffers)
#include "LevelData.h"
#include "Kosinski.h"

void LevelLayout_Resize(LevelLayoutPlane *plane, int width, int rows) {
    if (width < 0) width = 0;
    if (rows < 0) rows = 0;
    if (width == plane->width && rows == plane->rows)
        return;
    uint8_t *data = (width && rows) ? calloc((size_t)width * (size_t)rows, 1) : NULL;
    if (data != NULL && plane->data != NULL)
        for (int row = 0; row < rows && row < plane->rows; row++)
            memcpy(data + (size_t)row * width, plane->data + (size_t)row * plane->width, (size_t)(width < plane->width ? width : plane->width));
    free(plane->data);
    plane->data = data;
    plane->width = (uint16_t)(data ? width : 0);
    plane->rows = (uint16_t)(data ? rows : 0);
}

void LevelLayout_SetChunk(LevelLayoutPlane *plane, int x, int y, uint8_t chunk) {
    const int cx = x & (int)level_layout_col_mask, cy = y & (int)level_layout_row_mask;
    if (cx >= plane->width || cy >= plane->rows) {
        if (chunk == 0)
            return; // (nothing to write where there is nothing)
        LevelLayout_Resize(plane, cx >= plane->width ? cx + 1 : plane->width, cy >= plane->rows ? cy + 1 : plane->rows);
    }
    plane->data[(size_t)cy * plane->width + cx] = chunk;
}

void LevelLayout_ToInterleaved(uint8_t *interleaved) {
    memset(interleaved, 0, (size_t)LEVEL_LAYOUT_ROWS * 0x100);
    for (int row = 0; row < LEVEL_LAYOUT_ROWS; row++)
        for (int col = 0; col < LEVEL_LAYOUT_COLS; col++) {
            interleaved[row * 0x100 + col] = LevelLayout_Chunk(&level_layout_fg, col, row);
            interleaved[row * 0x100 + 0x80 + col] = LevelLayout_Chunk(&level_layout_bg, col, row);
        }
}

// The planes become the smallest size that holds everything that is not 0 in the fixed buffer
void LevelLayout_FromInterleaved(const uint8_t *interleaved) {
    for (int plane_i = 0; plane_i < 2; plane_i++) {
        LevelLayoutPlane *plane = plane_i ? &level_layout_bg : &level_layout_fg;
        int width = 0, rows = 0;
        for (int row = 0; row < LEVEL_LAYOUT_ROWS; row++)
            for (int col = 0; col < LEVEL_LAYOUT_COLS; col++)
                if (interleaved[row * 0x100 + plane_i * 0x80 + col] != 0) {
                    if (col + 1 > width) width = col + 1;
                    if (row + 1 > rows) rows = row + 1;
                }
        LevelLayout_Resize(plane, 0, 0);
        LevelLayout_Resize(plane, width, rows);
        for (int row = 0; row < rows; row++)
            memcpy(plane->data + (size_t)row * width, interleaved + row * 0x100 + plane_i * 0x80, (size_t)width);
    }
}

bool LevelLayout_Load(const uint8_t *file, size_t file_size) {
    level_layout_generation++;
    static uint8_t scratch[LEVEL_LAYOUT_ROWS * 0x100];
    const bool ok = LevelLayout_Unpack(file, file_size, scratch, LEVEL_LAYOUT_ROWS, &level_layout_size);
    // (each plane is exactly as big as its header says)
    for (int plane_i = 0; plane_i < 2; plane_i++) {
        LevelLayoutPlane *plane = plane_i ? &level_layout_bg : &level_layout_fg;
        const int width = plane_i ? level_layout_size.bg_width : level_layout_size.fg_width, rows = plane_i ? level_layout_size.bg_rows : level_layout_size.fg_rows;
        LevelLayout_Resize(plane, 0, 0);
        LevelLayout_Resize(plane, width, rows);
        for (int row = 0; row < plane->rows; row++)
            memcpy(plane->data + (size_t)row * plane->width, scratch + row * 0x100 + plane_i * 0x80, plane->width);
    }
    return ok;
}

bool LevelLayout_LoadKosinski(const uint8_t *compressed) {
    static uint8_t file[0x4000]; // (a file holds up to $88 bytes of header and 16 rows of 2 planes of $80)
    const size_t size = (size_t)(KosDec(compressed, file) - file);
    return LevelLayout_Load(file, size);
}
#endif
