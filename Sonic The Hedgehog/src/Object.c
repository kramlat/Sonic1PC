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
#include "MathUtil.h"

#include "Macros.h"

#include <string.h>

//Boss subroutines (shared by all bosses)

// Modified variant of SpeedToPos, moving a boss's own fixed-point base
// position (obBossX/obBossY, kept separately from its visible/bobbing
// position) by its current xsp/ysp.
void BossMove(Object *obj, dword_s *boss_x, dword_s *boss_y) {
	boss_x->v += obj->xsp << 8;
	boss_y->v += obj->ysp << 8;
}

// Periodically spawns a small explosion particle near a defeated boss.
void BossDefeated(Object *obj) {
	if ((uint8_t)frame_count & 7) // limits spawning explosions to every 8 frames
		return;

	Object *exp = FindFreeObj();
	if (exp == NULL)
		return;

	exp->type = ObjId_ExplosionBomb; // real Object 3F "Explosion" -- fiery boss-wreckage variant (Map_ExplodeBomb + sfx_Bomb), not id_ExplosionItem
	exp->routine = 0;
	exp->pos.l.x.f.u = obj->pos.l.x.f.u;
	exp->pos.l.y.f.u = obj->pos.l.y.f.u;

	uint32_t rand = RandomNumber();
	int16_t rand_x = (int16_t)(((uint8_t)rand >> 2) - 0x20);
	exp->pos.l.x.f.u = (int16_t)(exp->pos.l.x.f.u + rand_x);

	// Unlike the X-position, no shift is made for the Y-position. It's
	// hard to tell if it was intentional or not, but all explosions are
	// biased downwards because of this.
	int16_t rand_y = (int16_t)((uint8_t)(rand >> 8) >> 3);
	exp->pos.l.y.f.u = (int16_t)(exp->pos.l.y.f.u + rand_y);
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
// consecutive raw piece. The parent object itself becomes the
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

    const uint8_t *piece;
    Mappings_FramePieces((const uint8_t *)obj->mappings, obj->frame, &piece);

    Object *prev = obj;
    for (int i = 0; i < count; i++) {
        Object *frag = obj;
        if (i > 0) {
            frag = FindNextFreeObj(prev);
            if (frag == NULL)
                break;
            piece += SPRITE_PIECE_SIZE;
        }
        frag->routine = 4;
        frag->type = id;
        frag->mappings = piece;
        frag->render.b = render;
        frag->render.f.static_mappings = true;
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

    const uint8_t *piece;
    Mappings_FramePieces((const uint8_t *)obj->mappings, obj->frame, &piece);

    Object *prev = obj;
    for (int i = 0; i < count; i++) {
        Object *frag = obj;
        if (i > 0) {
            frag = FindNextFreeObj(prev);
            if (frag == NULL)
                break;
            piece += SPRITE_PIECE_SIZE;
        }
        frag->routine = 6;
        frag->type = id;
        frag->mappings = piece;
        frag->render.b = render;
        frag->render.f.static_mappings = true;
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
    return FindWall(obj, (int16_t)(obj->pos.l.x.f.u + x_off), obj->pos.l.y.f.u, META_SOLID_LRB, 0, 0x10, &angle);
}

int16_t ObjHitWallLeft(Object *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(obj, (int16_t)(obj->pos.l.x.f.u + x_off), obj->pos.l.y.f.u, META_SOLID_LRB, 0, -0x10, &angle);
}

// All three object wall/ceiling probes check the left/right/bottom solidity
// bit (hardcoded $D in P128 -- "MJ: set solid type to check (changed from
// $E)"), NOT the top-solid bit that ObjFloorDist uses. Top-solid data
// describes one-way floors, so probing it for a ceiling or wall reports bogus
// hits (e.g. LZ1's rising platforms got shoved 32px into the floor the moment
// they started to rise).
// Real hardware's own ObjHitCeiling: distance from the object's own top
// edge (obj->y_rad above center) to the nearest solid ceiling directly
// above it, using the same "check a tile from below" heightmap-flip
// convention already established by GetDistance_Up/GetDistance2_Up in
// LevelCollision.c/Sonic.c (META_Y_FLIP flip, -0x10 increment, ^0xF query
// coordinate). Negative return means a ceiling was hit.
int16_t ObjHitCeiling(Object *obj) {
    uint8_t angle;
    return FindFloor(obj, obj->pos.l.x.f.u, (int16_t)((obj->pos.l.y.f.u - obj->y_rad) ^ 0xF), META_SOLID_LRB, META_Y_FLIP, -0x10, &angle);
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
				if (player->routine >= 6 || debug_use)
					return 0;
				{
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
								// Bug fix: real hardware only clears the
								// pushing flag here if Sonic was airborne
								// (Solid_ResetFloor -> Sonic_ResetOnFloor,
								// itself conditional on in_air) -- landing on
								// top while still grounded (e.g. climbing an
								// object he was just pushing) leaves the
								// pushing flag stuck, showing the Push
								// animation while just standing/walking on
								// top. Sonic 2 and Sonic 3 & Knuckles clear
								// it unconditionally here instead.
								player->status.p.f.pushing = false;
								obj->status.o.f.player_push = false;
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
		// (The original forces the running animation here, which is the infamous "walk-jump bug": jumping off a monitor, a wall or a
		// solid object left Sonic with the walking animation in the air. Not done here.)
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
