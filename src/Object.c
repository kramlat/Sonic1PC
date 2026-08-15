#include "Object.h"

#include "Video.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"

#include "Object/Sonic.h"
#include "Object/Splash.h"
#include "Object/Waterfall.h"
#include "Object/PathSwapper.h"
#include "Object/AirBubbles.h"
#include "Object/DrownCount.h"
#include "Object/WaterSurface.h"
#include "Object/InvisibleBarrier.h"

#include "Game.h"
#include "PLC.h"
#include "Sound.h"

#include "Macros.h"

#include <string.h>

//Object draw queue
struct SpriteQueue {
	uint32_t size;
	Object *obj[0x3F];
} sprite_queue[8];

//Object indices
//#ifndef SCP_FIX_BUGS
//	#define Obj_Null ObjectFall //Thats right, all null objects point to ObjectFall
//#else
	void Obj_Null(Object *obj) {
		if (obj->respawn_index)
			objstate[obj->respawn_index] &= 0x7F;
		ObjectDelete(obj);
	}
//#endif
//Don't re-enable this until all objects are implemented
//...Trust me

void Obj_Sonic(Object *obj);
void Obj_SpecialSonic(Object *obj);
void Obj_Signpost(Object *obj);
void Obj_TitleSonic(Object *obj);
void Obj_PSB(Object *obj);
void Obj_GHZBridge(Object *obj);
void Obj_Crabmeat(Object *obj);
void Obj_HUD(Object *obj);
void Obj_BuzzBomber(Object *obj);
void Obj_BuzzMissile(Object *obj);
void Obj_BuzzExplode(Object *obj);
void Obj_Ring(Object *obj);
void Obj_Monitor(Object *obj);
void Obj_Checkpoint(Object *obj);
void Obj_Explosion(Object *obj);
void Obj_Chopper(Object *obj);
void Obj_MonitorItem(Object *obj);
void Obj_TitleCard(Object *obj);
void Obj_GotThroughCard(Object *obj);
void Obj_Animals(Object *obj);
void Obj_Points(Object *obj);
void Obj_Spikes(Object *obj);
void Obj_RingLoss(Object *obj);
void Obj_ShieldInvincibility(Object *obj);
void Obj_GameOverCard(Object *obj);
void Obj_GHZRock(Object *obj);
void Obj_SwingingPlatform(Object *obj);
void Obj_BigSpikeBall(Object *obj);
void Obj_Motobug(Object *obj);
void Obj_Spring(Object *obj);
void Obj_Newtron(Object *obj);
void Obj_GHZEdge(Object *obj);
void Obj_Credits(Object *obj);
void Obj_Waterfall(Object *obj);
void Obj_GiantRing(Object *obj);
void Obj_RingFlash(Object *obj);
void Obj_HiddenBonus(Object *obj);
void Obj_BasicPlatform(Object *obj);
void Obj_SmashWall(Object *obj);
void Obj_Scenery(Object *obj);
void Obj_CollapseLedge(Object *obj);
void Obj_CollapseFloor(Object *obj);
void Obj_VanishPlatform(Object *obj);
void Obj_Helix(Object *obj);
void Obj_MarbleBrick(Object *obj);
void Obj_Button(Object *obj);
void Obj_SmashBlock(Object *obj);
void Obj_MovingBlock(Object *obj);
void Obj_LargeGrass(Object *obj);
void Obj_GrassFire(Object *obj);
void Obj_LavaTag(Object *obj);
void Obj_Caterkiller(Object *obj);
void Obj_LavaMaker(Object *obj);
void Obj_LavaBall(Object *obj);
void Obj_GlassBlock(Object *obj);
void Obj_Basaran(Object *obj);
void Obj_ChainStomp(Object *obj);
void Obj_GeyserMaker(Object *obj);
void Obj_LavaGeyser(Object *obj);
void Obj_PushBlock(Object *obj);
void Obj_Yadrin(Object *obj);

