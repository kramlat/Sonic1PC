#include "Explosion.h"
#include "Sound.h"

void Obj_Explosion_Animal(Object *obj) {
    Object* animal = FindFreeObj();
    if (animal != NULL) {
        animal->type = ObjId_Animal;
        animal->pos.l.x.f.u = obj->pos.l.x.f.u;
        animal->pos.l.y.f.u = obj->pos.l.y.f.u;
        animal->scratch.u16[0xB] = obj->scratch.u16[0xB];
    }
}

void Obj_Explosion_Construct(Object *obj) {
    // Set object drawing information
    obj->mappings = Mappings_Explosion;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Explosion);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 1;

    // Initialize state
    obj->col_type = 0;
    obj->width_pixels = 12;
    obj->frame_time.b = 7;
    obj->frame = 0;

    PlaySound(sfx_BreakItem);
}

// Shared animate/delete tail, reused directly by Obj_ExplosionBomb below
// (real hardware's Object 3F literally branches into this same code).
void Obj_Explosion_Animate(Object *obj) {
    // Run animation and delete once done
    if (--obj->frame_time.b < 0) {
        obj->frame_time.b = 7;
        if (++obj->frame == 5) {
            ObjectDelete(obj);
            return;
        }
    }

    // Draw object
    DisplaySprite(obj);
}

// Explosion object
void Obj_Explosion(Object *obj) {
    switch (obj->routine) {
    case 0: // Spawn an animal
        // Increment routine and create animal object
        obj->routine += 2;

        Obj_Explosion_Animal(obj);

        // Fallthrough
    case 2: // Initialization
        // Increment routine
        obj->routine += 2;

        Obj_Explosion_Construct(obj);
        // Fallthrough
    case 4: // Animate
        Obj_Explosion_Animate(obj);
        break;
    }
}

// Object 3F - Fiery explosion from a destroyed boss, Walking Bomb badnik,
// or Ball Hog cannonball
void Obj_ExplosionBomb_Construct(Object *obj) {
    // Set object drawing information
    obj->mappings = Mappings_ExplodeBomb;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Explosion);
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 1;

    // Initialize state
    obj->col_type = 0;
    obj->width_pixels = 12;
    obj->frame_time.b = 7;
    obj->frame = 0;

    PlaySound(sfx_Bomb);
}

void Obj_ExplosionBomb(Object *obj) {
    switch (obj->routine) {
    case 0: // Initialization
        obj->routine += 2;

        Obj_ExplosionBomb_Construct(obj);
        // Fallthrough
    case 2: // Animate
        Obj_Explosion_Animate(obj);
        break;
    }
}
