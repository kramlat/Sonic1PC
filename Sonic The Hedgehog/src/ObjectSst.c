#include "ObjectSst.h"

#include "Level.h"
#include "Object/Explosion.h"
#include "SstLoad.h"
#include "MathUtil.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "LevelCollision.h"
#include "Standing.h"
#include "Sound.h"

// The solid and platform helpers for the status table's objects (see ObjectSst.h): Object.c's, field for field.

void SstMvSonicOnPtfm(Sst *obj, int16_t y, int16_t prev_x) {
    if (lock_multi & 0x80 || player->routine >= 6 || debug_use)
        return;

    player->y_pos.f.u = y - player->y_radius;
    player->x_pos.f.u += obj->x_pos.f.u - prev_x;
}

void SstPlatformObject(Sst *obj, uint16_t x_rad) {
    SstPlatformObject_CustomHeight(obj, x_rad, 8);
}

void SstPlatformObject_CustomHeight(Sst *obj, uint16_t x_rad, int16_t height) {
    if (player->y_vel < 0)
        return;

    int16_t x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
    if (x_off < 0 || x_off >= (x_rad << 1))
        return;

    SstPlatform3(obj, obj->y_pos.f.u - height);
}

void SstPlatform3(Sst *obj, int16_t top) {
    int16_t py = player->y_pos.f.u;
    int16_t by = py + player->y_radius + 4;
    if (top > by)
        return;
    top -= by;
    if (top < -16)
        return;

    if ((lock_multi & 0x80) || player->routine >= 6)
        return;

    player->y_pos.f.u = top + py + 3;

    obj->routine += 2;
    SstPlatform_SetStand(obj);
}

void SstPlatform_SetStand(Sst *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)player->own.u8;

    if (((player->status & SST_PSTATUS_OBJECT_STAND) != 0))
        Standing_Release(scratch->standing_obj, STANDING_RELEASE_PLATFORM);

    scratch->standing_obj = Standing_IdOfSst(obj);
    player->angle = 0;
    player->y_vel = 0;
    player->ground_vel = player->x_vel;
    if (((player->status & SST_PSTATUS_IN_AIR) != 0))
        Sonic_ResetOnFloor(player);

    player->status |= SST_PSTATUS_OBJECT_STAND;
    obj->status |= SST_STATUS_PLAYER_STAND;
}

bool SstExitPlatform(Sst *obj, uint16_t x_rad, uint16_t x_rad2, int16_t *x_off_p) {
    uint16_t x_dia = x_rad2 << 1;

    if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
        int16_t x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
        if (x_off_p != NULL)
            *x_off_p = x_off;
        if (x_off >= 0 && x_off < x_dia)
            return false;
    }

    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    obj->routine = 2;
    obj->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    return true;
}

void SstSlopeObject(Sst *obj, uint16_t x_rad, const uint8_t *heightmap) {
    if (player->y_vel < 0)
        return;

    int16_t x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
    if (x_off < 0 || x_off >= (int16_t)(x_rad << 1))
        return;

    uint16_t column = (uint16_t)x_off;
    if (obj->render_flags & SST_RENDER_X_FLIP)
        column = (uint16_t)(~column + (x_rad << 1));
    column >>= 1;

    SstPlatform3(obj, (int16_t)(obj->y_pos.f.u - heightmap[column]));
}

void SstSlopeObject_AssumeStoodOn(Sst *obj, uint16_t x_rad, const uint8_t *heightmap, int16_t prev_x) {
    if (!((player->status & SST_PSTATUS_OBJECT_STAND) != 0))
        return;

    int16_t x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
    uint16_t column = (uint16_t)x_off;
    if (obj->render_flags & SST_RENDER_X_FLIP)
        column = (uint16_t)(~column + (x_rad << 1));
    column >>= 1;

    player->y_pos.f.u = (int16_t)(obj->y_pos.f.u - heightmap[column] - player->y_radius);
    player->x_pos.f.u -= (int16_t)(prev_x - obj->x_pos.f.u);
}

static void Solid_ResetFloor(Sst *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)player->own.u8;

    if (((player->status & SST_PSTATUS_OBJECT_STAND) != 0))
        Standing_Release(scratch->standing_obj, STANDING_RELEASE_SOLID);

    scratch->standing_obj = Standing_IdOfSst(obj);
    player->angle = 0;
    player->y_vel = 0;
    player->ground_vel = player->x_vel;
    if (((player->status & SST_PSTATUS_IN_AIR) != 0))
        Sonic_ResetOnFloor(player);

    player->status |= SST_PSTATUS_OBJECT_STAND;
    obj->status |= SST_STATUS_PLAYER_STAND;
}

