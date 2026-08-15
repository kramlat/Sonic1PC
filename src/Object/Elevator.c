#include "Elevator.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/Elevator.h"

// Object 59 - platforms that move when you stand on them (SLZ)

// half-distance-to-move (in units of 8px) and action type, indexed by the
// low nibble of the placement subtype.
static const struct { uint8_t half_dist_div8; uint8_t type; } Elev_Var2[15] = {
    { 0x80 / 8, 1 }, { 0x100 / 8, 1 }, { 0x1A0 / 8, 1 }, // 0-2
    { 0x80 / 8, 3 }, { 0x100 / 8, 3 }, { 0x1A0 / 8, 3 }, // 3-5
    { 0xA0 / 8, 1 }, { 0x120 / 8, 1 }, { 0x160 / 8, 1 }, // 6-8
    { 0xA0 / 8, 3 }, { 0x120 / 8, 3 }, { 0x160 / 8, 3 }, // 9-B
    { 0x100 / 8, 5 }, { 0x100 / 8, 7 },                  // C-D
    { 0x180 / 8, 9 },                                    // E (from spawner)
};

static bool Elev_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

// Accelerates/decelerates the platform again until it reaches its target
// distance. The distance itself is generic -- each platform type below
// interprets it differently (rising, descending, diagonal, ...).
static void Elev_Move(Object *obj, Scratch_Elevator *scratch) {
    int16_t accel = scratch->acceleration;
    if (scratch->slowing_down) {
        if (accel != 0)
            accel -= 0x10;
    } else if (accel < 0x800) {
        accel += 0x10;
    }
    scratch->acceleration = accel;

    scratch->moved_distance += ((int32_t)accel) << 8;
    int16_t dist = (int16_t)(scratch->moved_distance >> 16);

    if ((uint16_t)dist > (uint16_t)scratch->half_distance)
        scratch->slowing_down = 1;

    if ((uint16_t)dist == (uint16_t)(scratch->half_distance * 2))
        obj->scratch.u8[0] = 0; // action type -> stationary
}

static void Elev_Rising(Object *obj, Scratch_Elevator *scratch) {
    Elev_Move(obj, scratch);
    int16_t dist = (int16_t)(scratch->moved_distance >> 16);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - dist);
}

static void Elev_Descending(Object *obj, Scratch_Elevator *scratch) {
    Elev_Move(obj, scratch);
    int16_t dist = (int16_t)(scratch->moved_distance >> 16);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + dist);
}

static void Elev_RiseRight(Object *obj, Scratch_Elevator *scratch) {
    Elev_Move(obj, scratch);
    int16_t dist = (int16_t)(scratch->moved_distance >> 16);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - (dist >> 1));
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + dist);
}

static void Elev_DescendLeft(Object *obj, Scratch_Elevator *scratch) {
    Elev_Move(obj, scratch);
    int16_t dist = (int16_t)(scratch->moved_distance >> 16);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + (dist >> 1));
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x - dist);
}

// Rises and deletes itself once its peak is reached (only ever created by
// Elev_Spawner).
static void Elev_FromSpawner(Object *obj, Scratch_Elevator *scratch) {
    Elev_Move(obj, scratch);
    int16_t dist = (int16_t)(scratch->moved_distance >> 16);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - dist);

    if (obj->scratch.u8[0] != 0) // has platform reached its final destination?
        return;

    if (obj->status.o.f.player_stand) {
        player->status.p.f.in_air = true;
        player->status.p.f.object_stand = false;
        player->routine = 2; // Sonic_Control
    }
    ObjectDelete(obj);
}

static void Elev_Types(Object *obj, Scratch_Elevator *scratch) {
    switch (obj->scratch.u8[0] & 0xF) {
    case 0: // stationary
        break;
    case 1: case 3: case 5: case 7: // go to next action type once stood on
        if (obj->routine == 4)
            obj->scratch.u8[0]++;
        break;
    case 2: Elev_Rising(obj, scratch); break;
    case 4: Elev_Descending(obj, scratch); break;
    case 6: Elev_RiseRight(obj, scratch); break;
    case 8: Elev_DescendLeft(obj, scratch); break;
    case 9: Elev_FromSpawner(obj, scratch); break;
    }
}

static void Elev_Main(Object *obj, Scratch_Elevator *scratch) {
    obj->routine = 2; // advance to Elev_Platform

    uint8_t subtype = obj->scratch.u8[0];
    if (subtype & 0x80) {
        obj->routine = 6; // Elev_Spawner
        int16_t delay = (int16_t)((subtype & 0x7F) * 6);
        scratch->half_distance = delay; // reused as elev_spawner_delay
        scratch->spawner_delaybase = delay;
        return; // keep spawner alive while invisible
    }

    obj->width_pixels = 80 / 2;
    obj->frame = 0;

    uint8_t idx = subtype & 0xF;
    scratch->half_distance = (int16_t)(Elev_Var2[idx].half_dist_div8 << 2);
    obj->scratch.u8[0] = Elev_Var2[idx].type; // repurpose subtype -> action type

    obj->mappings = Mappings_Elevator;
    obj->tile = TILE_MAP(0, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3
    obj->render.f.align_fg = true;
    obj->priority = 4;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;
}

static void Elev_Platform(Object *obj, Scratch_Elevator *scratch) {
    PlatformObject(obj, obj->width_pixels); // may set obj->routine = 4
    Elev_Types(obj, scratch);
}

// Returns true if the platform deleted itself this frame (Elev_FromSpawner
// reaching its peak) -- the caller must not touch it further.
static bool Elev_StoodOn(Object *obj, Scratch_Elevator *scratch) {
    ExitPlatform(obj, obj->width_pixels, obj->width_pixels, NULL); // may set obj->routine = 2

    int16_t prev_x = obj->pos.l.x.f.u;
    Elev_Types(obj, scratch);

    if (obj->type == ObjId_Null)
        return true;

    MvSonicOnPtfm(obj, obj->pos.l.y.f.u - 9, prev_x);
    return false;
}

static void Elev_Spawner(Object *obj, Scratch_Elevator *scratch) {
    if (--scratch->half_distance == 0) {
        scratch->half_distance = scratch->spawner_delaybase;

        Object *plat = FindFreeObj();
        if (plat != NULL) {
            plat->type = ObjId_Elevator;
            plat->pos.l.x.f.u = obj->pos.l.x.f.u;
            plat->pos.l.y.f.u = obj->pos.l.y.f.u;
            plat->scratch.u8[0] = 0xE; // Elev_Var2 entry $E -> action type 9
        }
    }

    if (Elev_OutOfRange(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}

void Obj_Elevator(Object *obj) {
    Scratch_Elevator *scratch = (Scratch_Elevator *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        Elev_Main(obj, scratch);
        if (obj->routine == 6)
            return; // spawner: no display while invisible
        break;
    case 2:
        Elev_Platform(obj, scratch);
        break;
    case 4:
        if (Elev_StoodOn(obj, scratch))
            return; // already deleted itself
        break;
    case 6:
        Elev_Spawner(obj, scratch);
        return; // spawner never reaches the shared display tail
    }

    // Elev_Types (via Elev_FromSpawner) can delete the platform even when
    // nobody's standing on it -- guard against touching it further.
    if (obj->type == ObjId_Null)
        return;

    if (Elev_OutOfRange(scratch->orig_x))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}
