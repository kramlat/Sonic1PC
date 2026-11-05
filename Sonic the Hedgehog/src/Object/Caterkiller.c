#include "Caterkiller.h"
#include "SstLoad.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Mappings/Caterkiller.h"

// Object 78 - Caterkiller enemy (MZ, SBZ). Per the real disassembly's own
// comment, "easily the most complex badnik object in the entire game, and
// also a contender for the most complex object overall": a head plus 3
// spiked body segments (separate objects, chained via a parent pointer),
// where each segment follows a short delay behind the position/floor
// history of the one ahead of it, and touching ANY segment breaks the
// whole thing apart (propagating both up to the head and down to the
// remaining children over a couple of frames).
//
// This targets REV01 + FixBugs behavior only, matching every other object
// ported this session -- the REV00-specific code paths in the real
// disassembly (a slightly different, buggier floor/wall-turnaround
// sequence) are not replicated. One REV01 micro-adjustment (a 1px subpixel
// parity nudge on the exact frame a segment hits a wall while facing a
// particular direction) is also simplified away as gameplay-invisible.

// Non-standard animation format (unique to this object): a flat 128-entry
// table indexed directly by (self->angle & 0x7F), NOT the standard
// duration+frame-list bytecode AnimateSprite expects elsewhere -- so this
// is walked by hand rather than going through AnimateSprite. Head frames
// are 0-7; body segments add 8 to reach their own equivalent frame widths.
static const int8_t cat_animation[128] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6,
    6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, -1, 7, 7, -1,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6,
    6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4,
    4, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0, 0, -1,
};

// Fragmentation launch X-speed, indexed by (obRoutine-2)/2: head, then
// each of the 3 body segments in order.
static const int16_t cat_frag_speed[4] = { -0x200, -0x180, 0x180, 0x200 };

// Real hardware's own Cat_Despawn: clears this object's own respawn-block
// flag (if any) and marks it for deletion NEXT frame via the plain
// Cat_Delete routine, rather than deleting immediately -- same idea as
// RememberState's own offscreen cleanup (SstOnScreenTest here).
static void Cat_Despawn(Sst *self) {
    if (self->respawn)
        (*SstLoad_State(self)) &= 0x7F;
    self->routine = 0xA;
}

static void Cat_FragmentateBody(Sst *self) {
    uint8_t index = (uint8_t)((self->routine - 2) / 2);
    int16_t speed = cat_frag_speed[index];
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        speed = (int16_t)-speed;
    self->x_vel = speed;
    self->y_vel = (int16_t)-0x400;
    self->routine = 0xC;
    self->mapping_frame &= 0xF8; // reset to frame 0 of this segment's own 8-frame set
}

// Notifies this segment's own parent that the whole Caterkiller should
// break apart (bubbling the break both upward, toward the head, and --
// via each segment's own .chkBroken check below -- downward toward any
// remaining children too), then starts fragmentating THIS segment.
static void Cat_FragmentateBody_NotifyHead(Sst *self, Sst *parent) {
    parent->status |= 0x80;
    Cat_FragmentateBody(self);
}

static void Cat_Undulate(Sst *self, Own_Caterkiller *own) {
    if ((int8_t)(--own->waittime) >= 0)
        return; // still waiting

    self->routine_secondary += 2; // -> Cat_Floor
    own->waittime = 17 - 1;
    self->x_vel = (int16_t)-0xC0;
    self->ground_vel = 0x40;

    bool was_open = (own->mode & 0x10) != 0;
    own->mode ^= 0x10; // toggle mouth open/moving-up <-> closed/moving-down
    if (!was_open) {
        // just opened -- cancel the leftward nudge above, use the inverted inertia instead
        self->x_vel = 0;
        self->ground_vel = (int16_t)-self->ground_vel;
    }
    own->mode |= 0x80; // request a head sprite update this tick
}

