// Chemical Plant's tube network: the prototype's object 1E (Obj_0x1E_Tube_Attributes, loc_16724 in its disassembly). An invisible object at the mouth of a tube: a character who comes near is taken, rolled and
// carried along a path of points inside the tube (a path relative to the object), and at its end either let go or sent on along one of the exit paths (absolute points) to the next tube, which takes him in
// turn. Each character has a state of his own: 0 free, 2 along an entry path, 4 along an exit path, 6 let go (waiting to be out of reach before the object may take him again).
// Subtype: bits 0-1 how far from the object he is taken ($A0, $100 or $120 pixels: the wider ones from the other side), bits 2-5 how he enters (loc_167EC), bits 2-7 which exit paths follow (loc_1693C).
#include "Object/CPZTubes.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Sound.h"

#include "Macros.h"

#include "Object/TubeData.h"
#include "Solid.h"

#include "Resource/Mappings/TubeSpring.h"

typedef struct {
    uint8_t state;
    int8_t side;    // which of the four ways in (0-3, set at the start), then 4 (an exit path follows), or negative ($FC) when an exit path is travelled backwards
    int8_t time;    // frames left on this part of the path
    uint8_t path;   // 0-11 an entry path, 12 and up an exit path (tube_exit[path - 12])
    int16_t count;  // bytes of the path still to travel
    uint16_t pos;   // the word of the path the next point is at
} TubeChar;

typedef struct {
    uint8_t subtype;
    uint8_t pad;
    uint16_t range; // $2A: how close he has to be along x to be taken
    TubeChar chr[2];
} Scratch_Tube;

#define TUBE_SPEED 0x800

static const uint16_t *PathData(uint8_t path) {
    return path < 12 ? tube_enter[path] : tube_exit[path - 12];
}

// loc_16A80: sets the character's speed to go from where he is to (tx, ty) at TUBE_SPEED along the longer axis, and how many frames it takes
static void TubeAim(Object *chr, TubeChar *tc, int16_t tx, int16_t ty) {
    int16_t sx = TUBE_SPEED, sy = TUBE_SPEED;
    int16_t dx = (int16_t)(tx - chr->pos.l.x.f.u), dy = (int16_t)(ty - chr->pos.l.y.f.u);
    int16_t ax = dx, ay = dy;
    if (dx < 0) {
        ax = (int16_t)-dx;
        sx = (int16_t)-sx;
    }
    if (dy < 0) {
        ay = (int16_t)-dy;
        sy = (int16_t)-sy;
    }
    if ((uint16_t)ay < (uint16_t)ax) { // along x
        int16_t frames = (int16_t)(((int32_t)dx * 65536) / sx);
        int16_t vy = 0;
        if (dy != 0 && frames != 0)
            vy = (int16_t)(((int32_t)dy * 65536) / frames);
        chr->ysp = vy;
        chr->xsp = sx;
        tc->time = (int8_t)((uint16_t)(frames < 0 ? -frames : frames) >> 8);
    } else { // along y
        int16_t frames = (int16_t)(((int32_t)dy * 65536) / sy);
        int16_t vx = 0;
        if (dx != 0 && frames != 0)
            vx = (int16_t)(((int32_t)dx * 65536) / frames);
        chr->xsp = vx;
        chr->ysp = sy;
        tc->time = (int8_t)((uint16_t)(frames < 0 ? -frames : frames) >> 8);
    }
}

// The character goes on to the point at the path's current word (plus the object's place, for an entry path) and takes the speed to get there (loc_16860's end)
static void TubeStart(Object *obj, Object *chr, TubeChar *tc, int16_t tx, int16_t ty) {
    obj->status.b &= (uint8_t)~0x20;
    chr->status.p.f.pushing = false;
    chr->status.p.f.in_air = true;
    ((Scratch_Sonic *)&chr->scratch)->jumping = 0;
    chr->tile &= (uint16_t)~TILE_PRIORITY_AND;
    chr->xsp = 0;
    chr->ysp = 0;
    TubeAim(chr, tc, tx, ty);
}

