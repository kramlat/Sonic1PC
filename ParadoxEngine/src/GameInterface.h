#pragma once

// What ParadoxEngine needs from the game it runs. The engine is a library with no game in it: each game (Sonic1Core, one day Sonic2Core)
// defines these, and the engine only declares them. The executable links one game library with the engine, so a symbol missing here is a link
// error, not a silent default -- except for the debug services (Peek_*, Console_*, Demo_*, the cli_* settings, frame_count), which have weak
// null versions in NullServices.c so a game that has none of that yet still runs; define them strongly to override.
//
//   Data the engine reads
//     palette_pointers, plcs    the palettes and pattern load cues the game loads by id          (EnginePalette.h, EnginePLC.h)
//     game_sound_bank           the sound id space: which ids are music, effects, commands; what plays for each (EngineSound.h)
//     Art_Text, res_Icon        the font the debug text and hex digits are drawn from, and the window icon (declared where used)
//   State and services behind the debug tools (the Qt window's menus and viewers; these headers have no game types in them)
//     DebugPeek.h               palettes and object slots, and the watchable variables
//     Console.h                 the debug console
//     Demo.h                    demo recording and playback
//   Identity                  game_info: the settings file name and its heading (GameInterface.h)
//   Objects                   game_objects, game_object_count: what runs for each object id (EngineObject.h); a missing id runs as the null object
//   Terrain                   Collision_Angle, Collision_HeightMap, Collision_WidthMap: the collision maps (LevelData.h); the level arrays and collision code are the engine's
//   Screens                   game_screens, game_screen_count, game_boot_screen: the game's screens by game mode (Screen.h)
//   Run-time state
//     frame_count               the game's own frame counter, which the debug log stamps its lines with
//     cli_start_level, cli_resolution   what the command line asked for (a level to start in, the picture size)

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// What identifies the game to the engine's window and files. The game defines one `game_info`.
typedef struct {
    const char *name;
    const char *task;
} GameCredit;

typedef struct {
    const char *app_id;         // the game's short machine name ("sonic1pc"): its desktop file, its KDE handbook topic, its install folders
    const char *app_name;       // what the windows call the game ("Sonic 1 PC")
    const char *game_title;     // the game's own title ("Sonic the Hedgehog")
    const char *player_name;    // the character the player controls ("Sonic"), for the labels that name him
    const char *description;    // the About box's one-line description
    const char *trademark;      // the About box's trademark line
    const char *data_dir;       // the folder name in the user's data directory that holds the settings file ("SonicPC")
    const char *url;            // the project's home page, or NULL
    const char *bug_url;        // where to report bugs, or NULL
    const GameCredit *authors;  // the About box's authors, a list ended by a { NULL, NULL } entry
    const GameCredit *credits;  // and its credits, likewise
    const char *settings_file;  // the settings file's name in the user's data directory ("Sonic1Settings.cfg"): the game's own, so games do not share one
    const char *settings_title; // the comment on the settings file's first line ("Sonic 1 PC settings")
    bool no_title_card;         // a level starts without its title card (the prototypes had none)
    bool split_screen;          // the game has a two-player split screen (Sonic 2 and later): the demo recorder offers it
    unsigned plc_capacity;      // how many art loads can wait in the queue (0: the original's 16). A game that starts its levels without a title card to drain the queue needs more
} GameInfo;
extern const GameInfo game_info;

// The zones and acts a game has, for the demo recorder's pickers (Tools > Record Demo / Play Demo). `id` is the zone as the game's level ids number it (the number the recorder and the demo file names carry),
// `acts` how many acts it has (1-4) and `act_names` their names (a list of that many, or NULL for "Act 1", "Act 2", ...). Each game has its own levels, so each defines its own list; a game that does not gets a single zone of three acts.
typedef struct {
    int id;
    const char *name;
    int acts;
    const char *const *act_names;
} GameZone;

typedef struct {
    int count;
    const GameZone *zones;
} GameZoneList;
extern const GameZoneList *game_zone_list;

// The objects a game makes when a level starts, beyond the player, the HUD and the splash the level start makes itself (Sonic 2: Tails). A weak null where the game has none.
void Game_LevelObjects(void);

// Which of the two collision paths (the primary or the secondary 16x16 collision index, and the solid bits that go with it) an object collides with: 0 or 1. A weak default gives them all the one global path
// `collision_path` (what Sonic 1's path swapper sets); Sonic 2 gives Sonic and Tails each their own and everything else the primary.
uint8_t Game_CollisionPath(const void *obj); // (an Object *)

extern uint16_t frame_count;
extern int32_t cli_start_level;
extern int32_t cli_resolution;

#ifdef __cplusplus
}
#endif
