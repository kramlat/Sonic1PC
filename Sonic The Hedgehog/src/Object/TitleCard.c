#define Titlecard_Build
#include "TitleCard.h"

#include "Constants.h"
#include "Game.h"
#include "Level.h"
#include "PLC.h"
#include "Sound.h"

#include "Resource/Mappings/TitleCard.h"
#include "Resource/Mappings/GotThrough.h"
#include "Resource/Mappings/SpecialResult.h"

// Title card configuration. The positions depend on the width and height of the picture, which are chosen at run time, so
// the tables are built where they are used (see the title card's creation routine).
struct TitleCard_Item {
    int16_t y;
    uint8_t routine, frame;
};

struct TitleCard_Config {
    int16_t x0, x1;
};

// "Got through" (end of act) card item data, transcribed from Got_ItemData
static const struct GotCard_Item {
    int16_t x0, x1; // start / target x-position
    int16_t y;
    uint8_t routine, frame;
} gotcard_item[7] = {
    { 0x0004, 0x0124, 0x00BC, 2, 0 }, // "SONIC HAS"
    { -0x0120, 0x0120, 0x00D0, 2, 1 }, // "PASSED"
    { 0x040C, 0x014C, 0x00D6, 2, 6 }, // "ACT" 1/2/3 (dynamic frame, see below)
    { 0x0520, 0x0120, 0x00EC, 2, 2 }, // Score tally
    { 0x0540, 0x0120, 0x00FC, 2, 3 }, // Time Bonus tally
    { 0x0560, 0x0120, 0x010C, 2, 4 }, // Ring Bonus tally
    { 0x020C, 0x014C, 0x00CC, 2, 5 }, // Blue oval
};

// Next-act lookup, indexed by [zone][act]. Transcribed from LevelOrder.asm
// (extracted from this same object in the original). An entry of 0
// (LEVEL_ID(ZoneId_GHZ, 0)) means "no next level configured", matching the
// original ("technically GHZ1"; dead code there in practice).
//
// SBZ Act 2 -> Act 3, and Act 3 -> Final Zone, are intentionally NOT
// included here: the original hardcodes those via a unique, one-off cutscene
// (Boundary_Bottom / DynamicLevelEvents), not the normal got-through-card
// flow, and that cutscene isn't ported yet either.
static const uint16_t level_order[ZoneId_Num][4] = {
    /* GHZ */ { LEVEL_ID(ZoneId_GHZ, 1), LEVEL_ID(ZoneId_GHZ, 2), LEVEL_ID(ZoneId_MZ, 0), 0 },
    /* LZ  */ { LEVEL_ID(ZoneId_LZ, 1),  LEVEL_ID(ZoneId_LZ, 2),  LEVEL_ID(ZoneId_SLZ, 0), 0 /* -> SBZ act 3, unhandled */ },
    /* MZ  */ { LEVEL_ID(ZoneId_MZ, 1),  LEVEL_ID(ZoneId_MZ, 2),  LEVEL_ID(ZoneId_SYZ, 0), 0 },
    /* SLZ */ { LEVEL_ID(ZoneId_SLZ, 1), LEVEL_ID(ZoneId_SLZ, 2), LEVEL_ID(ZoneId_SBZ, 0), 0 },
    /* SYZ */ { LEVEL_ID(ZoneId_SYZ, 1), LEVEL_ID(ZoneId_SYZ, 2), LEVEL_ID(ZoneId_LZ, 0),  0 },
    /* SBZ */ { LEVEL_ID(ZoneId_SBZ, 1), 0 /* SBZ act 2, unhandled */, 0 /* Final Zone, unhandled */, 0 },
    /* EndZ (unused) */ { 0, 0, 0, 0 },
};

// SBZ2 post-level cutscene: has the Ring Bonus element (the one that drives the tally) reached its slide-out
// routine? (real: cmpi.b #$E,(v_endcardring+obRoutine))
static bool GotCard_SlideOutStarted(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == ObjId_GotThroughCard && o->frame == 4)
            return o->routine == 0xE;
    }
    return false;
}

// Got_SBZ2_MoveOut: every element slides back to where it started, twice as fast as it came in. The Ring Bonus
// element then hands control back to the player and starts the Final Zone music; the rest delete themselves.
static void GotCard_SBZ2_MoveOut(Object *obj, Scratch_TitleCard *scratch) {
    int16_t speed = 0x20;
    if (obj->pos.s.x == scratch->final_x) {
        if (obj->frame != 4) {
            ObjectDelete(obj);
            return;
        }
        obj->routine += 2; // Got_SBZ2_Boundary
        lock_ctrl = false; // unlock controls
        QueueSound1(bgm_FZ);
        return;
    }
    if (obj->pos.s.x > scratch->final_x)
        speed = -speed;
    obj->pos.s.x += speed;
    if (obj->pos.s.x >= 0 && obj->pos.s.x < (0x200 + SCREEN_WIDEADD))
        DisplaySprite(obj);
}

