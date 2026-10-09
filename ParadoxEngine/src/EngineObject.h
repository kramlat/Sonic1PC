#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "Types.h"
#include "EngineSound.h" // PlaySoundLocal
#include "Mappings.h" // the sprite mappings format (every game on the engine uses it)
#include "Sprites.h" // the sprite pipeline's configuration (sprite_view)
#include "Backend/VDP.h"
 // Sonic 1's (it brings Constants.h): the engine's VDP.h only has the engine ones

//Object constants
#define RESERVED_OBJECTS 0x20
#define LEVEL_OBJECTS    0x60
#define OBJECTS          (RESERVED_OBJECTS + LEVEL_OBJECTS)

#define OBJECT_NULL 0 //The id of a free slot (and of the null object)

//Object types
#pragma pack(push)
#pragma pack(1)

typedef union {
	struct {
		unsigned int x_flip : 1;       //Horizontally flipped
		unsigned int y_flip : 1;       //Vertically flipped
		unsigned int level_fg : 1;     //Moves with the level foreground (positioned against the camera)
		unsigned int level_bg : 1;     //Moves with the level background (a leftover from Sonic 1; with level_fg set too, the third scroll layer)
		unsigned int explicit_height : 1;  //Draw culling uses y_rad as the height instead of assuming 32
		unsigned int static_mappings : 1; //`mappings` points to a lone sprite piece instead of a table of frames
		unsigned int multi_sprite : 1;  //The object has child sprites (children, child_count) drawn with it
		unsigned int on_screen : 1;    //Set if the object's on-screen (see BuildSprites)
	} f;
	uint8_t b;
} ObjectRender;

//The original's sprite_cam_field (render flag bit 2): the object is positioned against the level's foreground camera. Inits that `ori.b #sprite_cam_field,obRender(a0)` OR it in (`render.b |= SPRITE_CAM_FIELD`: the flips the
//layout gave the object stay); those that `move.b #sprite_cam_field,obRender(a0)` assign it (`render.b = SPRITE_CAM_FIELD`: they are cleared)
#define SPRITE_CAM_FIELD 0x04

typedef union {
	struct {
		unsigned int x_flip : 1;       //Horizontally flipped
		unsigned int y_flip : 1;       //Vertially flipped
		unsigned int flag2 : 1;        //Unused
		unsigned int player_stand : 1; //Player is standing on us
		unsigned int flag4 : 1;        //Unused
		unsigned int player_push : 1;  //Player is pushing us
		unsigned int flag6 : 1;        //Unused
		unsigned int flag7 : 1;        //Object-specific
	} f;
	uint8_t b;
} ObjectStatus;

typedef union {
	struct {
		unsigned int x_flip : 1;       //Horizontally flipped
		unsigned int in_air : 1;       //In mid-air
		unsigned int in_ball : 1;      //In ball-form
		unsigned int object_stand : 1; //Standing on an object
		unsigned int roll_jump : 1;    //Set when jumping from a roll
		unsigned int pushing : 1;      //Set if we're pushing
		unsigned int underwater : 1;   //Set if we're underwater
		unsigned int must_roll : 1;    //GHZTunnel-forced "pinball mode" -- can't jump out of or stop a roll while set
	} f;
	uint8_t b;
} PlayerStatus;

#pragma pack(pop)

//A child sprite of a multi_sprite object (Sonic 2's sub-sprites): where it is in the level, and which frame of the object's mappings it shows
#define OBJECT_CHILDREN 8
typedef struct {
	int16_t x;
	int16_t y;
	uint8_t frame;
} ObjectChild;

typedef struct {
	uint8_t type;            //Object type
	ObjectRender render;     //Object render
	uint16_t tile;           //Object base tile
	const void *mappings;    //Object mappings
	union {
		struct {
			dword_s x, y;
		} l; //Level
		struct {
			int16_t x; //X position
			int16_t y; //Y position
			uint16_t yl; //Y position (lower word for long accesses)
		} s; //Screen (VDP coordinates)
	} pos;                //Position
	int16_t xsp;          //Horizontal speed
	int16_t ysp;          //Vertical speed
	int16_t inertia;      //Speed rotated by angle
	int8_t x_rad, y_rad;  //Object radius
	uint8_t priority;     //Sprite priority (0-7, 0 drawn in front of 7)
	uint8_t width_pixels; //Culling and platform width of sprite
	uint8_t frame;        //Mapping frame
	uint8_t anim_frame;   //Frame index in animation
	uint8_t anim;         //Animation
	uint8_t prev_anim;    //Previous animation
	union {
		int8_t b;
		int16_t w;
	} frame_time;         //Frame duration remaining
	uint8_t col_type;     //Collision type
	uint8_t col_property; //Collision property (object-specific)
	union {
		ObjectStatus o;   //Object status
		PlayerStatus p;   //Player status
		uint8_t b;
	} status;
	uint8_t respawn_index; //Respawn index reference number
	uint8_t routine;       //Routine
	uint8_t routine_sec;   //Secondary routine
	uint8_t angle;         //Angle
	union {
		uint8_t  u8[0x18];
		int8_t   s8[0x18];
		uint16_t u16[0xC];
		int16_t  s16[0xC];
		uint32_t u32[0x6];
		int32_t  s32[0x6];
	} scratch;             //Scratch memory
	ObjectChild children[OBJECT_CHILDREN]; //Child sprites, drawn with the object if render.f.multi_sprite is set: `frame` is the main sprite's frame (0: none),
	uint8_t child_count;                   //`width_pixels` its width, `y_rad` its height (with render.f.explicit_height); the first child_count entries are drawn
} Object;

//Object globals
extern Object objects[OBJECTS];
extern Object *const player;       //Slot 0
extern Object *const level_objects; //The first level slot
extern uint8_t objstate[0x100];
extern int ExecuteObjects_i;

//The game's object table: what runs for each object id. Ids past the end, and NULL entries, run as the null object (Obj_Null), which deletes itself.
typedef void (*ObjectFunc)(Object *obj);
extern const ObjectFunc game_objects[];
extern const int game_object_count;
void Obj_Null(Object *obj);

//Object functions
Object *FindFreeObj(void);
Object *FindNextFreeObj(Object *obj);
void ExecuteObjects(void);

void BuildSpr_Normal(uint16_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces);
void BuildSprites(uint8_t *sprite_io);

void AnimateSprite(Object *obj, const uint8_t *anim_script);
void DisplaySprite(Object *obj);

void ObjectDelete(Object *obj);

// In the split screen an object's base tile is halved (double-height cells): its init calls this (Sonic 2's Adjust2PArtPointer)
void Object_Adjust2PArtPointer(Object *obj);

// Plays a sound effect only if the object is on screen (Sonic 2's PlaySoundLocal): something happening off screen makes no noise. With a split screen it is the
// view the sprites were last built for that counts (see BuildSprites).
void PlaySoundLocal(const Object *obj, uint8_t id);
void RememberState(Object *obj);
void SpeedToPos(Object *obj);
void ObjectFall(Object *obj);
