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
