#ifndef _HTZBACKGROUND_H
#define _HTZBACKGROUND_H

#include <stdbool.h>
#include <stdint.h>

extern int16_t htz_layerdef[2][0x12];

// The second view of a split screen has a set of the mountains' 32 tiles of its own, this many tiles on from the first's ($500): its background plane's tiles from $500 to $51F are drawn as these
#define HTZ_P2_TILES 0x40

void HTZBackground_Reset(void);
void HTZBackground_Animate(void);             // the animated art routine's part for Hill Top (before the flowers): one set of tiles for each view
void HTZBackground_Deform(int view);          // the usual branch of the background scroll, for a view (0 the first, 1 the second)

// (pure parts, for the tests)
int HTZBackground_Step(int16_t camera_x);
int HTZBackground_LastStep(void);
void HTZBackground_StepChunks(int step, int chunks[6]);
void HTZBackground_BuildStrips(int16_t camera_x, const int16_t *layers, uint8_t out[0x100]);

#endif //_HTZBACKGROUND_H
