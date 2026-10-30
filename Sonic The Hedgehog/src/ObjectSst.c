#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "LevelCollision.h"
#include "Standing.h"

// The solid and platform helpers for the status table's objects (see ObjectSst.h): Object.c's, field for field.

void SstMvSonicOnPtfm(Sst *obj, int16_t y, int16_t prev_x) {
    if (lock_multi & 0x80 || player->routine >= 6 || debug_use)
        return;

    player->pos.l.y.f.u = y - player->y_rad;
    player->pos.l.x.f.u += obj->x_pos.f.u - prev_x;
}

void SstPlatformObject(Sst *obj, uint16_t x_rad) {
    SstPlatformObject_CustomHeight(obj, x_rad, 8);
}

void SstPlatformObject_CustomHeight(Sst *obj, uint16_t x_rad, int16_t height) {
    if (player->ysp < 0)
        return;

    int16_t x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
    if (x_off < 0 || x_off >= (x_rad << 1))
        return;

    SstPlatform3(obj, obj->y_pos.f.u - height);
}

void SstPlatform3(Sst *obj, int16_t top) {
    int16_t py = player->pos.l.y.f.u;
    int16_t by = py + player->y_rad + 4;
    if (top > by)
        return;
    top -= by;
    if (top < -16)
        return;

    if ((lock_multi & 0x80) || player->routine >= 6)
        return;

    player->pos.l.y.f.u = top + py + 3;

    obj->routine += 2;
    SstPlatform_SetStand(obj);
}

void SstPlatform_SetStand(Sst *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&player->scratch;

    if (player->status.p.f.object_stand)
        Standing_Release(scratch->standing_obj, STANDING_RELEASE_PLATFORM);

    scratch->standing_obj = Standing_IdOfSst(obj);
    player->angle = 0;
    player->ysp = 0;
    player->inertia = player->xsp;
    if (player->status.p.f.in_air)
        Sonic_ResetOnFloor(player);

    player->status.p.f.object_stand = true;
    obj->status |= SST_STATUS_PLAYER_STAND;
}

bool SstExitPlatform(Sst *obj, uint16_t x_rad, uint16_t x_rad2, int16_t *x_off_p) {
    uint16_t x_dia = x_rad2 << 1;

    if (!player->status.p.f.in_air) {
        int16_t x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
        if (x_off_p != NULL)
            *x_off_p = x_off;
        if (x_off >= 0 && x_off < x_dia)
            return false;
    }

    player->status.p.f.object_stand = false;
    obj->routine = 2;
    obj->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    return true;
}

void SstSlopeObject(Sst *obj, uint16_t x_rad, const uint8_t *heightmap) {
    if (player->ysp < 0)
        return;

    int16_t x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
    if (x_off < 0 || x_off >= (int16_t)(x_rad << 1))
        return;

    uint16_t column = (uint16_t)x_off;
    if (obj->render_flags & SST_RENDER_X_FLIP)
        column = (uint16_t)(~column + (x_rad << 1));
    column >>= 1;

    SstPlatform3(obj, (int16_t)(obj->y_pos.f.u - heightmap[column]));
}

void SstSlopeObject_AssumeStoodOn(Sst *obj, uint16_t x_rad, const uint8_t *heightmap, int16_t prev_x) {
    if (!player->status.p.f.object_stand)
        return;

    int16_t x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
    uint16_t column = (uint16_t)x_off;
    if (obj->render_flags & SST_RENDER_X_FLIP)
        column = (uint16_t)(~column + (x_rad << 1));
    column >>= 1;

    player->pos.l.y.f.u = (int16_t)(obj->y_pos.f.u - heightmap[column] - player->y_rad);
    player->pos.l.x.f.u -= (int16_t)(prev_x - obj->x_pos.f.u);
}

static void Solid_ResetFloor(Sst *obj) {
    Scratch_Sonic *scratch = (Scratch_Sonic*)&player->scratch;

    if (player->status.p.f.object_stand)
        Standing_Release(scratch->standing_obj, STANDING_RELEASE_SOLID);

    scratch->standing_obj = Standing_IdOfSst(obj);
    player->angle = 0;
    player->ysp = 0;
    player->inertia = player->xsp;
    if (player->status.p.f.in_air)
        Sonic_ResetOnFloor(player);

    player->status.p.f.object_stand = true;
    obj->status |= SST_STATUS_PLAYER_STAND;
}

