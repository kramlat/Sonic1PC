#include "SDL.h"
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "Backend/Controls.h"
#include "Backend/VDP.h"
#include "Backend/Joypad.h"
#include "Backend/VDP.h"

#include "../Qt/QtHost.h"
// The window is a Qt one, so the keyboard comes from QtHost (same SDL scancode indexing as
// SDL_GetKeyboardState); gamepads still come from SDL.
#define KEYBOARD_STATE() QtHost_KeyState()

// Gamepad support. Deliberately uses SDL_GameController (not raw
// SDL_Joystick) so Steam Input's virtual controller -- which SDL sees as a
// standard Xbox-style pad regardless of the physical hardware or the user's
// Steam Input button/stick remapping -- works out of the box. Steam Input's
// virtual device can appear (or disappear, e.g. the overlay taking it over)
// after startup, so it's tracked via hotplug events rather than opened once.
static SDL_GameController *pad = NULL;
#define STICK_DEADZONE 8000 // out of a signed 16-bit axis range

// Rebindable controls. Defaults: arrows or WASD for the D-pad, B/N/M for A/B/C, Return for Start; on a pad
// X/A/B -> Genesis A/B/C and Start. F11 and the backtick (console) stay reserved.
int Controls_Key[CTL_COUNT][2] = {
	[CTL_UP]    = {SDL_SCANCODE_UP,    SDL_SCANCODE_W},
	[CTL_DOWN]  = {SDL_SCANCODE_DOWN,  SDL_SCANCODE_S},
	[CTL_LEFT]  = {SDL_SCANCODE_LEFT,  SDL_SCANCODE_A},
	[CTL_RIGHT] = {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D},
	[CTL_A]     = {SDL_SCANCODE_B,     SDL_SCANCODE_UNKNOWN},
	[CTL_B]     = {SDL_SCANCODE_N,     SDL_SCANCODE_UNKNOWN},
	[CTL_C]     = {SDL_SCANCODE_M,     SDL_SCANCODE_UNKNOWN},
	[CTL_START] = {SDL_SCANCODE_RETURN, SDL_SCANCODE_UNKNOWN},
};
int Controls_DeadzoneLeft = CONTROLS_DEADZONE_DEFAULT, Controls_DeadzoneRight = CONTROLS_DEADZONE_DEFAULT;
int Controls_DeadzoneRing = 0; // 0: a box (each axis on its own), 1: a ring (the stick's overall push)

// A stick's dead zone as a raw axis value.
static int DeadzoneRaw(int percent) {
	return percent * 32767 / 100;
}

// Which directions a stick is pushed in. A box dead zone looks at each axis on its own; a ring one first asks whether the stick is
// pushed far enough at all, in any direction (so drift that sits a little off to one side stays ignored), and then reads the
// direction as one of eight, a component counting when it is more than about 38% of the push (sin 22.5 degrees).
static void StickDirections(int x, int y, int percent, bool *left, bool *right, bool *up, bool *down) {
	int dead = DeadzoneRaw(percent);
	*left = *right = *up = *down = false;
	if (Controls_DeadzoneRing) {
		double push2 = (double)x * x + (double)y * y;
		if (push2 <= (double)dead * dead)
			return;
		double part = sqrt(push2) * 0.3827;
		*right = x > part;
		*left = x < -part;
		*down = y > part;
		*up = y < -part;
		return;
	}
	*right = x > dead;
	*left = x < -dead;
	*down = y > dead;
	*up = y < -dead;
}

int Controls_Pad[CTL_COUNT] = {
	[CTL_UP] = -1, [CTL_DOWN] = -1, [CTL_LEFT] = -1, [CTL_RIGHT] = -1,
	[CTL_A] = SDL_CONTROLLER_BUTTON_X, [CTL_B] = SDL_CONTROLLER_BUTTON_A,
	[CTL_C] = SDL_CONTROLLER_BUTTON_B, [CTL_START] = SDL_CONTROLLER_BUTTON_START,
};

void Controls_Reset(void) {
	Controls_DeadzoneLeft = Controls_DeadzoneRight = CONTROLS_DEADZONE_DEFAULT;
	Controls_DeadzoneRing = 0;
	static const int keys[CTL_COUNT][2] = {
		{SDL_SCANCODE_UP, SDL_SCANCODE_W}, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S},
		{SDL_SCANCODE_LEFT, SDL_SCANCODE_A}, {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D},
		{SDL_SCANCODE_B, 0}, {SDL_SCANCODE_N, 0}, {SDL_SCANCODE_M, 0}, {SDL_SCANCODE_RETURN, 0},
	};
	static const int pads[CTL_COUNT] = {-1, -1, -1, -1, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_A,
	                                    SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_START};
	memcpy(Controls_Key, keys, sizeof(keys));
	memcpy(Controls_Pad, pads, sizeof(pads));
}

static bool KeyBound(const uint8_t *key_state, int ctl) {
	for (int i = 0; i < 2; i++) {
		int sc = Controls_Key[ctl][i];
		if (sc > 0 && sc < SDL_NUM_SCANCODES && key_state[sc])
			return true;
	}
	return false;
}

