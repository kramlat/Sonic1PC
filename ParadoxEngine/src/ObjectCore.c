#include "EngineObject.h"

#include "Camera.h"
#include "EngineConstants.h"
#include "Video.h"

#include "Backend/VDP.h"

#include <string.h>

// The object slots. The reserved ones (0..RESERVED_OBJECTS-1) hold the player and the things the game keeps alive; the rest are the level's.
Object objects[OBJECTS];
Object* const player = objects;
Object* const level_objects = objects + RESERVED_OBJECTS;

// Which level objects have been loaded and may come back (the object position loader's memory; see Level.c)
uint8_t objstate[0x100];


// The null object: gives its respawn slot back and deletes itself. It is what an object id the game does not have runs as, so any id can be left out
// of (or removed from) the game's table without breaking anything.
void Obj_Null(Object *obj) {
	if (obj->respawn_index)
		objstate[obj->respawn_index] &= 0x7F;
	ObjectDelete(obj);
}

// Runs one object through the game's table
static void RunObject(Object *obj) {
	ObjectFunc func = (obj->type < game_object_count) ? game_objects[obj->type] : NULL;
	(func != NULL ? func : Obj_Null)(obj);
}

//Object functions
// The original zeroes a slot when its object is deleted, so every free slot it hands out
// is clean. Slots here can still hold stale bytes (a leftover anim_frame, routine, frame...
// made objects spawned from them misbehave or read past their animation scripts), so
// hand out zeroed slots.
static void ClearFreeSlot(Object *obj) {
	memset(obj, 0, sizeof(Object));
	obj->mappings = NULL;
}

Object *FindFreeObj(void) {
	Object *obj = level_objects;
	for (int i = 0; i < LEVEL_OBJECTS; i++, obj++)
		if (obj->type == OBJECT_NULL) {
			ClearFreeSlot(obj);
			return obj;
		}
	return NULL; //Original would return the address at the end of object space, I believe
}

Object *FindNextFreeObj(Object *obj) {
	for (; (obj - objects) < OBJECTS; obj++)
		if (obj->type == OBJECT_NULL) {
			ClearFreeSlot(obj);
			return obj;
		}
	return NULL; //Original would return the address at the end of object space, I believe
}

int ExecuteObjects_i;

void ExecuteObjects(void) {
	Object *obj;
	
	if (player->routine < 6) {
		//Run all objects
		obj = objects;
		ExecuteObjects_i = OBJECTS - 1;
		do {
			if (obj->type)
				RunObject(obj);
			obj++;
		} while (ExecuteObjects_i-- > 0);
	} else {
		//Run reserved objects
		obj = objects;
		ExecuteObjects_i = RESERVED_OBJECTS - 1;
		do {
			if (obj->type)
				RunObject(obj);
			obj++;
		} while (ExecuteObjects_i-- > 0);
		
		//Draw level objects
		ExecuteObjects_i = LEVEL_OBJECTS - 1;
		do {
			if (obj->type && obj->render.f.on_screen)
				DisplaySprite(obj);
			obj++;
		} while (ExecuteObjects_i-- > 0);
	}
}

//Object functions
void AnimateSprite(Object *obj, const uint8_t *anim_script) {
	//Check if animation changed
	uint8_t anim = obj->anim;
	if (anim != obj->prev_anim) {
		//Reset animation state
		obj->prev_anim = anim;
		obj->anim_frame = 0;
		obj->frame_time.b = 0;
	}
	
	//Wait for current animation frame to end
	if (--obj->frame_time.b >= 0)
		return;
	
	//Get animation script to use
	anim <<= 1;
	anim_script += (anim_script[anim] << 8) | (anim_script[anim + 1] << 0);
	obj->frame_time.b = anim_script[0];
	
	//Read current animation command
	uint8_t cmd = anim_script[1 + obj->anim_frame];
	
	if (!(cmd & 0x80)) {
		Anim_Next:
		//Set animation frame
		obj->frame = cmd & 0x1F;
		obj->render.f.x_flip = obj->status.o.f.x_flip ^ ((cmd >> 5) & 1);
		obj->render.f.y_flip = obj->status.o.f.y_flip ^ ((cmd >> 6) & 1);
		obj->anim_frame++;
	} else {
		if (++cmd == 0) {
			//Restart animation
			obj->anim_frame = 0;
			cmd = anim_script[1];
			goto Anim_Next;
		}
		if (++cmd == 0) {
			//Go back (next byte) frames
			obj->anim_frame -= anim_script[2 + obj->anim_frame];
			cmd = anim_script[1 + obj->anim_frame];
			goto Anim_Next;
		}
		if (++cmd == 0) {
			//Change animation
			obj->anim = anim_script[2 + obj->anim_frame];
		}
		if (++cmd == 0) {
			//Increment routine
			obj->routine += 2;
		}
		if (++cmd == 0) {
			//Clear secondary routine
			obj->routine_sec = 0;
		}
		if (++cmd == 0) {
			//Increment secondary routine
			obj->routine_sec += 2;
		}
	}
}


void PlaySoundLocal(const Object *obj, uint8_t id) {
	if (obj->render.f.on_screen)
		PlaySound(id);
}

void ObjectDelete(Object *obj) {
	//Clear object memory
	memset(obj, 0, sizeof(Object));
	obj->mappings = NULL; //NULL isn't guaranteed to be 0
}

void SpeedToPos(Object *obj) {
	obj->pos.l.x.v += obj->xsp << 8;
	obj->pos.l.y.v += obj->ysp << 8;
}

void ObjectFall(Object *obj) {
	obj->pos.l.x.v += obj->xsp << 8;
	obj->pos.l.y.v += obj->ysp << 8;
	obj->ysp += 0x38;
}

void RememberState(Object *obj) {
	if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
		//Off-screen
		if (obj->respawn_index)
			objstate[obj->respawn_index] &= 0x7F;
		ObjectDelete(obj);
	} else {
		//On-screen
		DisplaySprite(obj);
	}
}

