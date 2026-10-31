#pragma once

#include <stdint.h>
#include "TileBank.h"
#include "Viewport.h"
#include <stdbool.h>

#include "Types.h"
#include "EngineSound.h" // PlaySound
#include "Mappings.h" // the sprite mappings format (every game on the engine uses it)
#include "Sprites.h" // the sprite pipeline's configuration (sprite_view)
#include "Backend/VDP.h"
#include "Sst.h" // the objects: the status table of Sonic 3 & Knuckles, the draw queues and the handler that runs them

// The original's sprite_cam_field (render flag bit 2): the object is positioned against the level's foreground camera. Inits that `ori.b #sprite_cam_field,obRender(a0)` OR it in (`render_flags |= SPRITE_CAM_FIELD`: the flips
// the layout gave the object stay); those that `move.b #sprite_cam_field,obRender(a0)` assign it (`render_flags = SPRITE_CAM_FIELD`: they are cleared)
#define SPRITE_CAM_FIELD SST_RENDER_LEVEL

// Runs the frame's objects (Sst_ProcessPlayers, Sst_Process: the characters first, then the rest; Sst.h) after the level editor's cursor has been kept up to date
void ExecuteObjects(void);

void BuildSpr_Normal(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces);
// the same for mappings in the format of Sonic 3 & Knuckles (6-byte pieces)
void BuildSpr_NormalS3(sprite_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces);
// Turns the frame's draw queues (Sst_Draw) into the VDP's sprite table(s)
void BuildSprites(uint8_t *sprite_io);