static void (*object_func[])(Object*) = {
	/* ObjId_Null                */ NULL,
	/* ObjId_Sonic               */ Obj_Sonic,
	/* ObjId_02                  */ Obj_Null,
	/* ObjId_PathSwapper         */ Obj_PathSwapper,
	/* ObjId_04                  */ Obj_Null,
	/* ObjId_05                  */ Obj_Null,
	/* ObjId_06                  */ Obj_Null,
	/* ObjId_07                  */ Obj_Null,
	/* ObjId_Splash              */ Obj_Splash,
	/* ObjId_SpecialSonic        */ Obj_SpecialSonic,
	/* ObjId_DrownCount          */ Obj_DrownCount,
	/* ObjId_0B                  */ Obj_Null,
	/* ObjId_0C                  */ Obj_Null,
	/* ObjId_Signpost            */ Obj_Signpost,
	/* ObjId_TitleSonic          */ Obj_TitleSonic,
	/* ObjId_PSB                 */ Obj_PSB,
	/* ObjId_10                  */ Obj_Null,
	/* ObjId_GHZBridge           */ Obj_GHZBridge,
	/* ObjId_12                  */ Obj_Null,
	/* ObjId_LavaMaker           */ Obj_LavaMaker,
	/* ObjId_LavaBall            */ Obj_LavaBall,
	/* ObjId_SwingingPlatform    */ Obj_SwingingPlatform,
	/* ObjId_16                  */ Obj_Null,
	/* ObjId_Helix               */ Obj_Helix,
	/* ObjId_BasicPlatform       */ Obj_BasicPlatform,
	/* ObjId_19                  */ Obj_Null,
	/* ObjId_CollapseLedge       */ Obj_CollapseLedge,
	/* ObjId_WaterSurface        */ Obj_WaterSurface,
	/* ObjId_Scenery             */ Obj_Scenery,
	/* ObjId_1D                  */ Obj_Null,
	/* ObjId_1E                  */ Obj_Null,
	/* ObjId_Crabmeat            */ Obj_Crabmeat,
	/* ObjId_20                  */ Obj_Null,
	/* ObjId_HUD                 */ Obj_HUD,
	/* ObjId_BuzzBomber          */ Obj_BuzzBomber,
	/* ObjId_BuzzMissile         */ Obj_BuzzMissile,
	/* ObjId_BuzzExplode         */ Obj_BuzzExplode,
	/* ObjId_Ring                */ Obj_Ring,
	/* ObjId_Monitor             */ Obj_Monitor,
	/* ObjId_Explosion           */ Obj_Explosion,
	/* ObjId_Animal              */ Obj_Animals,
	/* ObjId_Points              */ Obj_Points,
	/* ObjId_2A                  */ Obj_Null,
	/* ObjId_Chopper             */ Obj_Chopper,
	/* ObjId_2C                  */ Obj_Null,
	/* ObjId_2D                  */ Obj_Null,
	/* ObjId_MonitorItem         */ Obj_MonitorItem,
	/* ObjId_LargeGrass          */ Obj_LargeGrass,
	/* ObjId_GlassBlock          */ Obj_GlassBlock,
	/* ObjId_ChainStomp          */ Obj_ChainStomp,
	/* ObjId_Button              */ Obj_Button,
	/* ObjId_PushBlock           */ Obj_PushBlock,
	/* ObjId_TitleCard           */ Obj_TitleCard,
	/* ObjId_GrassFire           */ Obj_GrassFire,
	/* ObjId_Spikes              */ Obj_Spikes,
	/* ObjId_RingLoss            */ Obj_RingLoss,
	/* ObjId_ShieldInvincibility */ Obj_ShieldInvincibility,
	/* ObjId_GameOverCard        */ Obj_GameOverCard,
	/* ObjId_GotThroughCard      */ Obj_GotThroughCard,
	/* ObjId_GHZRock             */ Obj_GHZRock,
	/* ObjId_SmashWall           */ Obj_SmashWall,
	/* ObjId_3D                  */ Obj_Null,
	/* ObjId_3E                  */ Obj_Null,
	/* ObjId_3F                  */ Obj_Null,
	/* ObjId_Motobug             */ Obj_Motobug,
	/* ObjId_Spring              */ Obj_Spring,
	/* ObjId_Newtron             */ Obj_Newtron,
	/* ObjId_43                  */ Obj_Null,
	/* ObjId_GHZEdge             */ Obj_GHZEdge,
	/* ObjId_45                  */ Obj_Null,
	/* ObjId_MarbleBrick         */ Obj_MarbleBrick,
	/* ObjId_Bumper              */ Obj_Null,//Bumper,
	/* ObjId_48                  */ Obj_Null,
	/* ObjId_49                  */ Obj_Waterfall,
	/* ObjId_4A                  */ Obj_Null,
	/* ObjId_GiantRing           */ Obj_GiantRing,
	/* ObjId_GeyserMaker         */ Obj_GeyserMaker,
	/* ObjId_LavaGeyser          */ Obj_LavaGeyser,
	/* ObjId_4E                  */ Obj_Null,
	/* ObjId_4F                  */ Obj_Null,
	/* ObjId_Yadrin              */ Obj_Yadrin,
	/* ObjId_SmashBlock          */ Obj_SmashBlock,
	/* ObjId_MovingBlock         */ Obj_MovingBlock,
	/* ObjId_CollapseFloor       */ Obj_CollapseFloor,
	/* ObjId_LavaTag             */ Obj_LavaTag,
	/* ObjId_Basaran             */ Obj_Basaran,
	/* ObjId_56                  */ Obj_Null,
	/* ObjId_57                  */ Obj_Null,
	/* ObjId_BigSpikeBall        */ Obj_BigSpikeBall,
	/* ObjId_59                  */ Obj_Null,
	/* ObjId_5A                  */ Obj_Null,
	/* ObjId_5B                  */ Obj_Null,
	/* ObjId_5C                  */ Obj_Null,
	/* ObjId_5D                  */ Obj_Null,
	/* ObjId_5E                  */ Obj_Null,
	/* ObjId_5F                  */ Obj_Null,
	/* ObjId_60                  */ Obj_Null,
	/* ObjId_61                  */ Obj_Null,
	/* ObjId_62                  */ Obj_Null,
	/* ObjId_63                  */ Obj_Null,
	/* ObjId_Bubble              */ Obj_Bubble,
	/* ObjId_65                  */ Obj_Null,
	/* ObjId_66                  */ Obj_Null,
	/* ObjId_67                  */ Obj_Null,
	/* ObjId_68                  */ Obj_Null,
	/* ObjId_69                  */ Obj_Null,
	/* ObjId_6A                  */ Obj_Null,
	/* ObjId_6B                  */ Obj_Null,
	/* ObjId_VanishPlatform      */ Obj_VanishPlatform,
	/* ObjId_6D                  */ Obj_Null,
	/* ObjId_6E                  */ Obj_Null,
	/* ObjId_6F                  */ Obj_Null,
	/* ObjId_70                  */ Obj_Null,
	/* ObjId_InvisibleBarrier    */ Obj_InvisibleBarrier,
	/* ObjId_72                  */ Obj_Null,
	/* ObjId_73                  */ Obj_Null,
	/* ObjId_74                  */ Obj_Null,
	/* ObjId_75                  */ Obj_Null,
	/* ObjId_76                  */ Obj_Null,
	/* ObjId_77                  */ Obj_Null,
	/* ObjId_Caterkiller         */ Obj_Caterkiller,
	/* ObjId_Checkpoint          */ Obj_Checkpoint,
	/* ObjId_7A                  */ Obj_Null,
	/* ObjId_7B                  */ Obj_Null,
	/* ObjId_RingFlash           */ Obj_RingFlash,
	/* ObjId_HiddenBonus         */ Obj_HiddenBonus,
	/* ObjId_7E                  */ Obj_Null,
	/* ObjId_7F                  */ Obj_Null,
	/* ObjId_80                  */ Obj_Null,
	/* ObjId_81                  */ Obj_Null,
	/* ObjId_82                  */ Obj_Null,
	/* ObjId_83                  */ Obj_Null,
	/* ObjId_84                  */ Obj_Null,
	/* ObjId_85                  */ Obj_Null,
	/* ObjId_86                  */ Obj_Null,
	/* ObjId_87                  */ Obj_Null,
	/* ObjId_88                  */ Obj_Null,
	/* ObjId_89                  */ Obj_Null,
	/* ObjId_Credits             */ Obj_Credits,
	/* ObjId_8B                  */ Obj_Null,
	/* ObjId_8C                  */ Obj_Null,
};

