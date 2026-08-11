#include "Caterkiller.h"

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
// table indexed directly by (obj->angle & 0x7F), NOT the standard
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
// RememberState's own offscreen cleanup (see Object.c).
static void Cat_Despawn(Object *obj) {
    if (obj->respawn_index)
        objstate[obj->respawn_index] &= 0x7F;
    obj->routine = 0xA;
}

static void Cat_FragmentateBody(Object *obj) {
    uint8_t index = (uint8_t)((obj->routine - 2) / 2);
    int16_t speed = cat_frag_speed[index];
    if (obj->status.o.f.x_flip)
        speed = (int16_t)-speed;
    obj->xsp = speed;
    obj->ysp = (int16_t)-0x400;
    obj->routine = 0xC;
    obj->frame &= 0xF8; // reset to frame 0 of this segment's own 8-frame set
}

// Notifies this segment's own parent that the whole Caterkiller should
// break apart (bubbling the break both upward, toward the head, and --
// via each segment's own .chkBroken check below -- downward toward any
// remaining children too), then starts fragmentating THIS segment.
static void Cat_FragmentateBody_NotifyHead(Object *obj, Object *parent) {
    parent->status.o.f.flag7 = true;
    Cat_FragmentateBody(obj);
}

static void Cat_Undulate(Object *obj, Scratch_Caterkiller *scratch) {
    if ((int8_t)(--scratch->waittime) >= 0)
        return; // still waiting

    obj->routine_sec += 2; // -> Cat_Floor
    scratch->waittime = 17 - 1;
    obj->xsp = (int16_t)-0xC0;
    obj->inertia = 0x40;

    bool was_open = (scratch->mode & 0x10) != 0;
    scratch->mode ^= 0x10; // toggle mouth open/moving-up <-> closed/moving-down
    if (!was_open) {
        // just opened -- cancel the leftward nudge above, use the inverted inertia instead
        obj->xsp = 0;
        obj->inertia = (int16_t)-obj->inertia;
    }
    scratch->mode |= 0x80; // request a head sprite update this tick
}

static void Cat_Floor(Object *obj, Scratch_Caterkiller *scratch) {
    if ((int8_t)(--scratch->waittime) < 0) {
        obj->routine_sec -= 2; // -> Cat_Undulate
        scratch->waittime = 8 - 1;
        obj->xsp = 0;
        obj->inertia = 0;
        return;
    }

    if (obj->xsp == 0)
        return;

    int32_t prev_full = obj->pos.l.x.v;
    int16_t speed = obj->status.o.f.x_flip ? (int16_t)-obj->xsp : obj->xsp;
    obj->pos.l.x.v += (int32_t)speed << 8;

    if ((int16_t)(prev_full >> 16) == obj->pos.l.x.f.u)
        return; // hasn't moved horizontally

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist < -8 || floor_dist >= 0xC) {
        // hit a wall or a ledge -- turn around
        scratch->floormap[scratch->segmentpos] = 0x80;
        obj->status.o.f.x_flip ^= 1;
        obj->render.b = obj->status.b;
        scratch->segmentpos = (scratch->segmentpos + 1) & 0xF;
        return;
    }
    obj->pos.l.y.f.u += floor_dist;

    scratch->floormap[scratch->segmentpos] = (uint8_t)floor_dist;
    scratch->segmentpos = (scratch->segmentpos + 1) & 0xF;
}

static void Cat_Head(Object *obj, Scratch_Caterkiller *scratch) {
    if (obj->status.o.f.flag7) { // a spiked body segment was touched
        Cat_FragmentateBody(obj);
        return;
    }

    if (obj->routine_sec == 0)
        Cat_Undulate(obj, scratch);
    else
        Cat_Floor(obj, scratch);

    if (scratch->mode & 0x80) {
        uint8_t angle = obj->angle & 0x7F;
        obj->angle += 4;
        int8_t frame = cat_animation[angle];
        if (frame < 0)
            scratch->mode &= ~0x80; // animation done -- stop requesting updates
        else
            obj->frame = (uint8_t)(frame + (scratch->mode & 0x10));
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        Cat_Despawn(obj);
        return;
    }
    DisplaySprite(obj);
}

// Shared tail for both body segment routines: propagates fragmentation
// both upward (parent already fragmenting) and cleans up after the head
// itself is gone (exploded normally, or about to self-delete offscreen).
static void Cat_BodySeg_ChkBroken(Object *obj, Object *parent) {
    if (parent->routine == 0xC) { // parent already fragmenting -- so do we
        Cat_FragmentateBody_NotifyHead(obj, parent);
        return;
    }
    if (parent->type == ObjId_Explosion) { // parent was stomped/rolled into normally
        obj->routine = 0xA;
        return; // don't display -- our own child will delete us next
    }
    if (parent->routine == 0xA) { // parent about to self-delete (e.g. went offscreen)
        ObjectDelete(parent); // delete it immediately rather than lingering a frame
        obj->routine = 0xA;
        return;
    }
    DisplaySprite(obj);
}

