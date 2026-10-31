#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "Types.h"
#include "TileBank.h"
#include "Sprites.h"
#include "Viewport.h"

// The status table of Sonic 3 & Knuckles (sonic3k.constants.asm: object_size $4A, the fields at $0-$4A) as the engine's object, with the handler that runs it (sonic3k.asm: Process_Sprites, Draw_Sprite, Delete_Current_Sprite,
// MoveSprite, Animate_Sprite). It is the engine's only object table.
//
// What S3&K calls a field keeps its name and its meaning here, in the same order; only the code pointer is a C function pointer (so the table is not $4A bytes, and nothing here depends on a byte offset). An
// object whose `code` is NULL is a free slot. The 68000 keeps a position as a word with a sub-pixel word after it (or a long when it needs the precision): a dword_s (Types.h) is both.

typedef struct Sst Sst;
typedef void (*SstCode)(Sst *self);

// render_flags (SCHG): bits 0 and 1 are the flips (the animation scripts copy them from `status`), bit 2 says the object is placed against the camera (clear: its position is a screen position), bit 5 that
// `mappings` is one lone frame, bit 6 that the object draws itself (multi-draw: its code builds the sprites), bit 7 that it was on screen at the last render
#define SST_RENDER_X_FLIP     0x01
#define SST_RENDER_Y_FLIP     0x02
#define SST_RENDER_LEVEL      0x04
#define SST_RENDER_STATIC     0x20
#define SST_RENDER_MULTI_DRAW 0x40
#define SST_RENDER_ON_SCREEN  0x80

// status: bit 0 x flip, bit 1 y flip (the others are the object's own)
#define SST_STATUS_X_FLIP 0x01
#define SST_STATUS_Y_FLIP 0x02
// sets or clears render flag bits to match a condition
#define SST_RENDER_SET(obj, bit, on) ((on) ? ((obj)->render_flags |= (uint8_t)(bit)) : ((obj)->render_flags &= (uint8_t)~(bit)))
// sets or clears status bits to match a condition
#define SST_STATUS_SET(obj, bit, on) ((on) ? ((obj)->status |= (uint8_t)(bit)) : ((obj)->status &= (uint8_t)~(bit)))

#define SST_CHILDREN 16
typedef struct {
    int16_t x, y;
    uint8_t frame;
} SstChild;

struct Sst {
    SstCode code;               // the object's routine: called once a frame; NULL is a free slot
    uint8_t render_flags;
    uint8_t routine;
    uint8_t height_pixels;      // half the height it draws (culling)
    uint8_t width_pixels;       // half the width it draws (culling)
    uint16_t priority;          // in units of $80: the draw queue it goes in is priority >> 7 (0 in front)
    uint16_t art_tile;          // PCCVH AAAAAAAAAAA: priority, palette line, flips, first cell of its art
    const uint8_t *mappings;
    const tilebank_t *bank;     // the tile bank its art is in (NULL: the main bank); with the palette group (this port's: the art_tile's palette line plus 4 times it) the cells a sprite names
    uint8_t palette_group;
    dword_s x_pos, y_pos;       // .f.u the pixel, .f.l the sub-pixel
    int16_t x_vel, y_vel;
    int16_t ground_vel;
    uint8_t y_radius, x_radius; // collision height / 2, width / 2
    uint8_t anim, prev_anim;    // when they differ the animation restarts
    uint8_t mapping_frame;
    uint8_t anim_frame;
    int8_t anim_frame_timer;     // (a signed byte: the objects count it down past zero)
    uint8_t angle;
    uint8_t collision_flags;    // TT SSSSSS: collision type, size
    uint8_t collision_property;
    uint8_t status, status_secondary, status_tertiary;
    uint8_t subtype;
    uint8_t routine_secondary;
    uint16_t parent;            // the slot of the object that owns or spawned this one (index + 1; 0: none)
    uint16_t respawn;           // the entry of this object's place in the respawn table (index + 1; 0: none)
    uint8_t child_count;        // multi-draw (render_flags bit 6): the main sprite (mapping_frame, 0: none) and these children, placed in the level (or on the screen) and each a frame of the same mappings
    SstChild children[SST_CHILDREN];
    union {                     // the rest of the table: each object's own (objoff_*)
        uint8_t u8[0x20];
        int8_t s8[0x20];
        uint16_t u16[0x10];
        int16_t s16[0x10];
        uint32_t u32[0x8];
        int32_t s32[0x8];
    } own;
};

