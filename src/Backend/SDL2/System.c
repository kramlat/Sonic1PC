#include "SDL.h"

#include "../MegaDrive.h"
#include "Audio.h"

#include <stdio.h>

//System interface
int System_Init(const MD_Header *header) {
	(void)header;
	
	//Initialize SDL2 (GAMECONTROLLER pulls in JOYSTICK too -- needed for
	//gamepad support, including Steam Input's virtual controller)
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) < 0) {
		printf("System_Init: %s\n", SDL_GetError());
		return -1;
	}

	Audio_Init();

	return 0;
}

void System_Quit(void) {
	Audio_Quit();

	//Quit SDL2
	SDL_Quit();
}

// Debug console only (Console.c) -- see Backend/Joypad.h's own comment on
// why these small SDL-specific hooks are forward-declared at their call
// site rather than exposed through a header, matching MegaDrive.c's own
// System_Init/System_Quit convention.
void System_SetClipboardText(const char *text) {
	SDL_SetClipboardText(text);
}