//Object functions
Object *FindFreeObj(void) {
	Object *obj = level_objects;
	for (int i = 0; i < LEVEL_OBJECTS; i++, obj++)
		if (obj->type == ObjId_Null)
			return obj;
	return NULL; //Original would return the address at the end of object space, I believe
}

Object *FindNextFreeObj(Object *obj) {
	for (; (obj - objects) < OBJECTS; obj++)
		if (obj->type == ObjId_Null)
			return obj;
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
				object_func[obj->type](obj);
			obj++;
		} while (ExecuteObjects_i-- > 0);
	} else {
		//Run reserved objects
		obj = objects;
		ExecuteObjects_i = RESERVED_OBJECTS - 1;
		do {
			if (obj->type)
				object_func[obj->type](obj);
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

//Object drawing
void BuildSpr_Normal(uint16_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, uint16_t tile, const uint8_t *mappings, uint8_t pieces) {
	do {
		//Don't overflow the sprite buffer
		if (*sprite_i >= BUFFER_SPRITES)
			break;
		
		//Read mappings
		int8_t map_y = *mappings++;
		uint8_t map_size = *mappings++;
		uint16_t map_tile = (mappings[0] << 8) | (mappings[1] << 0);
		mappings += 2;
		int8_t map_x = *mappings++;
		
		//Write sprite
		*(*sprite)++ = y + map_y; //y
		*(*sprite)++ = (map_size << 8) | ++(*sprite_i); //size and link
		*(*sprite)++ = map_tile + tile; //tile
		uint16_t px = x + map_x;
		#if (SCREEN_WIDTH <= 320)
			if ((px &= 0x1FF) == 0)
				px++; //Prevent sprite from being x=0 (acts as a mask)
		#else
			if (px == 0)
				px++;
		#endif
		*(*sprite)++ = px; //x
	} while (pieces-- > 0);
}

void BuildSprites_Draw(uint16_t **sprite, uint8_t *sprite_i, uint16_t x, uint16_t y, Object *obj, const uint8_t *mappings, uint8_t pieces) {
	if (obj->render.f.x_flip) {
		if (obj->render.f.y_flip) {
			//XY flip
			do {
				//Don't overflow the sprite buffer
				if (*sprite_i >= BUFFER_SPRITES)
					break;
				
				//Read mappings
				int8_t map_y = *mappings++;
				uint8_t map_size = *mappings++;
				uint16_t map_tile = (mappings[0] << 8) | (mappings[1] << 0);
				mappings += 2;
				int8_t map_x = *mappings++;
				
				//Write sprite
				*(*sprite)++ = y - map_y - (((map_size << 3) & 0x18) + 8); //y
				*(*sprite)++ = (map_size << 8) | ++(*sprite_i); //size and link
				*(*sprite)++ = (map_tile + obj->tile) ^ (TILE_Y_FLIP_AND | TILE_X_FLIP_AND); //tile
				uint16_t px = x - map_x - (((map_size << 1) & 0x18) + 8);
				#if (SCREEN_WIDTH <= 320)
					if ((px &= 0x1FF) == 0)
						px++; //Prevent sprite from being x=0 (acts as a mask)
				#else
					if (px == 0)
						px++;
				#endif
				*(*sprite)++ = px; //x
			} while (pieces-- > 0);
		} else {
			//X flip
			do {
				//Don't overflow the sprite buffer
				if (*sprite_i >= BUFFER_SPRITES)
					break;
				
				//Read mappings
				int8_t map_y = *mappings++;
				uint8_t map_size = *mappings++;
				uint16_t map_tile = (mappings[0] << 8) | (mappings[1] << 0);
				mappings += 2;
				int8_t map_x = *mappings++;
				
				//Write sprite
				*(*sprite)++ = y + map_y; //y
				*(*sprite)++ = (map_size << 8) | ++(*sprite_i); //size and link
				*(*sprite)++ = (map_tile + obj->tile) ^ TILE_X_FLIP_AND; //tile
				uint16_t px = x - map_x - (((map_size << 1) & 0x18) + 8);
				#if (SCREEN_WIDTH <= 320)
					if ((px &= 0x1FF) == 0)
						px++; //Prevent sprite from being x=0 (acts as a mask)
				#else
					if (px == 0)
						px++;
				#endif
				*(*sprite)++ = px; //x
			} while (pieces-- > 0);
		}
	} else if (obj->render.f.y_flip) {
		//Y flip
		do {
			//Don't overflow the sprite buffer
			if (*sprite_i >= BUFFER_SPRITES)
				break;
			
			//Read mappings
			int8_t map_y = *mappings++;
			uint8_t map_size = *mappings++;
			uint16_t map_tile = (mappings[0] << 8) | (mappings[1] << 0);
			mappings += 2;
			int8_t map_x = *mappings++;
			
			//Write sprite
			*(*sprite)++ = y - map_y - (((map_size << 3) & 0x18) + 8); //y
			*(*sprite)++ = (map_size << 8) | ++(*sprite_i); //size and link
			*(*sprite)++ = (map_tile + obj->tile) ^ TILE_Y_FLIP_AND; //tile
			uint16_t px = x + map_x;
			#if (SCREEN_WIDTH <= 320)
				if ((px &= 0x1FF) == 0)
					px++; //Prevent sprite from being x=0 (acts as a mask)
			#else
				if (px == 0)
					px++;
			#endif
			*(*sprite)++ = px; //x
		} while (pieces-- > 0);
	} else {
		BuildSpr_Normal(sprite, sprite_i, x, y, obj->tile, mappings, pieces);
	}
}

void BuildSprites(uint8_t *sprite_io) {
	//Draw each sprite priority queue
	uint16_t *sprite = &sprite_buffer[0][0];
	uint8_t sprite_i = 0;
	struct SpriteQueue *queue = sprite_queue;
	
	for (int i = 0; i < 8; i++, queue++) {
		//Iterate through all queued objects
		for (int j = 0; queue->size != 0; j++, queue->size--) {
			Object *obj = queue->obj[j];
			if (obj->mappings == NULL) //This line isn't in the original, but without it, the title screen segfaults
				continue;              //Basically, the bug that causes the 'PRESS START BUTTON' text to not appear gives the object null mappings
			if (obj->type != ObjId_Null) {
				//Get object position on screen and check if visible
				obj->render.f.on_screen = false;
				
				uint16_t x, y;
				if (obj->render.f.align_bg || obj->render.f.align_fg) {
					//Get screen position to use
					static int16_t *bs_scrpos[4][2] = {
						{NULL, NULL},
						{&scrpos_x.f.u,     &scrpos_y.f.u},
						{&bg_scrpos_x.f.u,  &bg_scrpos_y.f.u},
						{&bg3_scrpos_x.f.u, &bg3_scrpos_y.f.u},
					};
					int16_t **scrpos = bs_scrpos[(obj->render.f.align_bg << 1) | obj->render.f.align_fg];
					
					//Get object X position
					int16_t ox = obj->pos.l.x.f.u - *scrpos[0];
					if ((ox + obj->width_pixels) < 0 || (ox - obj->width_pixels) >= SCREEN_WIDTH)
						continue;
					x = 128 + ox; //VDP sprites start at 128
					
					//Get object Y position
					if (obj->render.f.yrad_height) {
						int16_t oy = obj->pos.l.y.f.u - *scrpos[1];
						// Real hardware zero-extends this byte (moveq #0,d0 / move.b
						// obHeight(a0),d0) rather than sign-extending it -- matters
						// because fragmentated objects (see FragmentatePlatform) never
						// get y_rad re-initialized on freshly allocated slots, so it
						// can hold garbage >=128 that must still read as a large
						// positive height here, not a negative one.
						uint8_t height = (uint8_t)obj->y_rad;
						if ((oy + height) < 0 || (oy - height) >= SCREEN_HEIGHT)
							continue;
						y = 128 + oy; //VDP sprites start at 128
					} else {
						int16_t oy = obj->pos.l.y.f.u - *scrpos[1] + 0x80;
						if (oy < 0x60 || oy >= (0x180 + SCREEN_TALLADD))
							continue;
						y = oy;
					}
				} else {
					//Positions map directly to VDP coordinates
					x = obj->pos.s.x;
					y = obj->pos.s.y;
				}

				//Get object mappings to use
				const uint8_t *mappings;
				uint8_t pieces;
				
				if (!obj->render.f.raw_mappings) {
					//Index mapping by frame
					const uint8_t *mapping_base = (const uint8_t*)obj->mappings;
					const uint8_t *mapping_ind = mapping_base + (obj->frame << 1);
					mappings = mapping_base + ((mapping_ind[0] << 8) | (mapping_ind[1] << 0));
					pieces = *mappings++;
				} else {
					//Directly use object mappings pointer
					mappings = obj->mappings;
					pieces = 1;
				}
				
				//Draw object
				if (pieces)
					BuildSprites_Draw(&sprite, &sprite_i, x, y, obj, mappings, pieces - 1);
				obj->render.f.on_screen = true;
			}
		}
	}
	
	//Terminate end of sprite list
	sprite_count = sprite_i;
	if (sprite_i >= BUFFER_SPRITES) {
		sprite[-3] &= 0xFF00; //Clear link byte
	} else {
		*sprite++ = 0;
		*sprite++ = 0;
	}
	if (sprite_io != NULL)
		*sprite_io = sprite_i;
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

void DisplaySprite(Object *obj) {
	//Get queue to use
	struct SpriteQueue *queue = &sprite_queue[obj->priority & 7];
	
	//Push to queue
	if (queue->size >= (sizeof(queue->obj) / sizeof(Object*)))
		return;
	queue->obj[queue->size++] = obj;
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

//Platform and solid objects
void MvSonicOnPtfm(Object *obj, int16_t y, int16_t prev_x) {
	//Check if player can be moved
	if (lock_multi & 0x80 || player->routine >= 6 || debug_use)
		return;
	
	player->pos.l.y.f.u = y - player->y_rad;
	player->pos.l.x.f.u += obj->pos.l.x.f.u - prev_x;
}

void PlatformObject(Object *obj, uint16_t x_rad) {
	//Check if player is colliding with platform
	if (player->ysp < 0)
		return;
	
	int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
	if (x_off < 0 || x_off >= (x_rad << 1))
		return;
	
	Platform3(obj, obj->pos.l.y.f.u - 8);
}

// Alternate version of PlatformObject with a custom solidity height input,
// instead of assuming 8px (only used by swinging platforms on chain links)
void PlatformObject_CustomHeight(Object *obj, uint16_t x_rad, int16_t height) {
	//Check if player is colliding with platform
	if (player->ysp < 0)
		return;

	int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
	if (x_off < 0 || x_off >= (x_rad << 1))
		return;

	Platform3(obj, obj->pos.l.y.f.u - height);
}

	void Platform3(Object *obj, int16_t top) {
	//Check if player is touching the top of platform
	int16_t py = player->pos.l.y.f.u;
	int16_t by = py + player->y_rad + 4;
	if (top > by)
		return;
	top -= by;
	if (top < -16)
		return;

	//Check if player can collide with platform
	if ((lock_multi & 0x80) || player->routine >= 6)
		return;

	//Clip on top of platform
	player->pos.l.y.f.u = top + py + 3;

	//Modify platform state
	obj->routine += 2;
	Platform_SetStand(obj);
}

void Platform_SetStand(Object *obj) {
	Scratch_Sonic *scratch = (Scratch_Sonic*)&player->scratch;
	
	//Release from last standing object
	if (player->status.p.f.object_stand)
	{
		Object *prv = objects + scratch->standing_obj;
		prv->status.o.f.player_stand = false;
		prv->routine_sec = 0;
		if (prv->routine == 4)
			prv->routine = 2;
	}
	
	//Modify player state
	scratch->standing_obj = obj - objects;
	player->angle = 0;
	player->ysp = 0;
	player->inertia = player->xsp;
	if (player->status.p.f.in_air)
		Sonic_ResetOnFloor(player);
	
	player->status.p.f.object_stand = true;
	obj->status.o.f.player_stand = true;
}

// Time bonus lookup, indexed by (total seconds / 15), clamped to the last
// entry (0 points) for times of 5 minutes or more.
#define TIME_BONUSES_NUM 20
static const uint16_t time_bonuses[TIME_BONUSES_NUM] = {
    5000, 5000, 1000, 500,  // 0:00 - 0:59
    400,  400,  300,  300,  // 1:00 - 1:59
    200,  200,  200,  200,  // 2:00 - 2:59
    100,  100,  100,  100,  // 3:00 - 3:59
    50,   50,   50,   50,   // 4:00 - 4:59
};

// Loads the end-of-act "GOT THROUGH" title card sequence -- shared by
// Signpost (routine 6, level's own goal) and Prison (routine $E, boss
// capsule's own animal release). Idempotent: real hardware's own first
// check is "already loaded? then don't do it again" (objects[23] is the
// fixed reserved slot the real v_endcard byte corresponds to), so it's
// safe to call from multiple objects/frames.
void GotThroughAct(void) {
    if (objects[23].type != ObjId_Null)
        return;

    // Reset game state
    limit_left2 = limit_right2;
    invincibility = false;
    time_count = false;

    // Load "Got through" card
    objects[23].type = ObjId_GotThroughCard;
    NewPLC(PlcId_TitleCard);
    endact_bonus = true;

    // Time Bonus
#ifdef SCP_FIX_BUGS
    // Time doesn't update while Debug Mode is enabled, which always
    // results in an annoying, unskippable 50,000 point time bonus
    // with it enabled.
    if (!debug_mode)
#endif
    {
        uint16_t total_sec = (uint16_t)level_time.min * 60 + level_time.sec;
        uint16_t index = total_sec / 15;
        if (index >= TIME_BONUSES_NUM)
            index = TIME_BONUSES_NUM - 1;
        time_bonus = time_bonuses[index];
    }

    // Ring Bonus
    ring_bonus = rings * 10;

    PlayMusic(bgm_GotThrough);
}

// Smashes a block into `count` fragment objects flying off at their own
// preset speed (frag_speeds: `count` {xsp,ysp} pairs) -- shared by GHZ/SLZ
// smashable walls and (eventually) MZ's smashable blocks. The object's
// CURRENT frame is used to look up its own raw per-piece mapping data
// (same "raw mappings" convention as Obj_MonitorItem's own frame->sub-
// mapping lookup, see its own comment); each fragment gets the NEXT
// consecutive 5-byte raw piece. The parent object itself becomes the
// first fragment (routine 4) rather than being replaced -- matches real
// hardware's own "movea.l a0,a1" reuse. Always uses the modern/FixBugs
// object-allocation order (FindNextFreeObj, not FindFreeObj) -- the real
// driver's un-fixed alternative additionally back-dates any fragment that
// landed earlier in object RAM than the parent so it still renders the
// same frame it's spawned on, which needs a second immediate-display call
// this project doesn't have a use for anywhere else, so it's not ported.
void SmashObject(Object *obj, int count, const int16_t *frag_speeds) {
    uint8_t id = obj->type;
    uint8_t render = obj->render.b;
    uint16_t tile = obj->tile;
    uint8_t priority = obj->priority;
    uint8_t width_pixels = obj->width_pixels;
    int16_t x = obj->pos.l.x.f.u;
    int16_t y = obj->pos.l.y.f.u;

    const uint8_t *piece = (const uint8_t *)obj->mappings + (obj->frame << 1);
    piece = (const uint8_t *)obj->mappings + (((piece[0] << 8) | piece[1]) + 1);

    Object *prev = obj;
    for (int i = 0; i < count; i++) {
        Object *frag = obj;
        if (i > 0) {
            frag = FindNextFreeObj(prev);
            if (frag == NULL)
                break;
            piece += 5;
        }
        frag->routine = 4;
        frag->type = id;
        frag->mappings = piece;
        frag->render.b = render;
        frag->render.f.raw_mappings = true;
        frag->pos.l.x.f.u = x;
        frag->pos.l.y.f.u = y;
        frag->tile = tile;
        frag->priority = priority;
        frag->width_pixels = width_pixels;
        frag->xsp = *frag_speeds++;
        frag->ysp = *frag_speeds++;
        prev = frag;
    }
    PlaySound(sfx_WallSmash);
}

// Shatters a collapsible platform into fragment pieces that each fall on
// their own delay (real hardware's shared "FragmentatePlatform"
// subroutine -- GHZ's collapsing ledges and MZ/SLZ/SBZ's collapsing
// floors are, per the real disasm's own framing, "more or less direct
// copies of each other" and share this exact fragment-spawning
// mechanism). Structurally identical to SmashObject (find-next-free-obj,
// raw-mappings per-piece assignment) except each fragment gets a FALL
// DELAY byte instead of an immediate launch velocity -- delays[i] is
// written to scratch offset 0x10 (objoff_38, `collapsible_timedelay` in
// the real disasm -- both GHZ's and MZ/SLZ/SBZ's own Scratch structs use
// this same offset, hence no caller-supplied offset parameter here).
void FragmentatePlatform(Object *obj, int count, const uint8_t *delays) {
    uint8_t id = obj->type;
    uint8_t render = obj->render.b;
    uint16_t tile = obj->tile;
    uint8_t priority = obj->priority;
    uint8_t width_pixels = obj->width_pixels;
    int16_t x = obj->pos.l.x.f.u;
    int16_t y = obj->pos.l.y.f.u;

    const uint8_t *piece = (const uint8_t *)obj->mappings + (obj->frame << 1);
    piece = (const uint8_t *)obj->mappings + (((piece[0] << 8) | piece[1]) + 1);

    Object *prev = obj;
    for (int i = 0; i < count; i++) {
        Object *frag = obj;
        if (i > 0) {
            frag = FindNextFreeObj(prev);
            if (frag == NULL)
                break;
            piece += 5;
        }
        frag->routine = 6;
        frag->type = id;
        frag->mappings = piece;
        frag->render.b = render;
        frag->render.f.raw_mappings = true;
        frag->pos.l.x.f.u = x;
        frag->pos.l.y.f.u = y;
        frag->tile = tile;
        frag->priority = priority;
        frag->width_pixels = width_pixels;
        frag->scratch.u8[0x10] = delays[i]; // collapsible_timedelay (objoff_38)
        prev = frag;
    }
    DisplaySprite(obj);
    PlaySound(sfx_Collapse);
}

// Sloped platform collision (GHZ collapsing ledges, SLZ seesaws) -- same
// core "is Sonic's foot inside the platform's top surface" check as
// Platform3 (reused directly), except the platform's own top Y comes from
// a per-column heightmap instead of a flat offset. heightmap has
// `x_rad*2` entries, one per pixel column across the platform's full
// width (mirrored if the object is currently x-flipped).
// Looks one 16px tile ahead of the object's own right/left edge for a
// solid wall -- real hardware's own thin wrappers around FindWall (angle
// buffer output/snap-to-flat-wall adjustment omitted here: none of this
// project's current callers need it, only the "did we hit something"
// distance). x_off is the real subroutine's own already-offset input
// (e.g. `obActWid` for the right wall, `~obActWid` for the left wall --
// callers should replicate the real ASM's own `not.w` pre-negation
// exactly, not just negate the plain width, to match real hardware's
// off-by-one).
int16_t ObjHitWallRight(Object *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(obj, (int16_t)(obj->pos.l.x.f.u + x_off), obj->pos.l.y.f.u, META_SOLID_TOP, 0, 0x10, &angle);
}

int16_t ObjHitWallLeft(Object *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(obj, (int16_t)(obj->pos.l.x.f.u + x_off), obj->pos.l.y.f.u, META_SOLID_TOP, 0, -0x10, &angle);
}

// Real hardware's own ObjHitCeiling: distance from the object's own top
// edge (obj->y_rad above center) to the nearest solid ceiling directly
// above it, using the same "check a tile from below" heightmap-flip
// convention already established by GetDistance_Up/GetDistance2_Up in
// LevelCollision.c/Sonic.c (META_Y_FLIP flip, -0x10 increment, ^0xF query
// coordinate). Negative return means a ceiling was hit.
int16_t ObjHitCeiling(Object *obj) {
    uint8_t angle;
    return FindFloor(obj, obj->pos.l.x.f.u, (int16_t)((obj->pos.l.y.f.u - obj->y_rad) ^ 0xF), META_SOLID_TOP, META_Y_FLIP, -0x10, &angle);
}

// Real hardware's own ChkObjectVisible: strict on-screen check against the
// visible 320x224 frame with NO margin (unlike IS_OFFSCREEN's own wider
// culling margin) -- used by spawner objects that are themselves invisible
// (so render.f.on_screen, only ever set by BuildSprites for objects that
// call DisplaySprite, is never meaningfully set for them) to decide
// whether it's OK to spawn something visibly right now.
bool ChkObjectVisible(Object *obj) {
    int16_t x = (int16_t)(obj->pos.l.x.f.u - scrpos_x.f.u);
    if (x < 0 || x >= SCREEN_WIDTH)
        return false;
    int16_t y = (int16_t)(obj->pos.l.y.f.u - scrpos_y.f.u);
    if (y < 0 || y >= SCREEN_HEIGHT)
        return false;
    return true;
}

// Real hardware's own ChkPartiallyVisible: same idea as ChkObjectVisible,
// but true as long as the object's own width_pixels/y_rad-sized box
// overlaps the screen at all (not just its center point) -- used by
// PushBlock to decide when it's safe to "exist" again after being forced
// back to its spawn position offscreen.
bool ChkPartiallyVisible(Object *obj) {
    int16_t x = (int16_t)(obj->pos.l.x.f.u - scrpos_x.f.u);
    if ((x + obj->width_pixels) < 0 || (x - obj->width_pixels) >= SCREEN_WIDTH)
        return false;
    int16_t y = (int16_t)(obj->pos.l.y.f.u - scrpos_y.f.u);
    if ((y + obj->y_rad) < 0 || (y - obj->y_rad) >= SCREEN_HEIGHT)
        return false;
    return true;
}

void SlopeObject(Object *obj, uint16_t x_rad, const uint8_t *heightmap) {
    if (player->ysp < 0)
        return;

    int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
    if (x_off < 0 || x_off >= (int16_t)(x_rad << 1))
        return;

    uint16_t column = (uint16_t)x_off;
    if (obj->render.f.x_flip)
        column = (uint16_t)(~column + (x_rad << 1));
    column >>= 1;

    int16_t top = (int16_t)(obj->pos.l.y.f.u - heightmap[column]);
    Platform3(obj, top);
}

bool ExitPlatform(Object *obj, uint16_t x_rad, uint16_t x_rad2, int16_t *x_off_p) {
	uint16_t x_dia = x_rad2 << 1;
	
	//Check if we've jumped off
	if (!player->status.p.f.in_air) {
		//Check if we've walked off
		int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
		if (x_off_p != NULL)
			*x_off_p = x_off;
		if (x_off >= 0 && x_off < x_dia)
			return false;
	}
	
	//Release player from platform
	player->status.p.f.object_stand = false;
	obj->routine = 2;
	obj->status.o.f.player_stand = false;
	return true;
}

static void Solid_ResetFloor(Object *obj, Object *pla) {
	Scratch_Sonic *scratch = (Scratch_Sonic*)&player->scratch;
	
	//Release player from last standing object
	if (player->status.p.f.object_stand) {
		Object *prv = objects + scratch->standing_obj;
		prv->status.o.f.player_stand = false;
		prv->routine_sec = 0;
	}
	
	//Modify player state
	scratch->standing_obj = obj - objects;
	player->angle = 0;
	player->ysp = 0;
	player->inertia = player->xsp;
	if (player->status.p.f.in_air)
		Sonic_ResetOnFloor(pla);
	
	player->status.p.f.object_stand = true;
	obj->status.o.f.player_stand = true;
}

// y_base is the object's own top-surface Y position collision is measured
// against -- ordinarily obj->pos.l.y.f.u (see Solid_ChkEnter below), but
// SolidObject_Heightmap passes a per-column value read from a heightmap
// instead (MZ's large grass platforms).
static int32_t Solid_ChkEnterY(Object *obj, uint16_t x_rad, uint16_t y_rad, int16_t y_base, int16_t *x_off, int16_t *y_off) {
	//Check if player is in horizontal range
	*x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
	uint16_t x_dia = x_rad << 1;
	if (*x_off >= 0 && *x_off <= x_dia) {
		//Check if player is in vertical range
		y_rad += player->y_rad;
		*y_off = player->pos.l.y.f.u - y_base + 4 + y_rad;
		uint16_t y_dia = y_rad << 1;
		
		if (*y_off >= 0 && *y_off < y_dia) {
			//Check if player can collide with object
			if (!(lock_multi & 0x80)) {
			#ifdef SCP_REV00
				if (player->routine >= 6) {
					if (debug_use)
						return 0;
			#else
				if (player->routine >= 6 || debug_use)
					return 0;
				{
			#endif
					//Get X clip
					uint16_t x_clip = *x_off;
					if (x_rad < *x_off) {
						*x_off -= x_dia;
						x_clip = -*x_off;
					}
					
					//Get Y clip
					uint16_t y_clip = *y_off;
					if (y_rad < *y_off) {
						*y_off -= (4 + y_dia);
						y_clip = -*y_off;
					}
					
					//Check if we're hitting the top/bottom or sides
					if (x_clip <= y_clip) {
						//Left/right
						if (y_clip > 4) {
							//Stop speed going towards object
							if (*x_off > 0) {
								if (player->xsp > 0) {
									player->xsp = 0;
									player->inertia = 0;
								}
							} else if (*x_off < 0) {
								if (player->xsp < 0) {
									player->xsp = 0;
									player->inertia = 0;
								}
							}
							
							//Clip and change push flags
							player->pos.l.x.f.u -= *x_off;
							if (!player->status.p.f.in_air) {
								//On ground, set push flags
								obj->status.o.f.player_push = true;
								player->status.p.f.pushing = true;
								return 1;
							}
						}
						
						//Mid-air or near edges, clear push flags
						obj->status.o.f.player_push = false;
						player->status.p.f.pushing = false;
						return 1;
					} else if (*y_off < 0) {
						//Bottom
						if (player->ysp != 0) {
							//Check if we should be clipped out the bottom
							if (player->ysp < 0 && *y_off < 0) {
								player->pos.l.y.f.u -= *y_off;
								player->ysp = 0;
							}
						}
						else if (!player->status.p.f.in_air) {
							//Squish Sonic
							KillSonic(player, obj);
						}
						return -1;
					} else {
						//Top
						//Check if we're going to land on the object
						if (*y_off < 16) {
							*y_off -= 4;
							
							//Check if we're within horizontal range and moving downwards
							uint16_t lx_rad = obj->width_pixels;
							uint16_t lx_dia = lx_rad << 1;
							int16_t lx_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + lx_rad;
							if (lx_off >= 0 && lx_off < lx_dia && player->ysp >= 0)
							{
								//Land on object
								player->pos.l.y.f.u -= *y_off + 1;
								Solid_ResetFloor(obj, player);
								obj->routine_sec = 2;
								obj->status.o.f.player_stand = true;
								return -1;
							}
							return 0;
						}
					}
				}
			}
		}
	}
	
	//Clear pushing state
	if (obj->status.o.f.player_push) {
		player->anim = SonAnimId_Run; //Not Walk
		obj->status.o.f.player_push = false;
		player->status.p.f.pushing = false;
	}
	return 0;
}

// Real hardware's own Solid_ChkEnter/Solid_ChkCollision (two labels for
// the same code) -- exported (unlike the rest of SolidObject's own
// internals) because PushBlock calls this directly instead of going
// through the full SolidObject dispatch: it layers its own extra
// "falling off a ledge"/"snapping to a ledge" states on top of the same
// 0=off/2=riding pair SolidObject itself already manages here, and needs
// the collision type AND x_off (to know which way to push the block) that
// only this lower-level entry point exposes.
int32_t Solid_ChkEnter(Object *obj, uint16_t x_rad, uint16_t y_rad, int16_t *x_off, int16_t *y_off) {
	return Solid_ChkEnterY(obj, x_rad, y_rad, obj->pos.l.y.f.u, x_off, y_off);
}

// Solid object collision against a per-column heightmap instead of a flat
// top surface (MZ's large grass platforms -- the platform's own top Y
// varies across its width, e.g. the hill-shaped types). heightmap has one
// entry per 2px column across the platform's full width (mirrored if the
// object is currently x-flipped), same convention as SlopeObject. Unlike
// SolidObject, this has no "already riding" state of its own -- the real
// hardware's own LargeGrass object tracks that itself and calls
// SlopeObject_AssumeStoodOn instead while riding (see below).
int32_t SolidObject_Heightmap(Object *obj, uint16_t x_rad, uint16_t y_rad, const uint8_t *heightmap) {
	int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
	uint16_t x_dia = x_rad << 1;
	if (x_off < 0 || x_off > x_dia)
		return 0;

	uint16_t column = (uint16_t)x_off;
	if (obj->render.f.x_flip)
		column = (uint16_t)(~column + x_dia);
	column >>= 1;

	int16_t spot_y = (int16_t)(obj->pos.l.y.f.u - (int16_t)(heightmap[column] - heightmap[0]));

	int16_t x_off_o, y_off_o;
	return Solid_ChkEnterY(obj, x_rad, y_rad, spot_y, &x_off_o, &y_off_o);
}

// Aligns Sonic to a heightmap-sloped platform's surface while he's already
// known to be standing on it (real hardware's own SlopeObject_AssumeStoodOn,
// used by MZ's large grass platforms while riding -- unlike SlopeObject,
// this doesn't do any landing detection of its own, it just repositions).
// prev_x is the object's own X position as of just before this frame's
// movement -- Sonic is nudged by however far the platform has since moved,
// same idea as MvSonicOnPtfm's own prev_x parameter (LargeGrass itself
// never moves horizontally, so its own caller always passes its CURRENT X,
// making this a no-op; a future horizontally-moving heightmap platform,
// e.g. SLZ's seesaw, would pass a genuinely earlier X here).
void SlopeObject_AssumeStoodOn(Object *obj, uint16_t x_rad, const uint8_t *heightmap, int16_t prev_x) {
	if (!player->status.p.f.object_stand)
		return;

	int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
	uint16_t column = (uint16_t)x_off;
	if (obj->render.f.x_flip)
		column = (uint16_t)(~column + (x_rad << 1));
	column >>= 1;

	player->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u - heightmap[column] - player->y_rad);
	player->pos.l.x.f.u -= (int16_t)(prev_x - obj->pos.l.x.f.u);
}

int32_t SolidObject(Object *obj, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x, int16_t *x_off, int16_t *y_off) {
	if (obj->routine_sec) {
		uint16_t x_dia = x_rad << 1;
		
		//Check if we've jumped off
		if (!player->status.p.f.in_air) {
			//Check if we've walked off
			int16_t x_off = player->pos.l.x.f.u - obj->pos.l.x.f.u + x_rad;
			if (x_off >= 0 && x_off <= x_dia) {
				//Move on platform
				MvSonicOnPtfm(obj, obj->pos.l.y.f.u - y_rad2, prev_x);
				return 0;
			}
		}
		
		//Release player from platform
		player->status.p.f.object_stand = false;
		obj->status.o.f.player_stand = false;
		obj->routine_sec = 0;
		return 0;
	}
	
	int16_t x_off_t = 0, y_off_t = 0;
	signed int res = Solid_ChkEnter(obj, x_rad, y_rad1, &x_off_t, &y_off_t);
	if (x_off != NULL)
		*x_off = x_off_t;
	if (y_off != NULL)
		*y_off = y_off_t;
	return res;
}