// The 110 slots of Object_RAM: Player_1, Player_2, the reserved one (a level's collision list cleaner), 90 dynamic ones and 17 fixed ones of the level
#define SST_PLAYERS  2
#define SST_RESERVED 3
#define SST_DYNAMIC  90
#define SST_LEVEL    17
#define SST_COUNT    (SST_RESERVED + SST_DYNAMIC + SST_LEVEL)
#define SST_FIRST_DYNAMIC SST_RESERVED
#define SST_FIRST_LEVEL   (SST_RESERVED + SST_DYNAMIC)

// The game's table of the routine for each object id (Sprite_Listing): a game defines it (an id with NULL has not moved to the status table). The loader (SstLoad.h) and Sst_Spawn read it, and the characters'
// routines are in it too
extern const SstCode sst_object_codes[0x100];

extern Sst sst[SST_COUNT];
#define SST_PLAYER_1 (&sst[0])
#define SST_PLAYER_2 (&sst[1])
// The first character (Player_1: Sonic, in the special stage and the ending the games' own versions of him)
extern Sst *const player;

// A character's status bits (S3&K's `status` for Player_1 and Player_2): the facing, in the air, in a ball (rolling), on an object, jumped from a roll, pushing, under water, and made to roll by a tunnel
#define SST_PSTATUS_X_FLIP       0x01
#define SST_PSTATUS_IN_AIR       0x02
#define SST_PSTATUS_IN_BALL      0x04
#define SST_PSTATUS_OBJECT_STAND 0x08
#define SST_PSTATUS_ROLL_JUMP    0x10
#define SST_PSTATUS_PUSHING      0x20
#define SST_PSTATUS_UNDERWATER   0x40
#define SST_PSTATUS_MUST_ROLL    0x80

// Process_Sprites: runs every slot's code in order. With `freeze_level` (the first player's routine is 6 or more and it is not a competition: it has died) the players and the dynamic objects still run but the level's
// fixed objects only draw themselves (those that are on screen)
void Sst_Process(bool freeze_level);
// ... the characters' two slots first (they run first, ahead of everything else), and then the rest (Sst_Process after it skips them)
void Sst_ProcessPlayers(bool run);

// Draw_Sprite: queues the object in its priority's draw queue (8 queues of 63 objects; a full one passes the object on to the next, and the last one drops it)
void Sst_Draw(Sst *obj);
#define SST_QUEUES 8
#define SST_QUEUE_SIZE 63
void Sst_ClearQueues(void);
int Sst_QueueCount(int queue);
Sst *Sst_QueueEntry(int queue, int index);

// Delete_Current_Sprite: the slot becomes free and clear
void Sst_Delete(Sst *obj);

// AllocateObject (Create_New_Sprite): a free slot, from the dynamic ones; NULL if there is none. As the original does it, the search begins at the dynamic slot after the first and runs one slot past the last (the
// first of the level's fixed slots); AllocateObjectAfterCurrent (Create_New_Sprite3) looks only after `after`, within the dynamic ones
Sst *Sst_Allocate(void);
Sst *Sst_AllocateAfter(const Sst *after);

// Clears every slot but the characters' and the queues (a level starts: the characters are made before the level's objects), and every slot
void Sst_Reset(void);
void Sst_ResetAll(void);

// MoveSprite and friends: the velocity added to the position (in 1/256 pixels); MoveSprite also adds gravity ($38) to the vertical speed after using it; the Test versions move the other way vertically when gravity
// is reversed
void Sst_MoveSprite(Sst *obj);
void Sst_MoveSprite2(Sst *obj);
extern uint8_t sst_reverse_gravity;
void Sst_MoveSpriteTestGravity(Sst *obj);
void Sst_MoveSpriteTestGravity2(Sst *obj);

// Animate_Sprite: `table` is the animation scripts (a big-endian word offset for each animation, from the table's start); a script is a delay, the frames, and an end code: $FF back to the first frame, $FE back
// by the byte after it, $FD another animation (the byte after it), $FC the routine counter on by 2, $FB off to the side (x $7F00). Animate_SpriteIrregularDelay's scripts are pairs of a frame and its own delay, with
// the same codes (each its own pair)
void Sst_Animate(Sst *obj, const uint8_t *table);
void Sst_AnimateIrregular(Sst *obj, const uint8_t *table);

// Render_Sprites for one draw queue of the Sonic 3 objects, into the sprite table (Sprites.c: BuildSprites calls it for each queue of each view, and empties the queues; `clear_on_screen` is for the first view: the on-screen flag then says whether the object was in either)
void SstRenderQueue(int queue, sprite_t **sprite, uint8_t *sprite_i, const SpriteView *view, int16_t top, int view_w, bool clear_on_screen);
