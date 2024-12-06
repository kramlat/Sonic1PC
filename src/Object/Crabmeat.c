#include "Crabmeat.h"

static uint8_t Obj_Crabmeat_SetAni(Object* obj) {
    if (obj->angle & 0x80) {
        if (obj->angle <= 0xFA)
            return (obj->status.o.f.x_flip ? 2 : 1);
    } else {
        if (obj->angle >= 0x06)
            return (obj->status.o.f.x_flip ? 1 : 2);
    }
    return 0;
}

void Obj_Crabmeat_Construct(Object* obj) {
    // Initialize collision
    obj->y_rad = 16;
    obj->x_rad = 8;

    // Initialize object drawing information
    obj->mappings = Mappings_Crabmeat;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x400);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;

    // Initialize more stuff
    obj->col_type = 0x06;
    obj->width_pixels = 21;
}

void Obj_Crabmeat_Fall(Object* obj, int16_t floor_dist) {
    // Clip and increment routine
    obj->pos.l.y.f.u += floor_dist;
    obj->ysp = 0;
    obj->angle = angle_buffer0;
    obj->routine += 2;
}

bool Obj_Crabmeat_Check_Timer(Scratch_Crabmeat* scratch) {
    return --scratch->time_delay >= 0;
}

void Obj_Crabmeat_TurnAround(Object* obj, Scratch_Crabmeat* scratch) {
    // Turn around
    obj->routine_sec += 2;
    scratch->time_delay = 127;
    obj->xsp = 0x80;
    obj->anim = 3 + Obj_Crabmeat_SetAni(obj);
    if ((obj->status.o.f.x_flip ^= 1))
        obj->xsp = -obj->xsp;
}

void Obj_Crabmeat_Fire_Projectile(Object* obj, Object* proj, uint8_t type, uint8_t routine, int16_t x, int16_t y, int16_t xsp) {
    proj = FindFreeObj();
    if (proj != NULL) {
        proj->type = type;
        proj->routine = routine;
        proj->pos.l.x.f.u = obj->pos.l.x.f.u + x;
        proj->pos.l.y.f.u = obj->pos.l.y.f.u + y;
        proj->xsp = xsp;
    }
}

void Obj_Crabmeat_Fire(Object* obj, Scratch_Crabmeat* scratch) {
    // Fire
    scratch->time_delay = 59;
    obj->anim = 6;

    // Create projectiles
    Object* proj;

    Obj_Crabmeat_Fire_Projectile(obj, proj, ObjId_Crabmeat, 6, -16, 0, -0x100);
    Obj_Crabmeat_Fire_Projectile(obj, proj, ObjId_Crabmeat, 6, 16, 0, 0x100);
}

bool Obj_Crabmeat_Check_CrabMode(Scratch_Crabmeat* scratch) {
    return ((scratch->crab_mode ^= 1) & 1);
}

bool Obj_Crabmeat_Check_EdgeDist(int16_t floor_dist) {
    return (floor_dist >= -8 && floor_dist < 12);
}

int16_t Obj_Crabmeat_Get_EdgeDist(Object* obj) {
    return ObjFloorDist(obj, obj->pos.l.x.f.u + (obj->status.o.f.x_flip ? -16 : 16));
}

void Obj_Crabmeat_NonZIF(Object* obj, int16_t floor_dist) {
    obj->pos.l.y.f.u += floor_dist;
    obj->angle = angle_buffer0;
    obj->anim = 3 + Obj_Crabmeat_SetAni(obj);
}

void Obj_Crabmeat_Stop(Object* obj, Scratch_Crabmeat* scratch) {
    // Stop for a moment
    obj->routine_sec -= 2;
    scratch->time_delay = 59;
    obj->xsp = 0;
    obj->anim = Obj_Crabmeat_SetAni(obj);
}

void Obj_Crabmeat_Projectile_Construct(Object* obj) {
    // Increment routine
    obj->routine += 2;

    // Initialize object drawing information
    obj->mappings = Mappings_Crabmeat;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x400);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;

    // Initialize more stuff
    obj->col_type = 0x87;
    obj->width_pixels = 8;
    obj->ysp = -0x400;
    obj->anim = 7;
}

bool Obj_Crabmeat_Check_BelowLevel(Object* obj) {
    return obj->pos.l.y.f.u >= limit_btm2 + SCREEN_HEIGHT;
}

void Obj_Crabmeat(Object* obj) {
    Scratch_Crabmeat* scratch = (Scratch_Crabmeat*)&obj->scratch;

    int16_t floor_dist;

    switch (obj->routine) {
    case 0: // Initialization
        Obj_Crabmeat_Construct(obj);

        // Fall before enabling
        ObjectFall(obj);
        if ((floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u)) >= 0)
            break;

        Obj_Crabmeat_Fall(obj, floor_dist);
        // Fallthrough
    case 2: // Moving
        switch (obj->routine_sec) {
        case 0: // Wait to fire
            // Wait for timer to expire
            if (Obj_Crabmeat_Check_Timer(scratch))
                break;

            // Check if we should fire
            if (!obj->render.f.on_screen || ((scratch->crab_mode ^= 2) & 2)) {
                Obj_Crabmeat_TurnAround(obj, scratch);
            } else {
                Obj_Crabmeat_Fire(obj, scratch);
            }
            break;
        case 2: // Walking
            // Wait for timer to expire
            if (Obj_Crabmeat_Check_Timer(scratch)) {
                // Move then perform collision
                SpeedToPos(obj);

                if (!Obj_Crabmeat_Check_CrabMode(scratch)) {
                    // Check if we're too close to edge
                    floor_dist = Obj_Crabmeat_Get_EdgeDist(obj);
                    if (Obj_Crabmeat_Check_EdgeDist(floor_dist))
                        break;
                } else {
                    // Clip out of floor
                    floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
                    Obj_Crabmeat_NonZIF(obj,floor_dist);
                    break;
                }
            }

            Obj_Crabmeat_Stop(obj,scratch);
            break;
        }

        // Animate, draw, and unload when off-screen
        AnimateSprite(obj, Animation_Crabmeat);
        RememberState(obj);
        break;
    case 4: // Delete
        ObjectDelete(obj);
        break;
    case 6: // Projectile initialization
        Obj_Crabmeat_Projectile_Construct(obj);
        // Fallthrough
    case 8: // Projectile move
        // Move and animate
        ObjectFall(obj);
        AnimateSprite(obj, Animation_Crabmeat);
        DisplaySprite(obj);

        // Delete if fallen below stage
        if (Obj_Crabmeat_Check_BelowLevel(obj))
            ObjectDelete(obj);
        break;
    }
}