static void Cat_Floor(Sst *self, Own_Caterkiller *own) {
    if ((int8_t)(--own->waittime) < 0) {
        self->routine_secondary -= 2; // -> Cat_Undulate
        own->waittime = 8 - 1;
        self->x_vel = 0;
        self->ground_vel = 0;
        return;
    }

    if (self->x_vel == 0)
        return;

    int32_t prev_full = self->x_pos.v;
    int16_t speed = ((self->status & SST_STATUS_X_FLIP) != 0) ? (int16_t)-self->x_vel : self->x_vel;
    self->x_pos.v += (int32_t)speed << 8;

    if ((int16_t)(prev_full >> 16) == self->x_pos.f.u)
        return; // hasn't moved horizontally

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist < -8 || floor_dist >= 0xC) {
        // hit a wall or a ledge -- turn around
        own->floormap[own->segmentpos] = 0x80;
        self->status ^= SST_STATUS_X_FLIP;
        self->render_flags = self->status;
        own->segmentpos = (own->segmentpos + 1) & 0xF;
        return;
    }
    self->y_pos.f.u += floor_dist;

    own->floormap[own->segmentpos] = (uint8_t)floor_dist;
    own->segmentpos = (own->segmentpos + 1) & 0xF;
}

static void Cat_Head(Sst *self, Own_Caterkiller *own) {
    if (((self->status & 0x80) != 0)) { // a spiked body segment was touched
        Cat_FragmentateBody(self);
        return;
    }

    if (self->routine_secondary == 0)
        Cat_Undulate(self, own);
    else
        Cat_Floor(self, own);

    if (own->mode & 0x80) {
        uint8_t angle = self->angle & 0x7F;
        self->angle += 4;
        int8_t frame = cat_animation[angle];
        if (frame < 0)
            own->mode &= ~0x80; // animation done -- stop requesting updates
        else
            self->mapping_frame = (uint8_t)(frame + (own->mode & 0x10));
    }

    if (SstOutOfRange(self->x_pos.f.u)) {
        Cat_Despawn(self);
        return;
    }
    Sst_Draw(self);
}

// Shared tail for both body segment routines: propagates fragmentation
// both upward (parent already fragmenting) and cleans up after the head
// itself is gone (exploded normally, or about to self-delete offscreen).
static void Cat_BodySeg_ChkBroken(Sst *self, Sst *parent) {
    if (parent->routine == 0xC) { // parent already fragmenting -- so do we
        Cat_FragmentateBody_NotifyHead(self, parent);
        return;
    }
    if (parent->code == Obj_Explosion) { // parent was stomped/rolled into normally
        self->routine = 0xA;
        return; // don't display -- our own child will delete us next
    }
    if (parent->routine == 0xA) { // parent about to self-delete (e.g. went offscreen)
        Sst_Delete(parent); // delete it immediately rather than lingering a frame
        self->routine = 0xA;
        return;
    }
    Sst_Draw(self);
}

// Routine 4, 8 -- first and third body segments. Follows its own parent's
// velocity/inertia and floor-height history (one position-slot behind),
// turning around in lockstep whenever the parent did.
static void Cat_BodySeg1(Sst *self, Own_Caterkiller *own) {
    Sst *parent = &sst[own->parent_index];

    if (((self->status & 0x80) != 0)) { // this segment itself was touched
        Cat_FragmentateBody_NotifyHead(self, parent);
        return;
    }

    Own_Caterkiller *pscratch = (Own_Caterkiller *)parent->own.u8;
    own->mode = pscratch->mode;
    self->routine_secondary = parent->routine_secondary;

    if (self->routine_secondary != 0) {
        self->ground_vel = parent->ground_vel;
        int16_t speed = (int16_t)(parent->x_vel + self->ground_vel);
        self->x_vel = speed;

        int32_t prev_full = self->x_pos.v;
        int16_t applied = ((self->status & SST_STATUS_X_FLIP) != 0) ? (int16_t)-speed : speed;
        self->x_pos.v += (int32_t)applied << 8;

        if ((int16_t)(prev_full >> 16) != self->x_pos.f.u) {
            uint8_t idx = own->segmentpos;
            uint8_t floor = pscratch->floormap[idx];
            if (floor == 0x80) {
                // parent hit a wall/ledge at this position -- follow suit
                own->floormap[idx] = 0x80;
                self->status ^= SST_STATUS_X_FLIP;
                self->render_flags = self->status;
            } else {
                self->y_pos.f.u += (int16_t)(int8_t)floor;
                own->floormap[idx] = floor; // propagate to whatever follows us
            }
            own->segmentpos = (own->segmentpos + 1) & 0xF;
        }
    }

    Cat_BodySeg_ChkBroken(self, parent);
}

