#include "Elevator.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Constants.h"

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

// Accelerates/decelerates the platform again until it reaches its target
// distance. The distance itself is generic -- each platform type below
// interprets it differently (rising, descending, diagonal, ...).
static void Elev_Move(Sst *self, Own_Elevator *own) {
    int16_t accel = own->acceleration;
    if (own->slowing_down) {
        if (accel != 0)
            accel -= 0x10;
    } else if (accel < 0x800) {
        accel += 0x10;
    }
    own->acceleration = accel;

    own->moved_distance += ((int32_t)accel) << 8;
    int16_t dist = (int16_t)(own->moved_distance >> 16);

    if ((uint16_t)dist > (uint16_t)own->half_distance)
        own->slowing_down = 1;

    if ((uint16_t)dist == (uint16_t)(own->half_distance * 2))
        self->subtype = 0; // action type -> stationary
}

static void Elev_Rising(Sst *self, Own_Elevator *own) {
    Elev_Move(self, own);
    int16_t dist = (int16_t)(own->moved_distance >> 16);
    self->y_pos.f.u = (int16_t)(own->orig_y - dist);
}

static void Elev_Descending(Sst *self, Own_Elevator *own) {
    Elev_Move(self, own);
    int16_t dist = (int16_t)(own->moved_distance >> 16);
    self->y_pos.f.u = (int16_t)(own->orig_y + dist);
}

static void Elev_RiseRight(Sst *self, Own_Elevator *own) {
    Elev_Move(self, own);
    int16_t dist = (int16_t)(own->moved_distance >> 16);
    self->y_pos.f.u = (int16_t)(own->orig_y - (dist >> 1));
    self->x_pos.f.u = (int16_t)(own->orig_x + dist);
}

static void Elev_DescendLeft(Sst *self, Own_Elevator *own) {
    Elev_Move(self, own);
    int16_t dist = (int16_t)(own->moved_distance >> 16);
    self->y_pos.f.u = (int16_t)(own->orig_y + (dist >> 1));
    self->x_pos.f.u = (int16_t)(own->orig_x - dist);
}

// Rises and deletes itself once its peak is reached (only ever created by
// Elev_Spawner).
static void Elev_FromSpawner(Sst *self, Own_Elevator *own) {
    Elev_Move(self, own);
    int16_t dist = (int16_t)(own->moved_distance >> 16);
    self->y_pos.f.u = (int16_t)(own->orig_y - dist);

    if (self->subtype != 0) // has platform reached its final destination?
        return;

    if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
        player->status |= SST_PSTATUS_IN_AIR;
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        player->routine = 2; // Sonic_Control
    }
    Sst_Delete(self);
}

static void Elev_Types(Sst *self, Own_Elevator *own) {
    switch (self->subtype & 0xF) {
    case 0: // stationary
        break;
    case 1: case 3: case 5: case 7: // go to next action type once stood on
        if (self->routine == 4)
            self->subtype++;
        break;
    case 2: Elev_Rising(self, own); break;
    case 4: Elev_Descending(self, own); break;
    case 6: Elev_RiseRight(self, own); break;
    case 8: Elev_DescendLeft(self, own); break;
    case 9: Elev_FromSpawner(self, own); break;
    }
}

static void Elev_Main(Sst *self, Own_Elevator *own) {
    self->routine = 2; // advance to Elev_Platform

    uint8_t subtype = self->subtype;
    if (subtype & 0x80) {
        self->routine = 6; // Elev_Spawner
        int16_t delay = (int16_t)((subtype & 0x7F) * 6);
        own->half_distance = delay; // reused as elev_spawner_delay
        own->spawner_delaybase = delay;
        return; // keep spawner alive while invisible
    }

    self->width_pixels = 80 / 2;
    self->mapping_frame = 0;

    uint8_t idx = subtype & 0xF;
    own->half_distance = (int16_t)(Elev_Var2[idx].half_dist_div8 << 2);
    self->subtype = Elev_Var2[idx].type; // repurpose subtype -> action type

    self->mappings = Mappings_Elevator;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;
}

static void Elev_Platform(Sst *self, Own_Elevator *own) {
    SstPlatformObject(self, self->width_pixels); // may set self->routine = 4
    Elev_Types(self, own);
}

// Returns true if the platform deleted itself this frame (Elev_FromSpawner
// reaching its peak) -- the caller must not touch it further.
static bool Elev_StoodOn(Sst *self, Own_Elevator *own) {
    SstExitPlatform(self, self->width_pixels, self->width_pixels, NULL); // may set self->routine = 2

    int16_t prev_x = self->x_pos.f.u;
    Elev_Types(self, own);

    if (self->code == NULL)
        return true;

    SstMvSonicOnPtfm(self, self->y_pos.f.u - 9, prev_x);
    return false;
}

static void Elev_Spawner(Sst *self, Own_Elevator *own) {
    if (--own->half_distance == 0) {
        own->half_distance = own->spawner_delaybase;

        Sst *plat = Sst_Allocate();
        if (plat != NULL) {
            plat->code = Obj_Elevator;
            plat->x_pos.f.u = self->x_pos.f.u;
            plat->y_pos.f.u = self->y_pos.f.u;
            plat->subtype = 0xE; // Elev_Var2 entry $E -> action type 9
        }
    }

    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}

void Obj_Elevator(Sst *self) {
    Own_Elevator *own = (Own_Elevator *)self->own.u8;

    switch (self->routine) {
    case 0:
        Elev_Main(self, own);
        if (self->routine == 6)
            return; // spawner: no display while invisible
        break;
    case 2:
        Elev_Platform(self, own);
        break;
    case 4:
        if (Elev_StoodOn(self, own))
            return; // already deleted itself
        break;
    case 6:
        Elev_Spawner(self, own);
        return; // spawner never reaches the shared display tail
    }

    // Elev_Types (via Elev_FromSpawner) can delete the platform even when
    // nobody's standing on it -- guard against touching it further.
    if (self->code == NULL)
        return;

    SstOnScreenTest2(self, own->orig_x);
}
