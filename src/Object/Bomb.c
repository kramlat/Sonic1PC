#include "Bomb.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Bomb.h"
#include "Resource/Mappings/Bomb.h"
#include "Sound.h"

// Object 5F - Walking Bomb enemy (SLZ, SBZ)

static const int16_t Bom_ShrSpeed[4][2] = {
    { -0x200, -0x300 }, // 1st shrapnel
    { -0x100, -0x200 }, // 2nd shrapnel
    {  0x200, -0x300 }, // 3rd shrapnel
    {  0x100, -0x200 }, // 4th shrapnel
};

static void Bom_Shrapnel(Object *obj) {
    SpeedToPos(obj);
    obj->ysp += 0x18;

    AnimateSprite(obj, Animation_Bomb);

    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// Advances the fuse timer and, once it expires, turns this object (plus 3
// more freshly-spawned objects) into flying shrapnel. Returns true once the
// fuse has expired and shrapnel has already been fully handled (displayed
// or deleted) for this frame -- the caller must not also animate/display it
// again as a fuse.
static bool Bom_BurnFuseAndExplode(Object *obj, Scratch_Bomb *scratch) {
    if (--scratch->time >= 0) {
        SpeedToPos(obj);
        return false;
    }

    scratch->time = 0;       // redundant, matches real ASM's own redundant clear
    obj->routine = 0;        // ditto
    obj->pos.l.y.f.u = scratch->orig_y;

    Object *seg;
    for (int i = 0; i < 4; i++) {
        if (i == 0) {
            seg = obj; // the fuse object itself becomes the first shrapnel
        } else {
            seg = FindNextFreeObj(obj);
            if (seg == NULL)
                continue;
        }
        seg->type = ObjId_Bomb;
        seg->pos.l.x.f.u = obj->pos.l.x.f.u;
        seg->pos.l.y.f.u = obj->pos.l.y.f.u;
        seg->scratch.u8[0] = 6; // subtype -> Bom_Shrapnel
        seg->anim = 4;
        seg->xsp = Bom_ShrSpeed[i][0];
        seg->ysp = Bom_ShrSpeed[i][1];
        seg->col_type = 0x18 | 0x80; // col_8x8 | col_hurt
        seg->render.f.on_screen = true; // don't get deleted by the offscreen check below before ever being drawn
    }

    obj->routine = 6; // Bom_Shrapnel
    Bom_Shrapnel(obj);
    return true;
}

// Checks if Sonic is close enough to start the fuse, and if so, spawns a
// separate fuse object that burns down independently of this Bomb.
static void Bom_CheckStartFuse(Object *obj, Scratch_Bomb *scratch) {
    int16_t dx = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (dx < 0) dx = (int16_t)-dx;
    if ((uint16_t)dx >= 96)
        return;

    int16_t dy = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u);
    if (dy < 0) dy = (int16_t)-dy;
    if ((uint16_t)dy >= 96)
        return;

    if (debug_use)
        return;

    obj->routine_sec = 4; // Bom_Action_WaitAndExplode
    scratch->time = (2 * 60) + 24 - 1;
    obj->xsp = 0;
    obj->anim = 2;

    Object *fuse = FindNextFreeObj(obj);
    if (fuse == NULL)
        return;
    fuse->type = ObjId_Bomb;
    fuse->pos.l.x.f.u = obj->pos.l.x.f.u;
    fuse->pos.l.y.f.u = obj->pos.l.y.f.u;
    Scratch_Bomb *fscratch = (Scratch_Bomb *)&fuse->scratch;
    fscratch->orig_y = obj->pos.l.y.f.u;
    fuse->status.b = obj->status.b;
    fuse->scratch.u8[0] = 4; // subtype -> Bom_Fuse routine directly
    fuse->anim = 3;

    fuse->ysp = 0x10;
    if (obj->status.o.f.y_flip) // is bomb upside-down?
        fuse->ysp = (int16_t)-fuse->ysp;

    fscratch->time = (2 * 60) + 24 - 1;
    // real ASM also stores a parent-object pointer here (bom_parent), but
    // it's set and never read anywhere -- not worth carrying over.
}

static void Bom_Action_Waiting(Object *obj, Scratch_Bomb *scratch) {
    Bom_CheckStartFuse(obj, scratch);

    if (--scratch->time >= 0)
        return;
    obj->routine_sec = 2; // Bom_Action_Walking
    scratch->time = (25 * 60) + 36 - 1;
    obj->xsp = 0x10;
    obj->anim = 1;
    obj->status.o.f.x_flip = !obj->status.o.f.x_flip;
    if (obj->status.o.f.x_flip)
        obj->xsp = (int16_t)-obj->xsp;
}

static void Bom_Action_Walking(Object *obj, Scratch_Bomb *scratch) {
    Bom_CheckStartFuse(obj, scratch);

    if (--scratch->time < 0) {
        obj->routine_sec = 0; // Bom_Action_Waiting
        scratch->time = (3 * 60) - 1;
        obj->xsp = 0;
        obj->anim = 0;
        return;
    }
    SpeedToPos(obj);
}

static void Bom_Action_WaitAndExplode(Object *obj, Scratch_Bomb *scratch) {
    if (--scratch->time >= 0)
        return;
    obj->type = ObjId_ExplosionBomb; // real Object 3F "Explosion" (Map_ExplodeBomb + sfx_Bomb); the actual shrapnel burst comes from the separate fuse object's own Bom_BurnFuseAndExplode
    obj->routine = 0;
}

static void Bom_Action(Object *obj, Scratch_Bomb *scratch) {
    switch (obj->routine_sec) {
    case 0: Bom_Action_Waiting(obj, scratch); break;
    case 2: Bom_Action_Walking(obj, scratch); break;
    case 4: Bom_Action_WaitAndExplode(obj, scratch); break;
    }

    AnimateSprite(obj, Animation_Bomb);
    RememberState(obj);
}

static void Bom_Main(Object *obj, Scratch_Bomb *scratch) {
    obj->routine = 2; // advance to Bom_Action
    obj->mappings = Mappings_Bomb;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x400); // ArtTile_Bomb
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;
    obj->width_pixels = 24 / 2;

    uint8_t subtype = obj->scratch.u8[0]; // 0 = normal badnik, 4 = fuse, 6 = shrapnel
    if (subtype != 0) {
        obj->routine = subtype; // directly set alternate routine, run on next call
        return;
    }

    obj->col_type = 0x1A | 0x80; // col_24x24 | col_hurt
    obj->status.o.f.x_flip = !obj->status.o.f.x_flip; // face right by default
}

void Obj_Bomb(Object *obj) {
    Scratch_Bomb *scratch = (Scratch_Bomb *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        Bom_Main(obj, scratch);
        break;
    case 2:
        Bom_Action(obj, scratch);
        break;
    case 4:
        if (Bom_BurnFuseAndExplode(obj, scratch))
            return;
        AnimateSprite(obj, Animation_Bomb);
        RememberState(obj);
        break;
    case 6:
        Bom_Shrapnel(obj);
        break;
    }
}
