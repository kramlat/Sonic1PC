#include "Explosion.h"
#include "Constants.h"
#include "Object.h"
#include "Sound.h"
#include "SstLoad.h"

// Explosion assets
#include "Resource/Mappings/Explosion.h"
// Real hardware's Map_ExplodeBomb shares 3 of its 5 frames with
// Map_ExplodeItem via backwards (negative) mappingsTable offsets -- not
// representable here since this engine's frame lookup treats mapping
// offsets as unsigned. Mappings_ExplodeBomb is therefore its own
// standalone resource with those shared frames duplicated in rather than
// referenced.
#include "Resource/Mappings/ExplodeBomb.h"

static void Explosion_Animal(Sst *self) {
    Sst *animal = Sst_Spawn(ObjId_Animal, self->x_pos.f.u, self->y_pos.f.u);
    if (animal != NULL)
        animal->own.u16[0xB] = self->own.u16[0xB];
}

static void Explosion_Construct(Sst *self, const uint8_t *mappings, uint8_t sound) {
    // Set object drawing information
    self->mappings = mappings;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Explosion);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 1 * 0x80;

    // Initialize state
    self->collision_flags = 0;
    self->width_pixels = 12;
    self->anim_frame_timer = 7;
    self->mapping_frame = 0;

    PlaySound(sound);
}

// Shared animate/delete tail, reused directly by the bomb's explosion below
// (real hardware's Object 3F literally branches into this same code).
static void Explosion_Animate(Sst *self) {
    // Run animation and delete once done
    if ((int8_t)--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 7;
        if (++self->mapping_frame == 5) {
            Sst_Delete(self);
            return;
        }
    }

    // Draw object
    Sst_Draw(self);
}

// Explosion object
void Obj_Explosion(Sst *self) {
    switch (self->routine) {
    case 0: // Spawn an animal
        self->routine += 2;
        Explosion_Animal(self);
        // Fallthrough
    case 2: // Initialization
        self->routine += 2;
        Explosion_Construct(self, Mappings_Explosion, sfx_BreakItem);
        // Fallthrough
    case 4: // Animate
        Explosion_Animate(self);
        break;
    }
}

// Object 3F - Fiery explosion from a destroyed boss, Walking Bomb badnik,
// or Ball Hog cannonball
void Obj_ExplosionBomb(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        self->routine += 2;
        Explosion_Construct(self, Mappings_ExplodeBomb, sfx_Bomb);
        // Fallthrough
    case 2: // Animate
        Explosion_Animate(self);
        break;
    }
}

// The old objects that turn into one (see Explosion.h): 5 frames of 8 is how long the explosion lasts
static void Explosion_Stand(Object *obj, ObjectId id) {
    if (obj->render.b != 0xFF) { // (first run)
        Sst *made = Sst_Spawn(id, obj->pos.l.x.f.u, obj->pos.l.y.f.u);
        if (made != NULL) {
            made->routine = obj->routine;
            made->own.u16[0xB] = obj->scratch.u16[0xB];
        }
        obj->render.b = 0xFF; // (it is only a marker now: invisible, and it does nothing)
        obj->frame_time.b = 5 * 8;
        obj->routine = 2;
        return;
    }
    if (--obj->frame_time.b < 0)
        ObjectDelete(obj);
}

void Obj_ExplosionObject(Object *obj) {
    Explosion_Stand(obj, ObjId_Explosion);
}

void Obj_ExplosionBombObject(Object *obj) {
    Explosion_Stand(obj, ObjId_ExplosionBomb);
}