static bool PadBound(int ctl) {
	return Controls_Pad[ctl] >= 0 && SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)Controls_Pad[ctl]);
}

int Controls_PollPadButton(void) {
	if (!pad || !SDL_GameControllerGetAttached(pad))
		return -1;
	SDL_GameControllerUpdate(); // (the game loop is not running while the controls dialog is up)
	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++) {
		if (i >= SDL_CONTROLLER_BUTTON_DPAD_UP && i <= SDL_CONTROLLER_BUTTON_DPAD_RIGHT)
			continue;
		if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)i))
			return i;
	}
	return -1;
}

bool Controls_PollAxes(int *lx, int *ly, int *rx, int *ry) {
	if (!pad || !SDL_GameControllerGetAttached(pad))
		return false;
	SDL_GameControllerUpdate();
	*lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
	*ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
	*rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
	*ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
	return true;
}

const char *Controls_PadButtonName(int button) {
	const char *n = button >= 0 ? SDL_GameControllerGetStringForButton((SDL_GameControllerButton)button) : NULL;
	return n ? n : "";
}

const char *Controls_KeyName(int scancode) {
	return scancode > 0 ? SDL_GetScancodeName((SDL_Scancode)scancode) : "";
}

static void OpenFirstPad(void) {
	if (pad)
		return;
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			pad = SDL_GameControllerOpen(i);
			if (pad)
				break;
		}
	}
}

//Backend input interface
int Input_HandleEvents(void) {
	SDL_Event e;
	QtHost_PumpEvents();
	if (QtHost_ShouldQuit())
		return 1;
	QtHost_KeyEvent qe;
	while (QtHost_PollKeyEvent(&qe)) {
		// F11 toggles fullscreen (works in every build, not just the debug console one). Key repeat is ignored so holding it
		// doesn't flicker. (The keys' text goes to the console drawer straight from Qt; nothing here needs it as an SDL
		// event, and pushing a synthetic text event crashed inside SDL for any key that types a character, Return included.)
		if (qe.scancode == SDL_SCANCODE_F11 && !qe.repeat)
			Render_ToggleFullscreen();
	}
	while (SDL_PollEvent(&e)) {
		switch (e.type) {
			case SDL_QUIT:
				return 1;
			case SDL_CONTROLLERDEVICEADDED:
				if (!pad)
					pad = SDL_GameControllerOpen(e.cdevice.which);
				break;
			case SDL_CONTROLLERDEVICEREMOVED:
				if (pad && e.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
					SDL_GameControllerClose(pad);
					pad = NULL;
					OpenFirstPad(); // fall back to another pad, if any
				}
				break;
			default:
				break;
		}
	}
	return 0;
}

// Debug console only (Console.c, via Backend/Joypad.h's
// Joypad_SetTextInputMode wrapper) -- SDL_TEXTINPUT events (used above)
// only fire while text-input mode is active.
void Input_SetTextInputMode(bool enable) {
	(void)enable; // text arrives with the Qt key events (QtHost_PollKeyEvent), not from SDL
	return;
	if (enable)
		SDL_StartTextInput();
	else
		SDL_StopTextInput();
}


uint8_t Input_GetState1(void) {

	//Get keyboard state
	const uint8_t *key_state = KEYBOARD_STATE();
	uint8_t start = KeyBound(key_state, CTL_START) ? JPAD_START : 0;
	uint8_t a     = KeyBound(key_state, CTL_A)     ? JPAD_A     : 0;
	uint8_t b     = KeyBound(key_state, CTL_B)     ? JPAD_B     : 0;
	uint8_t c     = KeyBound(key_state, CTL_C)     ? JPAD_C     : 0;
	uint8_t right = KeyBound(key_state, CTL_RIGHT) ? JPAD_RIGHT : 0;
	uint8_t left  = KeyBound(key_state, CTL_LEFT)  ? JPAD_LEFT  : 0;
	uint8_t down  = KeyBound(key_state, CTL_DOWN)  ? JPAD_DOWN  : 0;
	uint8_t up    = KeyBound(key_state, CTL_UP)    ? JPAD_UP    : 0;

	//Merge in gamepad state, if one's connected: D-pad and left stick both
	//drive the Genesis D-pad, X/A/B -> Genesis A/B/C, Start/Options -> Start.
	if (pad && SDL_GameControllerGetAttached(pad)) {
		int16_t lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		int16_t ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);

		bool stick_left, stick_right, stick_up, stick_down;
		StickDirections(lx, ly, Controls_DeadzoneLeft, &stick_left, &stick_right, &stick_up, &stick_down);

		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || stick_right)
			right = JPAD_RIGHT;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || stick_left)
			left = JPAD_LEFT;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || stick_down)
			down = JPAD_DOWN;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || stick_up)
			up = JPAD_UP;

		if (PadBound(CTL_A))
			a = JPAD_A;
		if (PadBound(CTL_B))
			b = JPAD_B;
		if (PadBound(CTL_C))
			c = JPAD_C;
		if (PadBound(CTL_START))
			start = JPAD_START;
	}

	//VDP peek view (VRAM/CRAM debug overlay): Tab or Select/Back toggles it
	//on/off -- edge-detected, since it's a flip rather than a level, and
	//holding the button shouldn't flicker it every frame. While it's
	//showing, LB/RB/LT/RT pick which of the 4 CRAM palettes to view, and
	//the right stick (or O/L keys) page through VRAM.
	//Debug builds only -- release builds shouldn't expose a VRAM/CRAM
	//dump to players, and VDP_PALETTE_DISPLAY defaults to false regardless.
