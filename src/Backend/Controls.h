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

void Controls_Reset(void);
// The first gamepad button (other than the D-pad) currently held, or -1. Used by the configuration dialog.
int Controls_PollPadButton(void);
const char *Controls_PadButtonName(int button);
const char *Controls_KeyName(int scancode);

#ifdef __cplusplus
}
#endif
