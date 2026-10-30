#pragma once

#include "Types.h"

// Sonic 2's background planes, kept by the engine's band layers (LevelScroll.c): the first view's and, in a split screen, the second's.
void LevelScroll_BackgroundReset(void); // a level's layout was loaded: which layer the zone has
void LevelScroll_BackgroundSync(int view, const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y); // the plane was drawn whole at these positions
void LevelScroll_BackgroundDraw(int view, const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y); // brings the plane up to them
