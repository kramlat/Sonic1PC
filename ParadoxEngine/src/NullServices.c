// The null services. The debug tools (the Qt window's palette/object/variable viewers, the console, demo recording) call into the game through
// DebugPeek.h, Console.h and Demo.h; a game that has none of that yet still links and runs, because each of those is defined here as a WEAK null:
// no objects, no variables, no console, no demos. A game that has the real thing defines it strongly and the executable's definition wins
// (a shared library's symbols are interposed by the executable's). What stays a link error when missing is the game's data and identity
// (game_info, the screen/object/sound/palette/PLC tables, the collision maps, Art_Text, res_Icon): those say what the game *is*.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Console.h"
#include "Demo.h"
#include "DebugPeek.h"
#include "GameInterface.h"
#include "LevelData.h"
#include "Oscillator.h"

#define WEAK __attribute__((weak))

WEAK uint16_t frame_count;
WEAK int32_t cli_start_level = -1;
WEAK int32_t cli_resolution = -1;
WEAK const GameZoneList *game_zone_list = NULL; // (the demo recorder's zone and act pickers get one zone of three acts)

WEAK const OscillatorData oscillator_data = { 0 }; // (the oscillators' table: none stand still at zero, all with no rate of change and an amplitude of 0; a game defines its own)

WEAK void Game_LevelObjects(void) {}

WEAK uint8_t Game_CollisionPath(const void *obj) { (void)obj; return collision_path; }

WEAK uint16_t Peek_GamePalette(int which, int pal, int index) { (void)which; (void)pal; (void)index; return 0; }
WEAK int Peek_DebugToolsAvailable(void) { return 0; }
WEAK int Peek_ObjectCount(void) { return 0; }
WEAK void Peek_GetObject(int slot, ObjectPeek *out) { (void)slot; (void)out; }
WEAK const uint8_t *Peek_ObjectBytes(int slot, int *size) { (void)slot; if (size) *size = 0; return NULL; }
WEAK int Peek_VarCount(void) { return 0; }
WEAK void Peek_GetVar(int i, VarPeek *out) { (void)i; (void)out; }
WEAK int64_t Peek_VarRead(int i, int element) { (void)i; (void)element; return 0; }
WEAK void Peek_VarWrite(int i, int element, int64_t value) { (void)i; (void)element; (void)value; }

WEAK void Console_SetToolPause(bool on) { (void)on; }
WEAK void Console_Toggle(void) {}
WEAK void Console_HandleKey(ConsoleKey key) { (void)key; }
WEAK void Console_SetInput(const char *text) { (void)text; }
WEAK uint32_t Console_LineCount(void) { return 0; }
WEAK const char *Console_GetLogLine(int index_from_bottom) { (void)index_from_bottom; return ""; }
WEAK const char *Console_GetInputLine(void) { return ""; }

WEAK bool Countdown_Available(void) { return false; }
WEAK void Countdown_Request(int music_id, int seconds) { (void)music_id; (void)seconds; }
WEAK void Demo_RequestRecording(const DemoRecordRequest *request) { (void)request; }
WEAK bool Demo_RequestPlayback(const DemoPlayRequest *request, const uint8_t *data, size_t length) { (void)request; (void)data; (void)length; return false; }
WEAK bool Demo_PlaybackActive(void) { return false; }
WEAK bool Demo_RecordingActive(void) { return false; }
WEAK int Demo_RecordedFrames(void) { return 0; }
WEAK void Demo_StopRecording(void) {}
WEAK const char *Demo_LastSavedPath(void) { return ""; }
WEAK int Demo_LastSavedFrames(void) { return 0; }
