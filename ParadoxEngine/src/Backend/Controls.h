#pragma once

// Rebindable controls (Settings > Configure Sonic the Hedgehog). The keyboard keys are SDL scancodes (two per
// button); the gamepad buttons are SDL_GameControllerButton values (-1 = none). The D-pad and left stick always drive
// the Genesis D-pad on a gamepad; only A, B, C and Start can be moved to other pad buttons.

enum {
	CTL_UP, CTL_DOWN, CTL_LEFT, CTL_RIGHT, CTL_A, CTL_B, CTL_C, CTL_START,
	CTL_COUNT
};

#ifdef __cplusplus
extern "C" {
#endif

extern int Controls_Key[CTL_COUNT][2];
extern int Controls_Pad[CTL_COUNT];

// How far a stick has to move before it counts, as a percentage of its travel: a worn stick that drifts can be told to ignore its rest
// position without Steam Input. The left stick drives the D-pad; the right one only the debug tools.
#define CONTROLS_DEADZONE_DEFAULT 24
#define CONTROLS_DEADZONE_MAX     90
extern int Controls_DeadzoneLeft, Controls_DeadzoneRight;
extern int Controls_DeadzoneRing; // the dead zone is a ring (the stick's overall push) instead of a box (each axis on its own)

void Controls_Reset(void);
// The first gamepad button (other than the D-pad) currently held, or -1. Used by the configuration dialog.
int Controls_PollPadButton(void);
const char *Controls_PadButtonName(int button);
const char *Controls_KeyName(int scancode);
// The sticks of the gamepad in use as raw values (-32768 to 32767, left X/Y then right X/Y), refreshed; false if there is none.
bool Controls_PollAxes(int *lx, int *ly, int *rx, int *ry);

#ifdef __cplusplus
}
#endif
