#include "BuzzBomber.h"

// Buzz Bomber's explosion
void Obj_BuzzExplode(Object* obj) {
    (void)obj;
}

void Obj_BuzzMissile_Construct(Object* obj) {
        obj->mappings = Mappings_BuzzMissile;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x444);
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 3;
        obj->width_pixels = 8;
        obj->status.o.f.flag2 = false;
        obj->status.o.f.player_stand = false;
        obj->status.o.f.flag4 = false;
        obj->status.o.f.player_push = false;
        obj->status.o.f.flag6 = false;
        obj->status.o.f.flag7 = false;
}

bool Obj_BuzzMissile_CheckNewtron(Object* obj, Scratch_BuzzMissile* scratch) {
    if (scratch->subtype) {
        obj->routine = 8;
        obj->col_type = 0x87;
        obj->anim = 1;
        AnimateSprite(obj, Animation_BuzzMissile);
        DisplaySprite(obj);
        return true;
    }
    return false;
}

void Obj_BuzzMissile_Charge(Object* obj, Scratch_BuzzMissile* scratch) {
    // Delete object if parent Buzz Bomber has exploded
    if (scratch->parent->type == ObjId_Explosion)
        ObjectDelete(obj);

    // Animate and draw
    AnimateSprite(obj, Animation_BuzzMissile);
    DisplaySprite(obj);
}

void Obj_BuzzMissile_Fire(Object* obj) {
    // Check if we've 'hit Sonic' (disabled)
    if (!obj->status.o.f.flag7) {
        // Use fired collision and animation
        obj->col_type = 0x87;
        obj->anim = 1;

        // Move and animate
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_BuzzMissile);
        DisplaySprite(obj);

        // Delete if fallen below stage
        if ((limit_btm2 + SCREEN_HEIGHT) < obj->pos.l.y.f.u)
            ObjectDelete(obj);

    } else {
        // Explode
        obj->type = ObjId_BuzzExplode;
        obj->routine = 0;
        Obj_BuzzExplode(obj);
    }
}

void Obj_BuzzMissile_NewtFire(Object* obj) {
    // Delete once off-screen
    if (!obj->render.f.on_screen)
        ObjectDelete(obj);
    else {
        // Move and animate
        SpeedToPos(obj);
        AnimateSprite(obj, Animation_BuzzMissile);
        DisplaySprite(obj);
    }
}

void Obj_BuzzMissile(Object* obj) {
    Scratch_BuzzMissile* scratch = (Scratch_BuzzMissile*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Initialization
        // Wait for timer to expire
        if (--scratch->time_delay >= 0)
            break;

        // Increment routine
        obj->routine += 2;

        // Set object drawing information
        Obj_BuzzMissile_Construct(obj);

        // Check if we were created by a Newtron
        if(Obj_BuzzMissile_CheckNewtron(obj, scratch))
            break;

        // Fallthrough
    case 2: // Charging
        Obj_BuzzMissile_Charge(obj, scratch);
        break;
    case 4: // Fired by Buzz Bomber
        Obj_BuzzMissile_Fire(obj);
        break;
    case 6: // Delete
        ObjectDelete(obj);
        break;
    case 8: // Fired by Newtron
        Obj_BuzzMissile_NewtFire(obj);
        break;
    }
}

void Obj_BuzzBomber_Construct(Object* obj) {
    // Set object drawing information
    obj->mappings = Mappings_BuzzBomber;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x444);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;
    obj->col_type = 0x08;
    obj->width_pixels = 24;
}

void Obj_BuzzBomber_Fly(Object* obj, Scratch_BuzzBomber* scratch) {
    // Not near Sonic
    obj->routine_sec += 2;
    scratch->time_delay = 127; // It's a word, Yuji!
    obj->xsp = 0x400;
    obj->anim = 1;
    if (!obj->status.o.f.x_flip)
        obj->xsp = -obj->xsp;
}

void Obj_BuzzBomber_Fire(Object* obj, Object* missile, Scratch_BuzzBomber* scratch) {
    Scratch_BuzzMissile* mscratch = (Scratch_BuzzMissile*)&missile->scratch;

    missile->type = ObjId_BuzzMissile;
    missile->pos.l.x.f.u = obj->pos.l.x.f.u;
    missile->pos.l.y.f.u = obj->pos.l.y.f.u + 28;
    missile->ysp = 0x200;
    missile->xsp = 0x200;
    if (!obj->status.o.f.x_flip) {
        missile->pos.l.x.f.u -= 24;
        missile->xsp = -missile->xsp;
    } else
        missile->pos.l.x.f.u += 24;

    missile->status.o.b = obj->status.o.b;
    mscratch->time_delay = 14;
    mscratch->parent = obj;

    // Set our state
    scratch->buzz_status = 1;
    scratch->time_delay = 59;
    obj->anim = 2;
}

bool Obj_BuzzBomber_CheckCloseToSonic(Object* obj) {
    // Get X difference and check if we're close enough to Sonic
    uint16_t x_off = (obj->pos.l.x.f.u < player->pos.l.x.f.u) ? (player->pos.l.x.f.u - obj->pos.l.x.f.u) : (obj->pos.l.x.f.u - player->pos.l.x.f.u);

    if (x_off >= 0x60 || !obj->render.f.on_screen)
        return false;
    return true;
}

void Obj_BuzzBomber_SetStatusAttack(Scratch_BuzzBomber* scratch) {
    // Set that we're near Sonic
    scratch->buzz_status = 2;
    scratch->time_delay = 29;
}

void Obj_BuzzBomber_TurnAround(Object* obj, Scratch_BuzzBomber* scratch) {
    // Change direction
    scratch->buzz_status = 0;
    obj->status.o.f.x_flip ^= 1;
    scratch->time_delay = 59;
}

void Obj_BuzzBomber_Stop(Object* obj) {
    // Stop
    obj->routine_sec -= 2;
    obj->xsp = 0;
    obj->anim = 0;
}

void Obj_BuzzBomber(Object* obj)
{
    Scratch_BuzzBomber* scratch = (Scratch_BuzzBomber*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine
        obj->routine += 2;

        Obj_BuzzBomber_Construct(obj);

        // Fallthrough
    case 2: // Moving
        switch (obj->routine_sec) {
        case 0: // Moving
            // Wait for timer to expire
            if (--scratch->time_delay >= 0)
                break;

            // Start firing missile if near Sonic
            if (!(scratch->buzz_status & 2)) {
                Obj_BuzzBomber_Fly(obj, scratch);
            } else {
                // Near Sonic
                // Create missile object
                Object* missile = FindFreeObj();
                if (missile == NULL)
                    break;
                Obj_BuzzBomber_Fire(obj,missile,scratch);
            }
            break;
        case 2: // Check near Sonic
            // Wait for timer to expire
            if (--scratch->time_delay >= 0) {
                // Move and check if we're near Sonic
                SpeedToPos(obj);

                if (scratch->buzz_status)
                    break;

                if(Obj_BuzzBomber_CheckCloseToSonic(obj)) {
                    Obj_BuzzBomber_SetStatusAttack(scratch);
                } else
                    break;
            } else {
                Obj_BuzzBomber_TurnAround(obj,scratch);
            }

            Obj_BuzzBomber_Stop(obj);
            break;
        }

        // Animate, draw, and unload when off-screen
        AnimateSprite(obj, Animation_BuzzBomber);
        RememberState(obj);
        break;
    case 4: // Delete
        ObjectDelete(obj);
        break;
    }
}