// "Got through" (end of act) card object
void Obj_GotThroughCard(Object *obj) {
    Scratch_TitleCard *scratch = (Scratch_TitleCard*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Wait for title card patterns to finish decompressing
        if (plc_buffer[0].art != NULL)
            return;

        // Fallthrough
    {
        // Spawn the 7 card elements (this object plus the next 6 in sequence)
        Object *a1 = obj;
        const struct GotCard_Item *item = gotcard_item;
        for (int i = 0; i < 7; i++, item++, a1++) {
            Scratch_TitleCard *scratch_a1 = (Scratch_TitleCard*)&a1->scratch;
            a1->type = ObjId_GotThroughCard;
            a1->pos.s.x = item->x0;
            scratch_a1->final_x = item->x0;
            scratch_a1->main_x = item->x1;
            a1->pos.s.y = item->y;
            a1->routine = item->routine;

            uint8_t frame = item->frame;
            if (frame == 6) // "ACT" element: add the current act number
                frame += LEVEL_ACT(level_id);
            a1->frame = frame;
            a1->mappings = Mappings_GotThrough;
            a1->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Title_Card);
            a1->width_pixels = 0;
            a1->render.b = 0;
            a1->priority = 0;
            a1->frame_time.b = 60;
        }
        return;
    }
    case 2: { // Moving to on-screen position
        int16_t speed = 0x10;
        if (obj->pos.s.x == scratch->main_x) {
            // Reached target position. In SBZ2 the card doesn't lead to the next act: once the Ring Bonus element
            // (which runs the tally) gets to its slide-out routine, every element slides back out.
            if (GotCard_SlideOutStarted()) {
                obj->routine = 0xE; // Got_SBZ2_MoveOut
                GotCard_SBZ2_MoveOut(obj, scratch);
                return; // (it displays itself)
            }

            if (obj->frame != 4) // Only the Ring Bonus element controls this
                break;

            obj->routine += 2; // -> Got_Wait
            obj->frame_time.w = 3 * 60; // 3 second delay before tally
            break;
        } else if (obj->pos.s.x > scratch->main_x) {
            speed = -speed;
        }
        obj->pos.s.x += speed;
        break;
    }
    case 4: // Wait routines: hold for a timer, then advance
    case 8:
    case 0xC: // (SBZ2: the post-tally wait, before sliding out)
        if (--obj->frame_time.w == 0)
            obj->routine += 2;
        break;
    case 6: { // Score tally
        uint16_t ticked = 0;

        if (time_bonus) {
            ticked += 10;
            time_bonus -= 10;
        }
        if (ring_bonus) {
            ticked += 10;
            ring_bonus -= 10;
        }

        if (ticked) {
            AddPoints(ticked);
            PlaySound(sfx_Switch);
        } else {
            PlaySound(sfx_Cash);
            obj->routine += 2; // -> Got_Wait, before Got_NextLevel
            if (level_id == LEVEL_ID(ZoneId_SBZ, 1))
                obj->routine += 4; // SBZ2: -> Got_Wait ($C), before Got_SBZ2_MoveOut -- no next act, the cutscene follows
            obj->frame_time.w = 3 * 60; // 3 second post-tally delay
        }
        break;
    }
    case 0xA: { // Advance to the next act
        uint16_t next = level_order[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)];
        level_id = next;

        if (next == 0) {
            // Matches the original: an unconfigured entry (GHZ1) returns to
            // the Sega screen instead. Dead code in practice.
            gamemode = GameMode_Sega;
        } else {
            last_lamp = 0;

            // Sonic jumped into a giant ring (Obj_RingFlash set big_ring): the next screen is a special stage
            if (big_ring)
                gamemode = GameMode_Special;
            else
                restart = 1;
        }
        break;
    }
    case 0xE: // Got_SBZ2_MoveOut
        GotCard_SBZ2_MoveOut(obj, scratch);
        return;
    case 0x10: // Got_SBZ2_Boundary: the Ring Bonus element opens the screen's right boundary for the cutscene room
        limit_right2 += 2;
        if (limit_right2 == 0x2050 + 0xB0) // boss_sbz2_x+$B0
            ObjectDelete(obj);
        return;
    }

    // Draw (card stays fully on-screen throughout this whole sequence)
    if (obj->pos.s.x >= 0 && obj->pos.s.x < (0x200 + SCREEN_WIDEADD))
        DisplaySprite(obj);
}

