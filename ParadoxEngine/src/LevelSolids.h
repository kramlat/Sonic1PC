#pragma once

#include <stddef.h>
#include <stdint.h>

// A level's solid index in the Sonic 3 & Knuckles layout (sonic3k.asm: LoadSolids, SolidIndexes): one file for both paths of the level's blocks, a byte for each block in each, the two interleaved (the
// byte of path 1, then the byte of path 2, for block 0, then block 1, ...; `LoadSolids_SK_Interleaved_Format`: the secondary is the primary's address plus one). The three arrays every level shares (the
// height map, the width map and the angles) are not part of it and stay as they are. Here the file is Kosinski compressed, as the layouts are.
//
// In memory each path is still its own array (LevelData.h: coll_index), which is what the collision code reads.

// Splits an interleaved file (`size` bytes, as decompressed) into the two paths; returns how many blocks each has
size_t LevelSolids_Unpack(const uint8_t *file, size_t size, uint8_t *path1, uint8_t *path2);

// Makes the interleaved file of two paths of `blocks` blocks each (`file` holds 2 * blocks bytes)
void LevelSolids_Pack(const uint8_t *path1, const uint8_t *path2, size_t blocks, uint8_t *file);

// Decompresses a Kosinski compressed solid index into the two paths of coll_index; returns how many blocks each has
size_t LevelSolids_LoadKosinski(const uint8_t *compressed);