static int32_t Solid_ChkEnterY(Sst *obj, uint16_t x_rad, uint16_t y_rad, int16_t y_base, int16_t *x_off, int16_t *y_off) {
    *x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
    uint16_t x_dia = x_rad << 1;
    if (*x_off >= 0 && *x_off <= x_dia) {
        y_rad += player->y_rad;
        *y_off = player->pos.l.y.f.u - y_base + 4 + y_rad;
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

                            player->pos.l.x.f.u -= *x_off;
                            if (!player->status.p.f.in_air) {
                                obj->status |= SST_STATUS_PLAYER_PUSH;
                                player->status.p.f.pushing = true;
                                return 1;
                            }
                        }

                        obj->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
                        player->status.p.f.pushing = false;
                        return 1;
                    } else if (*y_off < 0) {
                        if (player->ysp != 0) {
                            if (player->ysp < 0 && *y_off < 0) {
                                player->pos.l.y.f.u -= *y_off;
                                player->ysp = 0;
                            }
                        }
                        else if (!player->status.p.f.in_air) {
                            KillSonicFrom(player, false);
                        }
                        return -1;
                    } else {
                        if (*y_off < 16) {
                            *y_off -= 4;

                            uint16_t lx_rad = obj->width_pixels;
                            uint16_t lx_dia = lx_rad << 1;
                            int16_t lx_off = player->pos.l.x.f.u - obj->x_pos.f.u + lx_rad;
                            if (lx_off >= 0 && lx_off < lx_dia && player->ysp >= 0)
                            {
                                player->pos.l.y.f.u -= *y_off + 1;
                                Solid_ResetFloor(obj);
                                player->status.p.f.pushing = false;
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
        player->status.p.f.pushing = false;
    }
    return 0;
}

int32_t SstSolid_ChkEnter(Sst *obj, uint16_t x_rad, uint16_t y_rad, int16_t *x_off, int16_t *y_off) {
    return Solid_ChkEnterY(obj, x_rad, y_rad, obj->y_pos.f.u, x_off, y_off);
}

int32_t SstSolidObject_Heightmap(Sst *obj, uint16_t x_rad, uint16_t y_rad, const uint8_t *heightmap) {
    int16_t x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
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

        if (!player->status.p.f.in_air) {
            int16_t x_off = player->pos.l.x.f.u - obj->x_pos.f.u + x_rad;
            if (x_off >= 0 && x_off <= x_dia) {
                SstMvSonicOnPtfm(obj, obj->y_pos.f.u - y_rad2, prev_x);
                return 0;
            }
        }

        player->status.p.f.object_stand = false;
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

// The collision probes of an object (ObjFloorDist and Object.c's wall and ceiling ones) for a status table object. The probes only use the object for the collision path it walks on, which for every object but
// the characters is the first one's: a plain object stands in.
static Object collision_proxy;

int16_t SstFloorDist(Sst *obj, int16_t x) {
    return FindFloor(&collision_proxy, x, obj->y_pos.f.u + (int8_t)obj->y_radius, META_SOLID_TOP, 0, 0x10, &angle_buffer0);
}

int16_t SstHitWallRight(Sst *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(&collision_proxy, (int16_t)(obj->x_pos.f.u + x_off), obj->y_pos.f.u, META_SOLID_LRB, 0, 0x10, &angle);
}

int16_t SstHitWallLeft(Sst *obj, int16_t x_off) {
    uint8_t angle;
    return FindWall(&collision_proxy, (int16_t)(obj->x_pos.f.u + x_off), obj->y_pos.f.u, META_SOLID_LRB, 0, -0x10, &angle);
}

int16_t SstHitCeiling(Sst *obj) {
    uint8_t angle;
    return FindFloor(&collision_proxy, obj->x_pos.f.u, (int16_t)((obj->y_pos.f.u - (int8_t)obj->y_radius) ^ 0xF), META_SOLID_LRB, META_Y_FLIP, -0x10, &angle);
}
