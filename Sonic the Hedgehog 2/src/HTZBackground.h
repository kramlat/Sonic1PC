#ifndef _HTZBACKGROUND_H
#define _HTZBACKGROUND_H

#include <stdbool.h>
#include <stdint.h>

extern int16_t htz_layerdef[0x12];

void HTZBackground_Reset(void);
void HTZBackground_Animate(bool two_player); // the animated art routine's part for Hill Top (before the flowers)
void HTZBackground_Deform(void);             // the usual branch of the background scroll

// (pure parts, for the tests)
int HTZBackground_Step(int16_t camera_x);
int HTZBackground_LastStep(void);
void HTZBackground_StepChunks(int step, int chunks[6]);
void HTZBackground_BuildStrips(int16_t camera_x, const int16_t *layers, uint8_t out[0x100]);

#endif //_HTZBACKGROUND_H
