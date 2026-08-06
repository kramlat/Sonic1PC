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

#define TO_ADD SCREEN_WIDEADD2
#define FROM_ADD (TO_ADD + ((SCREEN_WIDEADD2 + 0xF) & ~0xF))
#define FROM_SUB ((0x10 - TO_ADD) & 0xF)

// Title card configuration
static const struct TitleCard_Item {
    int16_t y;
    uint8_t routine, frame;
} titlecard_item[4] = {
    { 0x00D0 + SCREEN_TALLADD2, 0x02, 0x00 },
    { 0x00E4 + SCREEN_TALLADD2, 0x02, 0x06 },
    { 0x00EA + SCREEN_TALLADD2, 0x02, 0x07 },
    { 0x00E0 + SCREEN_TALLADD2, 0x02, 0x0A },
};

static const struct TitleCard_Config {
    int16_t x0, x1;
} titlecard_config[7][4] = {
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x0104 - FROM_SUB, 0x013C + TO_ADD }, { 0x0414 + FROM_ADD, 0x0154 + TO_ADD }, { 0x0214 + FROM_ADD, 0x0154 + TO_ADD } }, // GHZ
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x010C - FROM_SUB, 0x0134 + TO_ADD }, { 0x040C + FROM_ADD, 0x014C + TO_ADD }, { 0x020C + FROM_ADD, 0x014C + TO_ADD } }, // LZ
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x0120 - FROM_SUB, 0x0120 + TO_ADD }, { 0x03F8 + FROM_ADD, 0x0138 + TO_ADD }, { 0x01F8 + FROM_ADD, 0x0138 + TO_ADD } }, // MZ
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x0104 - FROM_SUB, 0x013C + TO_ADD }, { 0x0414 + FROM_ADD, 0x0154 + TO_ADD }, { 0x0214 + FROM_ADD, 0x0154 + TO_ADD } }, // SLZ
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x00FC - FROM_SUB, 0x0144 + TO_ADD }, { 0x041C + FROM_ADD, 0x015C + TO_ADD }, { 0x021C + FROM_ADD, 0x015C + TO_ADD } }, // SYZ
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x00FC - FROM_SUB, 0x0144 + TO_ADD }, { 0x041C + FROM_ADD, 0x015C + TO_ADD }, { 0x021C + FROM_ADD, 0x015C + TO_ADD } }, // SBZ
    { { 0x0000 - FROM_SUB, 0x0120 + TO_ADD }, { -0x011C - FROM_SUB, 0x0124 + TO_ADD }, { 0x03EC + FROM_ADD, 0x03EC + TO_ADD }, { 0x01EC + FROM_ADD, 0x012C + TO_ADD } }, // FZ
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
            a1->tile = TILE_MAP(1, 0, 0, 0, 0x580);
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
            // Reached target position.
            //TODO: the original also checks here whether the Scrap Brain
            //Zone Act 2 post-level cutscene should start (a one-off boss
            //transition sequence, not ported yet); skipped for now.

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

            //TODO: giant ring entry isn't implemented yet -- big_ring is
            //never set true anywhere, so this always takes the restart path.
            if (big_ring)
                gamemode = GameMode_Special;
            else
                restart = 1;
        }
        break;
    }
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

        // Get configuration
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
            a1->tile = TILE_MAP(1, 0, 0, 0, 0x580);
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
