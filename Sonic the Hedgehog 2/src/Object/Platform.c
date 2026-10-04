// The basic platforms for Sonic 2 (Nick Arcade's object 18, "shima"): Sonic 1's platform object with Nick Arcade's changes: it carries Sonic and Tails, has Emerald Hill's and Hill Top's own mappings (their
// tiles are the level's) and sizes from a table, and lets go of Sonic only (not Tails) when it starts to fall. Subtype: bits 4-7 the size, bits 0-3 the movement (0 and 9 still, 1 right and left, 2 down and up,
// 3 falls half a second after being stood on, 4 falling, 5 left and right, 6 up and down, 7 rises when a button is pressed, 8 rising, $A and $B a gentle up and down, $C and $D a slow one).
#include "Object/Platform.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/PlatformEHZ.h"
#include "Object/BasicPlatform.h" // (Sonic 1's, kept for the mappings of its zones that the debug list names)

typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[3];  // 0x29-0x2B
    dword_s base_y;   // 0x2C: where the platform is, 16.16 (without the sink under a character)
    int16_t base_x;   // 0x32
    int16_t centre_y; // 0x34: where it moves about
    uint8_t sink;     // 0x38: how far down the weight on it has pushed it (a sine angle)
    uint8_t pad1;     // 0x39
    int16_t timer;    // 0x3A
} Scratch_Platform;

// Width and mapping frame by size (Obj18_Conf)
static const uint8_t sizes[5][2] = { { 0x20, 0 }, { 0x20, 1 }, { 0x20, 2 }, { 0x40, 3 }, { 0x30, 4 } };

enum { PlatRoutine_Init = 0, PlatRoutine_Main = 2, PlatRoutine_Delete = 4, PlatRoutine_Falling = 6 };

static void Platform_ChangeMotion(Object *obj) {
    obj->angle = (uint8_t)(oscillatory.state[6][0] >> 8);
}

// The movement of the subtype (sub_8926)
static void Platform_Move(Object *obj, Scratch_Platform *scratch) {
    switch (scratch->subtype & 0xF) {
    case 1:
        obj->pos.l.x.f.u = scratch->base_x + (int8_t)(obj->angle - 0x40);
        Platform_ChangeMotion(obj);
        break;
    case 2:
        scratch->base_y.f.u = scratch->centre_y + (int8_t)(obj->angle - 0x40);
        Platform_ChangeMotion(obj);
        break;
    case 3: // waits for someone to stand on it, then half a second, then falls
        if (scratch->timer == 0) {
            if (obj->status.b & 8)
                scratch->timer = 0x1E;
        } else if (--scratch->timer == 0) {
            scratch->timer = 0x20;
            scratch->subtype++;
        }
        break;
    case 4: // falling: Sonic is let go after a moment (only he is)
        if (scratch->timer != 0 && --scratch->timer == 0) {
            if (obj->status.b & 8) {
                player->status.p.f.in_air = true;
                player->status.p.f.object_stand = false;
                player->routine = 2;
                obj->status.b &= (uint8_t)~8;
                obj->routine_sec = 0;
                player->ysp = obj->ysp;
            }
            obj->routine = PlatRoutine_Falling;
        }
        scratch->base_y.v += (int32_t)obj->ysp << 8;
        obj->ysp += 0x38;
        if ((uint16_t)(limit_btm2 + 0xE0) < (uint16_t)scratch->base_y.f.u)
            obj->routine = PlatRoutine_Delete;
        break;
    case 5:
        obj->pos.l.x.f.u = scratch->base_x + (int8_t)(0x40 - obj->angle);
        Platform_ChangeMotion(obj);
        break;
    case 6:
        scratch->base_y.f.u = scratch->centre_y + (int8_t)(0x40 - obj->angle);
        Platform_ChangeMotion(obj);
        break;
    case 7: // waits for a button, then a second, then rises
        if (scratch->timer == 0) {
            if (f_switch[scratch->subtype >> 4])
                scratch->timer = 0x3C;
        } else if (--scratch->timer == 0) {
            scratch->subtype++;
        }
        break;
    case 8:
        scratch->base_y.f.u -= 2;
        if ((int16_t)(scratch->centre_y - 0x200) == scratch->base_y.f.u)
            scratch->subtype = 0;
        break;
    case 0xA:
        scratch->base_y.f.u = scratch->centre_y + ((int8_t)(obj->angle - 0x40) >> 1);
        Platform_ChangeMotion(obj);
        break;
    case 0xB:
        scratch->base_y.f.u = scratch->centre_y + ((int8_t)(0x40 - obj->angle) >> 1);
        Platform_ChangeMotion(obj);
        break;
    case 0xC:
        scratch->base_y.f.u = scratch->centre_y + (int8_t)((uint8_t)(oscillatory.state[3][0] >> 8) - 0x30);
        Platform_ChangeMotion(obj);
        break;
    case 0xD:
        scratch->base_y.f.u = scratch->centre_y + (int8_t)(0x30 - (uint8_t)(oscillatory.state[3][0] >> 8));
        Platform_ChangeMotion(obj);
        break;
    }
}

// Its height: the base, and a sine of the weight on it (sub_890C)
static void Platform_Sink(Object *obj, Scratch_Platform *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->sink, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)(scratch->base_y.f.u + ((sin * 0x400) >> 16));
}

static void Platform_Display(Object *obj, Scratch_Platform *scratch) {
    if (IS_OFFSCREEN(scratch->base_x)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

void Obj_BasicPlatform(Object *obj) {
    Scratch_Platform *scratch = (Scratch_Platform *)&obj->scratch;

    switch (obj->routine) {
    case PlatRoutine_Init: {
        obj->routine += 2;
        const uint8_t *size = sizes[((scratch->subtype >> 4) & 0xF) % 5];
        obj->width_pixels = size[0];
        obj->frame = size[1];
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level);
        obj->mappings = Mappings_PlatformEHZ; // (the prototype has one mapping for every zone but Neo Green Hill: the tiles of Emerald Hill's level art)
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 4;
        scratch->base_y.f.u = obj->pos.l.y.f.u;
        scratch->base_y.f.l = 0;
        scratch->centre_y = obj->pos.l.y.f.u;
        scratch->base_x = obj->pos.l.x.f.u;
        obj->angle = 0x80;
        scratch->subtype &= 0xF;
    }
        // Fallthrough
    case PlatRoutine_Main: {
        // The weight on it pushes it down, and it springs back
        if (!(obj->status.b & 0x18)) {
            if (scratch->sink != 0)
                scratch->sink -= 4;
        } else if (scratch->sink != 0x40) {
            scratch->sink += 4;
        }

        int16_t x = obj->pos.l.x.f.u;
        Platform_Move(obj, scratch);
        Platform_Sink(obj, scratch);
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) { // (even on the frame it starts to fall)
            Object *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->type != 0 ? TAILS_OBJ : NULL);
            if (chr != NULL)
                Solid_Platform(obj, chr, who, obj->width_pixels, 8, x);
        }
        Platform_Display(obj, scratch);
        break;
    }
    case PlatRoutine_Falling:
        Platform_Move(obj, scratch);
        Platform_Sink(obj, scratch);
        Platform_Display(obj, scratch);
        break;
    case PlatRoutine_Delete:
        ObjectDelete(obj);
        break;
    }
}
