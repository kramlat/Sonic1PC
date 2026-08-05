#pragma once

// Passive SDL audio device: opened with no callback, fed once per frame via
// SDL_QueueAudio from Audio_Update() instead of a background thread pulling
// samples on its own schedule -- matching the game's own frame-driven
// (60Hz VBlank) architecture rather than fighting it with a separate audio
// thread's timing.
void Audio_Init(void);
void Audio_Update(void); // Call once per frame -- generates and queues that frame's audio
void Audio_Quit(void);
