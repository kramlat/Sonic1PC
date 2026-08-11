#include "BasicPlatform.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"

// Object 18 - basic platforms (GHZ, SYZ, SLZ)

static void Plat_Nudge(Object *obj, Scratch_BasicPlatform *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->nudge, &sin, &cos);
    // Depress by at most 4px -- amplitude $400 in 16.16, matches the 4px
    // real hardware comment (asl.w #8 equivalent baked into the multiply).
    obj->pos.l.y.f.u = (int16_t)(scratch->raw_y.f.u + ((sin * 0x400) >> 16));
}

// Refreshes the shared (all-platforms-in-lockstep) triangle-wave motion
// value into this platform's own angle field, matching the real driver's
// Plat_ChangeMotion (frequency 8, middle $40 -- v_oscillate+$1A/state[6]).
static void Plat_ChangeMotion(Object *obj) {
    obj->angle = (uint8_t)(oscillatory.state[6][0] >> 8);
}

static void Plat_MoveHorizontal(Object *obj, Scratch_BasicPlatform *scratch, int8_t delta) {
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + delta);
    Plat_ChangeMotion(obj);
}

static void Plat_MoveVertical(Object *obj, Scratch_BasicPlatform *scratch, int8_t delta) {
    scratch->raw_y.f.u = (int16_t)(scratch->orig_y + delta);
    Plat_ChangeMotion(obj);
}

static void Plat_FallAfterStand(Object *obj, Scratch_BasicPlatform *scratch) {
    if (scratch->delay != 0) {
        if (--scratch->delay == 0) {
            scratch->delay = 32; // keep Sonic attached to the platform for 32 frames after it starts falling
            scratch->subtype++;  // advance to Falling
        }
        return;
    }
    if (obj->status.o.f.player_stand)
        scratch->delay = 30; // half a second before falling
}

static void Plat_FallingDown(Object *obj, Scratch_BasicPlatform *scratch) {
    if (scratch->delay != 0) {
        if (--scratch->delay == 0) {
            if (obj->status.o.f.player_stand) {
                player->status.p.f.in_air = true;
                player->status.p.f.object_stand = false;
                player->routine = 2; // force Sonic to Sonic_Control
                obj->status.o.f.player_stand = false;
                player->ysp = obj->ysp;
            }
            obj->routine = 8; // Plat_Action -- no longer calls PlatformObject, so it's no longer solid; also blocks re-entry
        }
    }

    scratch->raw_y.v += (int32_t)obj->ysp << 8;
    obj->ysp += 0x38; // gravity

    if (limit_btm2 + SCREEN_HEIGHT >= scratch->raw_y.f.u)
        return;
    obj->routine = 6; // Plat_Delete
}

static void Plat_RiseOnSwitch(Scratch_BasicPlatform *scratch) {
    if (scratch->delay != 0) {
        if (--scratch->delay == 0)
            scratch->subtype++; // advance to Rising
        return;
    }
    if (f_switch[scratch->subtype >> 4])
        scratch->delay = 1 * 60;
}

static void Plat_Rising(Scratch_BasicPlatform *scratch) {
    scratch->raw_y.f.u -= 2;
    if (scratch->orig_y - 0x200 != scratch->raw_y.f.u)
        return;
    scratch->subtype = 0; // stationary
}

static void Plat_DownUp_LargeGHZ2(Object *obj, Scratch_BasicPlatform *scratch) {
    int8_t delta = (int8_t)(obj->angle - 0x40);
    scratch->raw_y.f.u = (int16_t)(scratch->orig_y + (delta >> 1)); // asr.w #1 -- half range
    Plat_ChangeMotion(obj);
}

