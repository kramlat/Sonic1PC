#pragma once

#include <stdint.h>
#include <stddef.h>

//Palette ids: an index into the game's palette_pointers table (which the game defines: the engine only loads what it is asked to)
typedef unsigned PaletteId;

//One entry of that table: where the colours are, where they go in the palette lines, and how many
typedef struct PalettePointer {
	const uint16_t *palette;
	uint16_t *target;
	size_t colours;
} PalettePointer;
extern PalettePointer palette_pointers[];

typedef struct {
	uint8_t ind, len;
} PaletteFade;

//Palette globals
extern int16_t pal_chgspeed;

extern uint16_t dry_palette[4][16];
extern uint16_t dry_palette_dup[4][16];
extern uint16_t wet_palette[4][16];
extern uint16_t wet_palette_dup[4][16];

extern PaletteFade palette_fade;

//Palette interface
void PalLoad1(PaletteId id);
void PalLoad2(PaletteId id);
void PalLoad3_Water(PaletteId id);
void PalLoad4_Water(PaletteId id);

//Palette fading
void FadeIn_FromBlack(void);
void PaletteFadeIn(void);
void PaletteFadeIn_At(uint8_t ind, uint8_t len);
void FadeOut_ToBlack(void);
void PaletteFadeOut(void);
void PaletteFadeOut_At(uint8_t ind, uint8_t len);

void WhiteIn_FromWhite(void);
void PaletteWhiteIn(void);
void PaletteWhiteIn_At(uint8_t ind, uint8_t len);
void WhiteOut_ToWhite(void);
void PaletteWhiteOut(void);
void PaletteWhiteOut_At(uint8_t ind, uint8_t len);
