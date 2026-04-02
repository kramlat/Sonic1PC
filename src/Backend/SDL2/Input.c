#include "SDL.h"
#include <stdbool.h>
#include "../../Game.h"

#include "Backend/Joypad.h"

//Backend input interface
int Input_HandleEvents()
{
	SDL_Event e;
	while (SDL_PollEvent(&e))
	{
		switch (e.type)
		{
			case SDL_QUIT:
				return 1;
			default:
				break;
		}
	}
	return 0;
}


uint8_t Input_GetState1()
{

	//Get keyboard state
	const uint8_t *key_state = SDL_GetKeyboardState(NULL);
	uint8_t start = key_state[SDL_SCANCODE_RETURN] ? JPAD_START : 0;
	uint8_t a     = key_state[SDL_SCANCODE_A]      ? JPAD_A     : 0;
	uint8_t b     = key_state[SDL_SCANCODE_S]      ? JPAD_B     : 0;
	uint8_t c     = key_state[SDL_SCANCODE_D]      ? JPAD_C     : 0;
	uint8_t right = key_state[SDL_SCANCODE_RIGHT]  ? JPAD_RIGHT : 0;
	uint8_t left  = key_state[SDL_SCANCODE_LEFT]   ? JPAD_LEFT  : 0;
	uint8_t down  = key_state[SDL_SCANCODE_DOWN]   ? JPAD_DOWN  : 0;
	uint8_t up    = key_state[SDL_SCANCODE_UP]     ? JPAD_UP    : 0;
	
        if(key_state[SDL_SCANCODE_TAB])
            VDP_PALETTE_DISPLAY = !VDP_PALETTE_DISPLAY;
        if(key_state[SDL_SCANCODE_1] & VDP_PALETTE_DISPLAY)
            CRAMPAL = 0;
        if(key_state[SDL_SCANCODE_2] & VDP_PALETTE_DISPLAY)
			CRAMPAL = 1;
        if(key_state[SDL_SCANCODE_3] & VDP_PALETTE_DISPLAY)
            CRAMPAL = 2;
        if(key_state[SDL_SCANCODE_4] & VDP_PALETTE_DISPLAY)
            CRAMPAL = 3;
        if(key_state[SDL_SCANCODE_O] & VDP_PALETTE_DISPLAY)
			if(VRAMADDR > 0)
				VRAMADDR = VRAMADDR - 0x200;
        if(key_state[SDL_SCANCODE_L] & VDP_PALETTE_DISPLAY)
			if (VRAMADDR < 0xF800)
				VRAMADDR = VRAMADDR + 0x200;

	//Return as bitfield
	return start | a | c | b | right | left | down | up;
}

uint8_t Input_GetState2()
{
	//No use in Sonic 1
	return 0;
}