// The character moves by his speed (loc_168DC)
static void TubeMove(Object *chr) {
    chr->pos.l.x.v += chr->xsp * 256;
    chr->pos.l.y.v += chr->ysp * 256;
}

// An exit path starts (loc_16A10): a positive number goes forward along path n of the exit table, a negative one backwards along path -n
static void TubeExitStart(Object *chr, TubeChar *tc, int n) {
    const bool back = n < 0;
    if (back)
        n = -n;
    tc->path = (uint8_t)(12 + n); // (the prototype's exit table index n as it is: its entries 0 and 1 are the same path)
    const uint16_t *p = PathData(tc->path);
    tc->side = back ? (int8_t)0xFC : tc->side;
    tc->count = (int16_t)(p[0] - 4);
    if (back) {
        const uint16_t *last = p + 1 + (p[0] >> 1) - 2; // the last point
        chr->pos.l.x.f.u = (int16_t)last[0];
        chr->pos.l.y.f.u = (int16_t)last[1];
        tc->pos = (uint16_t)(last - p - 2); // (the one before it)
    } else {
        chr->pos.l.x.f.u = (int16_t)p[1];
        chr->pos.l.y.f.u = (int16_t)p[2];
        tc->pos = 3;
    }
    p = PathData(tc->path);
    TubeAim(chr, tc, (int16_t)p[tc->pos], (int16_t)p[tc->pos + 1]);
    PlaySound(sfx_Roll); // ($BE: his spin sound as he is taken in and as he turns on to an exit path)
    tc->state += 2;
}