// Executes movement behavior for the platform's own subtype (low nibble)
static void Plat_Move(Object *obj, Scratch_BasicPlatform *scratch) {
    switch (scratch->subtype & 0xF) {
    case 0x0: // stationary
    case 0x9:
        break;
    case 0x1: // right-left
        Plat_MoveHorizontal(obj, scratch, (int8_t)(obj->angle - 0x40));
        break;
    case 0x2: // down-up
        Plat_MoveVertical(obj, scratch, (int8_t)(obj->angle - 0x40));
        break;
    case 0x3:
        Plat_FallAfterStand(obj, scratch);
        break;
    case 0x4:
        Plat_FallingDown(obj, scratch);
        break;
    case 0x5: // left-right
        Plat_MoveHorizontal(obj, scratch, (int8_t)(0x40 - obj->angle));
        break;
    case 0x6: // up-down
        Plat_MoveVertical(obj, scratch, (int8_t)(0x40 - obj->angle));
        break;
    case 0x7:
        Plat_RiseOnSwitch(scratch);
        break;
    case 0x8:
        Plat_Rising(scratch);
        break;
    case 0xA:
        Plat_DownUp_LargeGHZ2(obj, scratch);
        break;
    case 0xB: { // down-up, slow (frequency 2, middle $30 -- v_oscillate+$E/state[3])
        int8_t delta = (int8_t)((uint8_t)(oscillatory.state[3][0] >> 8) - 0x30);
        scratch->raw_y.f.u = (int16_t)(scratch->orig_y + delta);
        break;
    }
    case 0xC: { // up-down, slow
        int8_t delta = (int8_t)(0x30 - (uint8_t)(oscillatory.state[3][0] >> 8));
        scratch->raw_y.f.u = (int16_t)(scratch->orig_y + delta);
        break;
    }
    }
}

static void Plat_ChkDel(Object *obj, Scratch_BasicPlatform *scratch) {
    if (IS_OFFSCREEN(scratch->orig_x)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

void Obj_BasicPlatform(Object *obj) {
    Scratch_BasicPlatform *scratch = (Scratch_BasicPlatform *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;

        obj->tile = TILE_MAP(0, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3 (real hardware's palette lines are 1-indexed -- Tile_Pal3 = 0-indexed line 2)
        obj->mappings = Mappings_GHZPlatforms;
        obj->width_pixels = 64 / 2;

        if (LEVEL_ZONE(level_id) == ZoneId_SYZ) {
            obj->mappings = Mappings_SYZPlatforms;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SLZ) {
            obj->mappings = Mappings_SLZPlatforms;
            scratch->subtype = 3; // SLZ always forces Plat_FallAfterStand, regardless of level data
        }

        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 4;
        scratch->raw_y.f.u = obj->pos.l.y.f.u;
        scratch->orig_y = obj->pos.l.y.f.u;
        scratch->orig_x = obj->pos.l.x.f.u;
        obj->angle = 0x80; // begin oscillating movement from the center

        obj->frame = 0;
        if (scratch->subtype == 0xA) {
            obj->frame = 1; // large vertical GHZ2 platform
            obj->width_pixels = 64 / 2;
        }
        // Fallthrough
    case 2: // Solid
        if (scratch->nudge == 0) {
            // reduce nudging while Sonic isn't on the platform
        } else {
            scratch->nudge -= 4;
        }
        PlatformObject(obj, obj->width_pixels);
        // Fallthrough
    case 8: // Action
        Plat_Move(obj, scratch);
        Plat_Nudge(obj, scratch);
        Plat_ChkDel(obj, scratch);
        break;
    case 4: { // StoodOn
        if (scratch->nudge != 0x40)
            scratch->nudge += 4;

        // ExitPlatform (shared helper, see its own definition in Object.c)
        // sets obj->routine back to 2 and clears both objects' stood-on
        // flags once Sonic steps/jumps off -- but the rest of THIS frame
        // keeps running regardless (matches the real disasm exactly: no
        // branch on ExitPlatform's own result here), so the platform still
        // moves and repositions Sonic one final time even on the exact
        // frame he leaves it.
        int16_t prev_x = obj->pos.l.x.f.u;
        ExitPlatform(obj, obj->width_pixels, obj->width_pixels, NULL);

        Plat_Move(obj, scratch);
        Plat_Nudge(obj, scratch);
        MvSonicOnPtfm(obj, obj->pos.l.y.f.u - 8, prev_x);

        Plat_ChkDel(obj, scratch);
        break;
    }
    case 6: // Delete
        ObjectDelete(obj);
        break;
    }
}
