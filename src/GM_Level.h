#pragma once

void GM_Level(void);

// Matches PauseGame in the disassembly -- shared with GM_Special.c, which
// pauses the same way during Special Stages.
void PauseGame(void);

// Plays the current level's own zone music (indexed by LEVEL_ZONE(level_id)
// -- see the zone-order table in GM_Level.c). Shared so anything that plays
// temporary music over the top of the level track (invincibility, shield,
// extra life, ...) can hand playback back to the right track once that's
// done, instead of just leaving it stopped.
void ResumeLevelMusic(void);
