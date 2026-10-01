#include "SDL.h"
#include <stdbool.h>
#include <string.h>
#include "../../Game.h"

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
		memset(&e, 0, sizeof(e));
		e.type = SDL_KEYDOWN;
		e.key.keysym.scancode = (SDL_Scancode)qe.scancode;
		e.key.repeat = qe.repeat;
		SDL_PushEvent(&e); // handled by the SDL_KEYDOWN case below, same as a real key press
		if (qe.text[0]) {
			memset(&e, 0, sizeof(e));
			e.type = SDL_TEXTINPUT;
			strncpy(e.text.text, qe.text, sizeof(e.text.text) - 1);
			SDL_PushEvent(&e);
		}
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
			case SDL_KEYDOWN:
				// F11 toggles fullscreen (works in every build, not just the debug
				// console one). Ignore key repeat so holding it doesn't flicker.
				if (e.key.keysym.scancode == SDL_SCANCODE_F11 && !e.key.repeat) {
					Render_ToggleFullscreen();
					break;
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
	uint8_t start = key_state[SDL_SCANCODE_RETURN] ? JPAD_START : 0;
	// J/K/L -> A/B/C, and WASD -> D-pad (alongside the arrow keys below) --
	// keeps movement and jump on separate, non-overlapping key clusters,
	// closer to a standard PC control scheme than the old A/S/D-for-jump
	// (which collided with using A/D for movement).
	uint8_t a     = key_state[SDL_SCANCODE_B]      ? JPAD_A     : 0;
	uint8_t b     = key_state[SDL_SCANCODE_N]      ? JPAD_B     : 0;
	uint8_t c     = key_state[SDL_SCANCODE_M]      ? JPAD_C     : 0;
	uint8_t right = (key_state[SDL_SCANCODE_RIGHT] || key_state[SDL_SCANCODE_D]) ? JPAD_RIGHT : 0;
	uint8_t left  = (key_state[SDL_SCANCODE_LEFT]  || key_state[SDL_SCANCODE_A]) ? JPAD_LEFT  : 0;
	uint8_t down  = (key_state[SDL_SCANCODE_DOWN]  || key_state[SDL_SCANCODE_S]) ? JPAD_DOWN  : 0;
	uint8_t up    = (key_state[SDL_SCANCODE_UP]    || key_state[SDL_SCANCODE_W]) ? JPAD_UP    : 0;

	//Merge in gamepad state, if one's connected: D-pad and left stick both
	//drive the Genesis D-pad, X/A/B -> Genesis A/B/C, Start/Options -> Start.
	if (pad && SDL_GameControllerGetAttached(pad)) {
		int16_t lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
		int16_t ly = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);

		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > STICK_DEADZONE)
			right = JPAD_RIGHT;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -STICK_DEADZONE)
			left = JPAD_LEFT;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > STICK_DEADZONE)
			down = JPAD_DOWN;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -STICK_DEADZONE)
			up = JPAD_UP;

		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X))
			a = JPAD_A;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A))
			b = JPAD_B;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B))
			c = JPAD_C;
		if (SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START))
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

		int16_t ry = pad ? SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) : 0;
		if ((key_state[SDL_SCANCODE_O] || ry < -STICK_DEADZONE) && VRAMADDR > 0)
			VRAMADDR = VRAMADDR - 0x200;
		// VDP_DrawScanline's own palette-display overlay reads up to
		// VRAMADDR+0x963 bytes ahead of this cursor (the highest of its four
		// 8-row blocks, offset 0x600, at i=15/y=95, plus its own +3 byte
		// span) -- capping the cursor at 0xF800 still let that window run
		// off the end of the 64KB vdp_vram array. 0xF600 is the highest
		// multiple of the 0x200 step that keeps every block's read in
		// bounds.
		if ((key_state[SDL_SCANCODE_L] || ry > STICK_DEADZONE) && VRAMADDR < 0xF600)
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
		if (rx < -STICK_DEADZONE)
			state |= JPAD_EXT_SUBTYPE_DEC;
		if (rx > STICK_DEADZONE)
			state |= JPAD_EXT_SUBTYPE_INC;
	}
	return state;
}

uint8_t Input_GetState2(void) {
	//No use in Sonic 1
	return 0;
}