// Routine 4, 8 -- first and third body segments. Follows its own parent's
// velocity/inertia and floor-height history (one position-slot behind),
// turning around in lockstep whenever the parent did.
static void Cat_BodySeg1(Object *obj, Scratch_Caterkiller *scratch) {
    Object *parent = &objects[scratch->parent_index];

    if (obj->status.o.f.flag7) { // this segment itself was touched
        Cat_FragmentateBody_NotifyHead(obj, parent);
        return;
    }

    Scratch_Caterkiller *pscratch = (Scratch_Caterkiller *)&parent->scratch;
    scratch->mode = pscratch->mode;
    obj->routine_sec = parent->routine_sec;

    if (obj->routine_sec != 0) {
        obj->inertia = parent->inertia;
        int16_t speed = (int16_t)(parent->xsp + obj->inertia);
        obj->xsp = speed;

        int32_t prev_full = obj->pos.l.x.v;
        int16_t applied = obj->status.o.f.x_flip ? (int16_t)-speed : speed;
        obj->pos.l.x.v += (int32_t)applied << 8;

        if ((int16_t)(prev_full >> 16) != obj->pos.l.x.f.u) {
            uint8_t idx = scratch->segmentpos;
            uint8_t floor = pscratch->floormap[idx];
            if (floor == 0x80) {
                // parent hit a wall/ledge at this position -- follow suit
                scratch->floormap[idx] = 0x80;
                obj->status.o.f.x_flip ^= 1;
                obj->render.b = obj->status.b;
            } else {
                obj->pos.l.y.f.u += (int16_t)(int8_t)floor;
                scratch->floormap[idx] = floor; // propagate to whatever follows us
            }
            scratch->segmentpos = (scratch->segmentpos + 1) & 0xF;
        }
    }

    Cat_BodySeg_ChkBroken(obj, parent);
}

// Routine 6 -- second (middle) body segment only: also walks the same
// non-standard animation table as the head (offset +8 for body frames),
// peeking one step ahead so it can pre-emptively skip past the sentinel
// value instead of needing its own end-of-table handling, then falls
// through to the same movement logic as the other two segments.
static void Cat_BodySeg2(Object *obj, Scratch_Caterkiller *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_Caterkiller *pscratch = (Scratch_Caterkiller *)&parent->scratch;
    scratch->mode = pscratch->mode;

    if (scratch->mode & 0x80) {
        uint8_t idx = obj->angle & 0x7F;
        obj->angle += 4;
        if (cat_animation[(idx + 4) & 0x7F] < 0)
            obj->angle += 4;
        obj->frame = (uint8_t)(cat_animation[idx] + 8);
    }

    Cat_BodySeg1(obj, scratch);
}

static void Cat_Fragment(Object *obj) {
    ObjectFall(obj);

    if (obj->ysp < 0) {
        if (!obj->render.f.on_screen) {
            Cat_Despawn(obj);
            return;
        }
        DisplaySprite(obj);
        return;
    }

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist < 0) {
        obj->pos.l.y.f.u += floor_dist;
        obj->ysp = (int16_t)-0x400; // bounce
    }

    if (!obj->render.f.on_screen) {
        Cat_Despawn(obj);
        return;
    }
    DisplaySprite(obj);
}

void Obj_Caterkiller(Object *obj) {
    Scratch_Caterkiller *scratch = (Scratch_Caterkiller *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main -- head only, re-runs every frame until it lands
        obj->y_rad = 14 / 2;
        obj->x_rad = 16 / 2;

        ObjectFall(obj);
        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist >= 0) {
            // still falling
            if ((uint16_t)obj->pos.l.y.f.u > 0x7FF) // fell below max level height
                ObjectDelete(obj);
            break;
        }
        obj->pos.l.y.f.u += floor_dist;
        obj->ysp = 0;
        obj->routine += 2;
        obj->mappings = Mappings_Caterkiller;
        obj->tile = (LEVEL_ZONE(level_id) == ZoneId_SBZ)
            ? TILE_MAP(0, 1, 0, 0, 0x2B0)  // ArtTile_SBZ_Caterkiller | Tile_Pal2
            : TILE_MAP(0, 1, 0, 0, 0x4FF); // ArtTile_MZ_SYZ_Caterkiller | Tile_Pal2

        obj->render.b &= 0x03; // keep only the spawn-time x/y flip
        obj->render.f.align_fg = true;
        obj->status.b = obj->render.b;
        obj->priority = 4;
        obj->width_pixels = 16 / 2;
        obj->col_type = 0x0B; // col_16x16 | col_badnik

        int16_t seg_x = obj->pos.l.x.f.u;
        int16_t gap = obj->status.o.f.x_flip ? -12 : 12;

        Object *parent = obj;
        uint8_t routine = 4;
        for (int i = 0; i < 3; i++) {
            Object *seg = FindNextFreeObj(parent);
            if (seg == NULL) {
                // Object RAM full -- despawn the whole Caterkiller (matches
                // REV01; any segments already spawned clean themselves up
                // via Cat_BodySeg_ChkBroken once they see the head's
                // routine go to Cat_Delete).
                Cat_Despawn(obj);
                return;
            }
            seg->type = ObjId_Caterkiller;
            seg->routine = routine;
            routine += 2;
            seg->mappings = obj->mappings;
            seg->tile = obj->tile;
            seg->priority = 5;
            seg->width_pixels = 16 / 2;
            seg->col_type = 0xCB; // col_16x16 | col_special
            seg_x = (int16_t)(seg_x + gap);
            seg->pos.l.x.f.u = seg_x;
            seg->pos.l.y.f.u = obj->pos.l.y.f.u;
            seg->status.b = obj->status.b;
            seg->render.b = obj->status.b;
            seg->frame = 8; // ".body1" frame

            Scratch_Caterkiller *sscratch = (Scratch_Caterkiller *)&seg->scratch;
            sscratch->parent_index = (uint8_t)(parent - objects);
            sscratch->segmentpos = (uint8_t)(4 * (i + 1));

            parent = seg;
        }

        scratch->waittime = 8 - 1;
        scratch->segmentpos = 0;
        break;
    }
    case 2:
        Cat_Head(obj, scratch);
        break;
    case 4:
    case 8:
        Cat_BodySeg1(obj, scratch);
        break;
    case 6:
        Cat_BodySeg2(obj, scratch);
        break;
    case 0xA: // Delete
        ObjectDelete(obj);
        break;
    case 0xC: // Fragment
        Cat_Fragment(obj);
        break;
    }
}