#ifndef NDEBUG
	static bool toggle_held_prev = false;
	bool toggle_held = false; // the VDP viewer is a window opened from the View menu
	if (toggle_held && !toggle_held_prev)
		VDP_PALETTE_DISPLAY = !VDP_PALETTE_DISPLAY;
	toggle_held_prev = toggle_held;

	if (VDP_PALETTE_DISPLAY) {
		bool lb = pad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
		bool rb = pad && SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
		bool lt = pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > STICK_DEADZONE;
		bool rt = pad && SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > STICK_DEADZONE;

		if (key_state[SDL_SCANCODE_1] || lb)
			CRAMPAL = 0;
		if (key_state[SDL_SCANCODE_2] || rb)
			CRAMPAL = 1;
		if (key_state[SDL_SCANCODE_3] || lt)
			CRAMPAL = 2;
		if (key_state[SDL_SCANCODE_4] || rt)
			CRAMPAL = 3;

		int16_t rx = pad ? SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) : 0;
		int16_t ry = pad ? SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) : 0;
		bool rs_left, rs_right, rs_up, rs_down;
		StickDirections(rx, ry, Controls_DeadzoneRight, &rs_left, &rs_right, &rs_up, &rs_down);
		if ((key_state[SDL_SCANCODE_O] || rs_up) && VRAMADDR > 0)
			VRAMADDR = VRAMADDR - 0x200;
		// VDP_DrawScanline's own palette-display overlay reads up to
		// VRAMADDR+0x963 bytes ahead of this cursor (the highest of its four
		// 8-row blocks, offset 0x600, at i=15/y=95, plus its own +3 byte
		// span) -- capping the cursor at 0xF800 still let that window run
		// off the end of the 64KB vdp_vram array. 0xF600 is the highest
		// multiple of the 0x200 step that keeps every block's read in
		// bounds.
		if ((key_state[SDL_SCANCODE_L] || rs_down) && VRAMADDR < 0xF600)
			VRAMADDR = VRAMADDR + 0x200;
	}

	//Z80 Peek (live YM2612/SN76489 register dump): Left Alt toggles it,
	//same edge-detected on/off flip as the VDP peek view above. No
	//controller equivalent bound yet (all the natural analog-stick/shoulder
	//buttons are already spoken for by VDP peek's own controls above).
	static bool z80_peek_toggle_held_prev = false;
	bool z80_peek_toggle_held = false; // the Sound Viewer is a window opened from the View menu
	if (z80_peek_toggle_held && !z80_peek_toggle_held_prev)
		Z80_PEEK_DISPLAY = !Z80_PEEK_DISPLAY;
	z80_peek_toggle_held_prev = z80_peek_toggle_held;
#endif

	//Return as bitfield
	return start | a | c | b | right | left | down | up;
}

// Extended (non-Genesis) bindings -- see Backend/Joypad.h's own comment.
// Currently just debug mode's "cycle item backward": / on keyboard, a real
// physical gamepad's Y button (not the Genesis-style virtual pad mapping
// used by Input_GetState1 above -- SDL_CONTROLLER_BUTTON_Y specifically).
uint8_t Input_GetExtState1(void) {
	const uint8_t *key_state = KEYBOARD_STATE();
	uint8_t state = key_state[SDL_SCANCODE_SLASH] ? JPAD_EXT_Y : 0;
	if (key_state[SDL_SCANCODE_COMMA] || key_state[SDL_SCANCODE_LEFTBRACKET])
		state |= JPAD_EXT_SUBTYPE_DEC;
	if (key_state[SDL_SCANCODE_PERIOD] || key_state[SDL_SCANCODE_RIGHTBRACKET])
		state |= JPAD_EXT_SUBTYPE_INC;

	if (pad && SDL_GameControllerGetAttached(pad)) {
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y))
			state |= JPAD_EXT_Y;
		// Right stick specifically -- left stick already drives Genesis
		// D-pad movement (see Input_GetState1 above), so this is the one
		// analog input on a modern pad with no Genesis-era meaning yet.
		int16_t rx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
		int16_t ry = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
		bool rs_left, rs_right, rs_up, rs_down;
		StickDirections(rx, ry, Controls_DeadzoneRight, &rs_left, &rs_right, &rs_up, &rs_down);
		if (rs_left)
			state |= JPAD_EXT_SUBTYPE_DEC;
		if (rs_right)
			state |= JPAD_EXT_SUBTYPE_INC;
	}
	return state;
}

uint8_t Input_GetState2(void) {
	//No use in Sonic 1
	return 0;
}
