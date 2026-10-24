#pragma once

// The level editor's terrain tools (tabs of its window): the palette, the 8x8 tiles, the 16x16 blocks, the 128x128 chunks and the solids (the collision curve of each block), changed in place while the
// level runs (EditorLive.h: Editor_Art, Editor_Blocks, Editor_Chunks, Editor_Solids, Editor_SetColour) and saved as the files the game keeps in res/.

#include <QString>

class QTabWidget;

namespace TerrainEditor {

void AddTabs(QTabWidget *tabs); // adds the terrain tools' tabs to the level editor's window
void Refresh();                 // shows the level's data again (it was loaded, or changed elsewhere)
bool Dirty();                   // something has been changed and not saved
void Reset();                   // a level (or none) is being edited anew: nothing is changed
bool Save(const QString &slot_folder, int zone, int act, QString *message); // writes the terrain files of the zone to <slot_folder>/<kind folder>/<zone>, and the layout of the act

} // namespace TerrainEditor