// One character against one tube (loc_16770): his state picks what happens
static void TubeStep(Object *obj, Scratch_Tube *scratch, Object *chr, TubeChar *tc) {
    switch (tc->state) {
    case 0: { // free: taken when he is near the mouth
        if (debug_use)
            return;
        int16_t range = (int16_t)scratch->range;
        int16_t d0 = (int16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u);
        if ((uint16_t)d0 >= (uint16_t)range)
            return;
        int16_t d1 = (int16_t)(chr->pos.l.y.f.u - obj->pos.l.y.f.u);
        if ((uint16_t)d1 >= 0x80)
            return;
        int d3 = 0;
        if (range != 0xA0) {
            d3 = 8;
            if (range != 0x120) {
                d3 = 4;
                d0 = (int16_t)(-d0 + 0x100);
            }
        }
        int d2;
        if ((uint16_t)d0 >= 0x80) {
            d2 = tube_side_by_subtype[(scratch->subtype >> 2) & 0xF];
            if (d2 == 2)
                d2 = frame_count & 1;
        } else {
            d2 = 2;
            if ((uint16_t)d1 < 0x40)
                d2 = 3;
        }
        tc->side = (int8_t)d2;
        tc->path = (uint8_t)((d2 + d3) & 0xF);
        if (tc->path >= 12)
            return; // (the prototype's table only has twelve: the rest is not an entry)
        const uint16_t *p = PathData(tc->path);
        tc->count = (int16_t)(p[0] - 4);
        chr->pos.l.x.f.u = (int16_t)(p[1] + obj->pos.l.x.f.u);
        chr->pos.l.y.f.u = (int16_t)(p[2] + obj->pos.l.y.f.u);
        tc->pos = 3;
        tc->state += 2;
        OBJ_CONTROL(chr) = 0x81;
        chr->anim = SonAnimId_Roll;
        chr->inertia = TUBE_SPEED;
        TubeStart(obj, chr, tc, (int16_t)(p[3] + obj->pos.l.x.f.u), (int16_t)(p[4] + obj->pos.l.y.f.u));
        PlaySound(sfx_Roll); // ($BE: his spin sound as he is taken in and as he turns on to an exit path)
        return;
    }
    case 2: { // along an entry path (points relative to the object)
        if (--tc->time >= 0) {
            TubeMove(chr);
            return;
        }
        const uint16_t *p = PathData(tc->path);
        chr->pos.l.x.f.u = (int16_t)(p[tc->pos] + obj->pos.l.x.f.u);
        chr->pos.l.y.f.u = (int16_t)(p[tc->pos + 1] + obj->pos.l.y.f.u);
        tc->pos += tc->side < 0 ? -2 : 2;
        tc->count -= 4;
        if (tc->count == 0) { // the end of the path (loc_16902)
            if ((uint8_t)tc->side < 4) {
                int row = (scratch->subtype & 0xFC) + tc->side;
                tc->side = 4;
                int n = tube_exit_by_subtype[row];
                if (n != 0) {
                    TubeExitStart(chr, tc, n);
                    return;
                }
            }
            chr->pos.l.y.f.u &= 0x07FF;
            tc->state = 6;
            OBJ_CONTROL(chr) = 0;
            PlaySound(sfx_Teleport); // ($BC: the sound of the spin dash's release, as he leaves the tube)
            return;
        }
        TubeAim(chr, tc, (int16_t)(p[tc->pos] + obj->pos.l.x.f.u), (int16_t)(p[tc->pos + 1] + obj->pos.l.y.f.u));
        return;
    }
    case 4: { // along an exit path (absolute points)
        if (--tc->time >= 0) {
            TubeMove(chr);
            return;
        }
        const uint16_t *p = PathData(tc->path);
        chr->pos.l.x.f.u = (int16_t)p[tc->pos];
        chr->pos.l.y.f.u = (int16_t)p[tc->pos + 1];
        tc->pos += (tc->side < 0) ? -2 : 2;
        tc->count -= 4;
        if (tc->count == 0) {
            chr->pos.l.y.f.u &= 0x07FF;
            tc->state = 0;
            PlaySound(sfx_Teleport); // ($BC: the sound of the spin dash's release, as he leaves the tube)
            return;
        }
        TubeAim(chr, tc, (int16_t)p[tc->pos], (int16_t)p[tc->pos + 1]);
        return;
    }
    case 6: { // let go: waits until he is out of reach
        int16_t d0 = (int16_t)(chr->pos.l.x.f.u - obj->pos.l.x.f.u);
        int16_t d1 = (int16_t)(chr->pos.l.y.f.u - obj->pos.l.y.f.u);
        if ((uint16_t)d0 < scratch->range && (uint16_t)d1 < 0x80)
            return;
        tc->state = 0;
        return;
    }
    }
}

void Obj_CPZTube(Object *obj) {
    Scratch_Tube *scratch = (Scratch_Tube *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        static const uint16_t ranges[4] = { 0xA0, 0x100, 0x120, 0xA0 };
        scratch->range = ranges[(scratch->subtype << 1 & 6) >> 1];
    }
    TubeStep(obj, scratch, player, &scratch->chr[0]);
    Object *tails = TAILS_OBJ;
    if (tails->type != 0)
        TubeStep(obj, scratch, tails, &scratch->chr[1]);

    // MarkObjGone3: once neither character is in it, it goes when out of range, and forgets that it was loaded (the tubes remember their state in the layout:
    // a tube deleted with its mark still set never came back, and a character whose exit path ended at its mouth was left frozen there)
    if (scratch->chr[0].state == 0 && scratch->chr[1].state == 0 && IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        ObjectDelete(obj);
    }
}

// The prototype's object 7B (Obj_0x7B_Spring_Tubes, loc_1D74C): the spring at the mouth of a tube that throws up the character who stands on it (loc_1D862: like the other springs, $1000 or, for the weaker one,
// $A00, tumbling by the subtype), and opens (animations 2 and 3, one for each character) while one of them is in the box above the tube. Subtype: as the springs' (bit 0 tumble, 1 weaker, 2-3 path, 7 stop sideways).
#define ARTTILE_TUBE_SPRING 0x3E0 // ($7C00 in the zones' art lists)

