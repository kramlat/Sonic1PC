#include "EngineObject.h"
#include "EditorLive.h"
#include "GameInterface.h"

#include "Sst.h"

void ExecuteObjects(void) {
	Editor_Hold(); // (the level editor's cursor is where the first player is: see EditorLive.h)

	Sst_ProcessPlayers(!(editor_present && !editor_playtest && Game_EditorSkipObject(player))); // (the characters run first, as they always did)

	//The objects of the status table (Sst.h)
	Sst_Process(player->routine >= 6);
}
