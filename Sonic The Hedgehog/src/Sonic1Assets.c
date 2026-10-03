#include <stdint.h>

// The assets ParadoxEngine draws with but does not own: the game provides them (the engine declares them extern).

// The font the debug text and the hex digits are drawn from (Z80 Peek, the console, HUD_WriteHex): the engine's VDP and renderer read it
#include "Resource/Art/Text.h"

// The window icon
#include "Resource/Icon.h"

// What names the game to the engine: its settings file (kept as it always was, so the player's settings carry over) and the file's heading
#include <stddef.h>
#include "GameInterface.h"
static const GameCredit authors[] = {
    {"Mark Toman", "Maintainer: ending, credits, special stage, sound engine, tools"},
    {NULL, NULL},
};
static const GameCredit credits[] = {
    {"CuckyDev", "The Sonic 1 C port this one grew from"},
    {"Clownacy", "Contributions to the port, and the assembler used for the mappings"},
    {"The Sonic 1 disassembly project", "The source of the game's logic and data"},
    {NULL, NULL},
};
const GameInfo game_info = {
    .app_id = "sonic1pc",
    .app_name = "Sonic 1 PC",
    .game_title = "Sonic the Hedgehog",
    .player_name = "Sonic",
    .description = "A native recompilation of Sonic the Hedgehog (1991, Mega Drive/Genesis).",
    .trademark = "Sonic the Hedgehog is a trademark of SEGA.",
    .data_dir = "SonicPC",
    .url = "https://github.com/kramlat/Sonic1PC",
    .bug_url = "https://github.com/kramlat/Sonic1PC/issues",
    .authors = authors,
    .credits = credits,
    .settings_file = "Sonic1Settings.cfg",
    .settings_title = "Sonic 1 PC settings",
};
