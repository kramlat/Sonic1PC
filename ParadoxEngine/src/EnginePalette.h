#pragma once

#include <stdint.h>
#include <stddef.h>

//Palette ids: an index into the game's palette_pointers table (which the game defines: the engine only loads what it is asked to)
typedef unsigned PaletteId;

//How the colours of a palette are stored. The Mega Drive's own is the default (what a table that does not say gets); the 24 bit ones are for art that is not limited to its 9 bit colour (the loader brings it down to the
//machine's 3 bits a channel for now, as the palette lines and the fades are in that format; wider lines come later)
typedef enum {
	PAL_FORMAT_GENESIS = 0, //16 bit big-endian words, 0x0BGR: 3 bits a channel, in bits 9-11, 5-7 and 1-3
	PAL_FORMAT_RGB24 = 1,   //3 bytes a colour: red, green, blue
	PAL_FORMAT_0RGB32 = 2,  //4 bytes a colour, big-endian like the Genesis words: 0x00RRGGBB (the top byte is ignored)
} PaletteFormat;

//One entry of that table: where the colours are, where they go in the palette lines, how many, and how they are stored
typedef struct PalettePointer {
	const void *palette;
	uint16_t *target;
	size_t colours;
	PaletteFormat format;
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

//One 24 bit colour as the machine's 0x0BGR (each channel rounded to the nearest of its 8 levels)
uint16_t Palette_FromRGB24(uint8_t r, uint8_t g, uint8_t b);

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
