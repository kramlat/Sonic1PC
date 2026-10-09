#ifndef _SPLITSCREEN_H
#define _SPLITSCREEN_H

#include <stdbool.h>
#include <stdint.h>

// Sonic 2's Two_player_mode: set by the level select when B is the button that picks the level (the title screen's plain Start clears it). When it is on, the level loader sets up the split screen.
extern uint8_t two_player_mode;

// The split screen is two ordinary views of the level, not the real hardware's double-height picture: the first follows Sonic, the second Tails (the second player, on pad 2). On a wide picture
// (8:5 or wider) they sit side by side, each half the picture's width; on any other they are stacked, each a whole picture squashed into half the height (the engine's VDP does the squashing).

// The level loader's hook for the split screen (Game_LevelObjects calls it once Sonic and Tails exist, at the start of every level, whatever the mode): in a level started in two-player mode it sets the
// views, the second camera, the second foreground plane and the sprite tables up, in any other it makes sure all of that is off. (Water levels are played split too, though the sea's palette line follows the first view only.)
void SplitScreen_LoadLevel(void);

// Is the level a split screen now?
bool SplitScreen_Active(void);

// How far from the view's left edge the first camera keeps Sonic (144 in the original's whole picture)
int16_t SplitScreen_FollowX(void);

// The end sign's lock: the cameras that see an object at x (half_width wide each way) stop being held back from the right edge of the level (limit_left2 = limit_right2 for the first, its own for the second;
// with no split screen, the one camera). The second camera's left limit is otherwise the level's own.
void SplitScreen_LockCameras(int16_t x, int16_t half_width);

// Once a frame, with the first camera moved: moves the second camera after Tails and fills the second view's scroll
void SplitScreen_Scroll(void);

// At the vertical blank: draws the second plane's new rows and columns and copies its scroll
void SplitScreen_VBlank(void);

#endif //_SPLITSCREEN_H