static const uint8_t Animation_TubeSpring[] = {
    0x00, 0x08, 0x00, 0x0B, 0x00, 0x0F, 0x00, 0x0F,
    0x0F, 0x00, 0xFF,
    0x00, 0x03, 0xFD, 0x00,
    0x05, 0x01, 0x02, 0x02, 0x02, 0x04, 0xFD, 0x00, 0x00,
};

typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    int16_t power;
} Scratch_TubeSpring;

static void TubeSpring_Launch(Object *obj, Object *chr, int16_t power, uint8_t subtype) {
    Scratch_Sonic *sscratch = (Scratch_Sonic *)&chr->scratch;

    obj->anim = 1;
    obj->prev_anim = 0;
    chr->pos.l.y.f.u += 4;
    chr->ysp = power;
    chr->status.p.f.in_air = true;
    chr->status.p.f.object_stand = false;
    chr->anim = SonAnimId_Spring;
    chr->routine = 2;
    if (subtype & 0x80)
        chr->xsp = 0;
    if (subtype & 1) {
        chr->inertia = 1;
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = 0;
        sscratch->flip_speed = 4;
        if (!(subtype & 2))
            sscratch->flips_remaining = 1;
        if (chr->status.p.f.x_flip) {
            sscratch->flip_angle = (uint8_t)-sscratch->flip_angle;
            chr->inertia = -chr->inertia;
        }
    }
    switch (subtype & 0xC) {
    case 4:
        sscratch->top_solid_bit = 0xC;
        sscratch->lrb_solid_bit = 0xD;
        break;
    case 8:
        sscratch->top_solid_bit = 0xE;
        sscratch->lrb_solid_bit = 0xF;
        break;
    }
    PlaySound(sfx_Spring);
}

void Obj_CPZTubeSpring(Object *obj) {
    Scratch_TubeSpring *scratch = (Scratch_TubeSpring *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_TubeSpring;
        obj->tile = TILE_MAP(0, 0, 0, 0, ARTTILE_TUBE_SPRING);
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x10;
        obj->priority = 1;
        scratch->power = (scratch->subtype & 2) ? -0xA00 : -0x1000;
    }

    if (obj->frame != 1) { // (shut while it is open)
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
            Object *chr = who == SolidChar_Sonic ? player : TAILS_OBJ;
            if (who == SolidChar_Tails && chr->type == 0)
                continue;
            Solid_Character(obj, chr, who, 0x1B, 8, 0x10, obj->pos.l.x.f.u, NULL);
            if (obj->status.b & (1 << (3 + who)))
                TubeSpring_Launch(obj, chr, scratch->power, scratch->subtype);
        }
    }

    // It opens while a character is in the box over the tube: animation 2 for Sonic, 3 for Tails
    const uint16_t x0 = (uint16_t)(obj->pos.l.x.f.u - 0x10), x1 = (uint16_t)(obj->pos.l.x.f.u + 0x10);
    const uint16_t y0 = (uint16_t)obj->pos.l.y.f.u, y1 = (uint16_t)(obj->pos.l.y.f.u + 0x30);
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Object *chr = who == SolidChar_Sonic ? player : TAILS_OBJ;
        if (who == SolidChar_Tails && chr->type == 0)
            continue;
        uint16_t cx = (uint16_t)chr->pos.l.x.f.u, cy = (uint16_t)chr->pos.l.y.f.u;
        if (cx >= x0 && cx < x1 && cy >= y0 && cy < y1)
            obj->anim = (uint8_t)(2 + who);
    }
    AnimateSprite(obj, Animation_TubeSpring);

    RememberState(obj); // (MarkObjGone: drawn, or gone when out of range, letting it come back)
}