// Routine 6 -- second (middle) body segment only: also walks the same
// non-standard animation table as the head (offset +8 for body frames),
// peeking one step ahead so it can pre-emptively skip past the sentinel
// value instead of needing its own end-of-table handling, then falls
// through to the same movement logic as the other two segments.
static void Cat_BodySeg2(Sst *self, Own_Caterkiller *own) {
    Sst *parent = &sst[own->parent_index];
    Own_Caterkiller *pscratch = (Own_Caterkiller *)parent->own.u8;
    own->mode = pscratch->mode;

    if (own->mode & 0x80) {
        uint8_t idx = self->angle & 0x7F;
        self->angle += 4;
        if (cat_animation[(idx + 4) & 0x7F] < 0)
            self->angle += 4;
        self->mapping_frame = (uint8_t)(cat_animation[idx] + 8);
    }

    Cat_BodySeg1(self, own);
}

static void Cat_Fragment(Sst *self) {
    Sst_MoveSprite(self);

    if (self->y_vel < 0) {
        if (SstOutOfRange(self->x_pos.f.u)) {
            Cat_Despawn(self);
            return;
        }
        Sst_Draw(self);
        return;
    }

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist < 0) {
        self->y_pos.f.u += floor_dist;
        self->y_vel = (int16_t)-0x400; // bounce
    }

    if (SstOutOfRange(self->x_pos.f.u)) {
        Cat_Despawn(self);
        return;
    }
    Sst_Draw(self);
}

void Obj_Caterkiller(Sst *self) {
    Own_Caterkiller *own = (Own_Caterkiller *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main -- head only, re-runs every frame until it lands
        self->y_radius = 14 / 2;
        self->x_radius = 16 / 2;

        Sst_MoveSprite(self);
        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist >= 0) {
            // still falling
            if ((uint16_t)self->y_pos.f.u > 0x7FF) // fell below max level height
                Sst_Delete(self);
            break;
        }
        self->y_pos.f.u += floor_dist;
        self->y_vel = 0;
        self->routine += 2;
        self->mappings = Mappings_Caterkiller;
        self->art_tile = (LEVEL_ZONE(level_id) == ZoneId_SBZ)
            ? TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Caterkiller)  // | Tile_Pal2
            : TILE_MAP(0, 1, 0, 0, ArtTile_MZ_SYZ_Caterkiller); // | Tile_Pal2

        self->render_flags &= 0x03; // andi.b #sprite_xflip|sprite_yflip: keep only the spawn-time x/y flip
        self->render_flags |= SST_RENDER_LEVEL;
        self->status = self->render_flags;
        self->priority = 4 * 0x80;
        self->width_pixels = 16 / 2;
        self->collision_flags = 0x0B; // col_16x16 | col_badnik

        int16_t seg_x = self->x_pos.f.u;
        int16_t gap = ((self->status & SST_STATUS_X_FLIP) != 0) ? -12 : 12;

        Sst *parent = self;
        uint8_t routine = 4;
        for (int i = 0; i < 3; i++) {
            Sst *seg = Sst_AllocateAfter(parent);
            if (seg == NULL) {
                // Object RAM full -- despawn the whole Caterkiller (matches
                // REV01; any segments already spawned clean themselves up
                // via Cat_BodySeg_ChkBroken once they see the head's
                // routine go to Cat_Delete).
                Cat_Despawn(self);
                return;
            }
            seg->code = Obj_Caterkiller;
            seg->routine = routine;
            routine += 2;
            seg->mappings = self->mappings;
            seg->art_tile = self->art_tile;
            seg->priority = 5 * 0x80;
            seg->width_pixels = 16 / 2;
            seg->collision_flags = 0xCB; // col_16x16 | col_special
            seg_x = (int16_t)(seg_x + gap);
            seg->x_pos.f.u = seg_x;
            seg->y_pos.f.u = self->y_pos.f.u;
            seg->status = self->status;
            seg->render_flags = self->status;
            seg->mapping_frame = 8; // ".body1" frame

            Own_Caterkiller *sscratch = (Own_Caterkiller *)seg->own.u8;
            sscratch->parent_index = (uint8_t)(parent - sst);
            sscratch->segmentpos = (uint8_t)(4 * (i + 1));

            parent = seg;
        }

        own->waittime = 8 - 1;
        own->segmentpos = 0;
        break;
    }
    case 2:
        Cat_Head(self, own);
        break;
    case 4:
    case 8:
        Cat_BodySeg1(self, own);
        break;
    case 6:
        Cat_BodySeg2(self, own);
        break;
    case 0xA: // Delete
        Sst_Delete(self);
        break;
    case 0xC: // Fragment
        Cat_Fragment(self);
        break;
    }
}