static int32_t Solid_ChkEnterY(Sst *obj, uint16_t x_rad, uint16_t y_rad, int16_t y_base, int16_t *x_off, int16_t *y_off) {
    *x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
    uint16_t x_dia = x_rad << 1;
    if (*x_off >= 0 && *x_off <= x_dia) {
        y_rad += player->y_radius;
        *y_off = player->y_pos.f.u - y_base + 4 + y_rad;
        uint16_t y_dia = y_rad << 1;

        if (*y_off >= 0 && *y_off < y_dia) {
            if (!(lock_multi & 0x80)) {
                if (player->routine >= 6 || debug_use)
                    return 0;
                {
                    uint16_t x_clip = *x_off;
                    if (x_rad < *x_off) {
                        *x_off -= x_dia;
                        x_clip = -*x_off;
                    }

                    uint16_t y_clip = *y_off;
                    if (y_rad < *y_off) {
                        *y_off -= (4 + y_dia);
                        y_clip = -*y_off;
                    }

                    if (x_clip <= y_clip) {
                        if (y_clip > 4) {
                            if (*x_off > 0) {
                                if (player->x_vel > 0) {
                                    player->x_vel = 0;
                                    player->ground_vel = 0;
                                }
                            } else if (*x_off < 0) {
                                if (player->x_vel < 0) {
                                    player->x_vel = 0;
                                    player->ground_vel = 0;
                                }
                            }

                            player->x_pos.f.u -= *x_off;
                            if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
                                obj->status |= SST_STATUS_PLAYER_PUSH;
                                player->status |= SST_PSTATUS_PUSHING;
                                return 1;
                            }
                        }

                        obj->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
                        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                        return 1;
                    } else if (*y_off < 0) {
                        if (player->y_vel != 0) {
                            if (player->y_vel < 0 && *y_off < 0) {
                                player->y_pos.f.u -= *y_off;
                                player->y_vel = 0;
                            }
                        }
                        else if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
                            KillSonicFrom(player, false);
                        }
                        return -1;
                    } else {
                        if (*y_off < 16) {
                            *y_off -= 4;

                            uint16_t lx_rad = obj->width_pixels;
                            uint16_t lx_dia = lx_rad << 1;
                            int16_t lx_off = player->x_pos.f.u - obj->x_pos.f.u + lx_rad;
                            if (lx_off >= 0 && lx_off < lx_dia && player->y_vel >= 0)
                            {
                                player->y_pos.f.u -= *y_off + 1;
                                Solid_ResetFloor(obj);
                                player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
                                obj->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
                                obj->routine_secondary = 2;
                                obj->status |= SST_STATUS_PLAYER_STAND;
                                return -1;
                            }
                            return 0;
                        }
                    }
                }
            }
        }
    }

    if (obj->status & SST_STATUS_PLAYER_PUSH) {
        obj->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    }
    return 0;
}

int32_t SstSolid_ChkEnter(Sst *obj, uint16_t x_rad, uint16_t y_rad, int16_t *x_off, int16_t *y_off) {
    return Solid_ChkEnterY(obj, x_rad, y_rad, obj->y_pos.f.u, x_off, y_off);
}

int32_t SstSolidObject_Heightmap(Sst *obj, uint16_t x_rad, uint16_t y_rad, const uint8_t *heightmap) {
    int16_t x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
    uint16_t x_dia = x_rad << 1;
    if (x_off < 0 || x_off > x_dia)
        return 0;

    uint16_t column = (uint16_t)x_off;
    if (obj->render_flags & SST_RENDER_X_FLIP)
        column = (uint16_t)(~column + x_dia);
    column >>= 1;

    int16_t spot_y = (int16_t)(obj->y_pos.f.u - (int16_t)(heightmap[column] - heightmap[0]));

    int16_t x_off_o, y_off_o;
    return Solid_ChkEnterY(obj, x_rad, y_rad, spot_y, &x_off_o, &y_off_o);
}

int32_t SstSolidObject(Sst *obj, uint16_t x_rad, uint16_t y_rad1, uint16_t y_rad2, int16_t prev_x, int16_t *x_off, int16_t *y_off) {
    if (obj->routine_secondary) {
        uint16_t x_dia = x_rad << 1;

        if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
            int16_t x_off = player->x_pos.f.u - obj->x_pos.f.u + x_rad;
            if (x_off >= 0 && x_off <= x_dia) {
                SstMvSonicOnPtfm(obj, obj->y_pos.f.u - y_rad2, prev_x);
                return 0;
            }
        }

        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        obj->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
        obj->routine_secondary = 0;
        return 0;
    }

    int16_t x_off_t = 0, y_off_t = 0;
    signed int res = SstSolid_ChkEnter(obj, x_rad, y_rad1, &x_off_t, &y_off_t);
    if (x_off != NULL)
        *x_off = x_off_t;
    if (y_off != NULL)
        *y_off = y_off_t;
    return res;
}

bool SstChkObjectVisible(const Sst *obj) {
    int16_t x = (int16_t)(obj->x_pos.f.u - scrpos_x.f.u);
    if (x < 0 || x >= SCREEN_WIDTH)
        return false;
    int16_t y = (int16_t)(obj->y_pos.f.u - scrpos_y.f.u);
    if (y < 0 || y >= SCREEN_HEIGHT)
        return false;
    return true;
}