// Title card object
void Obj_TitleCard(Object *obj) {
    Scratch_TitleCard *scratch = (Scratch_TitleCard*)&obj->scratch;

    switch (obj->routine) {
    case 0: {
        Object* a1 = obj;

        // Get title card to render
        uint8_t d0 = LEVEL_ZONE(level_id);
        if (level_id == LEVEL_ID(ZoneId_LZ, 3))
            d0 = 5;

        uint16_t d2 = d0;
        if (level_id == LEVEL_ID(ZoneId_SBZ, 2)) {
            d0 = 6;
            d2 = 11;
        }

        // Get configuration (built now: it depends on the size of the picture)
        const int16_t to_add = SCREEN_WIDEADD2;
        const int16_t from_add = to_add + ((SCREEN_WIDEADD2 + 0xF) & ~0xF);
        const int16_t from_sub = (0x10 - to_add) & 0xF;
        (void)from_add; (void)from_sub;
        const struct TitleCard_Item titlecard_item[4] = {
    { 0x00D0 + SCREEN_TALLADD2, 0x02, 0x00 },
    { 0x00E4 + SCREEN_TALLADD2, 0x02, 0x06 },
    { 0x00EA + SCREEN_TALLADD2, 0x02, 0x07 },
    { 0x00E0 + SCREEN_TALLADD2, 0x02, 0x0A },
};
        const struct TitleCard_Config titlecard_config[7][4] = {
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x0104 - from_sub, 0x013C + to_add }, { 0x0414 + from_add, 0x0154 + to_add }, { 0x0214 + from_add, 0x0154 + to_add } }, // GHZ
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x010C - from_sub, 0x0134 + to_add }, { 0x040C + from_add, 0x014C + to_add }, { 0x020C + from_add, 0x014C + to_add } }, // LZ
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x0120 - from_sub, 0x0120 + to_add }, { 0x03F8 + from_add, 0x0138 + to_add }, { 0x01F8 + from_add, 0x0138 + to_add } }, // MZ
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x0104 - from_sub, 0x013C + to_add }, { 0x0414 + from_add, 0x0154 + to_add }, { 0x0214 + from_add, 0x0154 + to_add } }, // SLZ
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x00FC - from_sub, 0x0144 + to_add }, { 0x041C + from_add, 0x015C + to_add }, { 0x021C + from_add, 0x015C + to_add } }, // SYZ
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x00FC - from_sub, 0x0144 + to_add }, { 0x041C + from_add, 0x015C + to_add }, { 0x021C + from_add, 0x015C + to_add } }, // SBZ
    { { 0x0000 - from_sub, 0x0120 + to_add }, { -0x011C - from_sub, 0x0124 + to_add }, { 0x03EC + from_add, 0x03EC + to_add }, { 0x01EC + from_add, 0x012C + to_add } }, // FZ
};
        const struct TitleCard_Config* config = &titlecard_config[d0][0];
        const struct TitleCard_Item* item = titlecard_item;

        // Create card objects
        for (int i = 0; i < 4; i++, config++, item++, a1++) {
            // Write object info from configuration
            Scratch_TitleCard* scratch_a1 = (Scratch_TitleCard*)&a1->scratch;
            a1->type = ObjId_TitleCard;
            a1->pos.s.x = config->x0;
            scratch_a1->final_x = config->x0;
            scratch_a1->main_x = config->x1;
            a1->pos.s.y = item->y;
            a1->routine = item->routine;
            if ((d0 = item->frame) == 0)
                d0 = d2;

            // Initialize object graphics
            if (d0 == 7) {
                d0 += LEVEL_ACT(level_id);
                if (LEVEL_ACT(level_id) == 3)
                    d0--;
            }

            a1->frame = d0;
            a1->mappings = Mappings_TitleCard;
            a1->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Title_Card);
            a1->width_pixels = 0;
            a1->render.b = 0;
            a1->priority = 0;
            a1->frame_time.b = 60;
        }
    }
        // Fallthrough
    case 2: // Moving to on-screen position
        // Move
        if (obj->pos.s.x > scratch->main_x)
            obj->pos.s.x -= 16;
        else if (obj->pos.s.x < scratch->main_x)
            obj->pos.s.x += 16;

        // Draw
        if (obj->pos.s.x >= 0 && obj->pos.s.x < (0x200 + SCREEN_WIDEADD))
            DisplaySprite(obj);
        break;
    case 4: // Moving off-screen
    case 6:
        // Wait for timer to expire
        if (obj->frame_time.b) {
            obj->frame_time.b--;
            DisplaySprite(obj);
            break;
        }

        // Delete and/or load level art once off-screen
        if (!obj->render.f.on_screen || obj->pos.s.x == scratch->final_x) {
            if (obj->routine == 4) {
                AddPLC(PlcId_Explode);
                AddPLC(PlcId_GHZAnimals + LEVEL_ZONE(level_id));
            }
            ObjectDelete(obj);
            break;
        }

        // Move
        if (obj->pos.s.x >= scratch->final_x)
            obj->pos.s.x -= 16;
        else
            obj->pos.s.x += 16;

        // Draw
        if (obj->pos.s.x >= 0 && obj->pos.s.x < (0x200 + SCREEN_WIDEADD))
            DisplaySprite(obj);
        break;
    }
}
