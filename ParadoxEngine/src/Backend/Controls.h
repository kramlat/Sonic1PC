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
// The second player's (a game with a two-player mode reads them through Joypad_GetState2): the keyboard keys and the buttons of the second gamepad, a pad of its own
extern int Controls_Key2[CTL_COUNT][2];
extern int Controls_Pad2[CTL_COUNT];

// The binding of a player (0 or 1): the pointers to change it by
int *Controls_KeySlot(int player, int ctl, int slot);
int *Controls_PadSlot(int player, int ctl);

// How far a stick has to move before it counts, as a percentage of its travel: a worn stick that drifts can be told to ignore its rest
// position without Steam Input. The left stick drives the D-pad; the right one only the debug tools.
#define CONTROLS_DEADZONE_DEFAULT 24
#define CONTROLS_DEADZONE_MAX     90
extern int Controls_DeadzoneLeft, Controls_DeadzoneRight;
extern int Controls_DeadzoneRing; // the dead zone is a ring (the stick's overall push) instead of a box (each axis on its own)
// The second player's left stick has a dead zone of its own (his gamepad is another device, with its own drift)
extern int Controls_DeadzoneLeft2;
extern int Controls_DeadzoneRing2;

// Which gamepad is which player's. A player's choice is a connected gamepad (its GUID and which of the pads with that GUID it is), or none: automatic, the
// first not taken by the other player's choice (for the first player, then the second's). The device list is the connected game controllers in the order
// SDL has them.
extern char Controls_PadGuid[2][40];
extern int Controls_PadNth[2];
int Controls_PadDeviceCount(void);
const char *Controls_PadDeviceName(int device);
int Controls_PadAssigned(int player);                         // the device a player has now, or -1
void Controls_ChoosePad(int player, int device);              // device >= 0 pins that pad to the player, -1 returns to automatic
void Controls_ReassignPads(void);                             // opens the pads again by the choices (done on hot-plug and by ChoosePad)

void Controls_Reset(void);
// The first gamepad button (other than the D-pad) currently held on a player's gamepad (the first connected is the first player's, the second the second's), or -1. Used by the configuration dialog.
int Controls_PollPadButton(int player);
const char *Controls_PadButtonName(int button);
const char *Controls_KeyName(int scancode);
// The sticks of a player's gamepad as raw values (-32768 to 32767, left X/Y then right X/Y), refreshed; false if he has none.
bool Controls_PollAxes(int player, int *lx, int *ly, int *rx, int *ry);

#ifdef __cplusplus
}
#endif
