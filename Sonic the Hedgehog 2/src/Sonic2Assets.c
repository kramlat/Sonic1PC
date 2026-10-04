#include <stddef.h>
#include <stdint.h>

// Sonic 2's identity, and the assets ParadoxEngine draws with but does not own. The font (the level select's and the debug text's) is the Simon Wai prototype's; the icon is
// still Sonic 1's (reused with the rest of the in-between game) until Sonic 2 gets its own.
#define S2Art_Text Art_Text
#include "Resource/S2Art/Text.h"
#include "Resource/Icon.h"

#include "GameInterface.h"

static const GameCredit authors[] = {
    {"Mark Toman", "Maintainer"},
    {NULL, NULL},
};
static const GameCredit credits[] = {
    {"CuckyDev", "The Sonic 1 C port this one grew from"},
    {"Clownacy", "Contributions to the port, and the assembler used for the mappings"},
    {"The Sonic 1 and Sonic 2 disassembly projects", "The source of the games' logic and data"},
    {NULL, NULL},
};

const GameInfo game_info = {
    .app_id = "sonic2pc",
    .app_name = "Sonic 2 PC",
    .game_title = "Sonic the Hedgehog 2",
    .player_name = "Sonic",
    .description = "A native recompilation of Sonic the Hedgehog 2 (1992, Mega Drive/Genesis), built the way its makers built it: out of Sonic 1.",
    .trademark = "Sonic the Hedgehog is a trademark of SEGA.",
    .data_dir = "SonicPC",
    .url = "https://github.com/kramlat/Sonic1PC",
    .bug_url = "https://github.com/kramlat/Sonic1PC/issues",
    .authors = authors,
    .credits = credits,
    .settings_file = "Sonic2Settings.cfg",
    .settings_title = "Sonic 2 PC settings",
    .no_title_card = true,
    .split_screen = true,
	.plc_capacity = 32, // (the level starts without a title card, so its queue is not drained as it is filled) // the prototypes have no title cards
};

// The zones and acts of the demo recorder's pickers: the zone slots that are built (ZoneIds.h), under their final names where the prototype's differ (Emerald Hill is the prototype's Green Hill), with the number of
// acts each has in the prototype's files (Metropolis' third act is a zone slot of its own, and Hidden Palace has one act)
static const char *const s2_act_mtz3[] = { "Act 3" };
static const GameZone s2_zones[] = {
    { 0x00, "Emerald Hill", 2, NULL },
    { 0x02, "Wood", 2, NULL },
    { 0x04, "Metropolis", 2, NULL },
    { 0x05, "Metropolis (third act)", 1, s2_act_mtz3 },
    { 0x07, "Hill Top", 2, NULL },
    { 0x08, "Hidden Palace", 1, NULL },
    { 0x0A, "Oil Ocean", 2, NULL },
    { 0x0B, "Dust Hill", 2, NULL },
    { 0x0C, "Casino Night", 2, NULL },
    { 0x0D, "Chemical Plant", 2, NULL },
    { 0x0F, "Neo Green Hill", 2, NULL },
};
static const GameZoneList s2_zone_list = { (int)(sizeof(s2_zones) / sizeof(s2_zones[0])), s2_zones };
const GameZoneList *game_zone_list = &s2_zone_list;