bool SstChkPartiallyVisible(const Sst *obj) {
    int16_t x = (int16_t)(obj->x_pos.f.u - scrpos_x.f.u);
    if ((x + obj->width_pixels) < 0 || (x - obj->width_pixels) >= SCREEN_WIDTH)
        return false;
    int16_t y = (int16_t)(obj->y_pos.f.u - scrpos_y.f.u);
    if ((y + obj->y_radius) < 0 || (y - obj->y_radius) >= SCREEN_HEIGHT)
        return false;
    return true;
}

int16_t SstHitWallRight(Sst *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(Collision_Proxy(), (int16_t)(obj->x_pos.f.u + x_off), obj->y_pos.f.u, META_SOLID_LRB, 0, 0x10, &angle);
}

int16_t SstHitWallLeft(Sst *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(Collision_Proxy(), (int16_t)(obj->x_pos.f.u + x_off), obj->y_pos.f.u, META_SOLID_LRB, 0, -0x10, &angle);
}

int16_t SstHitCeiling(Sst *obj) {
    uint8_t angle;
    return FindFloor(Collision_Proxy(), obj->x_pos.f.u, (int16_t)((obj->y_pos.f.u - (int8_t)obj->y_radius) ^ 0xF), META_SOLID_LRB, META_Y_FLIP, -0x10, &angle);
}

void SstSmashObject(Sst *obj, int count, const int16_t *frag_speeds) {
    SstCode code = obj->code;
    uint8_t render = obj->render_flags;
    uint16_t tile = obj->art_tile;
    uint16_t priority = obj->priority;
    uint8_t width_pixels = obj->width_pixels;
    int16_t x = obj->x_pos.f.u;
    int16_t y = obj->y_pos.f.u;

    const uint8_t *piece;
    Mappings_FramePieces((const uint8_t *)obj->mappings, obj->mapping_frame, &piece);

    Sst *prev = obj;
    for (int i = 0; i < count; i++) {
        Sst *frag = obj;
        if (i > 0) {
            frag = Sst_AllocateAfter(prev);
            if (frag == NULL)
                break;
            piece += SPRITE_PIECE_SIZE_S3;
        }
        frag->routine = 4;
        frag->code = code;
        frag->mappings = piece;
        frag->render_flags = render | SST_RENDER_STATIC;
        frag->x_pos.f.u = x;
        frag->y_pos.f.u = y;
        frag->art_tile = tile;
        frag->priority = priority;
        frag->width_pixels = width_pixels;
        frag->x_vel = *frag_speeds++;
        frag->y_vel = *frag_speeds++;
        prev = frag;
    }
    PlaySound(sfx_WallSmash);
}

void SstFragmentatePlatform(Sst *obj, int count, const uint8_t *delays) {
    SstCode code = obj->code;
    uint8_t render = obj->render_flags;
    uint16_t tile = obj->art_tile;
    uint16_t priority = obj->priority;
    uint8_t width_pixels = obj->width_pixels;
    int16_t x = obj->x_pos.f.u;
    int16_t y = obj->y_pos.f.u;

    const uint8_t *piece;
    Mappings_FramePieces((const uint8_t *)obj->mappings, obj->mapping_frame, &piece);

    Sst *prev = obj;
    for (int i = 0; i < count; i++) {
        Sst *frag = obj;
        if (i > 0) {
            frag = Sst_AllocateAfter(prev);
            if (frag == NULL)
                break;
            piece += SPRITE_PIECE_SIZE_S3;
        }
        frag->routine = 6;
        frag->code = code;
        frag->mappings = piece;
        frag->render_flags = render | SST_RENDER_STATIC;
        frag->x_pos.f.u = x;
        frag->y_pos.f.u = y;
        frag->art_tile = tile;
        frag->priority = priority;
        frag->width_pixels = width_pixels;
        frag->own.u8[SST_COLLAPSE_DELAY] = delays[i];
        prev = frag;
    }
    Sst_Draw(obj);
    PlaySound(sfx_Collapse);
}

// BossMove for the status table: the boss's own position (kept apart from the object's, in 16.16) moves by the object's velocity
void SstBossMove(const Sst *obj, dword_s *boss_x, dword_s *boss_y) {
	boss_x->v += (int32_t)obj->x_vel << 8;
	boss_y->v += (int32_t)obj->y_vel << 8;
}

// BossDefeated for the status table: every 8th frame the wrecked boss makes a fiery explosion (Object 3F) somewhere on it
void SstBossDefeated(const Sst *obj) {
	if ((uint8_t)frame_count & 7)
		return;

	Sst *exp = Sst_SpawnCode(Obj_ExplosionBomb, obj->x_pos.f.u, obj->y_pos.f.u);
	if (exp == NULL)
		return;

	uint32_t rand = RandomNumber();
	exp->x_pos.f.u = (int16_t)(exp->x_pos.f.u + (int16_t)(((uint8_t)rand >> 2) - 0x20));
	// (no shift is made for the Y: all the explosions are biased downwards, as in the original)
	exp->y_pos.f.u = (int16_t)(exp->y_pos.f.u + (int16_t)((uint8_t)(rand >> 8) >> 3));
}
